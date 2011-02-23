/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
#ifndef _PLATFORM_H
#define _PLATFORM_H

/* Detect platform */
#if defined(__unix__)
#define OS_UNIX 1
#if !defined(__KERNEL__)
#define OS_USER 1
#endif
#endif

#if defined(__linux__)
#define OS_LINUX 1
#if defined(__KERNEL__)
#define OS_KERNEL 1
#define OS_LINUX_KERNEL 1
#else
#define OS_USER 1
#endif
#endif /* OS_LINUX */

#if defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__APPLE__)
#define OS_BSD 1
#define OS_USER 1
#endif

#if defined(__APPLE__)
#define OS_MACOSX 1
#define OS_USER 1
#endif

/* TODO: Detect these in configure */
#if defined(OS_LINUX)
#define HAVE_LIBIO 1
#define HAVE_PPOLL 1
#define HAVE_PSELECT 1
#endif

#if defined(OS_ANDROID)
#undef OS_KERNEL
#define HAVE_OFFSETOF 1
#undef HAVE_LIBIO
#undef HAVE_PPOLL
#undef HAVE_PSELECT
#include <linux/if_ether.h>
#endif

#if defined(OS_BSD)
#include <net/ethernet.h>
#define ETH_HLEN ETHER_HDR_LEN
#define ETH_ALEN ETHER_ADDR_LEN
#define ETH_P_IP ETHERTYPE_IP 
#define EBADFD EBADF
#include "platform_tcpip.h"
#endif

#if defined(OS_LINUX_KERNEL)
#include <linux/kernel.h>
#include <linux/version.h>
#include <net/sock.h>
#define MALLOC(sz, prio) kmalloc(sz, prio)
#define ZALLOC(sz, prio) kzalloc(sz, prio)
#define FREE(m) kfree(m)

typedef uint32_t socklen_t;

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);

#endif /* OS_LINUX_KERNEL */

#if defined(OS_USER)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#if defined(OS_LINUX)
#include <endian.h>
#elif defined(OS_MACOSX)
#include <machine/endian.h>
#endif
#if HAVE_LIBIO
#include <libio.h>
#endif

#define LINUX_VERSION_CODE 132643 /* corresponds to 2.6.35 */
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))

typedef unsigned char gfp_t;
#define GFP_KERNEL 0
#define GFP_ATOMIC 1
#define MALLOC(sz, prio) malloc(sz)
#define ZALLOC(sz, prio) ({                     \
                        void *ptr = malloc(sz); \
                        memset(ptr, 0, sz);     \
                        ptr; })
#define FREE(m) free(m)

#define EXPORT_SYMBOL(x)
#define EXPORT_SYMBOL_GPL(x)

static inline void yield(void) {}

#define min_t(type, x, y) ({			\
	type __min1 = (x);			\
	type __min2 = (y);			\
	__min1 < __min2 ? __min1: __min2; })

#define max_t(type, x, y) ({			\
	type __max1 = (x);			\
	type __max2 = (y);			\
	__max1 > __max2 ? __max1: __max2; })

#define __init
#define __exit

#define panic(name) { int *foo = NULL; *foo = 1; } /* Cause a sefault */
#define WARN_ON(cond)

#if !HAVE_OFFSETOF
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)

int memcpy_toiovec(struct iovec *iov, unsigned char *kdata, int len);
int memcpy_fromiovec(unsigned char *kdata, struct iovec *iov, int len);
int memcpy_fromiovecend(unsigned char *kdata, const struct iovec *iov,
			int offset, int len);

#define cpu_to_be16(n) htons(n)

#if !defined(HAVE_PPOLL)
#include <poll.h>
#include <sys/select.h>
#include <signal.h>

int ppoll(struct pollfd fds[], nfds_t nfds, struct timespec *timeout, sigset_t *set);

#endif /* OS_ANDROID */

#if defined(ENABLE_DEBUG)
#include <assert.h>
#ifndef BUG_ON
#define BUG_ON(x) assert(!x)
#endif 
#else
#ifndef BUG_ON
#define BUG_ON(x) 
#endif 
#endif /* ENABLE_DEBUG */

#endif /* OS_USER */

uint16_t in_cksum(const void *data, size_t len);
const char *mac_ntop(const void *src, char *dst, size_t size);
int mac_pton(const char *src, void *dst);
const char *get_strtime(void);

static inline const char *hexdump(const void *data, int datalen, 
                                  char *buf, int buflen)
{
        int i = 0, len = 0;
        const unsigned char *h = (const unsigned char *)data;
        
        while (i < datalen) {
                unsigned char c = (i + 1 < datalen) ? h[i+1] : 0;
                len += snprintf(buf + len, buflen - len, 
                                "%02x%02x ", h[i], c);
                i += 2;
        }
        return buf;
}

#endif /* _PLATFORM_H */
