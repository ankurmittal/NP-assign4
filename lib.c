#include "lib.h"

void print_eth_hdr(struct ethhdr *eh) {
    int length = 0;

    printf("proto: %d, ", ntohs(eh->h_proto));

    printf("destination_mac: ");
    length = IF_HADDR;
    do {
	printf(" %.2x%s", *(eh->h_dest - length + IF_HADDR) & 0xff, (length == 1) ? " " : ":");
    } while (--length > 0);

    printf(", source_mac: ");    
    length = IF_HADDR;
    do {
	printf(" %.2x%s", *(eh->h_source - length + IF_HADDR) & 0xff, (length == 1) ? " " : ":");
    } while (--length > 0);

    printf("\n\n");
}

void print_arp_hdr(struct arp_header *ah, char *msg) {

    int length = IF_HADDR;

    printf("%s\n", msg);

    printf(" id: %d, hard_type: %d, proto_type: %d, ", ah->id, ntohs(ah->hard_type), ntohs(ah->proto_type));
    printf("hard_size: %d, prot_size: %d, ", ah->hard_size, ah->prot_size);

    if(ah->op)
	printf("type: Request, ");
    else
	printf("type: Response, ");

    printf("Sender IP: %s, ", inet_ntoa(*((struct in_addr *)&(ah->senderIPAddr))));
    printf("Target IP: %s, ", inet_ntoa(*((struct in_addr *)&(ah->targetIPAddr))));

    printf("sender_mac: ");
    do {
	printf(" %.2x%s", *(ah->senderEthAddr - length + IF_HADDR) & 0xff, (length == 1) ? " " : ":");
    } while (--length > 0);

    printf(", target_mac: ");
    length = IF_HADDR;
    do {
	printf(" %.2x%s", *(ah->targetEthAddr - length + IF_HADDR) & 0xff, (length == 1) ? " " : ":");
    } while (--length > 0);

    printf("\n\n");
}

/*
 * returns number of bytes read when successful, else returns -1
 */
int areq(struct sockaddr *IPaddr, socklen_t sockaddrlen, struct hwaddr *HWaddr) {
    int sockfd, tfd, length;
    struct sockaddr_un arpaddr, cliaddr;   
    struct areqStruct reqMsg;
    struct sockaddr_in *sin = (struct sockaddr_in *) IPaddr;    
    static struct timeval selectTime;
    int n = 0;
    fd_set allset;

    printf("areq: seeking hardware address for %s\n", inet_ntoa(sin->sin_addr));

    unsigned char *buffer = zalloc(20);

    sockfd = Socket(AF_LOCAL, SOCK_STREAM, 0);
    bzero(&arpaddr, sizeof(arpaddr));
    bzero(&cliaddr, sizeof(arpaddr));
    arpaddr.sun_family = AF_LOCAL;
    cliaddr.sun_family = AF_LOCAL;
    strcpy(arpaddr.sun_path, ARP_PATH);
    tfd = mkstemp(cliaddr.sun_path);
    unlink(cliaddr.sun_path);
    //Bind(sockfd, (SA *) &cliaddr, sizeof(arpaddr));
    n = connect (sockfd, (SA*)&arpaddr, sizeof (arpaddr));
    if(n < 0)
    {
	perror("Cannot connect to arp");
	goto exit;
    }

    reqMsg.targetIP = sin->sin_addr.s_addr;
    reqMsg.interface = HWaddr->sll_ifindex;
    reqMsg.hard_type = HWaddr->sll_hatype;
    reqMsg.addr_len = HWaddr->sll_halen;

    n = write(sockfd, &reqMsg, sizeof(struct areqStruct));

    if(n < 0) {
	perror("Error writing to arp socket\n");
	goto exit;
    }

    selectTime.tv_sec = 3;
    selectTime.tv_usec = 0;

    FD_ZERO(&allset);
    FD_SET(sockfd, &allset);

    select(sockfd+1, &allset, NULL, NULL, &selectTime);

    if(FD_ISSET(sockfd, &allset)) {
	n = read(sockfd,buffer,20);
	if (n < 0) {
	    perror("areq: ERROR reading from socket");
	    goto exit;
	}
    } else {
	printf("areq: timeout occured\n");
	printdebuginfo("Timeout occured in receive message..!!\n");
	n = -ETIME;
	errno = -ETIME;
	goto exit;
    }

    memcpy(HWaddr->sll_addr, buffer, 6);
    printf("areq: hardware_address: ");
    length = IF_HADDR;
    do {
	printf(" %.2x%s", *(HWaddr->sll_addr + (IF_HADDR - length)) & 0xff, (length == 1) ? " " : ":");
    } while (--length > 0);
    printf("\n");
    memcpy(&HWaddr->sll_ifindex, buffer + 6, 4);
exit:
    free(buffer);
    close(sockfd);
    return n;
}

int sendframe(int framefd, char *destmac, int interface, char *srcmac, void *data, int data_length, short proto, int print_info)
{    
    struct sockaddr_ll socket_address;
    int n;
    /*buffer for ethernet frame*/
    void* buffer;
    char src_name[10], dest_name[10];

    /*userdata in ethernet frame*/
    void* data_p;

    /*another pointer to ethernet header*/
    struct ethhdr *eh;

    int len = sizeof(struct ethhdr) + data_length;

    int send_result = 0;

    buffer = zalloc(len);
    eh = buffer;

    data_p = buffer + sizeof(struct ethhdr);

    /*prepare sockaddr_ll*/

    /*RAW communication*/
    socket_address.sll_family   = PF_PACKET;   
    socket_address.sll_protocol = htons(0);   

    /*index of the network device*/
    socket_address.sll_ifindex  = interface;

    /*ARP hardware identifier is ethernet*/
    socket_address.sll_hatype   = ARPHRD_ETHER;

    /*target is another host*/
    socket_address.sll_pkttype  = PACKET_OTHERHOST;

    /*address length*/
    socket_address.sll_halen    = ETH_ALEN;	
    /*MAC - begin*/
    socket_address.sll_addr[0]  = destmac[0];	    
    socket_address.sll_addr[1]  = destmac[1];	    
    socket_address.sll_addr[2]  = destmac[2];	    
    socket_address.sll_addr[3]  = destmac[3];	    
    socket_address.sll_addr[4]  = destmac[4];	    
    socket_address.sll_addr[5]  = destmac[5];	    
    socket_address.sll_addr[6]  = destmac[6];	    
    /*MAC - end*/
    socket_address.sll_addr[6]  = 0x00;/*not used*/
    socket_address.sll_addr[7]  = 0x00;/*not used*/

    /*set the frame header*/
    memcpy((void*)buffer, (void*)destmac, ETH_ALEN);
    memcpy((void*)(buffer+ETH_ALEN), (void*)srcmac, ETH_ALEN);
    eh->h_proto = htons(proto);
    memcpy(data_p, data, data_length);
    /*fill the frame with some data*/

    /*send the packet*/
    printdebuginfo("sending message at: %d to: ", interface);
    n = 6;
    while(n-- > 0)
	printdebuginfo("%.2x::", *(destmac + 5 - n) & 0xff);
    printdebuginfo(" from mac: ", interface);
    n = 6;
    while(n-- > 0)
	printdebuginfo("%.2x::", *(srcmac + 5 - n) & 0xff);
    printdebuginfo("\n");

    if (print_info) {
	print_eth_hdr(eh);
    }

    n = sendto(framefd, buffer, len, 0, 
	    (struct sockaddr*)&socket_address, sizeof(socket_address));
    if (n < 0) 
    { 
	perror("Error while sending packet.");
    }
    free(buffer);
    return n;
}

void recieveframe(int framefd, struct recv_frame *recv_frame)
{
    struct sockaddr_ll socket_address;
    socklen_t addrlen = sizeof(socket_address);
    char *ptr;
    char *my_mac;
    unsigned char src_mac[IF_HADDR];
    void* buffer = (void*)zalloc(ETH_FRAME_LEN); /*Buffer for ethernet frame*/
    int length = 0, n; /*length of the received frame*/ 
    int datalength = 0;

    memset(&socket_address, 0, addrlen);
    length = recvfrom(framefd, buffer, ETH_FRAME_LEN, 0, (SA *)&socket_address, &addrlen);
    if (length < 0) { perror("Error while recieving packet"); return;}

    datalength = length - sizeof(struct ethhdr);

    recv_frame->data = zalloc(datalength);

    memcpy(&(recv_frame->eh), buffer, sizeof(struct ethhdr));

    memcpy(recv_frame->data, buffer+sizeof(struct ethhdr), datalength);
    printdebuginfo("\n message recieved, len:%d, at interface %d with mac address: ", length, socket_address.sll_ifindex);

    ptr = buffer + ETH_ALEN;
    length = IF_HADDR;
    memcpy(recv_frame->src_mac, ptr, IF_HADDR);

    recv_frame->interfaceNo = socket_address.sll_ifindex;

    do {
	printdebuginfo(" %.2x%s", *(recv_frame->src_mac - length + IF_HADDR), (length == 1) ? " " : ":");
    } while (--length > 0);
    printdebuginfo("\n");

exit:
    free(buffer);
}
