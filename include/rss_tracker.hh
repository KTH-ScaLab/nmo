#ifndef RSS_TRACKER_H
#define RSS_TRACKER_H

#include <thread>
#include <fstream>

class RssTracker
{
public:
    RssTracker(const char *file);
    void start_thread();

private:
    void run_thread();

    std::thread _thread;
    std::ofstream _os;
};

#endif
