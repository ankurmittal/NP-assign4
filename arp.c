#include "lib.h"

static char hostname[5];
static uint32_t cononicalip;
static struct interface_info *tempinterface, *hinterface = NULL;
static struct ll_Node *cacheHead = NULL;
static unsigned char b_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static int localfd;

struct Entry
{
    uint32_t ip;
    unsigned char mac[6];
    int interface;
    int sll_hatype;
    char *sunpath;
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

void ll_update(struct ll_Node *ll_ptr, uint32_t ip, char *mac) {
    struct ll_Node *ll_pointer = ll_ptr;
    int n=0;
    while(ll_pointer != NULL) {
        if(ll_pointer->data->ip == ip) {
            memcpy(ll_pointer->data->mac, mac, 6);
            if(ll_pointer->data->sunpath != NULL) {
                struct sockaddr_un destaddr;
                bzero(&destaddr, sizeof(destaddr));
                destaddr.sun_family = AF_LOCAL;
                strcpy(destaddr.sun_path, ll_pointer->data->sunpath);
                ll_pointer->data->sunpath = NULL;
                n = sendto(localfd, mac, 6, 0, (SA *) &destaddr, sizeof(destaddr));
                if(n < 0) {
                    perror("Error writing to tour..!!\n");
                }
            }
            return;
        }
        ll_pointer = ll_pointer->next;
    }
}

void free_interface_info() {
    while(hinterface != NULL) {
        tempinterface = hinterface;
        hinterface = hinterface->next;
        free(tempinterface);
    }
}

int build_interface_info() 
{
    struct hwa_info *hwa, *hwahead;
    int tinterfaces = 0;
    for (hwahead = hwa = Get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next) {
        struct sockaddr_in  *sin = (struct sockaddr_in *) (hwa->ip_addr);
        if(strcmp(hwa->if_name, "lo") == 0)
            continue;
        if(strcmp(hwa->if_name, "eth0") == 0) {
            cononicalip = htonl(sin->sin_addr.s_addr);
            printdebuginfo(" Cononical IP: %lu\n", ntohl(cononicalip));
        } else {
            struct interface_info *iinfo = (struct interface_info*)malloc(sizeof(struct interface_info));
            int i;
            memset(iinfo, 0, sizeof(struct interface_info));
            tinterfaces++;
            if(hinterface == NULL)
                hinterface = iinfo;
            else
                tempinterface->next = iinfo;

            iinfo->ip=sin->sin_addr.s_addr;
            iinfo->interfaceno = hwa->if_index;
            for(i = 0; i < IF_HADDR; i++)
                iinfo->if_haddr[i] = hwa->if_haddr[i];
            iinfo->next = NULL;

            tempinterface = iinfo;
        }
    }

    free_hwa_info(hwahead);
    return tinterfaces;
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
    if(header->op == 0) {
        struct interface_info *temp = tempinterface;
        if(header->targetIPAddr == cononicalip) {

            // this is destination node

            while(temp != NULL) {
                if(temp->interfaceno == recv_frame->interfaceNo)
                    break;
                temp = temp->next;
            }
            memcpy(dest_mac, header->senderEthAddr, 6);
            memcpy(header->targetEthAddr, temp->if_haddr, IF_HADDR);
            memcpy(header->senderEthAddr, temp->if_haddr, IF_HADDR);

            senderIP = header->senderIPAddr;
            header->senderIPAddr = cononicalip;
            header->op = 1;

            ll_update(cacheHead, senderIP, dest_mac);

            sendframe(framefd, dest_mac, recv_frame->interfaceNo, temp->if_haddr, recv_frame->data, sizeof(struct arp_header), PROTO);

        } else {

            // this is intermediate node

            ll_update(cacheHead, header->senderIPAddr, header->senderEthAddr);

            while(temp != NULL) {
                if(temp->interfaceno != recv_frame->interfaceNo) {
                    sendframe(framefd, b_mac, recv_frame->interfaceNo, temp->if_haddr, recv_frame->data, sizeof(struct arp_header), PROTO);
                }
                temp = temp->next;
            }
            return;
        }

    } else if(header->op == 1) {

            ll_update(cacheHead, header->senderIPAddr, header->senderEthAddr); 
    }
}

int main(int argc, char *argv[])
{
    struct sockaddr_un servaddr, cliaddr;
    socklen_t clilen;
    fd_set allset;
    int n, tinterfaces = 0, localfd, framefd;
    struct recv_frame *recv_frame;
    struct hostent *ent;

    gethostname(hostname, sizeof(hostname));
    if((ent = gethostbyname(hostname)) == NULL) {
        perror("Cannot find hostname");
        printf("for %s", hostname);
        exit(1);
    }

    // Unix Domain Socket

    printdebuginfo("creating localfd\n");

    localfd = Socket(AF_LOCAL, SOCK_STREAM, 0);
    unlink(ARP_PATH);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sun_family = AF_LOCAL;
    strcpy(servaddr.sun_path, ARP_PATH);
    
    printdebuginfo("Binding.. sunpath: %s\n", servaddr.sun_path);

    Bind(localfd, (SA *) &servaddr, sizeof(servaddr));
    tinterfaces = build_interface_info();

    printdebuginfo("total interfaces = %d, creating framefd\n", tinterfaces);
    
    framefd = Socket(AF_PACKET, SOCK_RAW, htons(PROTO));

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
            recv_frame = zalloc(sizeof(struct recv_frame));
            recieveframe(framefd, recv_frame);
            processFrame(recv_frame, framefd);
            free(recv_frame->data);
            free(recv_frame);
        }
        if(FD_ISSET(localfd, &allset)) {
            clilen = sizeof(cliaddr);
            memset(&cliaddr, 0, sizeof(cliaddr));
//            n = recvfrom(localfd, &msg_content, sizeof(msg_content), 0, (SA*)&cliaddr, &clilen);
//            printdebuginfo(" Message from client/server %d, %d, %s, %s\n", msg_content.port, msg_content.flag, msg_content.msg, msg_content.ip);
//            printdebuginfo(" Cli sun_name:%s\n", cliaddr.sun_path);
        }
    }
exit:
    close(localfd);
    close(framefd);
    free_interface_info();
}
