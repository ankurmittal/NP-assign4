#include "lib.h"

int main(int argc, char *argv[])
{
	int vmCount=argc;
	int flag1=1, flag2=1, flag3=1, flag4=1;
	unsigned long *vmIPList = malloc(vmCount * sizeof(unsigned long));
	struct hostent *ent;
	int i=0, sockfd_rt, sockfd_pg, sockfd_pf, sockfd_udp;
	char hostname[5];
	unsigned int myip;

	if(argc == 1) {
		printdebuginfo("No nodes to walk around\n");
		exit(0);
	}
	
	// find own hostname and ip address
	gethostname(hostname, sizeof(hostname));
	if((ent = gethostbyname(hostname)) == NULL) {
		perror("Cannot find hostname");
		printf("for %s", hostname);
		exit(1);
	}
	inet_pton(PF_INET, ent->h_addr_list[0], &vmIPList[0]);
	vmIPList[0] = htons(vmIPList[0]);
	printdebuginfo(" VM: %s, IP: %lu\n", hostname, vmIPList[0]);

	// find ip addresses of all vms in tourlist and store them
	for (i=0; i<vmCount; i++) {
		if((ent = gethostbyname(argv[i+1])) == NULL) {
			perror("Cannot find hostname");
			printf("for %s", argv[i+1]);
			exit(1);
		}
		inet_pton(PF_INET, ent->h_addr_list[0], &vmIPList[i+1]);
		vmIPList[i+1] = htons(vmIPList[i+1]);
		printdebuginfo(" VM: %s, IP: %lu\n", argv[i+1], vmIPList[i+1]);

		if(vmIPList[i] == vmIPList[i+1]) {
			printdebuginfo("Two vm's next to each other in route\n");
			exit(1);
		}
	}

	sockfd_rt = Socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
	sockfd_pg = Socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	sockfd_pf = Socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP));
	sockfd_udp = Socket(PF_PACKET, SOCK_DGRAM, 0);

	setsockopt(sockfd_rt, IPPROTO_IP, IP_HDRINCL, &flag1, sizeof(flag1));
	setsockopt(sockfd_pg, IPPROTO_IP, IP_HDRINCL, &flag2, sizeof(flag2));
	setsockopt(sockfd_pf, SOL_SOCKET, SO_REUSEADDR, &flag3, sizeof(flag3));
	setsockopt(sockfd_udp, SOL_SOCKET, SO_REUSEADDR, &flag4, sizeof(flag4));

	exit(0);
}
