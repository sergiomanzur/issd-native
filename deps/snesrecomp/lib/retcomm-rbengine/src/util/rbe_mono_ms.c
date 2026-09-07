#include "retcomm_rbengine/mono_ms.h"

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <time.h>
#endif

uint32_t rbe_mono_ms(void)
{
#if defined(_WIN32)
    static LARGE_INTEGER freq;
    LARGE_INTEGER c;
    if (!freq.QuadPart)
        QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&c);
    return (uint32_t)((double)c.QuadPart * 1000.0 / (double)freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000u + (uint64_t)(ts.tv_nsec / 1000000L));
#endif
}
