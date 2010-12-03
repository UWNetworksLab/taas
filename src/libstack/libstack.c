/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
#include <libstack/ctrlmsg.h>
#include <libstack/callback.h>
#include <event.h>
#include "debug.h"

struct libstack_callbacks *callbacks = NULL;
extern int eventloop_init(void);
extern void eventloop_fini(void);

int libstack_configure_interface(const char *ifname, 
                                 const struct as_addr *asaddr,
                                 const struct host_addr *haddr,
                                 unsigned short flags)
{
	struct ctrlmsg_iface_conf cm;

	cm.cmh.type = CTRLMSG_TYPE_IFACE_CONF;
	cm.cmh.len = sizeof(cm);
	strncpy(cm.ifname, ifname, IFNAMSIZ - 1);
        memcpy(&cm.asaddr, asaddr, sizeof(*asaddr));
        memcpy(&cm.haddr, haddr, sizeof(*haddr));
	cm.flags = flags;

	return event_sendmsg(&cm, cm.cmh.len);
}

int libstack_register_callbacks(struct libstack_callbacks *calls)
{
	if (callbacks)
		return -1;

	callbacks = calls;
	
	return 0;
}

void libstack_unregister_callbacks(struct libstack_callbacks *calls)
{
	if (callbacks == calls)
		callbacks = NULL;
}

int libstack_init(void)
{
	return eventloop_init();
}

void libstack_fini(void) 
{
	eventloop_fini();
}
