#include <cstring>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <stdlib.h>
#include <perfmon/perf_event.h>
#include <perfmon/pfmlib.h>
#include <perfmon/pfmlib_perf_event.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <fcntl.h>
#include "monitor.hh"
#include "threads.hh"
#include "clock.hh"
#include "nmo_exception.hh"

#define RING_BUFFER_PAGES 128
#define PAGE_SIZE 4096

#define EVENTFD_INDEX (-1)
#define SAMPLER_DRAIN 1
#define SAMPLER_STOP 2

perf_event_attr init_perf_attr(const char *event_name, bool per_thread, uint64_t sample_period)
{
    perf_event_attr attr;
    memset(&attr, 0, sizeof(perf_event_attr));
    attr.size = sizeof(perf_event_attr);
    pfm_perf_encode_arg_t arg;
    memset(&arg, 0, sizeof(arg));
    arg.size = sizeof (pfm_perf_encode_arg_t);
    arg.attr = &attr;
    char *fstr;
    arg.fstr = &fstr;
    if (PFM_SUCCESS != pfm_get_os_event_encoding(event_name,
            PFM_PLM0 | PFM_PLM3, PFM_OS_PERF_EVENT, &arg))
    {
        std::stringstream ss;
        ss << "pfm_get_os_event_encoding "<< event_name <<" fail";
        throw NmoException(ss.str());
    }
    else
    {
        //printf("pfm_get_os_event_encoding %s successfully \n", event_name);
    }

    if (!per_thread) {
        attr.inherit = 1;
    }

    if (sample_period) {
        attr.sample_period = sample_period;
        attr.sample_type = PERF_SAMPLE_ADDR | PERF_SAMPLE_TIME;

        // Enable PEBS
        attr.precise_ip = 2;

        attr.wakeup_events = SAMPLE_WAKEUP;
        //attr.watermark = 1;
        //attr.wakeup_watermark = 4096; // wakeup after 1 page filled

        // Prevent multiplexing. Maybe we will need it later, if we track more events.
        attr.pinned = 1;
    }

    attr.disabled = 1;


    //include fork notifications in ring buffer
    //attr.task = 1;

    attr.use_clockid = 1;
    attr.clockid = CLOCK_MONOTONIC_RAW;

    attr.exclude_kernel = 1;
    attr.exclude_hv = 1;

    attr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;

    return attr;
}

void open_fds(int pid, bool per_thread, int cpu, const event_spec spec, uint64_t sample_period, int *fds)
{
    int group_fd = -1;

    for (int i = 0; i < spec.n; i++) {
        // Start of new group marker
        if (spec.event_leader[i])
            group_fd = -1;

        perf_event_attr attr = init_perf_attr(spec.event_name[i], per_thread, sample_period);

        errno = 0;
        int fd = perf_event_open(&attr, pid, cpu, group_fd, 0);
        if (fd < 0) {
            std::stringstream ss;
            ss << "perf_event_open failed [i="<< i << ", errno=" << strerror(errno) << "]";
            throw NmoException(ss.str());
        }
        fds[i] = fd;

        ioctl(fd, PERF_EVENT_IOC_RESET, 0);

        // First member of group is leader
        if (group_fd == -1)
            group_fd = fd;
    }
}

Monitor::Monitor(const event_spec counter_spec, const event_spec sampler_spec, uint64_t sample_period, const char *name)
    : _num_counters(counter_spec.n)
    , _num_samplers(sampler_spec.n)
    , _num_cpus(get_nprocs())
    , _sample_period(sample_period)
    , _counter_spec(counter_spec)
    , _sampler_spec(sampler_spec)
    , _nonstop_mode(false)
    , _event_fd(-1)
    , _epoll_fd(-1)
    , _sampler_throttles(0)
{
    char fn[128];

    if (_num_samplers > 0)
        _sample_writers = new BinaryWriter[_num_samplers];
    else
        _sample_writers = nullptr;

    for (int i = 0; i < _num_samplers; i++) {
        snprintf(fn, sizeof(fn), "%s.sample%d", name, i);
        _sample_writers[i] = BinaryWriter(fn);
    }

    snprintf(fn, sizeof(fn), "%s.info", name);
    _info_file = std::ofstream(fn, std::ios::trunc);

    if (PFM_SUCCESS != pfm_initialize())
        throw NmoException("PFM init fail!");

    timespec res = {};
    if (clock_getres(CLOCK_MONOTONIC_RAW, &res) < 0 || res.tv_sec)
        throw NmoException("clock_getres failure (CLOCK_MONOTONIC_RAW)");
    _clock_res = res.tv_nsec*1000000000;

    int64_t sampler_tid = -1;
    if (_num_samplers > 0) {
        _epoll_fd = epoll_create(1024);
        if (_epoll_fd < 0)
            throw NmoException("epoll_create failed");

        _event_fd = eventfd(0, 0);
        if (_event_fd < 0)
            throw NmoException("eventfd failed");

        if (sem_init(&_sampler_drain, 0, 1) < 0) {
            perror("sem_init");
            throw NmoException("sem_init");
        }

        _sampler_thread = std::thread(&Monitor::run_sampler_thread, this);

        // Sampler thread writes its TID to _event_fd
        if (sizeof(sampler_tid) != read(_event_fd, &sampler_tid, sizeof(sampler_tid)))
            throw NmoException("eventfd read failed");

        epoll_event ep_event = {};
        ep_event.events = EPOLLIN;
        ep_event.data.fd = EVENTFD_INDEX;

        // From now, we will use _event_fd to signal to sampler thread to exit
        if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _event_fd, &ep_event) < 0)
            throw NmoException("epoll_ctl");
    }

    _prev_counters = new uint64_t[_num_counters + _num_samplers];
    for (int i = 0; i < _num_counters + _num_samplers; i++)
        _prev_counters[i] = 0;

    // Exclude sampler thread from monitoring
    auto threads = get_threads(sampler_tid);

    _num_threads = threads.size();
    _num_sampler_fds = _num_threads * _num_cpus * _num_samplers;

    _counter_fds = new int*[_num_threads];
    _sampler_fds = new int[_num_sampler_fds];
    _sampler_bufs = new void*[_num_sampler_fds];

    for (int i = 0; i < _num_threads; i++)
        add_thread(threads[i], i, counter_spec, sampler_spec);

    if (get_threads(sampler_tid) != threads) {
        throw NmoException("Thread set changed during Monitor init, results would be unreliable.");
    }

    std::cerr << "Monitor setup, " << _num_threads << " threads, " << _num_counters << " counters/thread, " << _num_samplers << " samplers/thread" << std::endl;
}

void Monitor::write_info(std::ofstream& info)
{
    info << "sample_period=" << _sample_period << std::endl;

    if (_num_samplers) {
        info << "sample_md5=";
        _sample_writers[0].md5_hex(info);
        for (int i = 1; i < _num_samplers; i++) {
            info << ",";
            _sample_writers[i].md5_hex(info);
        }
        info << std::endl;
    }

    info << "clock_res=" << 1.0 / _clock_res << std::endl;

    if (_kinfos.size()) {
        info << "tag=" << _kinfos[0].tag;
        for (size_t i = 1; i < _kinfos.size(); i++)
            info << "," << _kinfos[i].tag;
        info << std::endl;

        info << "offloaded=" << _kinfos[0].offloaded;
        for (size_t i = 1; i < _kinfos.size(); i++)
            info << "," << _kinfos[i].offloaded;
        info << std::endl;

        info << "num_samples=" << _kinfos[0].num_samples;
        for (size_t i = 1; i < _kinfos.size(); i++)
            info << "," << _kinfos[i].num_samples;
        info << std::endl;

        info << "sample_throttles=" << _kinfos[0].sample_throttles;
        uint64_t total_throttles = _kinfos[0].sample_throttles;
        for (size_t i = 1; i < _kinfos.size(); i++) {
            info << "," << _kinfos[i].sample_throttles;
            total_throttles += _kinfos[i].sample_throttles;
        }
        info << std::endl;
        if (total_throttles)
            std::cerr << "warning: sampler throttled " << total_throttles << "times" << std::endl;

        info << "duration=" << _kinfos[0].duration;
        for (size_t i = 1; i < _kinfos.size(); i++)
            info << "," << _kinfos[i].duration;
        info << std::endl;

        info << "clock_start=" << _kinfos[0].clock_start;
        for (size_t i = 1; i < _kinfos.size(); i++)
            info << "," << _kinfos[i].clock_start;
        info << std::endl;

        info << "clock_stop=" << _kinfos[0].clock_stop;
        for (size_t i = 1; i < _kinfos.size(); i++)
            info << "," << _kinfos[i].clock_stop;
        info << std::endl;

        info << "counters_are_not_cumulative=1" << std::endl;

        for (int counter = 0; counter < _num_counters; counter ++) {
            const char *name = _counter_spec.event_name[counter];
            info << name << "=" << _kinfos[0].counters[counter].value;
            for (size_t i = 1; i < _kinfos.size(); i++)
                info << "," << _kinfos[i].counters[counter].value;
            info << std::endl;

            info << "active_" << name << "=" << _kinfos[0].counters[counter].active_time;
            for (size_t i = 1; i < _kinfos.size(); i++)
                info << "," << _kinfos[i].counters[counter].active_time;
            info << std::endl;
        }
        for (int sampler = 0; sampler < _num_samplers; sampler ++) {
            const char *name = _sampler_spec.event_name[sampler];
            info << name << "=" << _kinfos[0].counters[_num_counters + sampler].value;
            for (size_t i = 1; i < _kinfos.size(); i++)
                info << "," << _kinfos[i].counters[_num_counters + sampler].value;
            info << std::endl;

            /* This is always 0.0104167? But values suggest that they are 100% active.
            info << "active_" << name << "=" << _kinfos[0].counters[_num_counters + sampler].active_time;
            for (size_t i = 1; i < _kinfos.size(); i++)
                info << "," << _kinfos[i].counters[_num_counters + sampler].active_time;
            info << std::endl;
            */
        }
    }

    if (_addr_tags.size()) {
        info << "addr_tags=" << _addr_tags[0].tag << ":" << _addr_tags[0].start << ":" << _addr_tags[0].end;
        for (size_t i = 1; i < _addr_tags.size(); i++)
            info << "," << _addr_tags[i].tag << ":" << _addr_tags[i].start << ":" << _addr_tags[i].end;
        info << std::endl;
    }

    if (_phases.size()) {
        info << "phases=" << _phases[0].tag << ":" << _phases[0].time;
        for (size_t i = 1; i < _phases.size(); i++)
            info << "," << _phases[i].tag << ":" << _phases[i].time;
        info << std::endl;
    }
}

void Monitor::mark_phase(const char *tag)
{
    phase_info p = {tag, nano_clock()};
    _phases.push_back(p);
}

Monitor::~Monitor()
{
    if (_event_fd != -1) {
        uint64_t efd_cmd = SAMPLER_STOP;
        if (sizeof(efd_cmd) != write(_event_fd, &efd_cmd, sizeof(efd_cmd))) {
            perror("eventfd write");
            abort();
        }
    }
    if (_sampler_thread.joinable())
        _sampler_thread.join();

    write_info(_info_file);
    _info_file.close();

    size_t total_samples = 0;
    for (auto& ki: _kinfos)
        total_samples += ki.num_samples;

    size_t total_bytes = 0;
    for (int i = 0; i < _num_samplers; i++) {
        total_bytes += _sample_writers[i].tell();
    }
    delete[] _sample_writers;
    _sample_writers = nullptr;

    if (total_bytes != total_samples*sizeof(uint64_t)*2) {
        std::cerr << "warning: Written bytes " << total_bytes << " does not match total samples " << total_samples << std::endl;
    }
}

void Monitor::add_thread(int tid, int i, event_spec counter_spec, event_spec sampler_spec)
{
    _counter_fds[i] = new int[_num_counters];
    open_fds(tid, false, -1, counter_spec, 0, _counter_fds[i]);

    // To enable inherit flag for sampling, we need to configure it once for
    // each cpu.  Otherwise, while perf_event_open() still succeeds, mmaping
    // the ring buffer fails with EINVAL. This behavior is not documented! I
    // suppose it does make sense that one ring buffer should not be shared
    // between two threads possibly executing simultaneously on separate cpus.
    for (int cpu = 0; cpu < _num_cpus; cpu++) {
        int offset = i*_num_cpus*_num_samplers + cpu*_num_samplers;

        open_fds(tid, false, cpu, sampler_spec, _sample_period, _sampler_fds + offset);

        for (int index = offset; index < offset + _num_samplers; index++) {
            void *m = mmap(0, (1 + RING_BUFFER_PAGES) * PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, _sampler_fds[index], 0);
            if (m == MAP_FAILED) {
                std::cerr << "fd=" << _sampler_fds[index] << " index=" << index << std::endl;
                perror("mmap");
                throw NmoException("mapping sampler ring buffer failed");
            }

            _sampler_bufs[index] = m;

            // data.fd == index into sampler arrays
            epoll_event ep_event = {};
            ep_event.events = EPOLLIN;
            ep_event.data.fd = index;

            if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _sampler_fds[index], &ep_event) < 0)
                throw NmoException("epoll_ctl");
        }
    }
}

void Monitor::run_sampler_thread()
{
    int64_t tid = gettid();
    if (write(_event_fd, &tid, sizeof(tid)) != sizeof(tid))
        throw NmoException("eventfd write failed");

    uint64_t efd_cmd = 0;

    const size_t MAX_EVENTS = 1024;
    struct epoll_event events[MAX_EVENTS];
    for (;;) {
        for (;;) {
            int n = epoll_wait(_epoll_fd, events, MAX_EVENTS, -1);

            if (n < 0) {
                if (errno == EINTR)
                    continue;

                perror("epoll_wait");
                throw NmoException("epoll_wait");
            }

            for (int i = 0; i < n; i++) {
                int index = events[i].data.fd;

                if (index == EVENTFD_INDEX) {
                    if (sizeof(efd_cmd) != read(_event_fd, &efd_cmd, sizeof(efd_cmd)))
                        throw NmoException("eventfd read");
                    goto drain;
                } else {
                    process_samples(index);
                }
            }
        }

drain:
        size_t extra_samples = 0;

        // Drain buffers
        for (int i = 0; i < _num_sampler_fds; i++) {
            extra_samples += process_samples(i);
        }

        /* Gives zeros for all but the last buffer?
        for (int i = 0; i < _num_sampler_fds; i++) {
            perf_event_mmap_page *header = (perf_event_mmap_page *) _sampler_bufs[i];
            std::cout << i << ":" << header->time_enabled << "-" << header->time_running << std::endl;
        }
        */

        if (efd_cmd == SAMPLER_STOP && extra_samples && !_nonstop_mode)
            std::cerr << "Found extra samples in buffer while stopping, forgot to call nmo_stop()?" << std::endl;

        if (efd_cmd != SAMPLER_STOP || extra_samples) {
            sem_post(&_sampler_drain);
        }

        if (efd_cmd == SAMPLER_STOP)
            break;
    }
}

size_t Monitor::process_samples(int index)
{

    perf_event_mmap_page *buf_header = (perf_event_mmap_page *) _sampler_bufs[index];
    uint64_t data_head = buf_header->data_head;
    uint64_t data_tail = buf_header->data_tail;

    // Memory barrier, according to man page we need rmb after data_head read.
    __sync_synchronize();

    if (data_head == data_tail)
        return 0;

    size_t num_samples = 0;

    while (data_tail < data_head) {
        char *data_start = (char*)_sampler_bufs[index] + PAGE_SIZE;
        size_t data_size = RING_BUFFER_PAGES*PAGE_SIZE;

        // header = u64
        perf_event_header *header = (perf_event_header*)(data_start + (data_tail % data_size));

        /*
        if (((char*)sample) + sample->header.size > ((char*)_sampler_bufs[index]+ PAGE_SIZE*(1+RING_BUFFER_PAGES)))
            std::cerr << "error: sample is not aligned to ring buffer" << std::endl;
        */

        switch (header->type) {
        case PERF_RECORD_SAMPLE:
        {
            struct
            {
                uint64_t time;
                uint64_t addr;
            } sample;

            sample.time = *(uint64_t*)(data_start + ((data_tail + sizeof(perf_event_header)) % data_size));
            sample.addr = *(uint64_t*)(data_start + ((data_tail + sizeof(perf_event_header) + sizeof(sample.time)) % data_size));

            int event = index % _num_samplers;
            _sample_writers[event].write((char*)&sample, sizeof(sample));
            num_samples++;
            break;
        }

        case PERF_RECORD_THROTTLE:
            _kinfos.back().sample_throttles += 1;
            break;

        case PERF_RECORD_UNTHROTTLE:
            break;

        default:
            std::cerr << "unknown record type " << header->type << std::endl;
            break;
        }

        data_tail += header->size;
    }

    if (data_head != data_tail)
        throw NmoException("corrupted samples mapping");

    __sync_synchronize();
    buf_header->data_tail = data_tail;

    _kinfos.back().num_samples += num_samples;

    return num_samples;
}

void Monitor::fds_ioctl(int request)
{
    for (int i = 0; i < _num_threads; i++)
        for (int j = 0; j < _num_counters; j++)
            ioctl(_counter_fds[i][j], request, 0);

    for (int i = 0; i < _num_sampler_fds; i++)
        ioctl(_sampler_fds[i], request, 0);
}

void Monitor::reset()
{
    fds_ioctl(PERF_EVENT_IOC_RESET);
}

void Monitor::start(const char *tag, bool offloaded)
{
    _kinfos.push_back({});

    if (tag) {
        if (strpbrk(tag, "=\n")) {
            char s[128];
            snprintf(s, sizeof(s), "Invalid tag '%s'", tag);
            throw NmoException(s);
        }
        _kinfos.back().tag.assign(tag);
    } else
        _kinfos.back().tag.assign("?");

    _kinfos.back().offloaded = offloaded;

    if (!_nonstop_mode && _num_samplers)
        sem_wait(&_sampler_drain);

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &_start_time) < 0)
        throw NmoException("failed clock_gettime");

    if (!_nonstop_mode)
        fds_ioctl(PERF_EVENT_IOC_ENABLE);
}

void Monitor::stop()
{
    if (!_nonstop_mode)
        fds_ioctl(PERF_EVENT_IOC_DISABLE);

    timespec stop_time;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &stop_time) < 0)
        throw NmoException("failed clock_gettime");

    if (!_nonstop_mode && _event_fd != -1) {
        // Notify sampler thread to drain
        int64_t val = SAMPLER_DRAIN;
        if (write(_event_fd, &val, sizeof(val)) != sizeof(val))
            throw NmoException("eventfd write failed");
    }

    kinfo& ki = _kinfos.back();

    ki.duration = (stop_time.tv_sec - _start_time.tv_sec) + 1e-9*(stop_time.tv_nsec - _start_time.tv_nsec);

    ki.clock_start = _start_time.tv_sec * _clock_res + _start_time.tv_nsec * _clock_res / 1000000000;
    ki.clock_stop = stop_time.tv_sec * _clock_res + stop_time.tv_nsec * _clock_res / 1000000000;

    ki.counters = new counter_data[_num_counters + _num_samplers];
    read_counters(ki.counters, true);
}

struct read_format {
    uint64_t value;
    uint64_t time_enabled;
    uint64_t time_running;
};

read_format read_fd(int fd)
{
    read_format count;
    ssize_t s = read(fd, &count, sizeof(count));
    if (s != sizeof(count))
        throw NmoException("read() on event fd failed");
    return count;
}

void Monitor::read_counters(counter_data *counters, bool include_samplers)
{
    for (int counter = 0; counter < _num_counters; counter++) {
        uint64_t enabled = 0;
        uint64_t running = 0;
        uint64_t value = 0;

        for (int thread = 0; thread < _num_threads; thread++) {
            read_format count = read_fd(_counter_fds[thread][counter]);
            value += count.value;
            enabled += count.time_enabled;
            running += count.time_running;
        }

        counters[counter].value = value - _prev_counters[counter];
        _prev_counters[counter] = value;

        counters[counter].active_time = running / (double)enabled;
    }

    if (include_samplers) {
        for (int sampler = 0; sampler < _num_samplers; sampler++) {
            uint64_t enabled = 0;
            uint64_t running = 0;
            uint64_t value = 0;

            for (int thread = 0; thread < _num_threads; thread++) {
                for (int cpu = 0; cpu < _num_cpus; cpu++) {
                    int index = thread*_num_cpus*_num_samplers + cpu*_num_samplers;

                    read_format count = read_fd(_sampler_fds[index]);
                    value += count.value;
                    enabled += count.time_enabled;
                    running += count.time_running;
                }
            }

            int off = _num_counters + sampler;
            counters[off].value = value - _prev_counters[off];
            _prev_counters[off] = value;
            counters[off].active_time = running / (double)enabled;
        }
    }
}

void Monitor::tag_addr(const char *tag, void *start, void *end)
{
    _addr_tags.push_back({tag, start, end});
}

BinaryWriter::BinaryWriter(const char *path)
    : _ofs(path, std::ios::binary|std::ios::trunc)
{
    MD5_Init(&_md5);
}

void BinaryWriter::write(char *a, size_t n)
{
    _ofs.write(a, n);
    MD5_Update(&_md5, a, n);
}

std::ostream& BinaryWriter::md5_hex(std::ostream &os)
{
    uint8_t digest[MD5_DIGEST_LENGTH];
    MD5_Final(digest, &_md5);
    char hex[MD5_DIGEST_LENGTH*2+1];
    for (int i = 0; i< MD5_DIGEST_LENGTH; i++)
        sprintf(hex+2*i, "%02x", digest[i]);

    os << hex;

    return os;
}
