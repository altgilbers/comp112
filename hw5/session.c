// this is a UDP messenger based upon a simple flooding strategy

/*=============================
  Starting solution for Assignment 5: strong consistency
  the intent of this file is to demonstrate how to 
  call the helper routines. It will help you avoid some
  common errors. IT DOES NOT SOLVE THE PROBLEM. 
  =============================*/ 

#include "help.h" 

#define TRUE  1
#define FALSE 0

// THESE VARIABLES MUST BE GLOBAL in order to be visible in shutdown handler

/* local messenger data */ 
int session_sockfd;			/* descriptor for socket endpoint */ 
struct sockaddr_in session_sockaddr;	/* address/port pair */ 
struct in_addr session_addr; 		/* network-format address */ 
char session_dotted[INET_ADDRSTRLEN];	/* human-readable address */ 
int session_port; 			/* local port */ 

int main(int argc, char **argv)
{
    int i; 

    /* intercept control-C and do an orderly shutdown */ 
    // not an option here, will use inelegant kill (-9) 
    // signal(SIGINT, handler); 

/* read arguments */ 
    if (argc == 2) { // must have an odd number of arguments
	session_port = atoi(argv[1]); 
	if (!session_arguments_valid(session_port)) { 
	    session_usage(); 
	    exit(1); 
	} 
    } else { 
	fprintf(stderr, "%s: wrong number of arguments\n",argv[0]); 
	session_usage(); 
        exit(1); 
    } 
/* get the primary IP address of this host */ 
    get_primary_addr(&session_addr); 
    inet_ntop(AF_INET, &session_addr, session_dotted, INET_ADDRSTRLEN);

/* make a socket*/ 
    session_sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if (session_sockfd<0) { 
	perror("can't open socket"); 
	exit(1); 
    } 

/* construct an endpoint address with primary address and given port */ 
    memset(&session_sockaddr, 0, sizeof(session_sockaddr));
    session_sockaddr.sin_family = PF_INET;
    memcpy(&session_sockaddr.sin_addr,&session_addr,
	sizeof(struct in_addr)); 
    session_sockaddr.sin_port=htons(session_port); 

/* bind it to an appropriate local address and choose port */ 
    if (bind(session_sockfd, (struct sockaddr *) &session_sockaddr, 
	     sizeof(session_sockaddr))<0) { 
	perror("can't bind local address"); 
	exit(1); 
    } 

/* recover and report random port assignment */ 
    socklen_t len = sizeof(struct sockaddr); 
    if (getsockname(session_sockfd, (struct sockaddr *) &session_sockaddr, 
          &len)<0) { 
	perror("can't recover bound local port"); 
	exit (1); 
    } 
    session_port = ntohs(session_sockaddr.sin_port); 

// print informative message about how to proceed
    fprintf(stderr, "%s running on address %s, port %d\n", 
	argv[0], session_dotted, session_port); 

    while (TRUE) { // infinite loop: end with control-C 

        // Watch sockfd to see when it has input.
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(session_sockfd, &rfds);
	if (select(session_sockfd+1, &rfds, NULL, NULL, NULL)<0) { 
	    perror("select"); 
	    exit(1); 
	} 

	// check for input from socket 
	if (FD_ISSET(session_sockfd, &rfds)) { 
	    struct packet net; 		  // network order 
	    struct packet local; 	  // local order 
	    struct sockaddr_in send_addr; // sender address 
	    ssize_t retval = recv_packet(session_sockfd, &net, 
		    (struct sockaddr *)&send_addr); 

	    if (retval<0) { 
	    /* error */ 
		perror("recv_packet"); 
		exit(1); 
	    } else { 
		char value[VALLEN]; 
	    /* get human-readable internet address */
		char send_dotted[INET_ADDRSTRLEN]; /* sender address */ 
		int send_port; 			   /* sender port */ 
		char red_dotted[INET_ADDRSTRLEN];  /* redirect address */ 
		int red_port; 			   /* redirect port */ 

		// convert sender address to local human-readable format 
		inet_ntop(AF_INET, (void *)&(send_addr.sin_addr.s_addr),  
		    send_dotted, INET_ADDRSTRLEN);
		send_port = ntohs(send_addr.sin_port); 

		// convert sent packet to local ordering 
		packet_network_to_local(&local, &net); 

		switch (local.magic) { 
		    case MAGIC_PUT: 
			fprintf(stderr, "%s:%d: put (%s=>%s)\n",
			    send_dotted,send_port,
			    local.u.put.key, local.u.put.value); 
			rbtree_put(local.u.put.key, local.u.put.value); 
			send_ack(session_sockfd, (struct sockaddr *)&send_addr, local.u.put.key, local.u.put.value); 
			break; 
		    case MAGIC_GET: 
			fprintf(stderr, "%s:%d: get (%s)\n",
			    send_dotted,send_port,
			    local.u.get.key); 
			if (rbtree_get(local.u.get.key, value)) { 
			    send_got(session_sockfd, (struct sockaddr *)&send_addr, local.u.get.key, value); 
			} else { 
			    send_not(session_sockfd, (struct sockaddr *)&send_addr, local.u.get.key); 
			} 
			break; 
		    default: 
			fprintf(stderr, "%s:%d: UNKNOWN PACKET MAGIC NUMBER %d\n", 
			    send_dotted, send_port, local.magic); 
			break; 
		} 
	    } 
	} 
    } 
}

