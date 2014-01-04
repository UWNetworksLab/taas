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


#define SENDBUF_SIZE (sizeof(char) * 1200)
#define NUM_PINGS "3"
static unsigned long dest_service_id;
static char *dest_service_id_str;
static unsigned long taas_service_id;
static char* taas_ip;   //required for failover forwarding
static char* taas_forwarding_ip;  //telling taas service where to forwrad packets to
static char* dest_ip;   //required for failure detection
static int num_packets;
static char* filepath;

static unsigned long CLIENT_SERVICE_ID = 100;

static int sock, sock1;

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

int doFailureDetection()
{
        char cmd_string[100];
        char o[200];

        memset(cmd_string, 0, sizeof(cmd_string));
        strcat(cmd_string, "ping -c ");
        strcat(cmd_string, NUM_PINGS);
        strcat(cmd_string, " ");
        strcat(cmd_string, dest_ip);
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

int client() {
	struct sockaddr_sv destSrvAddr, taasSrvAddr, cliaddr;
	int ret = 0, i, n;
        static size_t total_bytes = 0;
	unsigned N = 2000;
        int stop_sending = 0;
	char sbuf[SENDBUF_SIZE], rbuf[N+1];
        FILE *f;
        int failureDetected = 0; 

        struct {
                char padding[10];
                char sid[10];
                char forward_ip[20];
        } taas_rule;

        memset(&taas_rule, 0, sizeof(taas_rule));
        strcat(taas_rule.sid, dest_service_id_str);
        strcat(taas_rule.forward_ip, taas_forwarding_ip);

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

        sock1 = socket_sv(AF_SERVAL, SOCK_DGRAM, SERVAL_PROTO_UDP);
        if (sock == -1) {
                fprintf(stderr, "socket: %s\n",
                        strerror_sv(errno));
                return -1;
        }

	set_reuse_ok(sock1);

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
                        n = sendto_sv(sock, sbuf + count, nread - count, 0,
                                      (struct sockaddr *)&destSrvAddr, sizeof(destSrvAddr));

                        if (n < 0) {
                                fprintf(stderr, "\rerror sending data: %s\n",
                                        strerror_sv(errno));
                                return -1;
                        }
                        count =+ n;
                        total_bytes += n;
                }

                //sleep(1);
                //do failure detection here
                if (doFailureDetection() == -1) {
                        //failure detected => start forwarding to taas service
                        //first tell taas service to install the appropriate rule

                        if (failureDetected) {
                        }
                        ret = sendto_sv(sock1, &taas_rule, sizeof(taas_rule), 0, 
                                        (struct sockaddr *)&taasSrvAddr, sizeof(taasSrvAddr));
                        if (ret == -1) {
                                fprintf(stderr, "sendto: %s\n", strerror_sv(errno));
                        }
                        failureDetected = 1;

                        //rule sent to the taas service, now install the taas rule in own service table
                        
                }
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
        //taas_ip is the ip address that the taas service will forward the packet to
        //taas_forwarding ip is the ip address that the client tells that taas service to its rule
        if (argc != 8) {
                printf("Usage: udp_taas_client destination_service_id taas_service_id dest_ip taas_ip taas_forwading_ip num_packets filepath\n");
                exit(0);
        }
        dest_service_id = atoi(argv[1]);
        dest_service_id_str = argv[1];
        taas_service_id = atoi(argv[2]);
        dest_ip = argv[3];
        taas_ip = argv[4];
        taas_forwarding_ip = argv[5];
        num_packets = atoi(argv[6]);
        filepath =  argv[7];

        ret = client();

        printf("client done..\n");

        return ret;
}
