// this is a UDP messenger based upon a simple flooding strategy

/*=============================
  Starting solution for Assignment 3: clandestine messaging
  the intent of this file is to demonstrate how to 
  call the helper routines. It will help you avoid some
  common errors. IT DOES NOT SOLVE THE PROBLEM. 
  =============================*/ 

#include "help.h" 

#define TRUE  1
#define FALSE 0

// THESE VARIABLES MUST BE GLOBAL in order to be visible in shutdown handler

/* local messenger data */ 
int messenger_sockfd;			/* descriptor for socket endpoint */ 
struct sockaddr_in messenger_sockaddr;	/* address/port pair */ 
struct in_addr messenger_addr; 		/* network-format address */ 
char messenger_dotted[INET_ADDRSTRLEN];	/* human-readable address */ 
int messenger_port; 			/* local port */ 

/* list of targets to which to propogate information */ 
int ntargets = 0; 
struct sockaddr_in targets[MAXTARGETS]; // message targets for flooding

// an interrupt-handler for control-C (to enable orderly shutdown) 
void handler(int signal) { 
    int i; 
    fprintf(stderr,"messenger: received control-C\n"); 
    fprintf(stderr, "REACT HERE TO CONTROL-C\n"); 
    // simplistic shutdown: just turn off messages 
    for (i=0; i<ntargets; i++) { 
	send_redirect(messenger_sockfd, (struct sockaddr *) &targets[i], NULL); 
    } 
    exit(1); 
} 

int main(int argc, char **argv)
{
    int i; 

/* intercept control-C and do an orderly shutdown */ 
    signal(SIGINT, handler); 

/* read arguments */ 
    if (argc%2 == 1) { // must have an odd number of arguments
	for (i=1; i<argc; i+=2) { 
	    /* target server data */ 
	    char *target_dotted; 		/* human-readable address */ 
	    int target_port; 			/* port in local order */ 
	    struct in_addr target_addr; 	/* address in network order */ 
	    target_dotted = argv[i]; 
	    target_port = atoi(argv[i+1]); 
	    if (!messenger_arguments_valid(target_dotted, target_port)) { 
		    messenger_usage(); 
		    exit(1); 
	    } 
	    inet_pton(AF_INET, target_dotted, &target_addr);
	    memset(&targets[ntargets], 0, sizeof(struct sockaddr_in)); 
	    targets[ntargets].sin_family = PF_INET;
	    memcpy(&targets[ntargets].sin_addr,&target_addr,
		sizeof(struct in_addr)); 
	    targets[ntargets].sin_port   = htons(target_port);  
	    ntargets++; // one target 
	} 

    } else { 
	fprintf(stderr, "%s: wrong number of arguments\n",argv[0]); 
	messenger_usage(); 
        exit(1); 
    } 
/* get the primary IP address of this host */ 
    get_primary_addr(&messenger_addr); 
    inet_ntop(AF_INET, &messenger_addr, messenger_dotted, INET_ADDRSTRLEN);

/* make a socket*/ 
    messenger_sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if (messenger_sockfd<0) { 
	perror("can't open socket"); 
	exit(1); 
    } 

/* construct an endpoint address with primary address and random port */ 
    memset(&messenger_sockaddr, 0, sizeof(messenger_sockaddr));
    messenger_sockaddr.sin_family = PF_INET;
    memcpy(&messenger_sockaddr.sin_addr,&messenger_addr,
	sizeof(struct in_addr)); 
    messenger_sockaddr.sin_port   = 0; // choose port randomly 

/* bind it to an appropriate local address and choose port */ 
    if (bind(messenger_sockfd, (struct sockaddr *) &messenger_sockaddr, 
	     sizeof(messenger_sockaddr))<0) { 
	perror("can't bind local address"); 
	exit(1); 
    } 

/* recover and report random port assignment */ 
    socklen_t len = sizeof(struct sockaddr); 
    if (getsockname(messenger_sockfd, (struct sockaddr *) &messenger_sockaddr, 
          &len)<0) { 
	perror("can't recover bound local port"); 
	exit (1); 
    } 
    messenger_port = ntohs(messenger_sockaddr.sin_port); 

// print informative message about how to proceed
    fprintf(stderr, "Type '%s %s %d' to contact this server\n", 
	argv[0], messenger_dotted, messenger_port); 

// contact other targets, if any, with registration commands 
// this code does not run if the number of targets is zero
    for (i=0; i<ntargets; i++) { 
	send_register(messenger_sockfd, (struct sockaddr *) &targets[i]);
    } 

    while (TRUE) { // infinite loop: end with control-C 

        // Watch stdin, sockfd to see when either has input.
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(messenger_sockfd, &rfds);
	FD_SET(fileno(stdin), &rfds);
	if (select(messenger_sockfd+1, &rfds, NULL, NULL, NULL)<0) { 
	    perror("select"); 
	    exit(1); 
	} 

	// check for input from socket 
	if (FD_ISSET(messenger_sockfd, &rfds)) { 
	    struct packet net; 		  // network order 
	    struct packet local; 	  // local order 
	    struct sockaddr_in send_addr; // sender address 
	    ssize_t retval = recv_packet(messenger_sockfd, &net, 
		    (struct sockaddr *)&send_addr); 

	    if (retval<0) { 
	    /* error */ 
		perror("recv_packet"); 
		exit(1); 
	    } else { 
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
		    case MAGIC_MESSAGE: 
			// HANDLE MESSAGE PRINTING HERE. 
			fprintf(stderr, "%s:%d: %s\n",send_dotted,send_port,
			    local.u.mes.message); 
			break; 
		    case MAGIC_REGISTER: 
			// HANDLE REGISTRATION REQUESTS HERE. 
			fprintf(stderr, "%s:%d: REGISTRATION REQUEST\n", 
			    send_dotted, send_port); 
			break; 
		    case MAGIC_REDIRECT: 
			// HANDLE REDIRECTION REQUESTS HERE. 
			fprintf(stderr, "%s:%d: REDIRECT REQUEST\n", 
			    send_dotted, send_port); 
			for (i=0; i<local.u.red.nredirects; i++) { 

			    // convert redirect address to human-readable 
			    struct sockaddr_in *s = (struct sockaddr_in *) 
				&(local.u.red.redirects[i]); 

			    inet_ntop(AF_INET, 
				(void *)&(s->sin_addr.s_addr),  
				red_dotted, INET_ADDRSTRLEN);
			    red_port = ntohs(s->sin_port); 
			    fprintf(stderr, "  to %s:%d\n",red_dotted,red_port); 
			} 
			break; 
		    default: 
			fprintf(stderr, "%s:%d: UNKNOWN PACKET TYPE %d\n", 
			    send_dotted, send_port, local.magic); 
			break; 
		} 
	    } 
	} 

	// check for input from terminal (user typed \n)
	if (FD_ISSET(fileno(stdin), &rfds)) { 
	    // flood message to each destination in "targets" 
	    char message[MAXMESSAGE]; 
	    fgets(message, MAXMESSAGE, stdin); 
            message[strlen(message)-1]='\0'; 
	    unsigned char digest[MD5_DIGEST_LENGTH]; 
	    memset(digest, 0, MD5_DIGEST_LENGTH);
	    for (i=0; i<ntargets; i++) { 
		send_message(messenger_sockfd, (struct sockaddr *) 
			&targets[i],digest,message);
	    } 
	} 
    } 
}

