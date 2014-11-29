#include "lib.h"
#include <netinet/ip.h>

#define IP_ID (PROTO + 3)
#define PROTO_IP (PROTO + 5)

#define MPORT "8888"
#define MIP "239.192.10.10"

static unsigned long myip;
int msendfd = -1, mrecfd = -1;
static socklen_t salen;
static struct sockaddr *sasend = NULL, *sarecv;

struct tour_hdr
{
    unsigned short total_vms;
    unsigned short current_index;
    char ip[16];
    char port[6];
};

int send_rt(int fd, void *buffer, int lenght, unsigned long destip)
{
    void *packet = zalloc(lenght + sizeof(struct iphdr));
    struct iphdr *iphdr = packet;
    struct sockaddr_in daddr;
    socklen_t s_len = sizeof(daddr);
    int n;
    memset(&daddr, 0, s_len);
    daddr.sin_family = AF_INET;
    daddr.sin_addr.s_addr = destip;

    iphdr->ihl = 5;
    iphdr->version = 4;
    iphdr->tos = 0;
    iphdr->ttl = 255;
    iphdr->protocol = IPPROTO_RAW;
    iphdr->id = htons(IP_ID);
    iphdr->check = 0;
    iphdr->saddr = myip;
    iphdr->daddr = destip;
    printdebuginfo("Sending packet\n");
    memcpy(packet + sizeof(struct iphdr), buffer, lenght);
    n = sendto(fd, packet, lenght + sizeof(struct iphdr), 0, (SA*)&daddr, s_len);
    free(packet);
    if(n < 0)
    {
	perror("Error sending rt packet. Exiting");
	return n;
    }
    return 1;
}

void create_join_multicast(char *ip, char* port)
{
    int on = 1;
    //struct sockaddr_in *in = zalloc(sizeof(struct sockaddr_in));
    if(msendfd != -1)
	return;

    //in->sin_family = AF_INET;
    //in->sin_port = htons(8888);
    //inet_pton(AF_INET, ip, &(in->sin_addr));
    //salen = sizeof(struct sockaddr_in);

    msendfd = Udp_client(ip, port, &sasend, &salen);
    //msendfd = Socket(AF_INET, SOCK_DGRAM,0);
    //sasend = (SA *)in;
   //Bind(msendfd, sasend, salen);
    mrecfd = Socket(sasend->sa_family, SOCK_DGRAM, 0);
    setsockopt(mrecfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    sarecv = zalloc(salen);
    memcpy(sarecv, sasend, salen);
    Bind(mrecfd, sarecv, salen);
    Mcast_join(mrecfd, sasend, salen, NULL, 0);
    Mcast_set_loop(msendfd, 0);
    //setsockopt(msendfd, IPPROTO_IP, IP_MULTICAST_LOOP, 0, 1);
    printdebuginfo("%s, %s, %d, %d, %u, %d\n", ip, port,msendfd, mrecfd, salen, sasend->sa_family);
}

int recieve_rt(int fd)
{
    void *buffer = zalloc(IP_MAXPACKET);
    struct  iphdr  *iphdr;
    struct tour_hdr *tourhdr;
    unsigned long *ips;
    int n, pack_len, ret = 1;
    unsigned short tvms, cindex;
    printdebuginfo("Packet recieved\n");
    n = read(fd, buffer, IP_MAXPACKET);
    if(n < 0)
    {
	perror("Error reading from rt socket");
	ret = n;
	goto exit;
    }
    iphdr = (struct  iphdr *)buffer;
    printdebuginfo("ID: %d, %d\n", ntohs(iphdr->id), n);
    if(iphdr->id != htons(IP_ID))
    {
	printdebuginfo("Ignoring tour mesage\n");
	goto exit;
    }
    tourhdr = buffer + sizeof(struct iphdr);
    create_join_multicast(tourhdr->ip, tourhdr->port);
    tvms = tourhdr->total_vms;
    cindex = tourhdr->current_index;
    ips = (buffer + sizeof(struct  iphdr) + sizeof(struct tour_hdr));
    printdebuginfo("current index:%d, %d, %d\n", cindex, tvms, sizeof(struct iphdr));
    //Ping prev node
    if(cindex != tvms - 1)
    {
	tourhdr->current_index = cindex + 1;
	ret = send_rt(fd, tourhdr, n - sizeof(struct  iphdr), ips[cindex + 1]);
    } else {
	printdebuginfo("Tour Ended, %p\n", sasend);
	printdebuginfo("%d, %d, %u, %lu\n", msendfd, mrecfd, salen, htonl(((struct sockaddr_in *) sasend)->sin_addr.s_addr));
	n = sendto(msendfd, buffer, 0, 0, sasend, salen);
	printf("%d\n", errno);
	perror("sending pac");
    }

exit:
    free(buffer);
    return ret;
}

int main(int argc, char *argv[])
{
    int vm_count=argc;
    int flag1=1, flag2=1, flag3=1;
    unsigned long *vm_list = NULL;
    struct hostent *ent;
    int i=0, sockfd_rt, sockfd_pg, sockfd_pf, maxsockfd;
    char hostname[5];
    unsigned long myip;
    struct in_addr ** addr_list;
    fd_set allset;
    int n;

    gethostname(hostname, 5);
    printdebuginfo("my host name: %s\n", hostname);
    if((ent = gethostbyname(hostname)) == NULL) {
	perror("Cannot find hostname");
	printf("for %s", hostname);
	exit(1);
    }

    addr_list = (struct in_addr **)ent->h_addr_list;

    myip = addr_list[0]->s_addr;
    printdebuginfo(" my ip: %lu\n", myip);

    if(argc > 1)
    {
	vm_list = malloc(vm_count*sizeof(unsigned long));
	vm_list[0] = myip;
	// find ip addresses of all vms in tourlist and store them
	for (i=1; i < vm_count; i++) {
	    if((ent = gethostbyname(argv[i])) == NULL) {
		perror("Cannot find hostname");
		printf("for %s", argv[i]);
		exit(1);
	    }
	    addr_list = (struct in_addr **)ent->h_addr_list;
	    vm_list[i] = addr_list[0]->s_addr;
	    printdebuginfo(" VM: %s, IP: %lu\n", argv[i], vm_list[i]);

	    if(vm_list[i - 1] == vm_list[i]) {
		printdebuginfo("Same consecutive vms: %s\n", argv[i]);
		exit(1);
	    }
	}
    }

    sockfd_rt = Socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    sockfd_pg = Socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    sockfd_pf = Socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP));
    maxsockfd = max(max(sockfd_rt, sockfd_pg), sockfd_pf);

    setsockopt(sockfd_rt, IPPROTO_IP, IP_HDRINCL, &flag1, sizeof(flag1));
    setsockopt(sockfd_pg, IPPROTO_IP, IP_HDRINCL, &flag2, sizeof(flag2));
    setsockopt(sockfd_pf, SOL_SOCKET, SO_REUSEADDR, &flag3, sizeof(flag3));

    if(argc > 1)
    {
	int len = sizeof(struct tour_hdr) + argc * sizeof(unsigned long);
	void *buffer = zalloc(len);
	struct tour_hdr *t = buffer;
	t->current_index = 1;
	t->total_vms = vm_count;
	strcpy(t->ip, MIP);
	strcpy(t->port, MPORT);
	create_join_multicast(t->ip, t->port);
	memcpy(buffer + sizeof(struct tour_hdr), vm_list, len  - sizeof(struct tour_hdr));
	send_rt(sockfd_rt, buffer, len, vm_list[1]);
	free(buffer);
    }
    while(1)
    {
	FD_ZERO(&allset);
	FD_SET(sockfd_rt, &allset);
	FD_SET(sockfd_pf, &allset);
	FD_SET(sockfd_pg, &allset);
	if(msendfd != -1)
	{
	    FD_SET(mrecfd, &allset);
	    maxsockfd = max(maxsockfd, mrecfd);
	}
	n = select(maxsockfd + 1, &allset, NULL, NULL, NULL);
	if(FD_ISSET(sockfd_rt, &allset)) {
	    //Recieve tour packet
	    //Ping?
	    //is last? send broadcast
	    //forward msg
	    recieve_rt(sockfd_rt);
	}
	if(FD_ISSET(sockfd_pf, &allset)) {
	}
	if(FD_ISSET(sockfd_pg, &allset)) {
	}
	if(msendfd!=-1 && FD_ISSET(mrecfd, &allset)) {
	    printdebuginfo("Recieve multicast msg\n");
	    goto exit;
	}
    }


exit:
    close(sockfd_rt);
    close(sockfd_pg);
    close(sockfd_pf);
}
