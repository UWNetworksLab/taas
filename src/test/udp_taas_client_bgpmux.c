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


#define SENDBUF_SIZE (sizeof(char) * 1460)
static unsigned long dest_service_id;
static unsigned long taas_service_id;
static char* taas_ip;
static int num_packets;
static char* filepath;

static unsigned short CLIENT_SERVICE_ID = 4583;

static int sock;

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

int client() {
	struct sockaddr_sv destSrvAddr, taasSrvAddr, cliaddr;
	int ret = 0, i, n;
        static size_t total_bytes = 0;
	unsigned N = 2000;
        int stop_sending = 0;
	char sbuf[SENDBUF_SIZE], rbuf[N+1];
        FILE *f;

	bzero(&destSrvAddr, sizeof(destSrvAddr));
	destSrvAddr.sv_family = AF_SERVAL;
	destSrvAddr.sv_srvid.s_sid32[0] = htonl(dest_service_id);

        bzero(&taasSrvAddr, sizeof(taasSrvAddr));
	taasSrvAddr.sv_family = AF_SERVAL;
	taasSrvAddr.sv_srvid.s_sid32[0] = htonl(taas_service_id);

        bzero(&cliaddr, sizeof(cliaddr));
	cliaddr.sv_family = AF_SERVAL;
	cliaddr.sv_srvid.s_sid32[0] = htonl(CLIENT_SERVICE_ID);

	sock = socket_sv(AF_SERVAL, SOCK_DGRAM, SERVAL_PROTO_UDP);
        if (sock == -1) {
                fprintf(stderr, "socket: %s\n",
                        strerror_sv(errno));
                return -1;
        }

	set_reuse_ok(sock);

        ret = bind_sv(sock, (struct sockaddr *) &cliaddr, sizeof(cliaddr));
	if (ret < 0) {
		fprintf(stderr, "bind: %s\n",
                        strerror_sv(errno));
		return -1;
	}


        ret = connect_sv(sock, (struct sockaddr *)&destSrvAddr, sizeof(destSrvAddr));
	if (ret < 0) {
		fprintf(stderr, "connect: %s\n",
			strerror_sv(errno));
		return -1;
	}
	printf("connected to original destination service\n");

        ret = connect_sv(sock, (struct sockaddr *)&taasSrvAddr, sizeof(taasSrvAddr));
	if (ret < 0) {
		fprintf(stderr, "connect: %s\n",
			strerror_sv(errno));
		return -1;
	}
	printf("connected to taas service\n");

        f = fopen(filepath, "r");
        if (!f) {
                fprintf(stderr, "cannot open file %s : %s\n",
                        filepath, strerror_sv(errno));
                return -1;
        }

        for (i = 0; i < num_packets; i++) {

                if (stop_sending)
                        break;
		printf("client: sending packet %d to service ID %s\n",
                       i, service_id_to_str(&destSrvAddr.sv_srvid));

                size_t nread = fread(sbuf, sizeof(char), SENDBUF_SIZE, f);

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

                int count = 0;
                while (count < nread) {
                        n = send_sv(sock, sbuf + count, nread - count, 0);

                        if (n < 0) {
                                fprintf(stderr, "\rerror sending data: %s\n",
                                        strerror_sv(errno));
                                return -1;
                        }
                        count =+ n;
                        total_bytes += n;
                }

                ret = recv_sv(sock, rbuf, N, 0);
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

        //taas service installs the forwarding rule that the client sends to it
        if (argc != 6) {
                printf("Usage: udp_taas_client destination_service_id taas_service_id taas_address num_packets filepath\n");
                exit(0);
        }
        dest_service_id = atoi(argv[1]);
        taas_service_id = atoi(argv[2]);
        taas_ip = argv[3];
        num_packets = atoi(argv[4]);
        filepath =  argv[5];

        ret = client();

        printf("client done..\n");

        return ret;
}
