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

#elif defined(_WIN32) || defined(_WIN64)

#include <windows.h>
#include <assert.h>

HiResTimer::HiResTimer()
    : elapsedTicks(0), running(0), startTicks(0)
{
}

static unsigned long long _get_counter()
{
    LARGE_INTEGER counter;
    assert(QueryPerformanceCounter(&counter) != 0);
    
    return (unsigned long long)counter.QuadPart;
}

void HiResTimer::start()
{
    if (++running == 1)
        startTicks = _get_counter();
}

void HiResTimer::stop()
{
    if (--running == 0) {
        unsigned long long end = _get_counter();
        elapsedTicks += (end - startTicks);
    }
}

unsigned long long HiResTimer::elapsedNanos()
{
    LARGE_INTEGER frequency;
    assert(QueryPerformanceFrequency(&frequency) != 0);

    double performanceCounterRate = 1000000000.0 / (double)frequency.QuadPart;

    return (unsigned long long)((double)elapsedTicks * performanceCounterRate);
}


#else // Unices

#if defined(CLOCK_PROCESS_CPUTIME_ID) && defined(__linux__)
#    define CLOCK CLOCK_PROCESS_CPUTIME_ID
#elif defined(CLOCK_PROF) && defined(__FreeBSD__)
#    define CLOCK CLOCK_PROF
#else
#    define CLOCK CLOCK_REALTIME
#endif

#include <time.h>

HiResTimer::HiResTimer()
    : elapsedTicks(0), running(0), startTicks(0)
{
}

void HiResTimer::start()
{
    if (++running == 1) {
        struct timespec t;
        clock_gettime(CLOCK, &t);
        startTicks = (unsigned long long)t.tv_sec * 1000000000 + t.tv_nsec;
    }
}

void HiResTimer::stop()
{
    if (--running == 0) {
        struct timespec t;
        clock_gettime(CLOCK, &t);
        elapsedTicks = (unsigned long long)t.tv_sec * 1000000000 + t.tv_nsec - startTicks;
    }
}

unsigned long long HiResTimer::elapsedNanos()
{
    return elapsedTicks;
}

#endif // __APPLE__
