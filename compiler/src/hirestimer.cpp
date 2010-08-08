#include "hirestimer.hpp"

#ifdef __APPLE__

#include <mach/mach.h>
#include <mach/mach_time.h>

HiResTimer::HiResTimer()
    : elapsedTicks(0), running(0), startTicks(0)
{
}

void HiResTimer::start()
{
    if (++running == 1)
        startTicks = mach_absolute_time();
}

void HiResTimer::stop()
{
    if (--running == 0) {
        unsigned long long end = mach_absolute_time();
        elapsedTicks += (end - startTicks);
    }
}

unsigned long long HiResTimer::elapsedNanos()
{
    static mach_timebase_info_data_t timeBaseInfo;
    if (timeBaseInfo.denom == 0) {
        mach_timebase_info(&timeBaseInfo);
    }
    return elapsedTicks * timeBaseInfo.numer / timeBaseInfo.denom;
}

#else // __APPLE__

HiResTimer::HiResTimer() {}
void HiResTimer::start() {}
void HiResTimer::stop() {}
unsigned long long HiResTimer::elapsedNanos() { return 0; }

#endif // __APPLE__
