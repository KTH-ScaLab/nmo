#ifndef mt_MONITOR_H
#define mt_MONITOR_H

#include <vector>
#include <fstream>
#include <thread>
#include <semaphore.h>
#include <openssl/md5.h>

#define SAMPLE_WAKEUP 1024
#define MAX_TAG 64

struct event_spec
{
    const int n;
    const char **event_name;
    const bool *event_leader;
};

struct counter_data
{
    uint64_t value;
    double active_time;
};

struct kinfo
{
    std::string tag;
    size_t num_samples;
    size_t sample_throttles;
    double duration;
    counter_data *counters;
    uint64_t clock_start;
    uint64_t clock_stop;
    bool offloaded;
};

struct phase_info
{
    std::string tag;
    uint64_t time;
};

struct addr_tag
{
    std::string tag;
    void *start;
    void *end;
};

class BinaryWriter
{
public:
    BinaryWriter() {}
    BinaryWriter(const char *path);
    void write(char *a, size_t n);
    std::ostream& md5_hex(std::ostream &os);

    size_t tell()
    {
        if (_ofs.is_open())
            return _ofs.tellp();
        else
            return 0;
    }

private:
    std::ofstream _ofs;
    MD5_CTX _md5;
};

class Monitor
{
public:
    Monitor(const event_spec counter_spec, const event_spec sampler_spec, uint64_t sample_period,
        int ringbufsize, int auxbufsize, const char *name);
    ~Monitor();
    void reset();
    void start(const char *tag=0, bool offloaded=false);
    void stop();
    void read_counters(counter_data *counters, bool include_samplers);
    void read_samples();
    void tag_addr(const char *tag, void *start, void *end);
    void mark_phase(const char *tag);

    kinfo& last_kinfo()
    {
        return _kinfos.back();
    }

    void set_nonstop()
    {
        _nonstop_mode = true;
    }

private:
    const int _num_counters;
    const int _num_samplers;

    // We assume cpus are numbered [0,n)
    const int _num_cpus;

    const uint64_t _sample_period;

    const int _ringbufsize;
    const int _auxbufsize;

    // Lifetime of members?
    const event_spec _counter_spec;
    const event_spec _sampler_spec;

    bool _nonstop_mode;

    // Note: these are the initial threads at initialization. New threads then
    // inherit the counters and samplers.
    int _num_threads;

    int _num_sampler_fds;

    uint64_t* _prev_counters;

    // [thread][counter]
    int **_counter_fds;

    // [thread][cpu][sampler] (flattened)
    int *_sampler_fds;
    void **_sampler_bufs;

    std::thread _sampler_thread;
    int _event_fd;
    int _epoll_fd;
    unsigned long _sampler_throttles;

    // The purpose of this semaphore is to ensure that samples from different
    // kernel invokations are not mixed together. Also, it acts as a mutex
    // to avoid concurrent access from the sampler thread and main thread.
    sem_t _sampler_drain;

    BinaryWriter* _sample_writers;
    std::ofstream _info_file;
    std::vector<kinfo> _kinfos;

    timespec _start_time;
    uint64_t _clock_res;

    std::vector<addr_tag> _addr_tags;
    std::vector<phase_info> _phases;

    void add_thread(int tid, int i, event_spec counter_spec, event_spec sampler_spec);
    void fds_ioctl(int request);
    void run_sampler_thread();
    size_t process_samples(int index);
    void write_info(std::ofstream& info);
};

#endif
