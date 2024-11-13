#include <time.h>
#include <string.h>
#include "rss_tracker.hh"
#include "nmo_exception.hh"
#include "clock.hh"

RssTracker::RssTracker(const char *file)
    : _os(file)
{
    //_os << "time,rss,pss" << std::endl;
    _os << "time,local,remote,active" << std::endl;
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
        size_t total_active = 0;

        std::ifstream is("/proc/self/numa_maps");
        std::string line;
        while (std::getline(is, line)) {
            const char *sep = line.c_str();
            sep = strchr(sep, ' ');
            if (!sep) continue;
            sep = strchr(sep+1, ' ');
            if (!sep) continue;

            size_t lp = 0;
            size_t rp = 0;
            size_t page_kb = 4;
            ssize_t active = -1;

            for (;;) {
                const char *key_psize = " kernelpagesize_kB=";
                const char *key_active = " active=";

                if (sep[1] == 'N') {
                    int node = atoi(sep+2);
                    sep = strchr(sep, '=');
                    if (!sep) break;
                    long pages = atol(sep+1);

                    if (node == 0)
                        lp += pages;
                    else
                        rp += pages;
                } else if (!memcmp(sep, key_psize, strlen(key_psize))) {
                    page_kb = atoi(sep+strlen(key_psize));
                    if (!page_kb) {
                        throw NmoException("failed to parse page size field");
                    }
                } else if (!memcmp(sep, key_active, strlen(key_active))) {
                    active = atoi(sep+strlen(key_active));
                }

                sep = strchr(sep+1, ' ');
                if (!sep) break;
            }

            local += lp * page_kb * 1024;
            remote += rp * page_kb * 1024;

            if (active < 0)
                active = lp+rp;
            total_active += active * page_kb * 1024;
        }

        _os << now << "," << local << "," << remote << "," << total_active << std::endl;

        next += period;
        std::this_thread::sleep_until(next);
    }


}
