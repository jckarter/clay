#ifndef __CLAY_HIRESTIMER_HPP
#define __CLAY_HIRESTIMER_HPP

namespace clay {
struct HiResTimer {
    unsigned long long elapsedTicks;
    unsigned long long startTicks;
    int running;

    HiResTimer();
    void start();
    void stop();
    unsigned long long elapsedNanos();
    double elapsedMillis() { return (double)elapsedNanos() / (1000 * 1000); }
};
}

#endif
