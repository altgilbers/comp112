/*

   Comp112 Networks HW3
   Ian Altgilbers 

   I tried to implement the multi-connect mesh network.  Each node keep track of any
   nodes that register.  All messages are sent along with a hash of the message salted
   with the IP, port and time of day.  Any message received will be sent out to all
   nodes except the node from whence it came.  When each message is received, its hash
   is recorded in a rolling record of the last MAX_DIGESTS digests.  If there is a hash
   collision, the message is assumed to be a repeat and is discarded.

   When nodes leave the network, they fix the hole they would've left by connecting
   every node to another node.  I did this by basically promoting the first node
   in the client list to take over for the departing node.

   While the client is runnign you can enter ? to get a listing of current peers
   or you can enter ! to get a list of hashes the node has seen.

   Known issues:
   1. I don't do anything to handle registrations that put a node over its client limit.
   2. I only have calendar-second resolution on my salt..  So if a client types the same
   message twice within a second, only one will get through..

 */




#include "help.h" 
#include <time.h>

#define TRUE  1
#define FALSE 0
#define MAX_DIGESTS 20

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


void printClients();

void handler(int signal) { 
	int i; 
	fprintf(stderr,"messenger: received control-C\n"); 
	printClients(); 
	if(ntargets==1)
	{
		fprintf(stderr,"sending null redirect to target 0\n");								
		send_redirect(messenger_sockfd, (struct sockaddr *) &targets[0], NULL);
	}
	else if (ntargets>1)
	{
		for(i=1;i<ntargets;i++)
		{
			fprintf(stderr,"Sending redirect for (%d,%d)  -  %d to %d\n",0,i,ntohs(targets[0].sin_port),ntohs(targets[i].sin_port));
			fprintf(stderr,"Sending redirect for (%d,%d)  -  %d to %d\n",i,0,ntohs(targets[i].sin_port),ntohs(targets[0].sin_port));
			send_redirect(messenger_sockfd, (struct sockaddr *) &targets[0], (struct sockaddr *) &targets[i]);
			send_redirect(messenger_sockfd, (struct sockaddr *) &targets[i], (struct sockaddr *) &targets[0]);
		}
	}
	exit(1); 
} 

void SIGUSR1_handler(int signal)
{
	fprintf(stderr,"Got SIGUSR1.\n");
}
void printClients()
{
	int w;
	fprintf(stderr,"All clients:\n");
	for(w=0;w<ntargets;w++)
	{
		char dotted[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, (void *)&(targets[w].sin_addr.s_addr),dotted, INET_ADDRSTRLEN);
		fprintf(stderr,"client: %d, %s:%d\n",w,dotted,ntohs(targets[w].sin_port));
	}
	return;
}


void printDigests(unsigned char **digests,int latest_digest)
{
	int n,i;
	fprintf(stderr,"All digests seen so far:\n");
	for(n=0;n<MAX_DIGESTS;n++)
	{
		for (i=0; i<16; i++) {
			fprintf(stderr,"%02x",digests[n][i]);
		}
		fprintf(stderr,"\n");
	}

}

/*
   Take a digest and see if it is in the provided list of digests
 */
int messageSeen(unsigned char *digest, unsigned char **digests)
{
	int i,j;
	for(i=0;i<MAX_DIGESTS;i++)
	{
		for(j=0;j<MD5_DIGEST_LENGTH;j++)
		{
			if(digest[j]!=digests[i][j])
			{
				break;
			}
			fprintf(stderr,"Found matching MD5 checksum\n");	
			return 1;
		}
	}
	return 0;
}

int main(int argc, char **argv)
{
	int i,p,known; 

	/* to keep track of message digests I've seen */
	unsigned char digest[MD5_DIGEST_LENGTH];
	unsigned char **digests=(unsigned char **) malloc(sizeof(unsigned char *)*MAX_DIGESTS);
	for(i=0;i<MAX_DIGESTS;i++)
	{
		digests[i]=(unsigned char *) malloc(sizeof(unsigned char)*MD5_DIGEST_LENGTH);
	}
	int latest_digest=0;
	/* intercept control-C and do an orderly shutdown */ 
	signal(SIGINT, handler); 
	signal(SIGUSR1, SIGUSR1_handler);
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
		latest_digest=latest_digest%MAX_DIGESTS;
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

						// Check to see if this message comes from a known source
						known=0;
						for(p=0;p<ntargets;p++)
						{
							if(targets[p].sin_addr.s_addr==send_addr.sin_addr.s_addr 
									&& targets[p].sin_port==send_addr.sin_port)
							{
								known=1;
							}
						}
						if(known==0)
						{
							fprintf(stderr,"PROTOCOL: message from unknown client.  disregarding");
							break;
						}

						// Check to see if this message's digest has been seen before
						memset(digest, 0, sizeof(unsigned char)*MD5_DIGEST_LENGTH);
						memcpy(digest,local.u.mes.digest,sizeof(unsigned char)*MD5_DIGEST_LENGTH);
						if(messageSeen(digest,digests))
						{
							//fprintf(stderr,"PROTOCOL: received duplicate message from %s:%d.  not forwarding or displaying....\n",send_dotted,send_port);
							break;
						}


						/*  If we get here, then we have a new message from a known client..   
						    I'll display it, record its hash, and forward it to all registered 
						    clients (other than the source I got it from).
						 */

						// Display message
						fprintf(stderr, "%s:%d: %s\n",send_dotted,send_port,local.u.mes.message); 

						// recored hash for future reference
						memcpy(digests[latest_digest],local.u.mes.digest,sizeof(unsigned char)*MD5_DIGEST_LENGTH);
						latest_digest++;

						// loop through all clients i am aware of
						for (i=0; i<ntargets; i++) {
							if (targets[i].sin_addr.s_addr==send_addr.sin_addr.s_addr && targets[i].sin_port==send_addr.sin_port){
								//fprintf(stderr,"PROTOCOL: not sending message back to where we got this from...\n");
							}
							else{
								send_message(messenger_sockfd, (struct sockaddr *)&targets[i],local.u.mes.digest,local.u.mes.message);
							}
						}
						break; 
					case MAGIC_REGISTER: 
						// HANDLE REGISTRATION REQUESTS HERE. 
						//  pretty basic... if we get a Registration, add the address:port pair to our local client list
						fprintf(stderr, "%s:%d: REGISTRATION REQUEST\n", send_dotted, send_port); 
						memset(&targets[ntargets], 0, sizeof(struct sockaddr_in)); 
						targets[ntargets].sin_family = PF_INET;
						memcpy(&targets[ntargets].sin_addr,&send_addr.sin_addr,sizeof(struct in_addr)); 
						targets[ntargets].sin_port=send_addr.sin_port;  
						ntargets++; // one target 
						break; 
					case MAGIC_REDIRECT: 
						// HANDLE REDIRECTION REQUESTS HERE. 
						fprintf(stderr, "REDIRECT REQUEST from: %s:%d\n", send_dotted, send_port); 
						int j;
						/*  If there are multiple redirect targets, we'll be growing ntargets.
						    If there are redirect targets is NULL, we'll have a hole in our targets list...
						 */

						// no matter if it is an empty redirect or a multiple redirect, we're going to remove the host that send the redirect 
						for(j=0;j<ntargets;j++)  // loop through all targets
						{
							if (targets[j].sin_addr.s_addr==send_addr.sin_addr.s_addr && targets[j].sin_port==send_addr.sin_port)  // if the target[] entry matches sender, react
							{
								if(j!=ntargets-1)  // if it not the last slot, overwrite with last slot info
								{
									//fprintf(stderr,"overwriting slot %d with slot %d\n",j,ntargets-1);		
									memcpy(&targets[j].sin_addr,&targets[ntargets-1].sin_addr,sizeof(struct in_addr));
									targets[j].sin_port=targets[ntargets-1].sin_port;
								}
								//fprintf(stderr,"removing slot %d\n",ntargets-1);
								ntargets--;
							}
						}

						//loop through all redirects (really should only ever be one, unless I make my own send_redirect)
						for (i=0; i<local.u.red.nredirects; i++) 
						{ 
							//fprintf(stderr,"Got at least one redirect target\n");
							// convert redirect address to human-readable 
							struct sockaddr_in *s = (struct sockaddr_in *) &(local.u.red.redirects[i]); 
							inet_ntop(AF_INET,(void *)&(s->sin_addr.s_addr),red_dotted, INET_ADDRSTRLEN);
							red_port = ntohs(s->sin_port); 
							//fprintf(stderr, "  to %s:%d\n",red_dotted,red_port); 
							int found=0;

							// we add this redirect, but must make sure it isn't a node we're already tracking
							for(j=0;j<ntargets;j++)  // loop through all targets
							{
								if (targets[j].sin_addr.s_addr==s->sin_addr.s_addr && targets[j].sin_port==s->sin_port)  // if redirect source ip:port matches, replace
								{
									found=1; // we found this target already, so no need to re-add it
								}
							}
							if(!found) //we don't already have this client in our list, so let's add
							{
								memcpy(&targets[ntargets].sin_addr,&(s->sin_addr),sizeof(struct in_addr));
								targets[ntargets].sin_port=s->sin_port;
								ntargets++;
							}
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
			int i,n,m; 
			fgets(message, MAXMESSAGE, stdin); 
			message[strlen(message)-1]='\0'; 

			// diagnostic info...
			if(message[0]=='?')
			{
				printClients();
			}
			else if(message[0]=='!')
			{
				printDigests(digests,latest_digest);
			}

			else  // regular message
			{	
				char salted_message[MAXMESSAGE+13+5+25];  // make room for message plus salt

				time_t t = time(NULL);
				struct tm *tmptr = localtime(&t);

				sprintf(salted_message,"%s:%d:%d:%s",message,12345,12345,asctime(tmptr)); // construct salted message
				//fprintf(stderr,"salted message:%s\n",salted_message);
				unsigned char digest[MD5_DIGEST_LENGTH]; 
				memset(digest, 0, MD5_DIGEST_LENGTH);
				MD5((const unsigned char *)salted_message, strlen(salted_message), digest); // hash salted message

				memcpy(digests[latest_digest],digest,sizeof(unsigned char)*MD5_DIGEST_LENGTH);
				latest_digest++;

				for (i=0; i<ntargets; i++) { 
					send_message(messenger_sockfd, (struct sockaddr *) &targets[i],digest,message);
				} 
			}	
		}   // end FD_ISSET for stdin 
	} 

	for(i=0;i<MAX_DIGESTS;i++)
		free(digests[i]);
	free(digests);
}

