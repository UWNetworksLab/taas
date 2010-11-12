/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
#ifndef _PLATFORM_H
#define _PLATFORM_H

#include "thread.h"
#include "lock.h"

#if defined(__KERNEL__)
#include <linux/kernel.h>
#include <linux/time.h>
#define MALLOC(sz, prio) kmalloc(sz, prio)
#define FREE(m) kfree(m)
#else
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#define GFP_KERNEL 0
#define GFP_ATOMIC 1
#define MALLOC(sz, prio) malloc(sz)
#define FREE(m) free(m)

#define EXPORT_SYMBOL(x)
#define EXPORT_SYMBOL_GPL(x)

#define __init
#define __exit

#define panic(name) { int *foo = NULL; *foo = 1; } /* Cause a sefault */

#endif /* __KERNEL__ */

static inline const char *get_strtime(void)
{
    static char buf[512];
#if defined(__KERNEL__)
    struct timeval now;

    do_gettimeofday(&now);
    sprintf(buf, "%ld.%03ld", now.tv_sec, now.tv_usec / 1000);
#else
    time_t now = time(0);
    struct tm p;
    localtime_r(&now, &p);
    strftime(buf, 512, "%b %e %T", &p);
#endif
    return buf;
}

#include "debug.h"

#endif /* _PLATFORM_H */
