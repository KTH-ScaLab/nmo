#include <iostream>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include "nmo_exception.hh"
#include "threads.hh"
#include "rss_tracker.hh"
#include "nmo.h"
#include "periodic_profiler.hh"

#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 30
#include <sys/syscall.h>
#define gettid() syscall(SYS_gettid)
#endif

static RssTracker* lib_rss_tracker;
static PeriodicProfiler *lib_perp;

static int read_period()
{
    char *env = getenv("NMO_PERIOD");
    if (env) {
        return atoi(env);
    }
    return 0;
}

int parse_bufsize(const char* env_bufsize)
{
    int bufsize = atoi(env_bufsize);
    if (bufsize) {
        //Round down to power of 2
        int n = 1;
        while (n <= bufsize) {
            n = n << 1;
        }
        //While loop goes one step too far, shift down one step
        return n >> 1;
    }

    // Return a buffer size of 1 (MiB) by default
    return 1;
}

__attribute__((constructor)) 
static void lib_init()
{
    if (!getenv("NMO_ENABLE"))
        return;

    std::ifstream commf("/proc/self/comm");
    char comm[256];
    commf.get(comm, sizeof(comm));
    commf.close();

    const char *target = getenv("NMO_TARGET");
    if (target && *target)
        if (strcmp(comm, target))
            return;

    // Prevent child processes (like OMPI's orted) from being individually
    // tracked and profiled.
    unsetenv("NMO_ENABLE");

    uint64_t period = read_period();
    bool track_rss = !!getenv("NMO_TRACK_RSS");

    const char *name = getenv("NMO_NAME");
    if (!name)
        name = "nmo";
    const char *pidname = getenv("NMO_PIDNAME");
    if (pidname && atoi(pidname)) {
        int len = strlen(name);
        char *namebuf = (char *)malloc(len + 20);
        memcpy(namebuf, name, len);
        sprintf(namebuf+len, "%d", getpid());
        name = namebuf;
    }

    perp_mode perp_mode = PERP_OFF;
    const char *mode = getenv("NMO_MODE");

    if (mode && *mode) {
        if (!strcmp("perp", mode)) {
            perp_mode = PERP_ROOFLINE;
        } else if (!strcmp("noperp", mode)) {
            perp_mode = PERP_NONE;
        } else if (!strcmp("pf", mode)) {
            perp_mode = PERP_PREFETCH;
        } else {
            std::cerr << "nmo: unknown mode " << mode << std::endl;
            throw NmoException("unknown mode");
        }
    }

    const char *env_ringbufsize = getenv("NMO_BUFSIZE");
    int ringbufsize = 1;
    if (env_ringbufsize && *env_ringbufsize) {
        ringbufsize = parse_bufsize(env_ringbufsize);
    }

    const char *env_auxbufsize = getenv("NMO_AUXBUFSIZE");
    int auxbufsize = 1;
    if (env_auxbufsize && *env_auxbufsize) {
        auxbufsize = parse_bufsize(env_auxbufsize);
    }

    std::cerr << "===== " <<
        "NMO STARTER ENABLED: pid="
        << getpid() << " tid=" << gettid() << " comm=" << comm
        << " period=" << period << " track_rss=" << track_rss
        << " name=" << name << " perp_mode=" << perp_mode
        << " ringbufsize=" << ringbufsize << " [MiB/thread] auxbufsize=" << auxbufsize << " [MiB/thread]"
        << " =====" << std::endl;

    if (track_rss) {
        char rss_file[128];
        snprintf(rss_file, sizeof(rss_file), "%s.rss.csv", name);
        lib_rss_tracker = new RssTracker(rss_file);
        lib_rss_tracker->start_thread();
    }

    if (perp_mode) {
        lib_perp = new PeriodicProfiler(name, period, perp_mode, ringbufsize, auxbufsize);
    }
}

__attribute__((destructor)) 
static void lib_deinit()
{
    if (lib_perp) {
        delete lib_perp;
        lib_perp = nullptr;
    }

    if (lib_rss_tracker) {
        delete lib_rss_tracker;
        lib_rss_tracker = nullptr;
    }
}

extern "C"
void nmo_start(const char *tag)
{
    if (lib_perp)
        lib_perp->kernel_start(tag);
}

extern "C"
void nmo_stop()
{
    if (lib_perp) {
        lib_perp->kernel_stop();
    }
}

extern "C"
void nmo_tag_addr(const char *tag, void *start, void *end)
{
    if (lib_perp)
        lib_perp->_mon.tag_addr(tag, start, end);
}

extern "C"
void nmo_tag_from_maps(const char *tag, const char *file)
{
    if (lib_perp) {
        std::ifstream maps("/proc/self/maps");
        std::string line;
        while (std::getline(maps, line)) {
            const char *p = line.data();
            void *start = (void *)strtoull(p, (char**)&p, 16);
            p++;
            void *end = (void *)strtoull(p, (char**)&p, 16);
            for (int i = 0; i < 5; i++) {
                while (!isspace(*p))
                    p++;
                while (isspace(*p))
                    p++;
            }
            if (!strcmp(file, p))
                lib_perp->_mon.tag_addr(tag, start, end);
        }
    }
}
