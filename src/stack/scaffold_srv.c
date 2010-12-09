/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
#include <scaffold/platform.h>
#include <scaffold/skbuff.h>
#include <scaffold/debug.h>
#include <scaffold_sock.h>
#include <scaffold/netdevice.h>
#include <scaffold_srv.h>
#include <scaffold_ipv4.h>
#if defined(OS_LINUX_KERNEL)
#include <linux/if_ether.h>
#elif !defined(OS_ANDROID)
#include <netinet/if_ether.h>
#endif
#include "service.h"

extern int scaffold_tcp_rcv(struct sk_buff *);
extern int scaffold_udp_rcv(struct sk_buff *);

/* This function is currently bypassed */
int scaffold_srv_rcv(struct sk_buff *skb)
{
	return -1;
}

int scaffold_srv_xmit_skb(struct sock *sk, struct sk_buff *skb)
{
	struct service_id *srvid = skb_dst_service_id(skb);
	struct service_entry *se;
	struct net_device *dev;
	int err = 0;
	
	if (sk->sk_state == SF_BOUND) {
		err = scaffold_ipv4_xmit_skb(sk, skb);
                
		if (err < 0) {
			LOG_ERR("xmit failed\n");
			FREE_SKB(skb);
		}
		return err;
	}
	/* Unresolved packet, use service id */
	se = service_find(srvid);
	
	if (!se) {
		LOG_ERR("service lookup failed\n");
		FREE_SKB(skb);
		return -EADDRNOTAVAIL;
	}
	
	service_entry_dev_iterate_begin(se);
	
	dev = service_entry_dev_next(se);
	
	while (dev) {
		struct sk_buff *cskb;
		struct net_device *next_dev;
		
		next_dev = service_entry_dev_next(se);
		
		/* 
		   Send on all interfaces listed for this service.
		   
		   TODO: The service entry should be resolved here,
		   but this loop should really be at a lower layer,
		   just before transmission. However, there is
		   currently no way to easily pass the refcounted
		   service entry with the skb (should be a dst_entry)
		   and therefor the loop is currently here. 
		*/

		if (next_dev == NULL) {
			cskb = skb;
		} else {
			cskb = skb_clone(skb, 
					 current ? GFP_KERNEL : 
					 GFP_ATOMIC);
			
			if (!cskb) {
				LOG_ERR("Allocation failed\n");
				FREE_SKB(skb);
				break;
			}
		}
                
		skb_set_dev(cskb, dev);
		
		err = scaffold_ipv4_xmit_skb(sk, cskb);
                
		if (err < 0) {
			LOG_ERR("xmit failed\n");
			FREE_SKB(cskb);
			break;
		}
		dev = next_dev;
	}
	
	service_entry_dev_iterate_end(se);
	service_entry_put(se);
	
	return err;
}

