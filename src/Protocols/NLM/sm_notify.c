#include <memory.h> /* for memset */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include "nsm.h"

#define USAGE "usage: sm_notify -l <local address> -m <monitor host> -r <remote address> -s <state>"

/* Default timeout can be changed using clnt_control() */
static struct timeval TIMEOUT = { 25, 0 };

void *
nsm_notify_1(notify *argp, CLIENT *clnt)
{
	static char clnt_res;
	AUTH *nsm_auth;
	nsm_auth = authnone_create();

	memset((char *)&clnt_res, 0, sizeof(clnt_res));
	if (clnt_call(clnt, nsm_auth, SM_NOTIFY,
		(xdrproc_t) xdr_notify, (caddr_t) argp,
		(xdrproc_t) xdr_void, (caddr_t) &clnt_res,
		TIMEOUT) != RPC_SUCCESS) {
		return NULL;
	}
	return (void *)&clnt_res;
}

int main(int argc, char **argv)
{
	int c;
	int port, pflag = 0;
	int state, sflag = 0;
	char mon_client[100], mflag = 0;
	char remote_addr_s[100], rflag = 0;
	char local_addr_s[100], lflag = 0;
	notify arg;
	CLIENT *clnt;
	struct netbuf *buf;
	struct addrinfo *result;
	struct addrinfo hints;
	char port_str[20];

	struct sockaddr_in local_addr;
	struct sockaddr_in remote_addr;

	int fd;

	while ((c = getopt(argc, argv, "p:r:m:l:s:")) != EOF)
		switch (c) {
		case 'p':
			port = atoi(optarg);
			pflag = 1;
			break;
		case 's':
			state = atoi(optarg);
			sflag = 1;
			break;
		case 'm':
			strcpy(mon_client, optarg);
			mflag = 1;
			break;
		case 'r':
			strcpy(remote_addr_s, optarg);
			rflag = 1;
			break;
		case 'l':
			strcpy(local_addr_s, optarg);
			lflag = 1;
			break;
		case '?':
		default:
			fprintf(stderr, "%s\n", USAGE);
			exit(1);
			break;
	}

	if ((pflag + sflag + lflag + mflag + rflag) != 5) {
		fprintf(stderr, "%s\n", USAGE);
		exit(1);
	}


	/* create a udp socket */
	fd = socket(PF_INET, SOCK_DGRAM|SOCK_NONBLOCK, IPPROTO_UDP);
	if (fd < 0) {
		fprintf(stderr, "socket call failed. errno=%d\n", errno);
		exit(1);
	}

	/* set up the sockaddr for local endpoint */
	memset(&local_addr, 0, sizeof(struct sockaddr_in));
	local_addr.sin_family = PF_INET;
	local_addr.sin_port = htons(port);
	local_addr.sin_addr.s_addr = inet_addr(local_addr_s);

	if (bind(fd, (struct sockaddr *)&local_addr,
			sizeof(struct sockaddr)) < 0) {
		fprintf(stderr, "bind call failed. errno=%d\n", errno);
		exit(1);
	}

	/* find the port for SM service of the remote server */
	buf = rpcb_find_mapped_addr(
				"udp",
				SM_PROG, SM_VERS,
				remote_addr_s);

	/* handle error here, for example,
	 * client side blocking rpc call
	 */
	if (buf == NULL) {
		close(fd);
		return -1;
	}

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM; /* UDP */
	hints.ai_protocol = 0;  /* Any protocol */
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	/* convert port to string format */
	sprintf(port_str, "%d",
		htons(((struct sockaddr_in *)
		buf->buf)->sin_port));

	clnt = clnt_dg_ncreate(fd, buf, SM_PROG,
			SM_VERS, 0, 0);

	arg.my_name = mon_client;
	arg.state = state;
	nsm_notify_1(&arg, clnt);

	/* free resources */
	gsh_free(buf->buf);
	gsh_free(buf);
	freeaddrinfo(result);
	clnt_destroy(clnt);

	close(fd);

	return 0;
}
