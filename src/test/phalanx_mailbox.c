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


int mailbox(int sid) {

        int sock;
        struct sockaddr_sv mboxaddr, cliaddr;
        static size_t total_bytes = 0;

	socklen_t addrlen = sizeof(cliaddr);
    
        if ((sock = socket_sv(AF_SERVAL, SOCK_DGRAM, SERVAL_PROTO_UDP)) < 0) {
                fprintf(stderr, "error creating AF_SERVAL socket: %s\n", 
                        strerror_sv(errno));
                return -1;
        }
  
        memset(&mboxaddr, 0, sizeof(mboxaddr));
        mboxaddr.sv_family = AF_SERVAL;
        mboxaddr.sv_srvid.s_sid32[0] = htonl(sid);
  
        set_reuse_ok(sock);
  
        if (bind_sv(sock, (struct sockaddr *)&mboxaddr, 
                    sizeof(mboxaddr)) < 0) {
                fprintf(stderr, "error binding socket: %s\n", 
                        strerror_sv(errno));
                close_sv(sock);
                return -1;
        }
        
        printf("mailbox: bound to service id %d\n", sid);
        memset(&cliaddr, 0, sizeof(cliaddr));

        payload p;
        int n;
        while (1) {

                n = recvfrom_sv(sock, &p, sizeof(payload), 0, 
                                  (struct sockaddr *)&cliaddr, &addrlen);
		
                if (n == -1) {
                        fprintf(stderr, "recvfrom: %s\n", strerror_sv(errno));
                        return -1;
                }

                if (n == 0) {
                        fprintf(stderr, "server: received EOF");
                        break;
                }

                printf("Received a %zd byte packet number %d from \'%s\' \n", n, p.num, 
                       service_id_to_str(&cliaddr.sv_srvid));

                //rbuf[n] = '\0';
                total_bytes += n;
                
        }

        close_sv(sock);
        return -1;
}

int main(int argc, char **argv)
{
        int ret, i;

        if (argc != 2) {
                printf("Usage: phalanx_mailbox service_id \n");
                exit(0);
        }

        //ips are used for failure detection

        int sid = atoi(argv[1]);
        ret = mailbox(sid);
        return ret;
}
