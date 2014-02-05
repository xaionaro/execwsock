/*
    execwsock - creates unix socket, runs a programs and removes the socket 
        after the program ends. It's a way to control the process status that
        may be used for init systems or for single-instance locking scripts.

Copyright (c) 2014, Dmitry Yu Okunev <dyokunev@ut.mephi.ru> 0x8E30679C
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */

/* If you have any question, I'll recommend you to ask irc.freenode.net#openrc */

/* === includes === */

#include <stdio.h>	/* fprintf()		*/
#include <stdlib.h>	/* atexit()		*/
#include <string.h>	/* strerror()		*/
#include <pthread.h>	/* pthread_create()	*/
#include <unistd.h>	/* execve()		*/
#include <errno.h>	/* errno		*/
#include <sys/stat.h>	/* mkdir()		*/
#include <sys/types.h>	/* mkdir()		*/
#include <sys/socket.h>	/* socket()		*/
#include <sys/un.h>	/* struct sockaddr_un	*/
#include <sys/wait.h>	/* waitpid()		*/

/* === configuration === */

#define DIR_SOCKETS	"/run/openrc/execsockets"
#define ENV_SVCNAME	"RC_SVCNAME"
#define SOCKET_BACKLOG	5
#define UID		1
#define GID		1

/* === global variables === */

pthread_t		sock_ctrl_th	=  0;
int			sock		=  0;
struct sockaddr_un	sock_addr	= {0};

/* === code self === */

void cleanup()
{
	if (sock) {
		close(sock);
		unlink(sock_addr.sun_path);
		sock = 0;
	}

	return;
}

void *sock_ctrl(void *arg)
{
	while (sock) {
		int events;
		int client;
		fd_set rfds;

		FD_ZERO(&rfds);
		FD_SET(sock, &rfds);
		events = select(sock+1, &rfds, NULL, NULL, NULL);

		if (events < 0)
			break;

		if (!events)
			continue;

		client = accept(sock, NULL, NULL);
		close(client);
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	{ /* initializating cleanup function */
		if (atexit(cleanup)) {
			fprintf(stderr, "Got error while atexit(): %s.\n", strerror(errno));
			exit(errno);
		}
	}

	{ /* dropping privileges */
		if (getuid() == 0) {
			if (setgid(GID)) {
				fprintf(stderr, "Got error while setgid(%i): %s.\n", GID, strerror(errno));
				exit(errno);
			}
			if (setuid(UID)) {
				fprintf(stderr, "Got error while setuid(%i): %s.\n", UID, strerror(errno));
				exit(errno);
			}
		}
	}

	{ /* checking and preparing some stuff */
		if (getenv(ENV_SVCNAME) == NULL) {
			fprintf(stderr, "Environment variable \""ENV_SVCNAME"\" is not set.\n");
			exit(EINVAL);
		}

		if (mkdir(DIR_SOCKETS, 0700)) {
			if (errno != EEXIST) {
				fprintf(stderr, "Cannot create directory \""DIR_SOCKETS"\": %s.\n", strerror(errno));
				exit(errno);
			}
		}
	}

	{ /* preparing the socket */
		size_t sock_addr_len;
		sock_addr.sun_family = AF_UNIX;
		sprintf(sock_addr.sun_path, DIR_SOCKETS"/%s.sock", getenv(ENV_SVCNAME));	/* TODO: check  */

		sock_addr_len = sizeof(sock_addr.sun_family) + strlen(sock_addr.sun_path);

		sock = socket(AF_UNIX, SOCK_STREAM, 0);
		if (sock == -1) {
			fprintf(stderr, "Got error while socket(): %s.\n", strerror(errno));
			exit(errno);
		}

		if (bind(sock, (struct sockaddr *)&sock_addr, sock_addr_len)) {
			fprintf(stderr, "Cannot bind() on \"%s\": %s.\n", sock_addr.sun_path, strerror(errno));
			exit(errno);
		}

		if (listen(sock, SOCKET_BACKLOG)) {
			fprintf(stderr, "Cannot listen() on \"%s\": %s.\n", sock_addr.sun_path, strerror(errno));
			exit(errno);
		}
	}

	{ /* running a thread to accept and drop clients */
		if (pthread_create(&sock_ctrl_th, NULL, sock_ctrl, NULL)) {
			fprintf(stderr, "Cannot create a thread to control the socket: %s.\n", strerror(errno));
			exit(errno);
		}
	}

	{ /* running the process */
		pid_t pid;

		pid = fork();

		if (pid == -1) {
			fprintf(stderr, "Got error while fork()-ing: %s.\n", strerror(errno));
			exit(errno);
		}

		if (pid == 0) { /* the child */
			argv++;
			argc--;
			execvp(argv[0], argv);
			exit(EXIT_FAILURE);	/* exec never returns :) */
		} else {	/* the parent */
			int child_status;
			waitpid(pid, &child_status, 0);
			exit(WEXITSTATUS(child_status));
		}
	}

	exit(EXIT_FAILURE);	/* this's unreachable line */
}

