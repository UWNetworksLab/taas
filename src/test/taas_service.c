/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/serval.h>
#include <libserval/serval.h>

int install_rule(char *action, char *sid, char *forwarding_ip)
{

        /*
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL)
                fprintf(stdout, "Current working dir: %s\n", cwd);
        else
                perror("getcwd() error");
        */
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

int main(int argc, char **argv)
{
	int sock;
	unsigned long data = 0;
	ssize_t ret;
        struct sockaddr_sv servaddr, cliaddr;
	socklen_t addrlen = sizeof(cliaddr);
        
        struct {
                char padding[10];
                char sid[10];
                char forward_ip[20];
        } input_rule;

        memset(&input_rule, 0, sizeof(input_rule));

	sock = socket_sv(AF_SERVAL, SOCK_DGRAM, 0);

	if (sock == -1) { 
		fprintf(stderr, "could not create SERVAL socket: %s\n",
			strerror(errno));
		return -1;
	}

        memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sv_family = AF_SERVAL;
	servaddr.sv_srvid.s_sid32[0] = htonl(7);

        memset(&cliaddr, 0, sizeof(cliaddr));

	ret = bind_sv(sock, (struct sockaddr *)&servaddr, sizeof(servaddr));
	
	if (ret == -1) {
		fprintf(stderr, "bind: %s\n", strerror_sv(errno));
		return -1;
	}

        while (1) {
                ret = recvfrom_sv(sock, &input_rule, sizeof(input_rule), 0, 
                                  (struct sockaddr *)&cliaddr, &addrlen);
		
                if (ret == -1) {
                        fprintf(stderr, "recvfrom: %s\n", strerror_sv(errno));
                        return -1;
                }

                printf("Received a %zd byte packet from \'%s\' \n", ret,
                       service_id_to_str(&cliaddr.sv_srvid));
                printf("received service id = %s\n", input_rule.sid);
                printf("received forwarding ip = %s\n", input_rule.forward_ip);

                //first delete, and then add
                ret = install_rule("del", input_rule.sid, input_rule.forward_ip);
                if (ret == -1) {
                        fprintf(stderr, "error installing the rule\n");
                        return -1;
                }

                ret = install_rule("add", input_rule.sid, input_rule.forward_ip);
                if (ret == -1) {
                        fprintf(stderr, "error installing the rule\n");
                        return -1;
                }
                
        }

	return ret;
}
