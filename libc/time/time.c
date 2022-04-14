// #include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include <syscall.h>
#include <common.h>

static inline _syscall0(SYS_CURR_TIME_EPOCH, time_t, sys_curr_time_epoch)

// Get current clock time
int gettimeofday(struct timeval* tp, struct timezone * tz)
{
    // printf("gettimeofday(%u,%u)\n", tp, tz);
    (void) tz;
    tp->tv_sec = sys_curr_time_epoch();
    tp->tv_usec = 0;
    return 0;
}

time_t time(time_t *t)
{
    // printf("time(%u)\n", t);
    time_t curr = sys_curr_time_epoch();
    if(t) {
        *t = curr;
    }
    return curr;
}