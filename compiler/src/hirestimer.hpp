#ifndef __CLAY_HIRESTIMER_HPP
#define __CLAY_HIRESTIMER_HPP

struct HiResTimer {
    unsigned long long elapsedTicks;
    int running;
    unsigned long long startTicks;

    HiResTimer();
    void start();
    void stop();
    unsigned long long elapsedNanos();
    double elapsedMillis() { return (double)elapsedNanos() / (1000 * 1000); }
};

#endif
