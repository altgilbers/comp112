/*===========================================================================

  Comp 112 Networks HW2 - UDP File Transfer

  My solution to this problem takes a list of files, and sends a request for all
  blocks in that file.  It then waits for a response and handles the blocks sent
  from the server.  Each block is recorded into the appropriate file at the 
  appropriate offset.   Blocks are tracked and when all blocks have been received
  the program cleans up and exits.

  Trade-offs...   Due to the nature of UDP (exacerbated by the "goofiness" of the
  server, some blocks will not be delivered and must be re-requested.  If a time-
  out is set to low, the client may request blocks that have already been sent, 
  adding to the network overhead.  If the timeout is set too long, the time to
  deliver the file will be too long.

  I tried to create an adaptive timeout, which would "poll" the connection by 
  recording the time that the first request was transmitted then recording the 
  time of the first response packet (and adding a little cushion to account for 
  "jitter".  This didn't work too well, unless I padded with a lot of cushion.
  I don't know how much (if any) additional delay is introduced intentionally
  by the server.

  I tested it locally with 20 files simultaneously and with files as large as
  2^24 bytes. Larger files made the timeouts a little unpredictable

  =============================================================================*/

#include "help.h" 

#define TRUE  1
#define FALSE 0

// bookkeeping data structure
struct file_meta{
	char *file_name;				// name of file in filesystem
	int fd;							// file descriptor 
	int done;						// done flag..  set to -1 initially, zero when a block is received, and 1 when the file is complete
	long num_blocks;				// number of blocks in file, for convenience
	long blocks_written;		
	long resends;
	struct bits fbits;				// bit array for tracking which blocks have been received
	struct timeval last_request;	// timestamp of the last block received (used in determining when to quit)
};
typedef struct file_meta file_meta;

/* Function to request resends for missing blocks.   It aggregates contiguous block
   ranges to minimize the number of resend requests needed to complete the file.
   It takes a pointer to a file_meta record and the socket and file parameters.
 */
int resend(file_meta *F, int sockfd, const char *address, int port)
{
	int i=0,requested_ranges=0,c=-1,b=0;
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
	//fprintf(stderr,"Creating ranges for file: %s, with %llu blocks.\n",F->file_name,F->fbits.nbits);
	while(i<F->fbits.nbits) 					// loop through all blocks
	{
		if(bits_testbit(&F->fbits,i))			// found missing block, so start range
		{		
			c++;							// move to next range
			b++;							// increment block counter (stats)
			loc_cmd.nranges=c+1;			// update number of ranges
			loc_cmd.ranges[c].first_block = i;	// this is the start of the "hole"
			loc_cmd.ranges[c].last_block=F->num_blocks;  // if no more missing
			i++;
			while(i<F->fbits.nbits)
			{
				if(!bits_testbit(&F->fbits,i))	// next present block, so prev
					// block is end of the range
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
			command_local_to_network(&loc_cmd, &net_cmd);
			int ret = sendto(sockfd, (void *)&net_cmd, COMMAND_SIZE(loc_cmd.nranges), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
			if (ret<0)
				perror("send_command");
			// resets and bookkeeping:
			c=-1;  requested_ranges+=12;  loc_cmd.nranges=0;
		}
	}

	if(c>-1)  //unsent, partial command
	{
		command_local_to_network(&loc_cmd, &net_cmd); 
		int ret = sendto(sockfd, (void *)&net_cmd, COMMAND_SIZE(loc_cmd.nranges), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
		if (ret<0) 
			perror("send_command"); 
		requested_ranges+=c+1;
	}
	fprintf(stderr,"Requested resends for %d ranges, %d blocks for %s.\n",requested_ranges,b,loc_cmd.filename);
	F->resends+=b;
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
	int timeouts_since_last_response=0;
	// allocate space to store file_names (anything past 4th argument)
	file_meta *state = (file_meta *) malloc(file_count*sizeof(file_meta));
	if (state==NULL)
		exit(0);
	for (a=0;a<file_count;a++){
		state[a].file_name = (char *) malloc(strlen(argv[a+4])+1);
		strcpy(state[a].file_name,argv[a+4]);
		state[a].fd=open(state[a].file_name,O_RDWR|O_CREAT|O_TRUNC,S_IWUSR|S_IRUSR);
		if(state[a].fd==-1){
			perror("Can't open file"); exit(0);
		}
		//fprintf(stdout,"File descriptor used for \"%s\" is %d\n",state[a].file_name,state[a].fd);
		state[a].done=-1;
		state[a].resends=0;
		state[a].blocks_written=0;
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
	struct timeval start,tmp,end,timeout,inc,min;
	gettimeofday(&start,NULL);
	for (a=0;a<file_count;a++){  
		send_command(sockfd, server_dotted, server_port, state[a].file_name, 0, INT_MAX); 
		fprintf(stdout, "client: requesting %s blocks %d-%d\n",state[a].file_name, 0, INT_MAX);
	}

	long pos=0,offset=0;
	int i, which_file, finished = FALSE; // set to TRUE when you think you're done
	int got_any_packet=0;

	timeout.tv_sec=0;
	timeout.tv_usec=1000;
	min.tv_sec=0;
	min.tv_usec=250000;
	while (!finished) { 
		int retval; 
again: 
		if ((retval = select_block(sockfd, (int)timeout.tv_sec, (int)timeout.tv_usec))==0) { 
			timeouts_since_last_response++;	
			if(timeouts_since_last_response>10)
			{
				fprintf(stderr,"10 timeouts with no reply..   Server not responding!\n");
				finished=TRUE;
				break;
			}
			if(got_any_packet==0){		// haven't recieved a packet, let's increase the timeout
				gettimeofday(&end,NULL);
				timersub(&end,&start,&tmp);
				fprintf(stderr,"----Waiting for first packet ELAPSED TIME: %ld.%06d Timeout: %ld.%06d-------\n",tmp.tv_sec,tmp.tv_usec,timeout.tv_sec,timeout.tv_usec);
				timeradd(&timeout,&timeout,&tmp);
				timeout.tv_sec=tmp.tv_sec;
				timeout.tv_usec=tmp.tv_usec;
				continue;						
			}
			gettimeofday(&end,NULL);
			timersub(&end,&start,&tmp);
			fprintf(stderr,"----timed out (%ld.%06d). (%d consecutive)  asking for more: %ld.%06d -------\n",timeout.tv_sec,timeout.tv_usec,timeouts_since_last_response,tmp.tv_sec,tmp.tv_usec);
			timeradd(&timeout,&inc,&timeout);  // double the timeout, each time... for small files this won't matter.. large files seem to have more jitter
			/* timeout */ 
			// no new blocks, so let's check our files for completeness and request missing blocks.
			finished=TRUE;
			for(i=0;i<file_count;i++)
			{
				if(state[i].done<1)
					finished=FALSE;
			}
			if(finished)
				break;
			for (a=0;a<file_count;a++){  
				if(state[a].done==0){
					//bits_printlist(&state[a].fbits); fprintf(stderr,"Successfully printed bitlist\n");
					resend(&state[a],sockfd,server_dotted,server_port);
				}
				if(state[a].done==-1 && timeouts_since_last_response>1)
				{
					fprintf(stderr,"Never got a reply for %s, resending initial request.\n",state[a].file_name);	
			                state[a].resends++;
					send_command(sockfd, server_dotted, server_port, state[a].file_name, 0, INT_MAX);
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
			char resp_dotted[INET_ADDRSTRLEN]; 	/* human-readable address */ 
			int resp_port; 			/* port */ 
			struct block b; 

			recv_block(sockfd, &b, &resp_sockaddr);
			inet_ntop(AF_INET, (void *)&(resp_sockaddr.sin_addr.s_addr), resp_dotted, INET_ADDRSTRLEN);
			resp_port = ntohs(resp_sockaddr.sin_port); 

			//fprintf(stderr, "client: %s:%d sent %s block %llu (range 0-%llu)\n",resp_dotted, resp_port, b.filename, b.which_block, b.total_blocks);

			which_file=-1;
			for (a=0;a<file_count;a++){
				if (strcmp(state[a].file_name, b.filename)==0){
					which_file=a;
					timeouts_since_last_response=0;
					if(got_any_packet<1)		// got first packet... I'll use this to determine my select timeout
					{
						struct timeval tmp1,tmp2;
						gettimeofday(&tmp1,NULL);
						timersub(&tmp1,&start,&tmp2);

						if(tmp2.tv_sec<min.tv_usec && tmp2.tv_usec<min.tv_usec)	
						{
							fprintf(stderr,"Timeout too short... upping to .125s\n");
							timeout.tv_sec=min.tv_sec;
							timeout.tv_usec=min.tv_usec;
						}
						else
						{
							timeout.tv_sec=tmp2.tv_sec;
							timeout.tv_usec=tmp2.tv_usec;
						}
						got_any_packet++;
						fprintf(stderr,"First packet received after: %ld.%06d sec, setting timeout to:%ld.%06d sec.\n",tmp2.tv_sec,tmp2.tv_usec,timeout.tv_sec,timeout.tv_usec);
					}

				}}
			//fprintf(stderr,"preparing to write to file %d: %s, which has file descriptor %d\n",which_file,b.filename,state[which_file].fd);
			if(which_file==-1)  // this block is for a file we didn't request
			{ 
				fprintf(stdout, "client: received block with incorrect filename: %s\n", b.filename);
				goto again; 
			} 


			// First block for a file is special.. we need to set up bookkeeping

			if(state[which_file].done==-1){
				fprintf(stdout,"Received first block (%llu) for filename %s.\n",b.which_block,b.filename);
				bits_alloc(&(state[which_file].fbits), b.total_blocks);
				//fprintf(stderr,"[%s]\n",state[which_file].file_name);
				bits_setrange(&(state[which_file].fbits),0,b.total_blocks-1);
				state[which_file].done=0;

			}
			if(state[which_file].done==1)
			{
				fprintf(stderr,"recieved block for file %s that is complete... maybe the timeout is too short... ignoring..\n",b.filename);
				continue;
			}
			offset=b.which_block*PAYLOADSIZE;   // offset to where we want to seek
			pos=lseek(state[which_file].fd, offset, SEEK_SET); // seek to proper offset for block
			if(pos!=offset)
			{
				fprintf(stderr,"Error seeking\n");exit(0);
			}
			int written=write(state[which_file].fd, b.payload, b.paysize); 
			if (written != b.paysize)
			{
				fprintf(stderr,"Error writing");exit(0);
			}
			state[which_file].blocks_written++;
			gettimeofday(&state[which_file].last_request,NULL);
			bits_clearbit(&state[which_file].fbits, b.which_block); // mark block as written
			if (bits_empty(&state[which_file].fbits)){              // all blocks marked?
				fprintf(stdout,"-=-=-=-=-=-=-=-= %s - file complete!  -=-=-=-=-=-=-=-=\n",b.filename);
				state[which_file].done=TRUE;
				close(state[which_file].fd);

			} 
		} 
	}

	// Summary:
	fprintf(stderr,"--------------------------------------------------------------------\n");
	fprintf(stderr,"%-20s%10s\t%s\t%s\t%s\n","Filename","Complete","written","resends","ET");
	fprintf(stderr,"--------------------------------------------------------------------\n");
	long total_written=0,total_resent=0;
	for(i=0;i<file_count;i++)
	{
		timersub(&state[i].last_request,&start,&tmp);
		total_written+=state[i].blocks_written;
		total_resent+=state[i].resends;
		fprintf(stderr,"%-20s%-10s\t%ld\t%ld\t%ld.%06d\n",state[i].file_name,state[i].done>0?"yes":"no",state[i].blocks_written,state[i].resends,tmp.tv_sec,tmp.tv_usec);
	}
	fprintf(stderr,"--------------------------------------------------------------------\n");
	gettimeofday(&end,NULL); timersub(&end,&start,&tmp);
	fprintf(stderr,"%-20s%-10s\t%ld\t%ld\t%ld.%06d\n","Total:","-",total_written,total_resent,tmp.tv_sec,tmp.tv_usec);


	// Cleanup
	for(i=0;i<file_count;i++)
	{
		bits_free(&state[i].fbits);
		free(state[i].file_name);
		close(state[i].fd);  // in case it was still open
	}
	free(state);
}
