/*=============================
   help for Assignment 3: clandestine fabrics 
  =============================*/ 

#include "help.h" 

#define TRUE  1
#define FALSE 0

/*=============================
   low-level protocol help 
  =============================*/ 

// translate a local binary structure to network order 
void packet_local_to_network(struct packet *local, struct packet *net) { 
    int i; 
    net->magic = ntohl(local->magic); 
    switch (local->magic) { 
	case MAGIC_MESSAGE: 
	    memcpy(net->u.mes.digest,  local->u.mes.digest, MD5_DIGEST_LENGTH); 
	    memcpy(net->u.mes.message, local->u.mes.message, MAXMESSAGE); 
	    break; 
	case MAGIC_REGISTER: 
	    net->u.reg.unused = ntohl(local->u.reg.unused); 
	    break; 
	case MAGIC_REDIRECT: 
	    net->u.red.nredirects = ntohl(local->u.red.nredirects); 
	    for (i=0; i<MAXREDIRECTS; i++) { 
		memcpy(&net->u.red.redirects[i],
		       &local->u.red.redirects[i],
		       sizeof(struct sockaddr)); 
	    } 
	    break; 
    }  
} 

// translate from network order to local order 
void packet_network_to_local(struct packet *local, struct packet *net) { 
    int i; 
    local->magic = htonl(net->magic); 
    switch (local->magic) { 
	case MAGIC_MESSAGE: 
	    memcpy(local->u.mes.digest, net->u.mes.digest, MD5_DIGEST_LENGTH); 
	    memcpy(local->u.mes.message, net->u.mes.message, MAXMESSAGE); 
	    break; 
	case MAGIC_REGISTER: 
	    local->u.reg.unused = htonl(net->u.reg.unused); 
	    break; 
	case MAGIC_REDIRECT: 
	    local->u.red.nredirects = htonl(net->u.red.nredirects); 
	    for (i=0; i<MAXREDIRECTS; i++) { 
		memcpy(&local->u.red.redirects[i],
		       &net->u.red.redirects[i],
		       sizeof(struct sockaddr)); 
	    } 
	    break; 
    }  
} 

/*=============================
   high-level protocol help 
  =============================*/ 

// send a message to the fabric 
ssize_t send_message(int sockfd, const struct sockaddr *to_addr, 
	  	 const unsigned char *digest, const char *message) { 
    struct packet loc_msg; 
    struct packet net_msg; 
/* set up cmd structure */ 
    memset(&loc_msg, 0, sizeof(loc_msg)); 
    loc_msg.magic= MAGIC_MESSAGE; 
    memcpy(loc_msg.u.mes.digest, digest, MD5_DIGEST_LENGTH); 
    strncpy(loc_msg.u.mes.message, message, MAXMESSAGE); 
/* translate to network order */ 
    packet_local_to_network(&loc_msg, &net_msg) ; 
/* send the packet */ 
    int sendsize = sizeof(struct packet); 
    int ret = sendto(sockfd, (void *)&net_msg, sendsize, 0, 
	(struct sockaddr *)to_addr, sizeof(struct sockaddr)); 
    if (ret<0) perror("send_message"); 
    return ret;
} 

// send a registration request (no message) 
ssize_t send_register(int sockfd, const struct sockaddr *to_addr) { 
    struct packet loc_msg; 
    struct packet net_msg; 
/* set up cmd structure */ 
    memset(&loc_msg, 0, sizeof(loc_msg)); 
    loc_msg.magic=MAGIC_REGISTER; 
    loc_msg.u.reg.unused=0; 
/* translate to network order */ 
    packet_local_to_network(&loc_msg, &net_msg) ; 
/* send the packet */ 
    ssize_t ret = sendto(sockfd, (void *)&net_msg, sizeof(net_msg), 0, 
	(struct sockaddr *)to_addr, sizeof(struct sockaddr)); 
    if (ret<0) perror("send_register"); 
    return ret;
} 

// send a redirect to a server 
// if red_addr is NULL, just send a blank redirect. 
// if red_addr is non-zero, redirect to the given server 
ssize_t send_redirect(int sockfd, const struct sockaddr *to_addr, 
	const struct sockaddr *red_addr) { 
    struct packet loc_msg; 
    struct packet net_msg; 
/* set up cmd structure */ 
    memset(&loc_msg, 0, sizeof(loc_msg)); 
    loc_msg.magic=MAGIC_REDIRECT; 
    if (red_addr) { 
	loc_msg.u.red.nredirects=1; 
	memcpy(&loc_msg.u.red.redirects[0], red_addr, sizeof(struct sockaddr)); 
    } else { 
	loc_msg.u.red.nredirects=0; 
    } 
/* translate to network order */ 
    packet_local_to_network(&loc_msg, &net_msg) ; 
/* send the packet */ 
    ssize_t ret = sendto(sockfd, (void *)&net_msg, sizeof(net_msg), 0, 
	(struct sockaddr *)to_addr, sizeof(struct sockaddr)); 
    if (ret<0) perror("send_redirect"); 
    return ret;
} 

/* receive a packet from a server */ 
ssize_t recv_packet(int sockfd, struct packet *p, 
                    struct sockaddr *from_addr) { 
    socklen_t from_len; 		/* length of from addr */ 
    ssize_t mesglen; 			/* message length used */ 
    mesglen = 0; 
    from_len = sizeof(struct sockaddr);	/* MUST initialize this */ 
    mesglen = recvfrom(sockfd, p, sizeof(struct packet), 0, 
	from_addr, &from_len);
    /* check for socket error */ 
    if (mesglen<0) { 
	perror("recv_packet"); 
	return mesglen; 
    } 
    return mesglen; 
} 

// /* run a "select" on the local socket, with timeout */ 
// int select_packet(int sockfd) { 
//   /* set up a timeout wait for input */ 
//     fd_set rfds;
//   /* Watch sockfd to see when it has input. */
//     FD_ZERO(&rfds);
//     FD_SET(sockfd, &rfds);
//     FD_SET(fileno(stdin), &rfds);
//     int ret = select(sockfd+1, &rfds, NULL, NULL, NULL); 
//     if (FD_ISSET(sockfd, &rfds)) { 
// 	return sockfd; 
//     } else if (FD_ISSET(fileno(stdin), &rfds)) { 
// 	return fileno(stdin); 
//     } else return (-1); 
// } 

/*=============================
  messenger usage and argument checking 
  =============================*/
void messenger_usage() { 
    fprintf(stderr,
	"messenger usage: ./messenger [{server_address} {server_port}]\n");
    fprintf(stderr, 
	"  {server_address} is the IP address where another server is running.\n"); 
    fprintf(stderr, 
	"  {server_port} is the port number of that server.\n"); 
} 

int messenger_arguments_valid(const char *server_dotted, int server_port) { 

    int valid = TRUE; 
    struct in_addr server_addr; 	/* native address */ 
    if (inet_pton(AF_INET, server_dotted, &server_addr)<=0) { 
	fprintf(stderr, "messenger: server address %s is not valid\n", 
		server_dotted); 
	valid=FALSE; 
	
    } 
    
    if (server_port<1024 || server_port>65535) { 
	fprintf(stderr, "messenger: server_port %d is not allowed\n", 
		server_port); 
	valid=FALSE; 
    } 

    return valid; 
} 

/*=============================
  routines that only work properly in Halligan hall
  (because they understand Halligan network policies)
  read the primary IP address for an ECE/CS linux host 
  this is always the address bound to interface eth0
  this is used to avoid listening (by default) on 
  secondary interfaces. 
  =============================*/ 
int get_primary_addr(struct in_addr *a) { 
    struct ifaddrs * ifAddrStruct=NULL;
    struct ifaddrs * ifa=NULL;
    if (!getifaddrs(&ifAddrStruct)) // get linked list of interface specs
	for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
	    if (ifa ->ifa_addr->sa_family==AF_INET) {  // is an IP4 Address
		if (strcmp(ifa->ifa_name,"eth0")==0) { // is for interface eth0
		    void *tmpAddrPtr=&((struct sockaddr_in *)ifa->ifa_addr)
			->sin_addr;
		    memcpy(a, tmpAddrPtr, sizeof(struct in_addr)); 
		    if (ifAddrStruct!=NULL) freeifaddrs(ifAddrStruct);
		    return 0; // found 
		} 
	    } 
	}
    if (ifAddrStruct!=NULL) freeifaddrs(ifAddrStruct);
    return -1; // not found
} 
