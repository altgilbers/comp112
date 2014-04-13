/*=============================
   help for Assignment 5: strong consistency
  =============================*/ 

#include "help.h" 

#define TRUE  1
#define FALSE 0

/*=============================
   low-level protocol help 
  =============================*/ 

// translate a local binary structure to network order 
void packet_local_to_network(const struct packet *local, struct packet *net) { 
    int i; 
    net->magic = ntohl(local->magic); 
    switch (local->magic) { 
	case MAGIC_PUT: 
	    memcpy((void *)&(net->u.put), 
		   (const void *)&(local->u.put), 
		   sizeof(local->u.put)); 
	    break; 
	case MAGIC_ACK: 
	    memcpy((void *)&(net->u.ack), 
		   (const void *)&(local->u.ack), 
		   sizeof(local->u.ack)); 
	    break; 
	case MAGIC_GET: 
	    memcpy((void *)&(net->u.get), 
		   (const void *)&(local->u.get), 
		   sizeof(local->u.get)); 
	    break; 
	case MAGIC_GOT: 
	    memcpy((void *)&(net->u.got), 
		   (const void *)&(local->u.got), 
		   sizeof(local->u.got)); 
	    break; 
	case MAGIC_NOT: 
	    memcpy((void *)&(net->u.not), 
		   (const void *)&(local->u.not), 
		   sizeof(local->u.not)); 
	    break; 
	case MAGIC_DEL: 
	    memcpy((void *)&(net->u.del), 
		   (const void *)&(local->u.del), 
		   sizeof(local->u.del)); 
	    break; 
	case MAGIC_DAK: 
	    memcpy((void *)&(net->u.dak), 
		   (const void *)&(local->u.dak), 
		   sizeof(local->u.dak)); 
	    break; 
	//////////////////////////////
	// add your own types here
	//////////////////////////////
	default: 
	    fprintf(stderr, "packet_local_to_network: invalid magic number %ld\n", local->magic); 
	    break; 
    }  
} 

// translate from network order to local order 
void packet_network_to_local(struct packet *local, const struct packet *net) { 
    int i; 
    local->magic = htonl(net->magic); 
    switch (local->magic) { 
	case MAGIC_PUT: 
	    memcpy((void *)&(local->u.put), 
		   (const void *)&(net->u.put), 
		   sizeof(local->u.put)); 
	    break; 
	case MAGIC_ACK: 
	    memcpy((void *)&(local->u.ack), 
		   (const void *)&(net->u.ack), 
		   sizeof(local->u.ack)); 
	    break; 
	case MAGIC_GET: 
	    memcpy((void *)&(local->u.get), 
		   (const void *)&(net->u.get), 
		   sizeof(local->u.get)); 
	    break; 
	case MAGIC_GOT: 
	    memcpy((void *)&(local->u.got), 
		   (const void *)&(net->u.got), 
		   sizeof(local->u.got)); 
	    break; 
	case MAGIC_NOT: 
	    memcpy((void *)&(local->u.not), 
	   	   (const void *)&(net->u.not), 
		   sizeof(local->u.not)); 
	    break; 
	case MAGIC_DEL: 
	    memcpy((void *)&(local->u.del), 
	   	   (const void *)&(net->u.del), 
		   sizeof(local->u.del)); 
	    break; 
	case MAGIC_DAK: 
	    memcpy((void *)&(local->u.dak), 
	   	   (const void *)&(net->u.dak), 
		   sizeof(local->u.dak)); 
	    break; 
	//////////////////////////////
	// add your own types here
	//////////////////////////////
	default: 
	    fprintf(stderr, "packet_network_to_local: invalid magic number %ld\n", local->magic); 
	    break; 
    }  
} 

/*=============================
   high-level protocol help 
  =============================*/ 

// send a put request 
ssize_t send_put(int sockfd, const struct sockaddr *to_addr, const char *key, const char *value ) { 
    struct packet loc_msg; 
    struct packet net_msg; 
    // compute size of the union element we need. 
    ssize_t size = ((char *)((&net_msg.u.put)+1)-(char *)&net_msg);
/* set up cmd structure */ 
    memset(&loc_msg, 0, sizeof(loc_msg)); 
    loc_msg.magic=MAGIC_PUT; 
    strncpy(loc_msg.u.put.key, key, KEYLEN); 
    strncpy(loc_msg.u.put.value, value, VALLEN); 
/* translate to network order */ 
    packet_local_to_network(&loc_msg, &net_msg) ; 
/* send the packet */ 
    ssize_t ret = sendto(sockfd, (void *)&net_msg, size, 0, 
	(struct sockaddr *)to_addr, sizeof(struct sockaddr)); 
    if (ret<0) perror("send_put"); 
    return ret;
} 

// send an ack 
ssize_t send_ack(int sockfd, const struct sockaddr *to_addr, const char *key, const char *value ) { 
    struct packet loc_msg; 
    struct packet net_msg; 
    // compute size of the union element we need. 
    ssize_t size = ((char *)((&net_msg.u.ack)+1)-(char *)&net_msg);
/* set up cmd structure */ 
    memset(&loc_msg, 0, sizeof(loc_msg)); 
    loc_msg.magic=MAGIC_ACK; 
    strncpy(loc_msg.u.put.key, key, KEYLEN); 
    strncpy(loc_msg.u.put.value, value, VALLEN); 
/* translate to network order */ 
    packet_local_to_network(&loc_msg, &net_msg) ; 
/* send the packet */ 
    ssize_t ret = sendto(sockfd, (void *)&net_msg, size, 0, 
	(struct sockaddr *)to_addr, sizeof(struct sockaddr)); 
    if (ret<0) perror("send_ack"); 
    return ret;
} 

// send a get request 
ssize_t send_get(int sockfd, const struct sockaddr *to_addr, const char *key) { 
    struct packet loc_msg; 
    struct packet net_msg; 
    // compute size of the union element we need. 
    ssize_t size = ((char *)((&net_msg.u.get)+1)-(char *)&net_msg);
/* set up cmd structure */ 
    memset(&loc_msg, 0, sizeof(loc_msg)); 
    loc_msg.magic=MAGIC_GET; 
    strncpy(loc_msg.u.get.key, key, KEYLEN); 
/* translate to network order */ 
    packet_local_to_network(&loc_msg, &net_msg) ; 
/* send the packet */ 
    ssize_t ret = sendto(sockfd, (void *)&net_msg, size, 0, 
	(struct sockaddr *)to_addr, sizeof(struct sockaddr)); 
    if (ret<0) perror("send_get"); 
    return ret;
} 

// send a get "found" response
ssize_t send_got(int sockfd, const struct sockaddr *to_addr, const char *key, const char *value) { 
    struct packet loc_msg; 
    struct packet net_msg; 
    // compute size of the union element we need. 
    ssize_t size = ((char *)((&net_msg.u.got)+1)-(char *)&net_msg);
/* set up cmd structure */ 
    memset(&loc_msg, 0, sizeof(loc_msg)); 
    loc_msg.magic=MAGIC_GOT; 
    strncpy(loc_msg.u.got.key, key, KEYLEN); 
    strncpy(loc_msg.u.got.value, value, VALLEN); 
/* translate to network order */ 
    packet_local_to_network(&loc_msg, &net_msg) ; 
/* send the packet */ 
    ssize_t ret = sendto(sockfd, (void *)&net_msg, size, 0, 
	(struct sockaddr *)to_addr, sizeof(struct sockaddr)); 
    if (ret<0) perror("send_got"); 
    return ret;
} 

// send a get "not found" response
ssize_t send_not(int sockfd, const struct sockaddr *to_addr, const char *key) { 
    struct packet loc_msg; 
    struct packet net_msg; 
    // compute size of the union element we need. 
    ssize_t size = ((char *)((&net_msg.u.not)+1)-(char *)&net_msg);
/* set up cmd structure */ 
    memset(&loc_msg, 0, sizeof(loc_msg)); 
    loc_msg.magic=MAGIC_NOT; 
    strncpy(loc_msg.u.not.key, key, KEYLEN); 
/* translate to network order */ 
    packet_local_to_network(&loc_msg, &net_msg) ; 
/* send the packet */ 
    ssize_t ret = sendto(sockfd, (void *)&net_msg, size, 0, 
	(struct sockaddr *)to_addr, sizeof(struct sockaddr)); 
    if (ret<0) perror("send_not"); 
    return ret;
} 

// send a del request 
ssize_t send_del(int sockfd, const struct sockaddr *to_addr, const char *key) { 
    struct packet loc_msg; 
    struct packet net_msg; 
    // compute size of the union element we need. 
    ssize_t size = ((char *)((&net_msg.u.del)+1)-(char *)&net_msg);
/* set up cmd structure */ 
    memset(&loc_msg, 0, sizeof(loc_msg)); 
    loc_msg.magic=MAGIC_NOT; 
    strncpy(loc_msg.u.del.key, key, KEYLEN); 
/* translate to network order */ 
    packet_local_to_network(&loc_msg, &net_msg) ; 
/* send the packet */ 
    ssize_t ret = sendto(sockfd, (void *)&net_msg, size, 0, 
	(struct sockaddr *)to_addr, sizeof(struct sockaddr)); 
    if (ret<0) perror("send_del"); 
    return ret;
} 

// send a del acknowledgement
ssize_t send_dak(int sockfd, const struct sockaddr *to_addr, const char *key) { 
    struct packet loc_msg; 
    struct packet net_msg; 
    // compute size of the union element we need. 
    ssize_t size = ((char *)((&net_msg.u.dak)+1)-(char *)&net_msg);
/* set up cmd structure */ 
    memset(&loc_msg, 0, sizeof(loc_msg)); 
    loc_msg.magic=MAGIC_NOT; 
    strncpy(loc_msg.u.dak.key, key, KEYLEN); 
/* translate to network order */ 
    packet_local_to_network(&loc_msg, &net_msg) ; 
/* send the packet */ 
    ssize_t ret = sendto(sockfd, (void *)&net_msg, size, 0, 
	(struct sockaddr *)to_addr, sizeof(struct sockaddr)); 
    if (ret<0) perror("send_dak"); 
    return ret;
} 

//////////////////////////////
// add send routines for your own types here
//////////////////////////////

/* receive a packet from a server or client */ 
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

/*=============================
  session usage and argument checking 
  =============================*/
void session_usage() { 
    fprintf(stderr,
	"session usage: ./session {server_port}\n");
    fprintf(stderr, 
	"  {server_port} is the UDP port number of this server.\n"); 
} 

int session_arguments_valid(int server_port) { 

    int valid = TRUE; 
    if (server_port<1024 || server_port>65535) { 
	fprintf(stderr, "session: server_port %d is not allowed\n", 
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

#if 0
// test that size hack works properly 
// this allows you to make struct packet much larger without affecting my code. 
main()
{ 

    struct packet net_msg; 
    ssize_t size = ((char *)((&net_msg.u.put)+1)-(char *)&net_msg);
    if (size != sizeof(struct packet)) { 
	printf("oops: %d != %d\n", size, sizeof(struct packet)); 
    } else { 
	printf("yes!  %d == %d\n", size, sizeof(struct packet)); 
    } 
    
}
#endif /*0*/
