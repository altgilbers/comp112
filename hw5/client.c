
// this is a UDP key-value client 

/*=============================
  Starting solution for Assignment 5: strong consistency
  the intent of this file is to demonstrate how to 
  call the helper routines. It will help you avoid some
  common errors. IT DOES NOT SOLVE THE PROBLEM. 
  =============================*/ 

#include "help.h" 

void client_usage() { 
    fprintf(stderr,"client usage options:\n"); 
    fprintf(stderr,"   ./client {address} {port} put {key} {value}\n"); 
    fprintf(stderr,"   ./client {address} {port} get {key}\n"); 
    fprintf(stderr,"   ./client {address} {port} del {key}\n"); 
} 

#define TRUE  1
#define FALSE 0
#define CMDLEN 10 

/* local messenger data */ 
int client_sockfd;			/* descriptor for socket endpoint */ 
struct sockaddr_in client_sockaddr;	/* address/port pair */ 
struct in_addr client_addr; 		/* network-format address */ 
char client_dotted[INET_ADDRSTRLEN];	/* human-readable address */ 
int client_port; 			/* local port */ 

/* target server to contact */ 
struct in_addr target_addr; 		/* network-format address */ 
struct sockaddr_in target_sockaddr; 	/* address+port */ 
char target_dotted[INET_ADDRSTRLEN];	/* human-readable address */ 
int target_port; 			/* server port */ 
char target_command[CMDLEN]; 		/* what to do */ 
char target_key[KEYLEN]; 
char target_value[VALLEN]; 

void print_packet(struct packet *local, char *send_dotted, short send_port) { 
    switch (local->magic) { 
	case MAGIC_PUT: 
	    fprintf(stderr, "%s:%d: put (%s=>%s)\n",
		send_dotted,send_port,
		local->u.put.key, local->u.put.value); 
	    break; 
	case MAGIC_ACK: 
	    fprintf(stderr, "%s:%d: ack (%s=>%s)\n",
		send_dotted,send_port,
		local->u.ack.key, local->u.ack.value); 
	    break; 
	case MAGIC_GET: 
	    fprintf(stderr, "%s:%d: get (%s)\n",
		send_dotted,send_port,
		local->u.get.key); 
	    break; 
	case MAGIC_GOT: 
	    fprintf(stderr, "%s:%d: got (%s=>%s)\n",
		send_dotted,send_port,
		local->u.got.key, local->u.got.value); 
	    break; 
	case MAGIC_NOT: 
	    fprintf(stderr, "%s:%d: not (%s)\n",
		send_dotted,send_port,
		local->u.not.key); 
	    break; 
	case MAGIC_DEL: 
	    fprintf(stderr, "%s:%d: del (%s)\n",
		send_dotted,send_port,
		local->u.del.key); 
	    break; 
	case MAGIC_DAK: 
	    fprintf(stderr, "%s:%d: dak (%s)\n",
		send_dotted,send_port,
		local->u.dak.key); 
	    break; 
	default: 
	    fprintf(stderr, "%s:%d: UNKNOWN PACKET MAGIC NUMBER %d\n", 
		send_dotted, send_port, local->magic); 
	    break; 
    } 
}

int main(int argc, char **argv)
{
    int i; 

/* read arguments */ 
    if (argc > 4) { 
	strncpy(target_dotted,argv[1],sizeof(target_dotted)); 
	inet_pton(AF_INET, target_dotted, &target_addr);
	target_port = atoi(argv[2]); 
     
	// create the socket address pair 
        memset(&target_sockaddr, 0, sizeof(target_sockaddr));
	memcpy(&target_sockaddr.sin_addr,&target_addr, sizeof(struct in_addr)); 
	target_sockaddr.sin_port = htons(target_port);

	strncpy(target_command,argv[3],sizeof(target_command)); 
	if (strncmp(target_command, "get", 3)!=0 
         && strncmp(target_command, "put", 3)!=0) { 
	    fprintf(stderr,"Invalid command %s\n", target_command); 
	    client_usage(); 
	    exit(1); 
        } 
	strncpy(target_key,argv[4],sizeof(target_key)); 
	if (strcmp(target_command, "put")==0) { 
	    if (argc>5) { 
		strncpy(target_value,argv[5],sizeof(target_value)); 
	    } else { 
		fprintf(stderr,"No value specified for command %s\n", target_command); 
		client_usage(); 
		exit(1); 
	    } 
	} 
    } else { 
	fprintf(stderr, "%s: too few arguments\n",argv[0]); 
	client_usage(); 
        exit(1); 
    } 
/* get the primary IP address of this host */ 
    get_primary_addr(&client_addr); 
    inet_ntop(AF_INET, &client_addr, client_dotted, INET_ADDRSTRLEN);

/* make a socket*/ 
    client_sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if (client_sockfd<0) { 
	perror("can't open socket"); 
	exit(1); 
    } 

/* construct an endpoint address with primary address and given port */ 
    memset(&client_sockaddr, 0, sizeof(client_sockaddr));
    client_sockaddr.sin_family = PF_INET;
    memcpy(&client_sockaddr.sin_addr,&client_addr,
	sizeof(struct in_addr)); 
    client_sockaddr.sin_port=0; // choose randomly 

/* bind it to an appropriate local address and choose port */ 
    if (bind(client_sockfd, (struct sockaddr *) &client_sockaddr, 
	     sizeof(client_sockaddr))<0) { 
	perror("can't bind local address"); 
	exit(1); 
    } 

/* recover and report random port assignment */ 
    socklen_t len = sizeof(struct sockaddr); 
    if (getsockname(client_sockfd, (struct sockaddr *) &client_sockaddr, 
          &len)<0) { 
	perror("can't recover bound local port"); 
	exit (1); 
    } 
    client_port = ntohs(client_sockaddr.sin_port); 

// print informative message about how to proceed
    fprintf(stderr, "%s running at address %s, port %d\n", 
	argv[0], client_dotted, client_port); 

   if (strncmp(target_command, "put", 3)==0) { 
	fprintf(stderr, "command: put (%s=>%s)\n", target_key, target_value); 
	send_put(client_sockfd, 
	    (struct sockaddr *)&target_sockaddr, target_key, target_value); 

	// wait for response for a little while 
        // Watch sockfd to see when it has input.
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(client_sockfd, &rfds);
        struct timeval timeout = { 1, 0 }; 
        int result = select(client_sockfd+1, &rfds, NULL, NULL, &timeout); 
        if (result<0) { 
	    perror("select"); 
	    exit(1); 
	} 

	// check for input from socket 
	if (FD_ISSET(client_sockfd, &rfds)) { 
	    struct packet net; 		  // network order 
	    struct packet local; 	  // local order 
	    struct sockaddr_in send_addr; // sender address 
	    ssize_t retval = recv_packet(client_sockfd, &net, 
		    (struct sockaddr *)&send_addr); 

	    if (retval<0) { 
		/* error */ 
		perror("recv_packet"); 
		exit(1); 
	    } 

	    /* get human-readable internet address */
	    char send_dotted[INET_ADDRSTRLEN]; /* sender address */ 
	    int send_port; 		       /* sender port */ 

	    // convert sender address to local human-readable format 
	    inet_ntop(AF_INET, (void *)&(send_addr.sin_addr.s_addr),  
		    send_dotted, INET_ADDRSTRLEN);
	    send_port = ntohs(send_addr.sin_port); 

	    // convert sent packet to local ordering 
	    packet_network_to_local(&local, &net); 

	    // print packet for debugging purposes 
	    print_packet(&local, send_dotted, send_port); 
	} else { 
	    fprintf(stderr, "timed out waiting for response\n"); 
	    exit(1); 
        } 

   } else if (strncmp(target_command, "get", 3)==0) { 
	fprintf(stderr, "command: get (%s)\n", target_key); 
	send_get(client_sockfd, (struct sockaddr *)&target_sockaddr, target_key); 

	// wait for response for a little while 
        // Watch sockfd to see when it has input.
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(client_sockfd, &rfds);
        struct timeval timeout = { 1, 0 }; 
        int result = select(client_sockfd+1, &rfds, NULL, NULL, &timeout); 
        if (result<0) { 
	    perror("select"); 
	    exit(1); 
	} 

	// check for input from socket 
	if (FD_ISSET(client_sockfd, &rfds)) { 
	    struct packet net; 		  // network order 
	    struct packet local; 	  // local order 
	    struct sockaddr_in send_addr; // sender address 
	    ssize_t retval = recv_packet(client_sockfd, &net, 
		    (struct sockaddr *)&send_addr); 

	    if (retval<0) { 
		/* error */ 
		perror("recv_packet"); 
		exit(1); 
	    } 

	    /* get human-readable internet address */
	    char send_dotted[INET_ADDRSTRLEN]; /* sender address */ 
	    int send_port; 		       /* sender port */ 

	    // convert sender address to local human-readable format 
	    inet_ntop(AF_INET, (void *)&(send_addr.sin_addr.s_addr),  
		    send_dotted, INET_ADDRSTRLEN);
	    send_port = ntohs(send_addr.sin_port); 

	    // convert sent packet to local ordering 
	    packet_network_to_local(&local, &net); 

	    print_packet(&local, send_dotted, send_port); 
	} else { 
	    fprintf(stderr, "timed out waiting for response\n"); 
	    exit(1); 
	} 
    } else if (strncmp(target_command, "del", 3)==0) { 

	fprintf(stderr, "command: del (%s)\n", target_key); 
	send_del(client_sockfd, (struct sockaddr *)&target_sockaddr, target_key); 

	// wait for response for a little while 
        // Watch sockfd to see when it has input.
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(client_sockfd, &rfds);
        struct timeval timeout = { 1, 0 }; 
        int result = select(client_sockfd+1, &rfds, NULL, NULL, &timeout); 
        if (result<0) { 
	    perror("select"); 
	    exit(1); 
	} 

	// check for input from socket 
	if (FD_ISSET(client_sockfd, &rfds)) { 
	    struct packet net; 		  // network order 
	    struct packet local; 	  // local order 
	    struct sockaddr_in send_addr; // sender address 
	    ssize_t retval = recv_packet(client_sockfd, &net, 
		    (struct sockaddr *)&send_addr); 

	    if (retval<0) { 
		/* error */ 
		perror("recv_packet"); 
		exit(1); 
	    } 

	    /* get human-readable internet address */
	    char send_dotted[INET_ADDRSTRLEN]; /* sender address */ 
	    int send_port; 		       /* sender port */ 

	    // convert sender address to local human-readable format 
	    inet_ntop(AF_INET, (void *)&(send_addr.sin_addr.s_addr),  
		    send_dotted, INET_ADDRSTRLEN);
	    send_port = ntohs(send_addr.sin_port); 

	    // convert sent packet to local ordering 
	    packet_network_to_local(&local, &net); 

	    print_packet(&local, send_dotted, send_port); 
	} else { 
	    fprintf(stderr, "timed out waiting for response\n"); 
	    exit(1); 
	} 
    } else { 
	fprintf(stderr, "unrecognized command %s\n",target_command); 
	exit(1); 
    } 
}

