/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
#include <serval/platform.h>
#include <serval/skbuff.h>
#include <serval/list.h>
#include <serval/debug.h>
#include <serval/timer.h>
#include <serval/netdevice.h>
#include <netinet/serval.h>
#include <serval_sock.h>
#include <serval_srv.h>
#include <service.h>
#if defined(OS_LINUX_KERNEL)
#include <linux/ip.h>
#else
#include <netinet/ip.h>
#endif

atomic_t serval_nr_socks = ATOMIC_INIT(0);
static atomic_t serval_flow_id = ATOMIC_INIT(1);
static struct serval_table established_table;
static struct serval_table listen_table;

/* The number of (prefix) bytes to hash on in the serviceID */
#define SERVICE_KEY_LEN (8)

static const char *sock_state_str[] = {
        "UNDEFINED",
        "CLOSED",
        "REQUEST",
        "RESPOND",
        "CONNECTED",
        "CLOSING",
        "TIMEWAIT",
        "MIGRATE",
        "RECONNECT",
        "RRESPOND",
        "LISTEN",
        "CLOSEWAIT",
        "FINWAIT1",
        "FINWAIT2",
        "LASTACK"
};

static void serval_sock_destruct(struct sock *sk);

int __init serval_table_init(struct serval_table *table,
                             unsigned int (*hashfn)(struct serval_table *tbl, 
                                                    struct sock *sk),
                             struct serval_hslot *(*hashslot)(struct serval_table *tbl,
                                                              struct net *net,
                                                              void *key,
                                                              size_t keylen),
                             const char *name)
{
	unsigned int i;

	table->hash = MALLOC(SERVAL_HTABLE_SIZE_MIN *
                             2 * sizeof(struct serval_hslot), GFP_KERNEL);
	if (!table->hash) {
		/* panic(name); */
		return -1;
	}

	table->mask = SERVAL_HTABLE_SIZE_MIN - 1;
        table->hashfn = hashfn;
        table->hashslot = hashslot;

	for (i = 0; i <= table->mask; i++) {
		INIT_HLIST_HEAD(&table->hash[i].head);
		table->hash[i].count = 0;
		spin_lock_init(&table->hash[i].lock);
	}

	return 0;
}

void __exit serval_table_fini(struct serval_table *table)
{
        unsigned int i;

        for (i = 0; i <= table->mask; i++) {
                spin_lock_bh(&table->hash[i].lock);
                        
                while (!hlist_empty(&table->hash[i].head)) {
                        struct sock *sk;

                        sk = hlist_entry(table->hash[i].head.first, 
                                         struct sock, sk_node);
                        
                        hlist_del(&sk->sk_node);
                        table->hash[i].count--;
                        sock_put(sk);
                }
                spin_unlock_bh(&table->hash[i].lock);           
	}

        FREE(table->hash);
}

static struct sock *serval_sock_lookup(struct serval_table *table,
                                       struct net *net, void *key, 
                                       size_t keylen)
{
        struct serval_hslot *slot;
        struct hlist_node *walk;
        struct sock *sk = NULL;

        if (!key)
                return NULL;

        slot = table->hashslot(table, net, key, keylen);

        if (!slot)
                return NULL;

        spin_lock_bh(&slot->lock);
        
        hlist_for_each_entry(sk, walk, &slot->head, sk_node) {
                struct serval_sock *ssk = serval_sk(sk);
                if (memcmp(key, ssk->hash_key, keylen) == 0) {
                        sock_hold(sk);
                        goto out;
                }
        }
        sk = NULL;
out:
        spin_unlock_bh(&slot->lock);
        
        return sk;
}

struct sock *serval_sock_lookup_flowid(struct flow_id *flowid)
{
        return serval_sock_lookup(&established_table, &init_net, 
                                  flowid, sizeof(*flowid));
}

struct sock *serval_sock_lookup_serviceid(struct service_id *srvid)
{
        /* 
           return serval_sock_lookup(&listen_table, &init_net, 
                                  srvid, SERVICE_KEY_LEN);
        */
        struct service_entry *se = service_find(srvid);

        if (!se)
                return NULL;

        if (se->sk)
                sock_hold(se->sk);

        service_entry_put(se);

        return se->sk;
}

static inline unsigned int serval_sock_ehash(struct serval_table *table,
                                             struct sock *sk)
{
        return serval_hashfn(sock_net(sk), 
                             &serval_sk(sk)->local_flowid,
                             serval_sk(sk)->hash_key_len,
                             table->mask);
}

static inline unsigned int serval_sock_lhash(struct serval_table *table, 
                                             struct sock *sk)
{
        return serval_hashfn_listen(sock_net(sk), 
                                    &serval_sk(sk)->local_srvid, 
                                    serval_sk(sk)->hash_key_len * 8,
                                    table->mask);
}

static void __serval_table_hash(struct serval_table *table, struct sock *sk)
{
        struct serval_hslot *slot;

        sk->sk_hash = table->hashfn(table, sk);

        LOG_DBG("hash=%lu\n", sk->sk_hash);

        slot = &table->hash[sk->sk_hash];

        spin_lock(&slot->lock);
        slot->count++;
        hlist_add_head(&sk->sk_node, &slot->head);
#if defined(OS_LINUX_KERNEL)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25)
	sock_prot_inuse_add(sock_net(sk), sk->sk_prot, 1);
#else
        sock_prot_inc_use(sk->sk_prot);
#endif
#endif
        spin_unlock(&slot->lock);     
}

static void __serval_sock_hash(struct sock *sk)
{
        if (!hlist_unhashed(&sk->sk_node)) {
                LOG_ERR("socket %p already hashed\n", sk);
        }
        
        if (sk->sk_state == SERVAL_LISTEN) {
                int err = 0;

                LOG_DBG("adding socket %p based on service id %s\n",
                        sk, service_id_to_str(&serval_sk(sk)->local_srvid));
                serval_sk(sk)->hash_key = &serval_sk(sk)->local_srvid;
                serval_sk(sk)->hash_key_len = serval_sk(sk)->srvid_prefix_bits;
                /* __serval_table_hash(&listen_table, sk); */

                err = service_add(&serval_sk(sk)->local_srvid, 
                                  serval_sk(sk)->srvid_prefix_bits == 0 ? 
                                  sizeof(serval_sk(sk)->local_srvid) * 8: 
                                  serval_sk(sk)->srvid_prefix_bits, 
                                  NULL, NULL, 0, sk, GFP_ATOMIC);
                
                if (err < 0) {
                        LOG_ERR("could not add service for listening demux\n");
                }

        } else { 
                LOG_DBG("hashing socket %p based on socket id %s\n",
                        sk, flow_id_to_str(&serval_sk(sk)->local_flowid));
                serval_sk(sk)->hash_key = &serval_sk(sk)->local_flowid;
                serval_sk(sk)->hash_key_len = 
                        sizeof(serval_sk(sk)->local_flowid);
                __serval_table_hash(&established_table, sk);
        }
}

void serval_sock_hash(struct sock *sk)
{
        if (sk->sk_state != SERVAL_CLOSED) {
		local_bh_disable();
		__serval_sock_hash(sk);
		local_bh_enable();
	}
}

void serval_sock_unhash(struct sock *sk)
{
        struct net *net = sock_net(sk);
        spinlock_t *lock;

        LOG_DBG("unhashing socket %p\n", sk);

        /* grab correct lock */
        if (sk->sk_state == SERVAL_LISTEN) {
                /*
                lock = &listen_table.hashslot(&listen_table, net, 
                                              &serval_sk(sk)->local_srvid, 
                                              serval_sk(sk)->hash_key_len)->lock;
                */
                service_del(&serval_sk(sk)->local_srvid,
                            serval_sk(sk)->srvid_prefix_bits == 0 ?
                            sizeof(serval_sk(sk)->local_srvid) * 8 :
                            serval_sk(sk)->srvid_prefix_bits);
                
#if defined(OS_LINUX_KERNEL)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25)
                sock_prot_inuse_add(sock_net(sk), sk->sk_prot, -1);
#else
                sock_prot_dec_use(sk->sk_prot);
#endif
#endif
                return;
        } else {
                lock = &established_table.hashslot(&established_table,
                                                   net, &serval_sk(sk)->local_flowid, 
                                                   serval_sk(sk)->hash_key_len)->lock;
        }
        
	spin_lock_bh(lock);

        if (!hlist_unhashed(&sk->sk_node)) {
                hlist_del_init(&sk->sk_node);
#if defined(OS_LINUX_KERNEL)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25)
                sock_prot_inuse_add(sock_net(sk), sk->sk_prot, -1);
#else
                sock_prot_dec_use(sk->sk_prot);
#endif
#endif
        }
	spin_unlock_bh(lock);
}

int __init serval_sock_tables_init(void)
{
        int ret;

        ret = serval_table_init(&listen_table, 
                                serval_sock_lhash, 
                                serval_hashslot_listen,
                                "LISTEN");

        if (ret < 0)
                goto fail_table;
        
        ret = serval_table_init(&established_table, 
                                serval_sock_ehash, 
                                serval_hashslot,
                                "ESTABLISHED");

fail_table:
        return ret;
}

void __exit serval_sock_tables_fini(void)
{
        serval_table_fini(&listen_table);
        serval_table_fini(&established_table);
        if (sock_state_str[0]) {} /* Avoid compiler warning when
                                   * compiling with debug off */
}

int __serval_assign_flowid(struct sock *sk)
{
        struct serval_sock *ssk = serval_sk(sk);
       
        /* 
           TODO: 
           - Check for ID wraparound and conflicts 
           - Make sure code does not assume flowid is a short
        */
        return serval_sock_get_flowid(&ssk->local_flowid);
}

int serval_sock_get_flowid(struct flow_id *sid)
{
        sid->s_id = htons(atomic_inc_return(&serval_flow_id));

        return 0;
}

struct sock *serval_sk_alloc(struct net *net, struct socket *sock, 
                             gfp_t priority, int protocol, 
                             struct proto *prot)
{
        struct sock *sk;

        sk = sk_alloc(net, PF_SERVAL, priority, prot);

	if (!sk)
		return NULL;

	sock_init_data(sock, sk);
        sk->sk_family = PF_SERVAL;
	sk->sk_protocol	= protocol;
	sk->sk_destruct	= serval_sock_destruct;
        sk->sk_backlog_rcv = sk->sk_prot->backlog_rcv;

        /* Only assign socket id here in case we have a user
         * socket. If socket is NULL, then it means this socket is a
         * child socket from a LISTENing socket, and it will be
         * assigned the socket id from the request sock */
        if (sock && __serval_assign_flowid(sk) < 0) {
                LOG_DBG("could not assign sock id\n");
                sock_put(sk);
                return NULL;
        }

        atomic_inc(&serval_nr_socks);
                
        LOG_DBG("SERVAL socket %p created, %d are alive.\n", 
                sk, atomic_read(&serval_nr_socks));

        return sk;
}

void serval_sock_init(struct sock *sk)
{
        struct serval_sock *ssk = serval_sk(sk);

        sk->sk_state = 0;
        INIT_LIST_HEAD(&ssk->accept_queue);
        INIT_LIST_HEAD(&ssk->syn_queue);
        setup_timer(&ssk->retransmit_timer, 
                    serval_srv_rexmit_timeout,
                    (unsigned long)sk);

        setup_timer(&ssk->tw_timer, 
                    serval_srv_timewait_timeout,
                    (unsigned long)sk);

        serval_srv_init_ctrl_queue(sk);

#if defined(OS_LINUX_KERNEL)
        get_random_bytes(ssk->local_nonce, SERVAL_NONCE_SIZE);
        get_random_bytes(&ssk->snd_seq.iss, sizeof(ssk->snd_seq.iss));
#else
        {
                unsigned int i;
                unsigned char *seqno = (unsigned char *)&ssk->snd_seq.iss;
                for (i = 0; i < SERVAL_NONCE_SIZE; i++) {
                        ssk->local_nonce[i] = random() & 0xff;
                }
                for (i = 0; i < sizeof(ssk->snd_seq.iss); i++) {
                        seqno[i] = random() & 0xff;
                }
        }       
#endif

        /* Default to stop-and-wait behavior */
        ssk->rcv_seq.wnd = 1;
        ssk->snd_seq.wnd = 1;
        ssk->srtt = 0;
        ssk->rto = SERVAL_INITIAL_RTO;
}

void serval_sock_destroy(struct sock *sk)
{
        /* struct serval_sock *ssk = serval_sk(sk); */
	WARN_ON(sk->sk_state != SERVAL_CLOSED);

	/* It cannot be in hash table! */
	WARN_ON(!sk_unhashed(sk));

	if (!sock_flag(sk, SOCK_DEAD)) {
		LOG_WARN("Attempt to release alive inet socket %p\n", sk);
		return;
	}

	if (sk->sk_prot->destroy)
		sk->sk_prot->destroy(sk);

        LOG_DBG("SERVAL sock %p refcnt=%d tot_bytes_sent=%lu\n",
                sk, atomic_read(&sk->sk_refcnt) - 1, 
                serval_sk(sk)->tot_bytes_sent);
        
        LOG_DBG("sock rmem=%u wmem=%u omem=%u\n",
                atomic_read(&sk->sk_rmem_alloc),
                atomic_read(&sk->sk_wmem_alloc),
                atomic_read(&sk->sk_omem_alloc));

	sock_put(sk);
}

void serval_sock_done(struct sock *sk)
{
	serval_sock_set_state(sk, SERVAL_CLOSED);

	sk->sk_shutdown = SHUTDOWN_MASK;

	if (!sock_flag(sk, SOCK_DEAD))
		sk->sk_state_change(sk);
	else
		serval_sock_destroy(sk);
}

/* Destructor, called when refcount hits zero */
void serval_sock_destruct(struct sock *sk)
{
        struct serval_sock *ssk = serval_sk(sk);

        /* Stop timers */
        sk_stop_timer(sk, &ssk->retransmit_timer);
        sk_stop_timer(sk, &ssk->tw_timer);
        
        /* Purge queues */
        __skb_queue_purge(&sk->sk_receive_queue);
        __skb_queue_purge(&sk->sk_error_queue);

        /* Clean control queue */
        serval_srv_ctrl_queue_purge(sk);

        if (ssk->dev) {
                dev_put(ssk->dev);
        }

	if (sk->sk_type == SOCK_STREAM && 
            sk->sk_state != SERVAL_CLOSED) {
		LOG_ERR("Bad state %d %p\n",
                        sk->sk_state, sk);
		return;
	}

	if (!sock_flag(sk, SOCK_DEAD)) {
		LOG_DBG("Attempt to release alive serval socket: %p\n", sk);
		return;
	}

	if (atomic_read(&sk->sk_rmem_alloc)) {
                LOG_WARN("sk_rmem_alloc is not zero\n");
        }

	if (atomic_read(&sk->sk_wmem_alloc)) {
                LOG_WARN("sk_wmem_alloc is not zero\n");
        }

	atomic_dec(&serval_nr_socks);

	LOG_DBG("SERVAL socket %p destroyed, %d are still alive.\n", 
                sk, atomic_read(&serval_nr_socks));
}

int serval_sock_set_state(struct sock *sk, int new_state)
{
        /* TODO: state transition checks */
        
        if (new_state < SERVAL_SOCK_STATE_MIN ||
            new_state > SERVAL_SOCK_STATE_MAX) {
                LOG_ERR("invalid state\n");
                return -1;
        }

        LOG_DBG("%s -> %s\n",
                sock_state_str[sk->sk_state],
                sock_state_str[new_state]);

        sk->sk_state = new_state;

        return new_state;
}
