#include "lib.h"

int main(int argc, char *argv[])
{
	int vmCount=argc-1;
	int flag1=1, flag2=1, flag3=1, flag4=1;
	char vmIPList[vmCount][16];
	struct hostent *ent;
	int i=0, sockfd_rt, sockfd_pg, sockfd_pf, sockfd_udp;
	
	if(argc == 1) {
		printdebuginfo("No nodes to walk around\n");
		exit(0);
	}
	
	for (i=0; i<vmCount; i++) {
		if((ent = gethostbyname(argv[i+1])) == NULL) {
			perror("Cannot find hostname");
			printf("for %s", argv[i+1]);
			exit(1);
		}
		inet_ntop(PF_INET, ent->h_addr_list[0], vmIPList[i], 16);
		printdebuginfo(" VM: %s, IP: %s\n", argv[i+1], vmIPList[i]);
	}

	sockfd_rt = Socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
	sockfd_pg = Socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	sockfd_pf = Socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL)); //Use own proto
	sockfd_udp = Socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_ALL)); // Use own proto
	
	setsockopt(sockfd_rt, IPPROTO_IP, IP_HDRINCL, &flag1, siceof(flag1));
	setsockopt(sockfd_pg, IPPROTO_IP, IP_HDRINCL, &flag2, sizeof(flag2));
	setsockopt(sockfd_pf, SOL_SOCKET, SO_REUSEADDR, &flag3, sizeof(flag3));
	setsockopt(sockfd_udp, SOL_SOCKET, SO_REUSEADDR, &flag4, sizeof(flag4));
	
	exit(0);
}
