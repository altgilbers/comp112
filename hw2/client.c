// this is a UDP file service client based upon 
// a simple file completion strategy

/*=============================
  Starting solution for Assignment 2: UDP file transfer
  the intent of this file is to demonstrate how to 
  call the helper routines. It will help you avoid some
  common errors. IT DOES NOT SOLVE THE PROBLEM. 
  =============================*/ 

#include "help.h" 

#define TRUE  1
#define FALSE 0

int main(int argc, char **argv)
{

/* local client data */ 
    int sockfd;		   		/* file descriptor for endpoint */ 
    struct sockaddr_in client_sockaddr;	/* address/port pair */ 
    struct in_addr client_addr; 	/* network-format address */ 
    char client_dotted[INET_ADDRSTRLEN];/* human-readable address */ 
    int client_port; 			/* local port */ 

/* remote server data */ 
    char *server_dotted; 		/* human-readable address */ 
    int server_port; 			/* remote port */ 

/* the request */ 
    char *filename; 			/* filename to request */ 

/* read arguments */ 
    if (argc != 5) { 
	fprintf(stderr, "client: wrong number of arguments\n"); 
	client_usage(); 
        exit(1); 
    } 

    server_dotted = argv[1]; 
    server_port = atoi(argv[2]); 
    client_port = atoi(argv[3]); 
    filename = argv[4]; 

    if (!client_arguments_valid(
	server_dotted, server_port, 
	client_port, filename)) { 
	    client_usage(); 
	    exit(1); 
    } 

/* get the primary IP address of this host */ 
    get_primary_addr(&client_addr); 
    inet_ntop(AF_INET, &client_addr, client_dotted, INET_ADDRSTRLEN);

/* construct an endpoint address with primary address and desired port */ 
    memset(&client_sockaddr, 0, sizeof(client_sockaddr));
    client_sockaddr.sin_family      = PF_INET;
    memcpy(&client_sockaddr.sin_addr,&client_addr,sizeof(struct in_addr)); 
    client_sockaddr.sin_port        = htons(client_port);

/* make a socket*/ 
    sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if (sockfd<0) { 
	perror("can't open socket"); 
	exit(1); 
    } 

/* bind it to an appropriate local address and port */ 
    if (bind(sockfd, (struct sockaddr *) &client_sockaddr, 
	     sizeof(client_sockaddr))<0) { 
	perror("can't bind local address"); 
	exit(1); 
    } 
    fprintf(stderr, "client: Receiving on %s, port %d\n", 
	client_dotted, client_port); 

/* send a command */ 
    send_command(sockfd, server_dotted, server_port, filename, 0, MAXINT); 

    fprintf(stderr, "client: requesting %s blocks %d-%d\n", 
	filename, 0, MAXINT); 

/* receive the whole document and make naive assumptions */ 
    int done = FALSE; // set to TRUE when you think you're done
    while (!done) { 
        int retval; 
again: 
	if ((retval = select_block(sockfd, 1, 0))==0) { 
        /* timeout */ 
	    fprintf(stderr, "HANDLE LACK OF RESPONSE HERE\n"); 
       } else if (retval<0) { 
	/* error */ 
	    perror("select"); 
	    fprintf(stderr, "client: receive error\n"); 
       } else { 
	 /* input is waiting, read it */ 
	    struct sockaddr_in resp_sockaddr; 	/* address/port pair */ 
	    int resp_len; 			/* length used */ 
	    char resp_dotted[INET_ADDRSTRLEN]; 	/* human-readable address */ 
	    int resp_port; 			/* port */ 
	    int resp_mesglen; 			/* length of message */ 
	    struct block one_block; 

	/* use helper routine to receive a block */ 
	    recv_block(sockfd, &one_block, &resp_sockaddr);

	/* get human-readable internet address */
	    inet_ntop(AF_INET, (void *)&(resp_sockaddr.sin_addr.s_addr),  
		resp_dotted, INET_ADDRSTRLEN);
  	    resp_port = ntohs(resp_sockaddr.sin_port); 

	    fprintf(stderr, "client: %s:%d sent %s block %d (range 0-%d)\n",
		resp_dotted, resp_port, one_block.filename, 
		one_block.which_block, one_block.total_blocks);

	/* check block data for errors */
	    if (strcmp(filename, one_block.filename)!=0) { 
		fprintf(stderr, 
		    "client: received block with incorrect filename %s\n", 
		    one_block.filename); 
		goto again; 
	    } 
	    
	    fprintf(stderr, "RECORD BLOCK DATA HERE\n"); 
	} 
    } 
}

