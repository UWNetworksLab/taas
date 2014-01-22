/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/serval.h>
#include <libserval/serval.h>
#include <netdb.h>
#include "common.h"

int main(int argc, char **argv)
{
        int sock;
	ssize_t ret = 0;
        struct sockaddr_sv sv;
        unsigned long sid = atoi(argv[1]);
        
        sock = socket_sv(AF_SERVAL, SOCK_DGRAM, 0);

	if (sock == -1) { 
		fprintf(stderr, "could not create SERVAL socket: %s\n",
			strerror(errno));
		return -1;
        }

        memset(&sv, 0, sizeof(sv));
	sv.sv_family = AF_SERVAL;
	sv.sv_srvid.s_sid32[0] = htonl(sid);

        payload p;
        while (1) {
                ret = sendto_sv(sock, &p, sizeof(payload), 0, 
                                (struct sockaddr *)&sv, sizeof(sv));
                if (ret == -1) {
                        fprintf(stderr, "sendto: %s\n", strerror_sv(errno));
                        close(sock);
                return -1;
                }
        }

        close(sock);
	return ret;
}
