/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
#ifndef _LIBSERVAL_H_
#define _LIBSERVAL_H_

#include <netinet/serval.h>
#include <stdio.h>

/* Reserved Object IDs */
#define CONTROLLER_OID 0xFFFE
#define SERVAL_OID 0xFFFD
#define SERVAL_NULL_OID 0xFFFF

/* connect_sv() or listen_sv() flags
   typically SF_WANT_FAILOVER in connect_sv and
   SF_HAVE_FAILOVER in listen_sv */
#define SF_WANT_FAILOVER 0x01
#define SF_HAVE_FAILOVER 0x02

#if defined(SERVAL_NATIVE)

#define socket_sv socket
#define bind_sv bind
#define connect_sv connect
#define listen_sv listen
#define accept_sv accept
#define send_sv send
#define recv_sv recv
#define close_sv close
#define sendto_sv sendto
#define recvfrom_sv recvfrom
#define strerror_sv_r strerror_r
#define strerror_sv strerror

#include <sys/ioctl.h>
#define SIOCSFMIGRATE _IO(0,1)

static inline int migrate_sv(int socket) 
{
    return ioctl(socket, SIOCSFMIGRATE);
}

#else

#ifdef __cplusplus
extern "C"
#endif
int 
socket_sv(int domain, int type, int protocol);

#ifdef __cplusplus
extern "C"
#endif
int
bind_sv(int socket, const struct sockaddr *address, socklen_t address_len);

#ifdef __cplusplus
extern "C"
#endif
int 
connect_sv(int socket, const struct sockaddr *address, 
           socklen_t address_len);

#ifdef __cplusplus
extern "C"
#endif
int 
mlisten_sv(int socket, int backlog, 
           const struct sockaddr *addr, 
           socklen_t address_len);

/* top 16 bits of backlog reserved for flags (e.g., SF_HAVE_FAILOVER) */
#ifdef __cplusplus
extern "C"
#endif
int 
listen_sv(int socket, int backlog); 


#ifdef __cplusplus
extern "C"
#endif
int 
accept_sv(int socket, struct sockaddr *address, 
          socklen_t *addr_len);

#ifdef __cplusplus
extern "C"
#endif
ssize_t 
send_sv(int socket, const void *buffer, size_t length, int flags);

#ifdef __cplusplus
extern "C"
#endif
ssize_t 
recv_sv(int socket, void *buffer, size_t length, int flags);

#ifdef __cplusplus
extern "C"
#endif
int
close_sv(int filedes);

#ifdef __cplusplus
extern "C"
#endif
ssize_t 
sendto_sv(int socket, const void *buffer, size_t length, int flags,
          const struct sockaddr *dest_addr, socklen_t dest_len);

#ifdef __cplusplus
extern "C"
#endif
ssize_t 
recvfrom_sv(int socket, void *buffer, size_t length, int flags,
            struct sockaddr *address, socklen_t *address_len);

#ifdef __cplusplus
extern "C"
#endif
int
getsockopt_sv(int soc, int level, int option_name, 
              void *option_value, socklen_t *option_len);

#ifdef __cplusplus
extern "C"
#endif
char *
strerror_sv_r(int errnum, char *buf, size_t buflen);

#ifdef __cplusplus
extern "C"
#endif
char *
strerror_sv(int errnum);

/* sko begin */
#ifdef __cplusplus
extern "C"
#endif
int 
migrate_sv(int socket);
/* sko end */

/* Implemented in state.cc */
#ifdef __cplusplus
extern "C"
#endif
const char *
srvid_to_str(struct service_id srvid);

#endif /* SERVAL_NATIVE */


#endif /* _LIBSERVAL_H_ */
