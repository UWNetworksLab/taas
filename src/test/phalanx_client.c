/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
// Copyright (c) 2010 The Trustees of Princeton University (Trustees)

// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and/or hardware specification (the “Work”)
// in the Work without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Work, and to permit persons to whom the Work is
// furnished to do so, subject to the following conditions: The above
// copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Work.

// THE WORK IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE WORK OR THE USE OR OTHER
// DEALINGS IN THE WORK.
#include <libserval/serval.h>
#include <netinet/serval.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include "common.h"
#include "../../list/src/list.h"


#define NUM_PINGS "1"
#define SLEEP_INTERVAL 100000  //in microseconds

static int num_packets;
static char* filepath;

static unsigned long CLIENT_SERVICE_ID = 1000;

static int sock;

int install_rule(char *action, char *sid, char *forwarding_ip)
{
        char cmd_string[200];
        char o[1000];

        memset(cmd_string, 0, sizeof(cmd_string));
        strcat(cmd_string, "./src/tools/serv service ");
        strcat(cmd_string, action);
        strcat(cmd_string, " ");
        strcat(cmd_string, sid);
        strcat(cmd_string, " ");
        strcat(cmd_string, forwarding_ip);

        FILE *cmd = popen(cmd_string, "r");
        fgets(o, sizeof(char)*sizeof(o), cmd);
        pclose(cmd);
        printf("%s\n", o);
        if (strstr(o, action) == 0)
                return -1;
        
        return 0;
}
  
int set_reuse_ok(int soc)
{
	int option = 1;

	if (setsockopt(soc, SOL_SOCKET, SO_REUSEADDR,
                       &option, sizeof(option)) < 0) {
		fprintf(stderr, "proxy setsockopt error");
		return -1;
	}
	return 0;
}

int doFailureDetection(char* ip)
{
        char cmd_string[100];
        char o[200];

        memset(cmd_string, 0, sizeof(cmd_string));
        strcat(cmd_string, "ping -c ");
        strcat(cmd_string, NUM_PINGS);
        strcat(cmd_string, " ");
        strcat(cmd_string, ip);
        strcat(cmd_string, " | grep icmp");

        FILE *cmd = popen(cmd_string, "r");
        int i, num_failures = 0;
        for (i = 0; i < atoi(NUM_PINGS); i++) {
                fgets(o, sizeof(char)*sizeof(o), cmd);
                if (strstr(o, "time") == 0)
                        num_failures++;
        }
        pclose(cmd);
        if (num_failures > 0)
                return -1;
        return 0;
}

int client(mailbox *m, int num_mailboxes) {

        struct sockaddr_sv *addr = (struct sockaddr_sv **)calloc(num_mailboxes, sizeof(struct sockaddr_sv));
        
	struct sockaddr_sv cliaddr;
	int ret = 0, i, n, mbox_idx;
        static size_t total_bytes = 0;
        int stop_sending = 0;
        FILE *f;
        int failureDetected = 0;

        
        for (i=0; i < num_mailboxes; i++) {
                addr[i].sv_family = AF_SERVAL;
                addr[i].sv_srvid.s_sid32[0] = htonl(m[i].sid);
        }

        bzero(&cliaddr, sizeof(cliaddr));
	cliaddr.sv_family = AF_SERVAL;
	cliaddr.sv_srvid.s_sid32[0] = htonl(CLIENT_SERVICE_ID);

	sock = socket_sv(AF_SERVAL, SOCK_DGRAM, SERVAL_PROTO_UDP);
        if (sock == -1) {
                fprintf(stderr, "socket: %s\n",
                        strerror_sv(errno));
                free(addr);
                return -1;
        }

	set_reuse_ok(sock);

        ret = bind_sv(sock, (struct sockaddr *) &cliaddr, sizeof(cliaddr));
	if (ret < 0) {
		fprintf(stderr, "bind: %s\n",
                        strerror_sv(errno));
		return -1;
	}

        f = fopen(filepath, "r");
        if (!f) {
                fprintf(stderr, "cannot open file %s : %s\n",
                        filepath, strerror_sv(errno));
                return -1;
        }

        payload p;
        for (i = 0; i < num_packets; i++) {

                if (stop_sending)
                        break;
                mbox_idx = i % num_mailboxes;
		printf("client: sending packet %d to service ID %s at mailbox %d %s\n",
                       i, service_id_to_str(&addr[mbox_idx].sv_srvid), m[mbox_idx].id, m[mbox_idx].ip);

                size_t nread = fread(p.sbuf, sizeof(char), SENDBUF_SIZE, f);

                if (nread < SENDBUF_SIZE) {
                        if (feof(f) != 0) {
                                stop_sending = 1;
                                printf("\rEOF reached, file successfully sent\n");
                        } else {
                                fprintf(stderr, "\rError reading file\n");
                                return -1;
                        }
                }

                printf("Read %zu bytes from file %s\n", nread, filepath);

                p.num = i;
                n = sendto_sv(sock, &p, sizeof(payload), 0,
                              (struct sockaddr *)&addr[mbox_idx], sizeof(addr[mbox_idx]));
                if (n < 0) {
                        fprintf(stderr, "\rerror sending data: %s\n",
                                strerror_sv(errno));
                        return -1;
                }
                total_bytes += n;

                //do failure detection here
                usleep(SLEEP_INTERVAL);
                                
        }

	if (close_sv(sock) < 0)
		fprintf(stderr, "close: %s\n",
                        strerror_sv(errno));

        return ret;
}

int main(int argc, char **argv)
{
	struct sigaction action;
        int ret, i;

	memset(&action, 0, sizeof(struct sigaction));
        //action.sa_handler = signal_handler;

	/* The server should shut down on these signals. */
        //sigaction(SIGTERM, &action, 0);
	//sigaction(SIGHUP, &action, 0);
	//sigaction(SIGINT, &action, 0);

        if ((argc % 2 != 0) || (argc < 6)) {
                printf("Usage: phalanx_client num_mailboxes ['num_mailboxes' pairs of service ids and ips] num_packets filepath\n");
                exit(0);
        }

        //ips are used for failure detection

        int num_mailboxes = atoi(argv[1]);
        mailbox *m = (mailbox*)calloc(num_mailboxes, sizeof(mailbox));
        //list_t *list = list_new();
        for (i=0; i < num_mailboxes; i++) {
                m[i].id = i;
                m[i].sid = atoi(argv[i*2+2]);
                m[i].ip = argv[i*2 + 3];
        }

        num_packets = atoi(argv[num_mailboxes*2+2]);
        filepath =  argv[num_mailboxes*2+3];

        /*
        for (i=0; i < num_mailboxes; i++) {
                printf("%d\n", m[i].sid);
                printf("%s\n", m[i].ip);
        }
        */
        ret = client(m, num_mailboxes);

        printf("client done..\n");

        free(m);
        return ret;
}
