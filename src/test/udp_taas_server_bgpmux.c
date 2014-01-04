/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
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

int set_reuse_ok(int sock);
#define RECVBUF_SIZE (sizeof(char) * 1200)
static unsigned long sid;
static char* filepath;

int server()
{
        int sock;
        struct sockaddr_sv servaddr, cliaddr;
        static size_t total_bytes = 0;
        FILE *f;

        struct {
                int num;
                char rbuf[RECVBUF_SIZE];
        } payload;

	socklen_t addrlen = sizeof(cliaddr);
    
        if ((sock = socket_sv(AF_SERVAL, SOCK_DGRAM, SERVAL_PROTO_UDP)) < 0) {
                fprintf(stderr, "error creating AF_SERVAL socket: %s\n", 
                        strerror_sv(errno));
                return -1;
        }
  
        memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sv_family = AF_SERVAL;
        servaddr.sv_srvid.s_sid32[0] = htonl(sid);
  
        set_reuse_ok(sock);
  
        if (bind_sv(sock, (struct sockaddr *)&servaddr, 
                    sizeof(servaddr)) < 0) {
                fprintf(stderr, "error binding socket: %s\n", 
                        strerror_sv(errno));
                close_sv(sock);
                return -1;
        }
        
        printf("server: bound to service id %d\n", sid);
        memset(&cliaddr, 0, sizeof(cliaddr));

        f = fopen(filepath, "w");
        if (!f) {
                fprintf(stderr, "cannot open file %s : %s\n",
                        filepath, strerror_sv(errno));
                return -1;
        }

        int n;
        while (1) {

                n = recvfrom_sv(sock, &payload, sizeof(payload), 0, 
                                  (struct sockaddr *)&cliaddr, &addrlen);
		
                if (n == -1) {
                        fprintf(stderr, "recvfrom: %s\n", strerror_sv(errno));
                        return -1;
                }

                if (n == 0) {
                        fprintf(stderr, "server: received EOF");
                        break;
                }

                printf("Received a %zd byte packet number %d from \'%s\' \n", n, payload.num, 
                       service_id_to_str(&cliaddr.sv_srvid));

                //rbuf[n] = '\0';
                total_bytes += n;
                size_t nwrite = fwrite(payload.rbuf, sizeof(char), RECVBUF_SIZE, f);
                if (nwrite < RECVBUF_SIZE) {
                        fprintf(stderr, "\rError writing file\n");
                        return -1;        
                }
                fflush(f);
        }

        close_sv(sock);
        return -1;
}

int set_reuse_ok(int sock)
{
        int option = 1;
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, 
                       &option, sizeof(option)) < 0) {
                fprintf(stderr, "proxy setsockopt error");
                return -1;
        }
        return 0;
}

int main(int argc, char **argv)
{

        if (argc != 3) {
                printf("Usage: udp_taas_server service_id filepath\n");
                exit(0);
        }
        sid = atoi(argv[1]);
        filepath =  argv[2];
        return server();
}
