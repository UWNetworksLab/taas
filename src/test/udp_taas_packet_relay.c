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

int server(long serviceid)
{
        int sock1, sock2, backlog = 8;
        struct sockaddr_sv serveraddr, relayaddr, cliaddr;  
    
        if ((sock1 = socket_sv(AF_SERVAL, SOCK_DGRAM, SERVAL_PROTO_UDP)) < 0) {
                fprintf(stderr, "error creating AF_SERVAL socket: %s\n", 
                        strerror_sv(errno));
                return -1;
        }

        if ((sock2 = socket_sv(AF_SERVAL, SOCK_DGRAM, SERVAL_PROTO_UDP)) < 0) {
                fprintf(stderr, "error creating AF_SERVAL socket: %s\n", 
                        strerror_sv(errno));
                return -1;
        }
  
        memset(&relayaddr, 0, sizeof(relayaddr));
        relayaddr.sv_family = AF_SERVAL;
        relayaddr.sv_srvid.s_sid32[0] = htonl(serviceid);

        memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sv_family = AF_SERVAL;
        servaddr.sv_srvid.s_sid32[0] = htonl(serviceid);
  
        set_reuse_ok(sock1);
        set_reuse_ok(sock2);
  
        if (bind_sv(sock1, (struct sockaddr *)&relayaddr, 
                    sizeof(relayaddr)) < 0) {
                fprintf(stderr, "error binding socket: %s\n", 
                        strerror_sv(errno));
                close_sv(sock1);
                return -1;
        }

        if (bind_sv(sock2, (struct sockaddr *)&relayaddr, 
                    sizeof(relayaddr)) < 0) {
                fprintf(stderr, "error binding socket: %s\n", 
                        strerror_sv(errno));
                close_sv(sock2);
                return -1;
        }
        
        printf("relay: bound to service id %d\n", serviceid);

        listen_sv(sock1, backlog);

        if (connect_sv(sock2, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
                        fprintf(stderr, "connect: %s\n",
                                strerror_sv(errno));
                        return -1;
        }

        do {
                socklen_t l = sizeof(cliaddr);
                int k = 0;

                printf("calling accept\n");

                int fd = accept_sv(sock1, (struct sockaddr *)&cliaddr, &l);

                if (fd < 0) {
                        fprintf(stderr, "error accepting new conn %s\n", 
                                strerror_sv(errno));
                        return -1;
                }

                printf("relay: recv conn from service id %s; got fd = %d\n",
                       service_id_to_str(&cliaddr.sv_srvid), fd);
        
                do {
                        unsigned N = 2000;
                        char buf[N];
                        int n;

                        if ((n = recv_sv(fd, buf, N, 0)) < 0) {
                                fprintf(stderr, 
                                        "relay: error receiving client request: %s\n",
                                        strerror_sv(errno));
                                break;
                        }
                        if (n == 0) {
                                fprintf(stderr, "server: received EOF; "
                                        "listening for new conns\n");
                                break;
                        }
                        buf[n] = '\0';

                        if (n > 0) {
                                //send packet to server
                                if (send_sv(sock2, buf, strlen(buf), 0) < 0) {
                                        fprintf(stderr, "send failed (%s)\n", 
                                                strerror_sv(errno));
                                        break;
                                }
                        }
                        k++;
                } while (1);
                close_sv(fd);
                printf("relay listening for NEW connections\n");
        } while (1);
        close_sv(sock);

        printf("Exiting\n");

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
        
        progname = argv[0];
        argc--;
        argv++;
    
        while (argc && argv) {
                if (strcmp("-s", argv[0]) == 0 || 
                    strcmp("--serviceid", argv[0]) == 0) {
                        if (argv[1]) {
                                char *endptr = NULL;
                                unsigned long sid;
                                
                                sid = strtoul(argv[1], &endptr, 10);
                                
                                if (*endptr != '\0') {
                                        // conversion failure
                                } else {
                                        listen_srvid.s_sid32[0] = htonl(sid);
                                }
                                argc--;
                                argv++;
                        }
                } else if (strcmp("-h", argv[0]) == 0 || 
                           strcmp("--help", argv[0]) == 0) {
                        print_help();
                        return EXIT_SUCCESS;
                } else {
                        print_help();
                        return EXIT_FAILURE;
                }
                argc--;
                argv++;
        }

        return server();
}
