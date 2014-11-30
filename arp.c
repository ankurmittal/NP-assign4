#include "lib.h"

static char hostname[5];
static uint32_t cononicalip;
static struct ll_Node *cacheHead = NULL;
static unsigned char b_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static int localfd, eth0ino;
static unsigned char eth0macaddr[IF_HADDR];

struct Entry
{
    uint32_t ip;
    unsigned char mac[6];
    int interface;
    int sll_hatype;
    int fd;
};

struct ll_Node
{
    struct Entry *data;
    struct ll_Node *next;
    struct ll_Node *prev;
};

struct ll_Node* ll_insert(struct ll_Node *ll_ptr, struct Entry *data)
{
    if(ll_ptr == NULL) {
        struct ll_Node *ll_pointer;
        ll_pointer = (struct ll_Node *)zalloc(sizeof(struct ll_Node));
        ll_pointer->next = NULL;
        ll_pointer->prev = NULL;
        ll_pointer->data = data;
        return ll_pointer;
    }
    struct ll_Node *ll_pointer = ll_ptr;
    while(ll_pointer->next!=NULL)
    {
        ll_pointer = ll_pointer -> next;
    }
    ll_pointer->next = (struct ll_Node *)malloc(sizeof(struct ll_Node));
    (ll_pointer->next)->prev = ll_pointer;
    ll_pointer = ll_pointer->next;
    ll_pointer->data = data;
    ll_pointer->next = NULL;
    return ll_ptr;
}

struct Entry* ll_find(struct ll_Node *ll_pointer, uint32_t ip) {
    while(ll_pointer != NULL) {
        if(ll_pointer->data->ip == ip)
            return ll_pointer->data;
        ll_pointer = ll_pointer->next;
    }
    return NULL;
}

void sendToTour(int fd, unsigned char *mac, int interface) {

    int n;
    void *areqRes = zalloc(6*sizeof(uint8_t) + sizeof(int));            
    memcpy(areqRes, mac, 6*sizeof(uint8_t));
    memcpy(areqRes + 6*sizeof(uint8_t), &interface, sizeof(int));
    
    printdebuginfo("sending to tour\n");

    n = write(fd, areqRes, (6*sizeof(uint8_t) + sizeof(int)));
    if(n < 0) {

        printdebuginfo("Error writing back to tour\n");
    }
    
    free(areqRes);
    close(fd);
}

void ll_update(struct ll_Node *ll_ptr, uint32_t ip, unsigned char *mac, int insertFlag) {
    struct ll_Node *ll_pointer = ll_ptr;
    int n=0;
    while(ll_pointer != NULL) {
        if(ll_pointer->data->ip == ip) {
            memcpy(ll_pointer->data->mac, mac, 6);
            if(ll_pointer->data->fd != -1) {
                sendToTour(ll_pointer->data->fd, mac, eth0ino);
                ll_pointer->data->fd = -1;
            }
            return;
        }
        ll_pointer = ll_pointer->next;
    }
    if (insertFlag) {
        struct Entry *cacheEntry = zalloc(sizeof(struct Entry));
        cacheEntry->ip = ip;
        cacheEntry->interface = eth0ino;
        cacheEntry->sll_hatype = 1;
        cacheEntry->fd = -1;

        cacheHead = ll_insert(cacheHead, cacheEntry);
    }
}
void getmacinfo() 
{
    struct hwa_info *hwa, *hwahead;
    for (hwahead = hwa = Get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next) {
        struct sockaddr_in  *sin = (struct sockaddr_in *) (hwa->ip_addr);
        if(strcmp(hwa->if_name, "lo") == 0)
            continue;
        if(strcmp(hwa->if_name, "eth0") == 0) {
            cononicalip = sin->sin_addr.s_addr;
            printdebuginfo(" Cononical IP: %lu\n", cononicalip);
            eth0ino = hwa->if_index;
            memcpy(eth0macaddr, hwa->if_haddr, IF_HADDR);
        } else {
            continue;
        }
    }

    free_hwa_info(hwahead);
}

void processFrame(struct recv_frame *recv_frame, int framefd)
{
    unsigned char dest_mac[6];
    uint32_t senderIP;
    struct arp_header *header = recv_frame->data;
    if(header->id != PROTO+2) {
        printdebuginfo(" Discarding frame (ID mismatch)\n");
        return;
    }
    if(header->op == 1) {
        //printf("%" PRIu32 ", %" PRIu32 "\n", header->targetIPAddr, cononicalip);
        if(header->targetIPAddr == cononicalip) {

            // this is destination node
            printdebuginfo("Reached dest\n");

            memcpy(dest_mac, header->senderEthAddr, 6);
            memcpy(header->targetEthAddr, eth0macaddr, IF_HADDR);
            memcpy(header->senderEthAddr, eth0macaddr, IF_HADDR);

            senderIP = header->senderIPAddr;
            header->senderIPAddr = cononicalip;
            header->op = 2;

            ll_update(cacheHead, senderIP, dest_mac, 1);

            printf("Received Ethernet Header:\n");
            print_eth_hdr(&(recv_frame->eh));

            printf("Sending reply Ethernet Header:\n");
            sendframe(framefd, dest_mac, eth0ino, eth0macaddr, recv_frame->data, sizeof(struct arp_header), PROTO, 1);

        } else {
            if(header->senderIPAddr == cononicalip)
                return;
            // this is intermediate node
            ll_update(cacheHead, header->senderIPAddr, header->senderEthAddr, 0);
            return;
        }

    } else if(header->op == 2) {
        printdebuginfo("Recieved reply: ");
        int n = 6;
        while(n-->0)
            printdebuginfo("%.2x::",*(header->targetEthAddr+ 5 - n) & 0xff);
        printdebuginfo("\n");
        ll_update(cacheHead, header->senderIPAddr, header->targetEthAddr, 0); 
    }
}

int main(int argc, char *argv[])
{
    struct sockaddr_un servaddr, cliaddr;
    socklen_t clilen;
    struct areqStruct areq;
    fd_set allset;
    int n, localfd, framefd;
    struct recv_frame *recv_frame;
    struct hostent *ent;

    gethostname(hostname, sizeof(hostname));
    if((ent = gethostbyname(hostname)) == NULL) {
        perror("Cannot find hostname");
        printf("for %s", hostname);
        exit(1);
    }

    printdebuginfo("creating localfd\n");

    localfd = Socket(AF_LOCAL, SOCK_STREAM, 0);
    unlink(ARP_PATH);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sun_family = AF_LOCAL;
    strcpy(servaddr.sun_path, ARP_PATH);

    printdebuginfo("Binding.. sunpath: %s\n", servaddr.sun_path);

    Bind(localfd, (SA *) &servaddr, sizeof(servaddr));
    Listen(localfd, LISTENQ);

    getmacinfo();

    framefd = Socket(AF_PACKET, SOCK_RAW, htons(PROTO));

    printdebuginfo("before loop:\n localfd: %d, framefd: %d\n", localfd, framefd);

    while(1)
    {
        FD_ZERO(&allset);
        FD_SET(localfd, &allset);
        FD_SET(framefd, &allset);

        n = select(max(localfd, framefd) + 1, &allset, NULL, NULL, NULL);
        if(n < 0) {
            perror("Error during select, exiting.");
            goto exit;
        }
        if(FD_ISSET(framefd, &allset)) {
            printdebuginfo("1\n");
            recv_frame = zalloc(sizeof(struct recv_frame));
            recieveframe(framefd, recv_frame);
            processFrame(recv_frame, framefd);
            free(recv_frame->data);
            free(recv_frame);
        }
        if(FD_ISSET(localfd, &allset)) {
            struct arp_header arphdr;
            struct Entry *cacheEntry;
            int connfd;

            clilen = sizeof(cliaddr);
            memset(&cliaddr, 0, sizeof(cliaddr));
            connfd = Accept (localfd, (SA *) &cliaddr, &clilen);

            n = read(connfd, &areq, sizeof(struct areqStruct));

            cacheEntry = ll_find(cacheHead, areq.targetIP);

            if(cacheEntry != NULL) {
                if(cacheEntry->fd == -1) {
                    sendToTour(connfd, cacheEntry->mac, eth0ino);
                } else {
                    cacheEntry->fd = connfd;
                }
                continue;
            }

            cacheEntry = zalloc(sizeof(struct Entry));
            cacheEntry->ip = areq.targetIP;
            cacheEntry->interface = eth0ino;
            cacheEntry->sll_hatype = htons(areq.hard_type);
            cacheEntry->fd = connfd;

            cacheHead = ll_insert(cacheHead, cacheEntry);

            bzero(&arphdr, sizeof(arphdr));
            arphdr.id = PROTO + 2;
            arphdr.hard_type = htons(areq.hard_type);
            arphdr.proto_type = htons (PROTO);
            arphdr.hard_size = areq.addr_len;
            arphdr.prot_size = 4;
            arphdr.op = 1;
            memset (&arphdr.targetEthAddr, 0, 6 * sizeof (uint8_t));
            arphdr.targetIPAddr = areq.targetIP;
            arphdr.senderIPAddr = cononicalip;
            memcpy(&arphdr.senderEthAddr, eth0macaddr, 6);
            
            printf("Creating Request, Ethernet Header:\n");
            sendframe(framefd, b_mac, eth0ino,  eth0macaddr, &arphdr, sizeof(struct arp_header), PROTO, 1);
        }
    }
exit:
    close(localfd);
    close(framefd);
}
