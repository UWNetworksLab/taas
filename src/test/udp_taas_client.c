/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
// Copyright (c) 2010 The Trustees of Princeton University (Trustees)

// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and/or hardware specification (the “Work”) to deal
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

static unsigned short ECHO_SERVICE_ID = 16383;
static unsigned short BACKCHANNEL_PORT = 9899;

static int sock;

static int sock_backchannel;

void signal_handler(int sig)
{
        printf("signal caught! closing socket...\n");
        //close(sock);
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

int client(char *ip) {
	struct sockaddr_sv srvaddr;
        struct sockaddr_in myaddr;
        struct sockaddr_in dummyaddr;
	int ret = 0;
	unsigned N = 2000;
	char sbuf[N];
        char rbuf[N+1];

        sock_backchannel = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

        if (sock_backchannel == -1) {
                fprintf(stderr, "socket: %s\n",
                        strerror_sv(errno));
                return -1;
        }
        set_reuse_ok(sock_backchannel);
        memset((char *)&myaddr, 0, sizeof(myaddr));
        myaddr.sin_family = AF_INET;
        inet_aton(ip, &myaddr.sin_addr);
        myaddr.sin_port = htons(BACKCHANNEL_PORT);

        bind(sock_backchannel, (struct sockaddr *)&myaddr, sizeof(myaddr));

	bzero(&srvaddr, sizeof(srvaddr));
	srvaddr.sv_family = AF_SERVAL;
	srvaddr.sv_srvid.s_sid32[0] = htonl(ECHO_SERVICE_ID);

	sock = socket_sv(AF_SERVAL, SOCK_DGRAM, SERVAL_PROTO_UDP);


        if (sock == -1) {
                fprintf(stderr, "socket: %s\n",
                        strerror_sv(errno));
                return -1;
        }

	set_reuse_ok(sock);

        ret = connect_sv(sock, (struct sockaddr *)&srvaddr, sizeof(srvaddr));

	if (ret < 0) {
		fprintf(stderr, "connect: %s\n",
			strerror_sv(errno));
		return -1;
	}

	printf("connected\n");

	while (1) {
                sprintf(sbuf, "ping %s %d", ip, BACKCHANNEL_PORT);
		printf("client: sending \"%s\" to service ID %s\n",
                       sbuf, service_id_to_str(&srvaddr.sv_srvid));

                ret = sendto_sv(sock, sbuf, strlen(sbuf), 0, (struct sockaddr *)&srvaddr, sizeof(srvaddr));

		if (ret < 0) {
			fprintf(stderr, "send failed (%s)\n",
                                strerror_sv(errno));
                        break;
		}

		ret = recvfrom(backchannel_sock, rbuf, N, 0, (struct sockaddr *)&dummyaddr, sizeof(dummyaddr));
		rbuf[ret] = 0;

                if (ret == 0) {
                        printf("server closed\n");
                        break;
                } else {
                        printf("Response from server: %s\n", rbuf);

                        if (strcmp(sbuf, "quit") == 0)
                                break;
                }
                sleep(1);
	}

	if (close_sv(sock) < 0)
		fprintf(stderr, "close: %s\n",
                        strerror_sv(errno));

        return ret;
}

int main(int argc, char **argv)
{
	struct sigaction action;
        int ret;

	memset(&action, 0, sizeof(struct sigaction));
        action.sa_handler = signal_handler;

	/* The server should shut down on these signals. */
        //sigaction(SIGTERM, &action, 0);
	//sigaction(SIGHUP, &action, 0);
	//sigaction(SIGINT, &action, 0);

        ret = client(argv[1]);

        printf("client done..\n");

        return ret;
}
