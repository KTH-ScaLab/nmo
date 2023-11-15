#include <algorithm>
#include <dirent.h>
#include "threads.hh"
#include "nmo_exception.hh"

std::vector<int> get_threads(int ignored)
{
    DIR *dir = opendir("/proc/self/task");
    if (!dir)
        throw NmoException("Cannot open /proc/self/task");

    struct dirent *d;
    std::vector<int> tids;

    while ((d = readdir(dir))) {
        int tid = atoi(d->d_name);
        if (tid != 0 && tid != ignored)
            tids.push_back(tid);
    }

    closedir(dir);

    std::sort(tids.begin(), tids.end());

    return tids;
}
