#ifndef _LIB_H
#define _LIB_H

#include "unp.h"
#include <stdarg.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include "hw_addrs.h"
#include <assert.h>

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define ARP_PROTO PROTO+1
#define ARP_PATH "/tmp/arp_" STR(PROTO)

struct areqStruct {
    uint32_t targetIP;
    int interface;
    unsigned short hard_type;
    unsigned char addr_len;
};

struct interface_info 
{
    char if_haddr[IF_HADDR];
    unsigned long ip;
    int interfaceno;
    struct interface_info *next;
};

struct hwaddr {
    int             sll_ifindex;    /* Interface number */
    unsigned short  sll_hatype;     /* Hardware type */
    unsigned char   sll_halen;      /* Length of address */
    unsigned char   sll_addr[8];    /* Physical layer address */
};

static void printdebuginfo(const char *format, ...)
{
#ifdef NDEBUGINFO
    return;
#endif
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

/*
 * from ARP handout figure 4.3
 */
struct arp_header {
    uint16_t id;
    uint16_t hard_type;
    uint16_t proto_type;
    uint8_t hard_size;
    uint8_t prot_size;
    uint16_t op;	// 0 for request, 1 for response
    unsigned char senderEthAddr[6];
    uint32_t senderIPAddr;
    unsigned char targetEthAddr[6];
    uint32_t targetIPAddr;
};


struct recv_frame {
    int interfaceNo;
    unsigned char src_mac[6];
    void *data;
    struct ethhdr eh;
};
int sendframe(int framefd, char *destmac, int interface, char *srcmac, void *data, int data_length, short proto, int print_info);
int areq(struct sockaddr *IPaddr, socklen_t sockaddrlen, struct hwaddr *HWaddr);
void recieveframe(int framefd, struct recv_frame *recv_frame);

static void *zalloc(size_t size)
{
    void *p = malloc(size);
    assert(p!=NULL);
    memset(p, 0, size);
    return p;
}

static void gethostnamebyaddr(unsigned long ipaddr, char *buffer)
{
    struct hostent *he;
    int i;
    he = gethostbyaddr(&ipaddr, sizeof(ipaddr), AF_INET);
    for(i = 0; he->h_name[i] != '.'; i++)
    {
	buffer[i] = he->h_name[i];
    }
    buffer[i] = 0;
}


#endif
