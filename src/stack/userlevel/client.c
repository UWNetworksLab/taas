/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
#include <scaffold/debug.h>
#include <scaffold/timer.h>
#include <scaffold/net.h>
#include <scaffold_sock.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <userlevel/client.h>
#include <userlevel/client_msg.h>

struct client {
	client_type_t type;
	client_state_t state;
	int fd;
        struct socket *sock;
	unsigned int id;
	int pipefd[2];
	int should_exit;
        pthread_t thr;
        sigset_t sigset;
	struct sockaddr_un sa;
	struct timer_list timer;
	struct list_head link;
};

static pthread_key_t client_key;
static pthread_once_t key_once = PTHREAD_ONCE_INIT;

static void make_client_key(void)
{
	pthread_key_create(&client_key, NULL);
}

typedef int (*msg_handler_t)(struct client *, struct client_msg *);

static int dummy_msg_handler(struct client *c, struct client_msg *msg)
{
	LOG_DBG("Client %u handling message type %s\n", 
		c->id, client_msg_to_typestr(msg));

	return 0;
}

static int client_handle_bind_req_msg(struct client *c, struct client_msg *msg);
static int client_handle_connect_req_msg(struct client *c, struct client_msg *msg);
static int client_handle_send_req_msg(struct client *c, struct client_msg *msg);

msg_handler_t msg_handlers[] = {
	dummy_msg_handler,
	client_handle_bind_req_msg,
	dummy_msg_handler,
	client_handle_connect_req_msg,
	dummy_msg_handler,
	dummy_msg_handler,
	dummy_msg_handler,
	dummy_msg_handler,
	dummy_msg_handler,
	dummy_msg_handler,
	dummy_msg_handler,
	client_handle_send_req_msg,
	dummy_msg_handler,
	dummy_msg_handler,
	dummy_msg_handler,
	dummy_msg_handler,
	dummy_msg_handler,
	dummy_msg_handler,
	dummy_msg_handler,
	dummy_msg_handler,
	dummy_msg_handler
};
	
static void dummy_timer_callback(unsigned long data)
{
	LOG_DBG("Timer callback for client %u\n", 
                ((struct client *)data)->id);
}

struct client *client_get_current(void)
{
        return (struct client *)pthread_getspecific(client_key); 
}

static inline int client_type_to_prot_type(client_type_t type)
{
        if (type == CLIENT_TYPE_UDP)
                return SOCK_DGRAM;
        if (type == CLIENT_TYPE_TCP)
                return SOCK_STREAM;
        
        return -1;
}

/*
  Create client.

  We use a pipe to signal to clients when to exit. A pipe is useful,
  because we can "sleep" on it in a select()/poll().

 */
struct client *client_create(client_type_t type, 
			     int sock, unsigned int id, 
			     struct sockaddr_un *sa,
			     sigset_t *sigset)
{
	struct client *c;
        int err;

	pthread_once(&key_once, make_client_key);

	c = (struct client *)malloc(sizeof(struct client));

	if (!c)
		return NULL;

	memset(c, 0, sizeof(struct client));
	
	c->type = type;
	c->state = CLIENT_STATE_NOT_RUNNING;
	c->fd = sock;

        err = sock_create(PF_SCAFFOLD, 
                          client_type_to_prot_type(type), 
                          0, &c->sock);
        if (err < 0) {
                LOG_ERR("Could not create socket: %s\n", KERN_STRERROR(err));
                free(c);
                return NULL;                        
        }

	c->id = id;
	LOG_DBG("client %p id is %u\n", c, c->id);
	c->should_exit = 0;
	memcpy(&c->sa, sa, sizeof(*sa));

	if (sigset)
		memcpy(&c->sigset, sigset, sizeof(*sigset));
	
	if (pipe(c->pipefd) != 0) {
		LOG_ERR("could not open client pipe : %s\n",
			strerror(errno));
		free(c);
		return NULL;
	}

	/* Init a timer for test purposes. */
	c->timer.function = dummy_timer_callback;
	c->timer.expires = (id + 1) * 1000000;
	c->timer.data = (unsigned long)c;

	INIT_LIST_HEAD(&c->link);

	return c;
}

client_type_t client_get_type(struct client *c)
{
	return c->type;
}

client_state_t client_get_state(struct client *c)
{
	return c->state;
}

unsigned int client_get_id(struct client *c)
{
	return c->id;
}

pthread_t client_get_thread(struct client *c)
{
	return c->thr;
}

int client_get_sockfd(struct client *c)
{
        return c->fd;
}

int client_get_signalfd(struct client *c)
{
        return c->pipefd[0];
}

static int client_close(struct client *c)
{
	int ret = 0;

        if (c->fd != -1) {
                ret = close(c->fd);
                c->fd = -1;
        }

        if (c->sock) {
                sock_release(c->sock);
                c->sock = NULL;
        }
	return ret;
}

void client_destroy(struct client *c)
{
        client_close(c);
	list_del(&c->link);
	free(c);
}

int client_signal_pending(struct client *c)
{
        int ret;
        struct pollfd fds;

        fds.fd = c->pipefd[0];
        fds.events = POLLIN | POLLHUP;
        fds.revents = 0;

        ret = poll(&fds, 1, 0);
        
        if (ret == -1) {
                LOG_ERR("poll error: %s\n", strerror(errno));
        }

        return ret;
}

int client_signal_raise(struct client *c)
{
	char w = 'w';

	return write(c->pipefd[1], &w, 1);
}

int client_signal_exit(struct client *c)
{
	c->should_exit = 1;
	return client_signal_raise(c);
}

int client_signal_lower(struct client *c)
{
	ssize_t sz;
	int ret = 0;
	char r = 'r';

	do {
		sz = read(c->pipefd[1], &r, 1);

		if (sz == 1)
			ret = 1;
	} while (sz > 0);

	return sz == -1 ? -1 : ret;
}

int client_handle_bind_req_msg(struct client *c, struct client_msg *msg)
{
	struct client_msg_bind_req *req = (struct client_msg_bind_req *)msg;
        DEFINE_CLIENT_RESPONSE(rsp, MSG_BIND_RSP);
        struct socket *sock = c->sock;
        struct sockaddr_sf saddr;
	int ret;
        
	LOG_DBG("bind request for service id %s\n", 
		service_id_to_str(&req->srvid));	

        memset(&saddr, 0, sizeof(saddr));
        saddr.sf_family = AF_SCAFFOLD;
        memcpy(&saddr.sf_srvid, &req->srvid, sizeof(req->srvid));

        ret = sock->ops->bind(sock, (struct sockaddr *)&saddr, sizeof(saddr));

        if (ret < 0) {
                if (KERN_ERR(ret) == ERESTARTSYS) {
                        LOG_ERR("Bind was interrupted\n");
                        rsp.error = EINTR;
                        return client_msg_write(c->fd, &rsp.msghdr);
                }
                LOG_ERR("Bind failed: %s\n", KERN_STRERROR(ret));
                rsp.error = KERN_ERR(ret);
        }

        /* TODO: Bind should not return here... */
	return client_msg_write(c->fd, &rsp.msghdr);
}

int client_handle_connect_req_msg(struct client *c, struct client_msg *msg)
{
	///struct client_msg_connect_req *cr = (struct client_msg_connect_req *)msg;
	
	LOG_DBG("connect request for service id %s\n", 
		service_id_to_str(&((struct client_msg_connect_req *)msg)->srvid));

	return 0;
}

int client_handle_send_req_msg(struct client *c, struct client_msg *msg)
{
	struct client_msg_send_req *req = (struct client_msg_send_req *)msg;
        DEFINE_CLIENT_RESPONSE(rsp, MSG_SEND_RSP);
        struct socket *sock = c->sock;
        struct msghdr mh;
        struct iovec iov;
        struct sockaddr_sf saddr;
        int ret;

        memset(&saddr, 0, sizeof(saddr));
        saddr.sf_family = AF_SCAFFOLD;
        memcpy(&saddr.sf_srvid, &req->srvid, sizeof(req->srvid));

        memset(&mh, 0, sizeof(mh));
        mh.msg_name = &saddr;
        mh.msg_namelen = sizeof(saddr);
        mh.msg_iov = &iov;
        mh.msg_iovlen = 1;
        
        iov.iov_base = req->data;
        iov.iov_len = req->data_len;

	LOG_DBG("data_len=%u\n", req->data_len);
        
        ret = sock->ops->sendmsg(NULL, sock, &mh, req->data_len);
        
        if (ret < 0) {
                rsp.error = KERN_ERR(ret);
                LOG_ERR("sendmsg returned error %s\n", KERN_STRERROR(ret));
        }
        
	return client_msg_write(c->fd, &rsp.msghdr);
}

static int client_handle_msg(struct client *c)
{
	struct client_msg *msg;
	int ret;
	
	ret = client_msg_read(c->fd, &msg);

	if (ret < 1)
		return ret;

	ret = msg_handlers[msg->type](c, msg);

        if (ret == -1) {
                LOG_ERR("message handler error: %s\n", strerror(errno));
        }
        
	client_msg_free(msg);

	return ret;
}

#define MAX(x, y) (x >= y ? x : y)

static void signal_handler(int signal)
{
        switch (signal) {
        case SIGPIPE:
                LOG_DBG("received SIGPIPE\n");
                break;
        default:
                LOG_DBG("signal %d received\n", signal);
        }
}

static void *client_thread(void *arg)
{
	struct sigaction action;
	struct client *c = (struct client *)arg;
	int ret;

	memset(&action, 0, sizeof(struct sigaction));
        action.sa_handler = signal_handler;
	sigaction(SIGPIPE, &action, 0);

	c->state = CLIENT_STATE_RUNNING;

	ret = pthread_setspecific(client_key, c);

	if (ret != 0) {
                LOG_ERR("Could not set client key\n");
		return NULL;
	}

#if defined(PER_THREAD_TIMER_LIST)
	if (timer_list_per_thread_init() == -1)
		return NULL;
#endif
	LOG_DBG("Client %u running\n", c->id);

	while (!c->should_exit) {
		fd_set readfds;
		int nfds;
		
		FD_ZERO(&readfds);
		FD_SET(c->pipefd[0], &readfds);

		if (c->fd != -1)
			FD_SET(c->fd, &readfds);

		nfds = MAX(c->fd, c->pipefd[0]) + 1;

		ret = select(nfds, &readfds, NULL, NULL, NULL);

		if (ret == -1) {
			if (errno == EINTR) {
				LOG_INF("client %u select interrupted\n", 
					c->id);
				continue;
			}
			/* Error */
			LOG_ERR("client %u select error...\n",
				c->id);
			break;
		} else if (ret == 0) {
			/* Timeout */
			LOG_DBG("Client %u timeout\n", c->id);
		} else {
			if (FD_ISSET(c->pipefd[0], &readfds)) {
				/* Signal received, probably exit */
				client_signal_lower(c);
				LOG_DBG("Client %u exit signal\n", c->id);
				continue;
			}
			if (FD_ISSET(c->fd, &readfds)) {
				/* Socket readable */
				LOG_DBG("Client %u socket readable\n", c->id);
				ret = client_handle_msg(c);

				if (ret == 0) {
					/* Client close */
					LOG_DBG("Client %u closed\n", c->id);
					c->should_exit = 1;
				}
			} 
		}
	}

	LOG_DBG("Client %u exits\n", c->id);
	client_close(c);	
	c->state = CLIENT_STATE_GARBAGE;

	return NULL;
}

static void *test_client_thread(void *arg)
{
	struct client *c = (struct client *)arg;
#if defined(PER_THREAD_TIMER_LIST)
	if (timer_list_per_thread_init() == -1)
		return NULL;
#endif
	add_timer(&c->timer);

	return client_thread(arg);
}

int client_start(struct client *c)
{
	int ret;

	ret = pthread_create(&c->thr, NULL, client_thread, c);
        
        if (ret != 0) {
                LOG_ERR("could not start client thread\n");
        }

	return ret;
}

void client_list_add(struct client *c, struct list_head *head)
{	
	list_add_tail(&c->link, head);
}

void client_list_del(struct client *c)
{
	list_del(&c->link);
}

struct client *client_list_first_entry(struct list_head *head)
{
	return list_first_entry(head, struct client, link);
}

struct client *client_list_entry(struct list_head *lh)
{
	return list_entry(lh, struct client, link);
}

int test_client_start(struct client *c)
{
	int ret;

	ret = pthread_create(&c->thr, NULL, test_client_thread, c);
        
        if (ret != 0) {
                LOG_ERR("could not start client thread\n");
        }

	return ret;
}
