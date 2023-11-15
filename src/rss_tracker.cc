#include <time.h>
#include <string.h>
#include "rss_tracker.hh"
#include "nmo_exception.hh"
#include "clock.hh"

RssTracker::RssTracker(const char *file)
    : _os(file)
{
    //_os << "time,rss,pss" << std::endl;
    _os << "time,local,remote" << std::endl;
}

void RssTracker::start_thread()
{
    _thread = std::thread(&RssTracker::run_thread, this);
    _thread.detach();
}

void RssTracker::run_thread()
{
    //char fname[64];
    //snprintf(fname, sizeof(fname), "/proc/%d/smaps_rollup", pid);
    //const char *fname = "/proc/self/smaps_rollup";

    const auto period = std::chrono::seconds(1);

    auto start = std::chrono::steady_clock::now();
    auto next = start + period;

    for (;;) {
        auto now = nano_clock();

        /*
        FILE *fp = fopen(fname, "r");
        if (!fp) {
            perror("failed to open smaps_rollup");
            exit(EXIT_FAILURE);
        }

        size_t rss = 0, pss = 0, ref = 0, anon = 0;
        char tmp[128];

        while (fgets(tmp, sizeof(tmp), fp)) {
            sscanf(tmp, "Rss: %zu kB", &rss);
            sscanf(tmp, "Pss: %zu kB", &pss);
            sscanf(tmp, "Referenced: %zu kB", &ref);
            sscanf(tmp, "Anonymous: %zu kB", &anon);
        }

        _os << now << "," << 1024*rss << "," << 1024*pss << std::endl;
        */

        size_t local = 0;
        size_t remote = 0;

        std::ifstream is("/proc/self/numa_maps");
        std::string line;
        while (std::getline(is, line)) {
            const char *sep = line.c_str();
            sep = strchr(sep, ' ');
            if (!sep) continue;
            sep = strchr(sep+1, ' ');
            if (!sep) continue;

            for (;;) {
                if (sep[1] == 'N') {
                    int node = atoi(sep+2);
                    sep = strchr(sep, '=');
                    if (!sep) break;
                    long pages = atol(sep+1);

                    if (node == 0)
                        local += 4096*pages;
                    else
                        remote += 4096*pages;
                }
                sep = strchr(sep+1, ' ');
                if (!sep) break;
            }
        }

        _os << now << "," << local << "," << remote << std::endl;

        next += period;
        std::this_thread::sleep_until(next);
    }


}
