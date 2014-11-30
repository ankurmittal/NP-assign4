#include "lib.h"
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#define IP_ID (PROTO + 3)
#define PROTO_IP 110

#define MPORT "8888"
#define MIP "239.192.10.10"

#define IP4_HDRLEN 20
#define ICMP_HDRLEN  8

static unsigned long myip;
int msendfd = -1, mrecfd = -1, sockfd_pf;
static socklen_t salen;
static struct sockaddr *sasend = NULL, *sarecv;
static  char hostname[5];
static int eth0ino;
static unsigned char mymac[IF_HADDR];
static unsigned long pingips[10];
static pthread_t tids[10];
static int indexp = 0;
static short tour_over = 0, final_node = 0;
struct tour_hdr
{
    unsigned short total_vms;
    unsigned short current_index;
    char ip[16];
    char port[6];
};

// Checksum function
    uint16_t
checksum (uint16_t *addr, int len)
{
    int nleft = len;
    int sum = 0;
    uint16_t *w = addr;
    uint16_t answer = 0;

    while (nleft > 1) {
	sum += *w++;
	nleft -= sizeof (uint16_t);
    }

    if (nleft == 1) {
	*(uint8_t *) (&answer) = *(uint8_t *) w;
	sum += answer;
    }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    answer = ~sum;
    return (answer);
}
// Build IPv4 ICMP pseudo-header and call checksum function.
    uint16_t
icmp4_checksum (struct icmp icmphdr, uint8_t *payload, int payloadlen)
{
    char buf[IP_MAXPACKET];
    char *ptr;
    int chksumlen = 0;
    int i;

    ptr = &buf[0];  // ptr points to beginning of buffer buf

    // Copy Message Type to buf (8 bits)
    memcpy (ptr, &icmphdr.icmp_type, sizeof (icmphdr.icmp_type));
    ptr += sizeof (icmphdr.icmp_type);
    chksumlen += sizeof (icmphdr.icmp_type);

    // Copy Message Code to buf (8 bits)
    memcpy (ptr, &icmphdr.icmp_code, sizeof (icmphdr.icmp_code));
    ptr += sizeof (icmphdr.icmp_code);
    chksumlen += sizeof (icmphdr.icmp_code);

    // Copy ICMP checksum to buf (16 bits)
    // Zero, since we don't know it yet
    *ptr = 0; ptr++;
    *ptr = 0; ptr++;
    chksumlen += 2;

    // Copy Identifier to buf (16 bits)
    memcpy (ptr, &icmphdr.icmp_id, sizeof (icmphdr.icmp_id));
    ptr += sizeof (icmphdr.icmp_id);
    chksumlen += sizeof (icmphdr.icmp_id);

    // Copy Sequence Number to buf (16 bits)
    memcpy (ptr, &icmphdr.icmp_seq, sizeof (icmphdr.icmp_seq));
    ptr += sizeof (icmphdr.icmp_seq);
    chksumlen += sizeof (icmphdr.icmp_seq);

    // Copy payload to buf
    memcpy (ptr, payload, payloadlen);
    ptr += payloadlen;
    chksumlen += payloadlen;

    // Pad to the next 16-bit boundary
    for (i=0; i<payloadlen%2; i++, ptr++) {
	*ptr = 0;
	ptr++;
	chksumlen++;
    }

    return checksum ((uint16_t *) buf, chksumlen);
}


int sendping(int fd, unsigned char *destmac, unsigned char *srcmac, int ino,
	unsigned long destip, int seq)
{

    int datalen = 12, n;
    int buf_len = IP4_HDRLEN + ICMP_HDRLEN + datalen;
    void *buffer = zalloc(buf_len);
    struct ip iphdr;
    struct icmp icmphdr;
    char data[13] = "Echo test ", desth[20];
    // IPv4 header length (4 bits): Number of 32-bit words in header = 5
    iphdr.ip_hl = IP4_HDRLEN / sizeof (uint32_t);

    // Internet Protocol version (4 bits): IPv4
    iphdr.ip_v = 4;

    // Type of service (8 bits)
    iphdr.ip_tos = 0;

    // Total length of datagram (16 bits): IP header + ICMP header + ICMP data
    iphdr.ip_len = htons (buf_len);

    // ID sequence number (16 bits): unused, since single datagram
    iphdr.ip_id = htons(0);

    // Flags, and Fragmentation offset (3, 13 bits): 0 since single datagram

    iphdr.ip_off = htons (0);

    // Time-to-Live (8 bits): default to maximum value
    iphdr.ip_ttl = 64;

    // Transport layer protocol (8 bits): 1 for ICMP
    iphdr.ip_p = IPPROTO_ICMP;
    iphdr.ip_src.s_addr = myip;

    printdebuginfo("My ip: %lu\n", myip);
    iphdr.ip_dst.s_addr = destip;
    iphdr.ip_sum = checksum ((uint16_t *) &iphdr, IP4_HDRLEN);
    icmphdr.icmp_type = ICMP_ECHO;

    // Message Code (8 bits): echo request
    icmphdr.icmp_code = 0;

    // Identifier (16 bits): usually pid of sending process - pick a number
    icmphdr.icmp_id = htons (getpid());

    // Sequence Number (16 bits): starts at 0
    icmphdr.icmp_seq = htons (seq);

    // ICMP header checksum (16 bits): set to 0 when calculating checksum
    icmphdr.icmp_cksum = icmp4_checksum (icmphdr, data, datalen);

    memcpy(buffer, &iphdr, IP4_HDRLEN);
    memcpy(buffer + IP4_HDRLEN, &icmphdr, ICMP_HDRLEN);
    memcpy(buffer + IP4_HDRLEN + ICMP_HDRLEN, data, datalen);
    gethostnamebyaddr(destip, desth);
    printf("PING %s (%s): %d data bytes\n", desth, inet_ntoa(*((struct in_addr*)&destip)), ICMP_HDRLEN + datalen);
    n = sendframe(fd, destmac, ino, srcmac, buffer, buf_len, ETH_P_IP, 0);
    free(buffer);
    if(n < 0)
    {
	perror("Error while pinging");
    }
    return n;

}

void getmacinfo()
{
    struct hwa_info *hwa, *hwahead;
    for (hwahead = hwa = Get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next) {
	struct sockaddr_in  *sin = (struct sockaddr_in *) (hwa->ip_addr);
	if(strcmp(hwa->if_name, "lo") == 0)
	    continue;
	if(strcmp(hwa->if_name, "eth0") == 0) {
	    eth0ino = hwa->if_index;
	    memcpy(mymac, hwa->if_haddr, IF_HADDR);
	} else {
	    continue;
	}
    }

    free_hwa_info(hwahead);
}

void ping(void *buffer)
{
    struct hwaddr *hwaddr = buffer;
    unsigned long destip = *(unsigned long *)(buffer + sizeof(struct hwaddr));
    int seq = 1;
    while(!tour_over)
    {
	struct timeval t;
	t.tv_sec = 1;
	t.tv_usec = 0;
	sendping(sockfd_pf, hwaddr->sll_addr, mymac, hwaddr->sll_ifindex, destip, seq++);
	if(tour_over)
	    break;
	select(0,NULL,NULL,NULL,&t);
    }
    free(buffer);
}

void prepare_and_send_ping(unsigned long destip)
{

    //Check if already pinging
    struct hwaddr hwaddr;
    struct sockaddr_in addr;
    void *buffer = zalloc(sizeof(struct hwaddr) + sizeof(destip));
    int n, i;
    for(i = 0; i < indexp; i++)
    {
	if(pingips[i] == destip)
	    return;
    }
    bzero(&hwaddr, sizeof(hwaddr));
    bzero(&addr, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = destip;
    hwaddr.sll_hatype = 1;
    hwaddr.sll_halen = 6;
    hwaddr.sll_ifindex = eth0ino;
    n = areq((SA *)&addr, sizeof(addr), &hwaddr);
    if(n < 0)
    {
	perror("Error while getting mac");
	return;
    }
    pingips[indexp] = destip;
    memcpy(buffer, &hwaddr, sizeof(struct hwaddr));
    memcpy(buffer + sizeof(struct hwaddr), &destip, sizeof(destip));
    Pthread_create(&tids[indexp++], NULL, &ping, buffer);
}

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
    iphdr->protocol = PROTO_IP;
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
    if(msendfd != -1)
	return;

    msendfd = Udp_client(ip, port, &sasend, &salen);

    mrecfd = Socket(sasend->sa_family, SOCK_DGRAM, 0);
    setsockopt(mrecfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    sarecv = zalloc(salen);
    memcpy(sarecv, sasend, salen);
    Bind(mrecfd, sarecv, salen);
    Mcast_join(mrecfd, sasend, salen, NULL, 0);
    Mcast_set_loop(msendfd, 1);
    on = 1;
    setsockopt(msendfd, IPPROTO_IP, IP_MULTICAST_TTL, (void *) &on, 
	    sizeof(on));
}

int recieve_rt(int fd)
{
    void *buffer = zalloc(IP_MAXPACKET);
    struct  iphdr  *iphdr;
    struct tour_hdr *tourhdr;
    unsigned long *ips;
    int n, pack_len, ret = 1;
    unsigned short tvms, cindex;
    time_t ticks;
    char buff[MAXLINE], hname[20];
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
    ticks = time(NULL);
    snprintf(buff, sizeof(buff), "%.24s\r\n", ctime(&ticks));
    gethostnamebyaddr(ips[cindex - 1], hname);
    printf("<%s> received source routing packet from <%s>\n", buff, hname);
    
    prepare_and_send_ping(ips[cindex - 1]);
    //Ping prev node
    if(cindex != tvms - 1)
    {
	tourhdr->current_index = cindex + 1;
	ret = send_rt(fd, tourhdr, n - sizeof(struct  iphdr), ips[cindex + 1]);
    } else {
	final_node = 1;
    }

exit:
    free(buffer);
    return ret;
}

void printip(struct ip *ip)
{
    printf("%u, %u, %u, %d, %hu, %d, %u, %u, %u\n", ip->ip_hl, ip->ip_v, ip->ip_tos, ip->ip_len, ip->ip_id, ip->ip_off, ip->ip_ttl, ip->ip_p, ip->ip_sum);
}

void printicmp(struct icmp *icmp)
{
    printf("%u, %u, %u, %d\n", icmp->icmp_type, icmp->icmp_code, icmp->icmp_id, icmp->icmp_seq);
}
int main(int argc, char *argv[])
{
    int vm_count=argc;
    int flag1=1, flag2=1, flag3=1;
    unsigned long *vm_list = NULL;
    struct hostent *ent;
    int i=0, sockfd_rt, sockfd_pg, maxsockfd;
    struct in_addr ** addr_list;
    fd_set allset;
    struct timeval t;
    int n;
    getmacinfo();

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
	vm_list = zalloc(vm_count*sizeof(unsigned long));
	vm_list[0] = myip;
	// find ip addresses of all vms in tourlist and store them
	for (i=1; i < vm_count; i++) {
	    if((ent = gethostbyname(argv[i])) == NULL) {
		printf("Cannot find hostname for %s", argv[i]);
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

    sockfd_rt = Socket(AF_INET, SOCK_RAW, PROTO_IP);
    sockfd_pg = Socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    sockfd_pf = Socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP));
    maxsockfd = max(max(sockfd_rt, sockfd_pg), sockfd_pf);

    setsockopt(sockfd_rt, IPPROTO_IP, IP_HDRINCL, &flag1, sizeof(flag1));
    setsockopt(sockfd_pg, IPPROTO_IP, IP_HDRINCL, &flag2, sizeof(flag2));
    flag2 = 1;
    setsockopt(sockfd_pf, IPPROTO_IP, IP_HDRINCL, &flag2, sizeof(flag2));
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
	free(vm_list);
    }
    while(1)
    {
	FD_ZERO(&allset);
	FD_SET(sockfd_rt, &allset);
	//FD_SET(sockfd_pf, &allset);
	FD_SET(sockfd_pg, &allset);
	if(msendfd != -1)
	{
	    FD_SET(mrecfd, &allset);
	    maxsockfd = max(maxsockfd, mrecfd);
	}
	n = select(maxsockfd + 1, &allset, NULL, NULL, final_node? &t:NULL);
	if(FD_ISSET(sockfd_rt, &allset)) {
	    //Recieve tour packet
	    //Ping?
	    //is last? send broadcast
	    //forward msg
	    recieve_rt(sockfd_rt);
	    if(final_node) {
		t.tv_sec = 5;
		t.tv_usec = 0;
	    }
	}
	else if(FD_ISSET(sockfd_pg, &allset)) {
	    void *pingres = zalloc(100);
	    struct ip *iphdr = pingres;
	    struct icmp *icmphdr = pingres + 20;
	    int n = read(sockfd_pg, pingres, 100);
	    if ( n < 0)
	    {
		perror("Error reading ping response");
	    }
	    else {
		if(icmphdr->icmp_type == 0 && icmphdr->icmp_id == htons(getpid()))
		{
		    //printip(iphdr);
		    //printicmp(icmphdr);
		    char hname[20];
		    gethostnamebyaddr(iphdr->ip_src.s_addr, hname);
		    printf ("%d bytes from %s (%s): type = %d, code = %d\n",n - 20, hname, inet_ntoa(iphdr->ip_src),
			icmphdr->icmp_type, icmphdr->icmp_code);
		}
	    }
	    free(pingres);
	}
	else if(msendfd!=-1 && FD_ISSET(mrecfd, &allset)) {
	    char msg[200];
	    struct timeval t;
	    t.tv_sec = 5;
	    t.tv_usec = 0;
	    int n = read(mrecfd, msg, 100);
	    if(n < 0)
	    {
		perror("Error recieving multicast msg");
		goto exit;
	    }
	    tour_over = 1;
	    for(i = 0; i < indexp; i++)
	    {
		pthread_join(tids[i], NULL);
	    }
	    printf("Node %s. Received: %s\n", hostname, msg);
	    n = sprintf(msg, "Node %s. I am a member of the group.", hostname);
	    printf("Node %s. Sending: %s\n", hostname, msg);
	    n = sendto(msendfd, msg, n, 0, sasend, salen);
	    if(n < 0)
	    {
		perror("Error sending multicast msg");
		goto exit;
	    }
	    while(1)
	    {
		FD_ZERO(&allset);
		FD_SET(mrecfd, &allset);
		Select(mrecfd + 1, &allset, NULL, NULL, &t);
		if(FD_ISSET(mrecfd, &allset)) {
		    n = read(mrecfd, msg, 100);
		    printf("Node %s. Received: %s\n", hostname, msg);
		} else 
		    break;
	    }
	    printf("Exiting Tour\n");
	    goto exit;
	} else if(final_node) {
	    char msg[200];
	    int l = sprintf(msg, "This is node %s. Tour has ended. Group members please identify yourselves.", hostname);
	    printf("Node %s. Sending: %s\n", hostname, msg);
	    n = sendto(msendfd, msg, l, 0, sasend, salen);
	    if ( n < 0)
	    {
		perror("Error sending multicast packet.");
		goto exit;
	    }
	    final_node = 0;
	}
    }


exit:
    close(sockfd_rt);
    close(sockfd_pg);
    close(sockfd_pf);
}
