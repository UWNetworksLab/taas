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
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

static unsigned short CLIENT_OBJECT_ID = 32769;
static unsigned short ECHO_OBJECT_ID = 16385;

int set_reuse_ok(int soc)
{
	int option = 1;
	if (setsockopt(soc, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0) {
		fprintf(stderr, "proxy setsockopt error");
		return -1;
	}
	return 0;
}

void client(void) {
	int sock;
	struct sockaddr_sv cliaddr;
	struct sockaddr_sv srvaddr;
	int n;
	unsigned N = 512;
	char sbuf[N], rbuf[N + 1];

	bzero(&cliaddr, sizeof(cliaddr));
	cliaddr.sv_family = AF_SERVAL;
	cliaddr.sv_srvid.s_sid16 = htons(CLIENT_OBJECT_ID);

	bzero(&srvaddr, sizeof(srvaddr));
	srvaddr.sv_family = AF_SERVAL;
	srvaddr.sv_srvid.s_sid16 = htons(ECHO_OBJECT_ID);
  
	sock = socket_sv(AF_SERVAL, SOCK_DGRAM, SERVAL_PROTO_UDP);
	set_reuse_ok(sock);

	if (bind_sv(sock, (struct sockaddr *) &cliaddr, sizeof(cliaddr)) < 0) {
		fprintf(stderr, "error client binding socket: %s\n", strerror_sv(errno));
		exit(EXIT_FAILURE);
	}

	if (connect_sv(sock, (struct sockaddr *)&srvaddr, sizeof(srvaddr)) < 0) {
		fprintf(stderr, "error client connecting to socket: %s\n",
			strerror_sv(errno));
		exit(EXIT_FAILURE);
	}
	fprintf(stderr, "client: waiting on user input :>");
	while (fgets(sbuf, N, stdin) != NULL) {
		if (strlen(sbuf) == 1) {
			fprintf(stderr, "\n\nclient: waiting on user input :>");
			continue;
		}
		if (strlen(sbuf) < N) // remove new line
			sbuf[strlen(sbuf) - 1] = '\0';
		fprintf(stderr, "client: sending \"%s\" to object ID %s\n", sbuf,
			service_id_to_str(&srvaddr.sv_srvid));
		if (send_sv(sock, sbuf, strlen(sbuf), 0) < 0) {
			fprintf(stderr, "client: send_sv() failed (%s)\n", strerror_sv(errno));
			exit(EXIT_FAILURE);
		}
		n = recv_sv(sock, rbuf, N, 0);
		rbuf[n] = 0;

                if (n == -1) {
                        fprintf(stderr, "recv error: %s\n", strerror_sv(errno));
                } else {
                        fprintf(stderr, "Response from server: %s\n", rbuf);
                        if (strcmp(sbuf, "quit") == 0)
                                break;
                }
                fprintf(stderr, "client: waiting on user input :>");
	}
	if (close_sv(sock) < 0)
		fprintf(stderr, "client: error closing socket %s", strerror_sv(errno));
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	client();

	return 0;
}

