// SPDX-License-Identifier: GPL-2.0-only
/* Real native AF_QIPCRTR loopback only; no phone endpoint or remote firmware. */
#include <sys/socket.h>
#include <sys/time.h>
#include <linux/qrtr.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
static void check(int okay, const char *what)
{
	if (!okay) { perror(what); exit(1); }
}
int main(void)
{
	int fd[2], i, j;
	struct sockaddr_qrtr address[2], peer;
	struct timeval timeout = { .tv_sec = 2 };
	unsigned char sent[257], received[257];
	socklen_t length;
	for (i = 0; i < 2; i++) {
		fd[i] = socket(AF_QIPCRTR, SOCK_DGRAM, 0);
		check(fd[i] >= 0, "socket");
		check(!setsockopt(fd[i], SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)), "timeout");
		memset(&address[i], 0, sizeof(address[i]));
		address[i].sq_family = AF_QIPCRTR; address[i].sq_node = 1;
		check(!bind(fd[i], (struct sockaddr *)&address[i], sizeof(address[i])), "bind");
		length = sizeof(address[i]);
		check(!getsockname(fd[i], (struct sockaddr *)&address[i], &length), "getsockname");
		check(length == sizeof(address[i]) && address[i].sq_node == 1 && address[i].sq_port, "local address");
	}
	check(address[0].sq_port != address[1].sq_port, "distinct ports");
	for (i = 0; i < 128; i++) {
		int sender = i & 1, receiver = sender ^ 1;
		size_t bytes = (size_t)i * 2 + 1;
		for (j = 0; j < (int)bytes; j++) sent[j] = (unsigned char)(i * 17 + j * 31);
		check(sendto(fd[sender], sent, bytes, 0, (struct sockaddr *)&address[receiver], sizeof(address[receiver])) == (ssize_t)bytes, "sendto");
		length = sizeof(peer); memset(received, 0, sizeof(received));
		check(recvfrom(fd[receiver], received, sizeof(received), 0, (struct sockaddr *)&peer, &length) == (ssize_t)bytes, "recvfrom");
		check(!memcmp(sent, received, bytes), "payload");
		check(length == sizeof(peer) && peer.sq_node == address[sender].sq_node && peer.sq_port == address[sender].sq_port, "peer address");
	}
	close(fd[0]); close(fd[1]);
	puts("AVR_QRTR_LOCAL_PASS packets=128 scope=native-local-sockets-only");
	return 0;
}
