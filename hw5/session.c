#include "help.h" 
#include <pthread.h>
#define TRUE  1
#define FALSE 0

#define DEFAULT_PORT 9960
#define MAX_UNICAST_PEERS 10
#define MAX_MESG 80
#define LOGSIZE 1000
#define MAXPEERS 10


struct record
{
	char key[KEYLEN];
	char value[VALLEN];
	struct timeval timestamp;
	struct sockaddr_in source;
};
typedef struct record record;


struct host_record
{
	struct sockaddr_in address;
	struct timeval time;
	struct sockaddr_in source_address;
};
typedef struct host_record host_record;


// GLOBALS
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
record session_log[LOGSIZE];
int c=0;
host_record peers[MAXPEERS];
int port=DEFAULT_PORT;
int num_peers=0;


void printLog()
{
	int i;
	struct timeval tmp,now;
        gettimeofday(&now,NULL);
	for(i=0;i<c;i++)
	{
           timersub(&now,&(session_log[i].timestamp),&tmp);
	   fprintf(stderr,"%s\t%s\t%ld.%06ld\n",session_log[i].key,session_log[i].value,tmp.tv_sec,tmp.tv_usec);
	}
}

// This function runs in a separate thread and just broadcasts out a beacon, letting other nodes know that it is available
void* pinger(void *arg)
{
	int	sockfd,i;
	struct sockaddr_in	recv_addr;
	char *send_line = NULL; 
	memset(&recv_addr, 0, sizeof(recv_addr));
	recv_addr.sin_family = PF_INET;
	unsigned long broadcast_s_addr;
	broadcast_s_addr= htonl(INADDR_BROADCAST);
	recv_addr.sin_port = htons(port);
	sockfd = socket(PF_INET, SOCK_DGRAM, 0);
	const int broadcast=1;
	if((setsockopt(sockfd,SOL_SOCKET,SO_BROADCAST,&broadcast,sizeof broadcast)))
	{	
		perror("setsockopt - SO_SOCKET ");
		exit(1);
	} 
	while(1)  // loop forever broadcasting your aliveness to the local subnet
	{
		recv_addr.sin_addr.s_addr=broadcast_s_addr;		
		send_ping(sockfd,(struct sockaddr *)&recv_addr); 
		usleep(999999);
	}
}

//  This function prints some information about the state of the network...  known peers and known keys
void* printer(void* arg)
{
	int j;
	while(1)
	{
		system("clear");
		fprintf(stderr,"Host\tstaleness\tsource\n");
		pthread_mutex_lock(&mutex1);   // lock my state table so it doesn't get edited while I'm printing
		for(j=0;j<num_peers;j++)
		{
			struct timeval tmp,now;
			gettimeofday(&now,NULL);
			timersub(&now,&peers[j].time,&tmp);
			char dotted[INET_ADDRSTRLEN];         /* string ip address */
			inet_ntop(PF_INET, (void *)&(peers[j].address.sin_addr.s_addr),dotted, INET_ADDRSTRLEN);
			fprintf(stderr,"%s\t%ld.%06ld\tlocal\n",dotted,tmp.tv_sec,tmp.tv_usec);
		}
		pthread_mutex_unlock(&mutex1);
		printLog();
		usleep(250000);
	}
}



//  this thread does most of the work...  handles incoming packets and responds appropriately.
void* receiver(void* arg)
{
	int sockfd,send_port;
	struct sockaddr_in recv_addr, send_addr;
	int mesglen; char message[MAX_MESG];
	unsigned int send_len;
	char send_dotted[INET_ADDRSTRLEN];
	struct in_addr primary;
	primary.s_addr=INADDR_ANY;
	char primary_dotted[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &primary, primary_dotted, INET_ADDRSTRLEN);
	fprintf(stderr, "Running on %s, port %d\n",primary_dotted, port);

	/* make a socket*/
	sockfd = socket(PF_INET, SOCK_DGRAM, 0);

	memset(&recv_addr, 0, sizeof(recv_addr));
	recv_addr.sin_family      = PF_INET;
	recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	recv_addr.sin_port        = htons(port);

	if (bind(sockfd, (struct sockaddr *) &recv_addr, sizeof(recv_addr))<0)
		perror("bind");

	const int broadcast=1;
	if((setsockopt(sockfd,SOL_SOCKET,SO_BROADCAST, &broadcast,sizeof broadcast)))    // set broadcast flag
	{  
		perror("setsockopt"); exit(1);
	}


	struct packet net; 		  // network order 
	struct packet local; 	  // local order 
	char value[VALLEN]; 

	while(1) {
		send_len = sizeof(send_addr); /* MUST initialize this */
		mesglen = recvfrom(sockfd, &net, sizeof(struct packet), 0, (struct sockaddr *) &send_addr, &send_len);
		if (mesglen>=0) {
			inet_ntop(PF_INET, (void *)&(send_addr.sin_addr.s_addr), send_dotted, INET_ADDRSTRLEN);
			send_port = ntohs(send_addr.sin_port); 
			packet_network_to_local(&local, &net); 

			int found,z;
			switch (local.magic) { 
				case MAGIC_PUT: 
					fprintf(stderr, "%s:%d: put (%s=>{%s})\n",
							send_dotted,send_port,
							local.u.put.key, local.u.put.value);
					rbtree_put(local.u.put.key, local.u.put.value);
					rbtree_print();

					memcpy(&session_log[c].key,&local.u.put.key,KEYLEN);
					memcpy(&session_log[c].value,&local.u.put.value,VALLEN);
					memcpy(&session_log[c].source,&send_addr,sizeof(struct sockaddr));
					gettimeofday(&session_log[c].timestamp,NULL);
					c++;
					send_ack(sockfd, (struct sockaddr *)&send_addr, local.u.put.key, local.u.put.value); 
					break; 
				case MAGIC_GET: 
					fprintf(stderr, "%s:%d: get (%s)\n",
							send_dotted,send_port,
							local.u.get.key); 
					if (rbtree_get(local.u.get.key, value)) { 
						send_got(sockfd, (struct sockaddr *)&send_addr, local.u.get.key, value); 
					} else { 
						send_not(sockfd, (struct sockaddr *)&send_addr, local.u.get.key); 
					} 
					break; 
				case MAGIC_DEL:
					fprintf(stderr,"Got del command for key: %s",local.u.del.key);
					break;
				case MAGIC_PING:
					found=0;
					pthread_mutex_lock(&mutex1);
					for(z=0;z<num_peers;z++)
					{
						if(send_addr.sin_addr.s_addr==peers[z].address.sin_addr.s_addr)
						{
							gettimeofday(&peers[z].time,NULL);
							found=1;
							break;
						}
					}
					if(!found)
					{
						gettimeofday(&peers[num_peers].time,NULL);
						memcpy(&peers[num_peers].address,&send_addr,sizeof(struct sockaddr));
						num_peers++;
					}
					pthread_mutex_unlock(&mutex1);
					break;
				default: 
					fprintf(stderr, "%s:%d: UNKNOWN PACKET MAGIC NUMBER %d\n", 
							send_dotted, send_port, local.magic); 
					break; 
			}
		} else {
			perror("receive failed");
		}
	}
}



//  this function clears out any "stale" peers, which haven't pinged in 1 second.
void* cleaner(void* arg)
{
	struct timeval tmp,now;
	int z;
	while(1){
		pthread_mutex_lock(&mutex1);
		gettimeofday(&now,NULL);
		for(z=0;z<num_peers;z++)
		{
			timersub(&now,&peers[z].time,&tmp);
			if (tmp.tv_sec>=1)  // if this record is too old, purge it
			{
				if(z!=num_peers-1) //if it's not the last in the list, move the last element to fill the hole
				{
					memcpy(&peers[z],&peers[num_peers-1],sizeof(host_record));
				}
				memset(&peers[num_peers-1],0,sizeof(host_record));  // zero the last entry
				num_peers--;	// decrement...
			}
		}
		pthread_mutex_unlock(&mutex1);
		usleep(50000);   // every .05 seconds
	}
}

int main(int argc, char **argv)
{
	int i; 

	int x;
	pthread_t pinger_thread,printer_thread,cleaner_thread,receiver_thread;


	pthread_create(&pinger_thread,NULL,pinger,(void *) &x);
	pthread_create(&printer_thread,NULL,printer,(void *) &x);
	pthread_create(&cleaner_thread,NULL,cleaner,(void *) &x);
	pthread_create(&receiver_thread,NULL,receiver,(void *) &x);


	pthread_join(pinger_thread,NULL);
	pthread_join(receiver_thread,NULL);
	pthread_join(printer_thread,NULL);
	pthread_join(cleaner_thread,NULL);
	return(1);
}	
