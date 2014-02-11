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

struct file_meta{
	char *file_name;
	int fd;
	int done;
	long num_blocks;
	struct bits fbits;
	struct timeval last_request;
};
typedef struct file_meta file_meta;


int resend(file_meta *F, int sockfd, const char *address, int port)
{
	int ranges[MAXRANGES];
	int i=0,j=0,requested_ranges=0;
	struct command loc_cmd;
	struct command_network net_cmd;
	struct sockaddr_in server_addr;

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = PF_INET;
	inet_pton(PF_INET, address, &server_addr.sin_addr);
	server_addr.sin_port = htons(port);

	memset(&loc_cmd, 0, sizeof(loc_cmd));
	strcpy(loc_cmd.filename, F->file_name);
	loc_cmd.nranges=0;
	int c=-1,b=0;
	//fprintf(stderr,"Creating ranges for file: %s, with %llu total blocks.\n",F->file_name,F->fbits.nbits);
	while(i<F->fbits.nbits) // loop through all blocks
	{
		if(bits_testbit(&F->fbits,i))   	// found missing block, so start range
		{		
			c++;  				// move to next range
			b++;				// increment block counter (stats)
			loc_cmd.nranges=c+1;			// update number of ranges
			loc_cmd.ranges[c].first_block = i;	// this is the start of the "hole"
			loc_cmd.ranges[c].last_block=F->num_blocks;  // in case we get to the end and haven't set this..
			i++;
			while(i<F->fbits.nbits)
			{
				if(!bits_testbit(&F->fbits,i))  // found the next non-missing block, so the previous block is the end of the range
				{
					loc_cmd.ranges[c].last_block = i-1; 
					b+=loc_cmd.ranges[c].last_block-loc_cmd.ranges[c].first_block;
					break;
				}
				i++;
			}
		}
		i++;  
		if(c==MAXRANGES-1){	// filled up a resend packet, so send it away..
		//	fprintf(stderr,"Filled up a resend packet for %s...  sending it away\n",loc_cmd.filename);
		//	for(j=0;j<=c;j++)
		//		fprintf(stderr,"%d(%llu-%llu)%s",j+1,loc_cmd.ranges[j].first_block,loc_cmd.ranges[j].last_block,((j+1)%6==0)?"\n":",");
			command_local_to_network(&loc_cmd, &net_cmd);
			int sendsize = COMMAND_SIZE(loc_cmd.nranges);
			int ret = sendto(sockfd, (void *)&net_cmd, sendsize, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
			if (ret<0)
				perror("send_command");
	//		fprintf(stderr,"sent %llu ranges, for %s\n",loc_cmd.nranges,loc_cmd.filename);
			c=-1;  //reset range counter
			requested_ranges+=12;
			loc_cmd.nranges=0;
		}
	}

	if(c>-1)  //unsent, partial command
	{
	//	fprintf(stderr,"Sending non-full resend request\n");
	//	for(j=0;j<=c;j++)
	//		fprintf(stderr,"%d(%llu-%llu)%s",j+1,loc_cmd.ranges[j].first_block,loc_cmd.ranges[j].last_block,((j+1)%6==0)?"\n":",");
		command_local_to_network(&loc_cmd, &net_cmd); 
		int sendsize = COMMAND_SIZE(loc_cmd.nranges);
		int ret = sendto(sockfd, (void *)&net_cmd, sendsize, 0, (struct sockaddr *)&server_addr, sizeof(server_addr)); 
		if (ret<0) 
			perror("send_command"); 
	//	fprintf(stderr,"sent %llu ranges, for %s\n",loc_cmd.nranges,loc_cmd.filename);
		requested_ranges+=c+1;
	}
	fprintf(stderr,"Requested resends for %d ranges, %d blocks for %s.\n",requested_ranges,b,loc_cmd.filename);
	return 1;
}

int main(int argc, char **argv)
{

	/* local client data */ 
	int sockfd;		   		/* file descriptor for endpoint */ 
	struct sockaddr_in client_sockaddr;	/* address/port pair */ 
	struct in_addr client_addr; 	/* network-format address */ 
	char client_dotted[INET_ADDRSTRLEN];/* human-readable address */ 
	int client_port; 			/* local port */ 

	int a;
	int file_count=argc-4;   // all arguments past the first 4 are considered to be filenames

	// allocate space to store file_names (anything past 4th argument)
	file_meta *state = (file_meta *) malloc(file_count*sizeof(file_meta));
	if (state==NULL)
		exit(0);
	fprintf(stderr,"allocated some bytes..\n");
	for (a=0;a<file_count;a++){
		state[a].file_name = (char *) malloc(strlen(argv[a+4])+1);
		strcpy(state[a].file_name,argv[a+4]);
		state[a].fd=open(state[a].file_name,O_RDWR|O_CREAT|O_TRUNC,S_IWUSR|S_IRUSR);
		if(state[a].fd==-1){
			perror("Can't open file"); exit(0);
		}
		fprintf(stdout,"File descriptor used for \"%s\" is %d\n",state[a].file_name,state[a].fd);
		state[a].done=-1;
		state[a].num_blocks=-1;
		state[a].last_request.tv_sec=0; state[a].last_request.tv_usec=0;
	}


	/* remote server data */ 
	char *server_dotted; 		/* human-readable address */ 
	int server_port; 			/* remote port */ 

	/* the request */ 
	char *filename; 			/* filename to request */ 

	/* read arguments */ 
	if (argc < 5) { 
		fprintf(stderr, "client: wrong number of arguments\n"); 
		client_usage(); 
		exit(1); 
	} 

	server_dotted = argv[1]; 
	server_port = atoi(argv[2]); 
	client_port = atoi(argv[3]); 
	filename = argv[4]; 

	if (!client_arguments_valid(server_dotted, server_port, client_port, filename)) { 
		client_usage(); exit(1); 
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
	if (bind(sockfd, (struct sockaddr *) &client_sockaddr, sizeof(client_sockaddr))<0) { 
		perror("can't bind local address"); 
		exit(1); 
	} 
	fprintf(stderr, "client: Receiving on %s, port %d\n", client_dotted, client_port); 

	// send initial requests for files
	for (a=0;a<file_count;a++){  
		send_command(sockfd, server_dotted, server_port, state[a].file_name, 0, INT_MAX); 
		fprintf(stdout, "client: requesting %s blocks %d-%d\n",state[a].file_name, 0, INT_MAX);
	}

	int which_file;
	long pos=0,offset=0;
	int i;	
	int finished = FALSE; // set to TRUE when you think you're done
	long timeout=500000;
	while (!finished) { 
		int retval; 
again: 
		if ((retval = select_block(sockfd, 0, 125000))==0) { 
			/* timeout */ 
			// no new blocks, so let's check our files for completeness and request missing blocks.
			// TODO - don't want to re-request until we've 
			fprintf(stderr,"No blocks..  we hit select's timeout\n");
			finished=TRUE;
			for(i=0;i<file_count;i++)
			{
				if(!state[i].done)
					finished=FALSE;
			}
			if(finished)
				break;
			for (a=0;a<file_count;a++){  
				if(!state[a].done){
					//bits_printlist(&state[a].fbits);
					//fprintf(stderr,"Successfully printed bitlist\n");
					resend(&state[a],sockfd,server_dotted,server_port);
				}
			}

		} 

		else if (retval<0) { 
			/* error */ 
			perror("select"); 
			fprintf(stderr, "client: receive error\n"); 
		} 

		//  We got a packet, lets deal with it
		else { 
			/* input is waiting, read it */ 
			struct sockaddr_in resp_sockaddr; 	/* address/port pair */ 
			int resp_len; 			/* length used */ 
			char resp_dotted[INET_ADDRSTRLEN]; 	/* human-readable address */ 
			int resp_port; 			/* port */ 
			int resp_mesglen; 			/* length of message */ 
			struct block b; 

			recv_block(sockfd, &b, &resp_sockaddr);
			inet_ntop(AF_INET, (void *)&(resp_sockaddr.sin_addr.s_addr), resp_dotted, INET_ADDRSTRLEN);
			resp_port = ntohs(resp_sockaddr.sin_port); 

			//fprintf(stderr, "client: %s:%d sent %s block %llu (range 0-%llu)\n",resp_dotted, resp_port, b.filename, b.which_block, b.total_blocks);

			int which_file=-1;
			for (a=0;a<file_count;a++){
				//fprintf(stderr,"[%s] [%s]\n",state[a].file_name, b.filename);
				if (strcmp(state[a].file_name, b.filename)==0){
					which_file=a;
				}}
			//fprintf(stderr,"preparing to write to file %d: %s, which has file descriptor %d\n",which_file,b.filename,state[which_file].fd);
			if(which_file==-1)  // this block is for a file we didn't request
			{ 
				fprintf(stdout, "client: received block with incorrect filename: %s\n", b.filename);
				goto again; 
			} 


			// First block for a file is special.. we need to set up bookkeeping

			if(state[which_file].done==-1){
				//fprintf(stdout,"Received first block (%llu) for filename %s.\n",b.which_block,b.filename);
				bits_alloc(&(state[which_file].fbits), b.total_blocks);
				//fprintf(stderr,"[%s]\n",state[which_file].file_name);
				bits_setrange(&(state[which_file].fbits),0,b.total_blocks-1);
				state[which_file].done=0;

			}
			if(state[which_file].done==1)
			{
				fprintf(stderr,"recieved block for file %s that is complete... ignoring..\n",b.filename);
				continue;
			}
			offset=b.which_block*PAYLOADSIZE;   // offset to where we want to seek
			pos=lseek(state[which_file].fd, offset, SEEK_SET); // seek to proper offset for block
			int written=write(state[which_file].fd, b.payload, b.paysize); 
			bits_clearbit(&state[which_file].fbits, b.which_block); // mark block as written
			if (bits_empty(&state[which_file].fbits)){              // all blocks marked?
				fprintf(stdout,"-=-=-=-=-=-=-=-= %s - file complete!  -=-=-=-=-=-=-=-=\n",b.filename);
				state[which_file].done=TRUE;
				close(state[which_file].fd);

			} 
		} 
	}
	//cleanup
	//for (a=0;a<file_count;a++){
	//		if(done[a]==1)
	//		  bits_free(&fbits[a]);
	//		free(file_names[a]);
	//	}
	//	free(fbits);
	free(state);


}
