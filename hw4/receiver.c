/*
 * Comp112 HW4
 * Ian Altgilbers
 *
 * This program takes a port number on which to communicate and optionally 
 * a list of remote hosts to exchange information with.  The program broadcasts
 * to the local subnet, advertising that it is running the "service".  It employs
 * 4 threads:
 * pinger - sends out the broadcast
 * printer - prints out the state table
 * cleaner - cleans out stale entries
 * receiver - listens for other peers
 *
 * I didn't have time to finish the multi-subnet extension
 */ 


#include <stdlib.h> 
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/time.h>
#include <pthread.h>
#include <ifaddrs.h>

struct host_record
{
	struct sockaddr_in address;
	struct timeval time;
        struct sockaddr_in source_address;
};
typedef struct host_record host_record;

#define DEFAULT_PORT 9960
#define MAX_UNICAST_PEERS 10
#define MAX_MESG 80

int port=DEFAULT_PORT;
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
host_record peers[100];
struct sockaddr_in unicast_peers[MAX_UNICAST_PEERS];
int num_peers_unicast;
int num_peers=0;




//  This function is called as a thread and simply pings out on the local subnet's broadcast address
//  I define "local" as eth0

void* pinger(void *arg)
{
	int	sockfd,i;
	struct sockaddr_in	recv_addr;
	char *send_line = NULL; 

	memset(&recv_addr, 0, sizeof(recv_addr));
	recv_addr.sin_family = PF_INET;

	int broadcast_set=0;   
	struct ifaddrs *ifap, *ifa;
	struct sockaddr_in *sa;
	unsigned long broadcast_s_addr;
	char *addr;
	getifaddrs (&ifap);    // get interface addresses
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {	// iterate through
		if (strncmp("eth0",ifa->ifa_name,4)==0){	// if we're looking at eth0
			if (ifa->ifa_addr->sa_family==AF_INET) {   //  and it's an IPv4 address
				sa = (struct sockaddr_in *) ifa->ifa_addr;
				addr = inet_ntoa(sa->sin_addr);
				printf("Interface: %s\tAddress: %s\n", ifa->ifa_name, addr);
				sa = (struct sockaddr_in *) ifa->ifa_broadaddr;
				addr = inet_ntoa(sa->sin_addr);
				printf("Interface: %s\tBroadcast: %s\n", ifa->ifa_name, addr);

				recv_addr.sin_addr.s_addr=sa->sin_addr.s_addr; 
				broadcast_s_addr=sa->sin_addr.s_addr;
				broadcast_set=1;
			}
		}
	}


	if(!broadcast_set){
		fprintf(stderr,"Couldn't find broadcast address for eth0, so using INADDR_BROADCAST\n");
		broadcast_s_addr= htonl(INADDR_BROADCAST);
	}
	recv_addr.sin_port = htons(port);
	send_line = "my message"; 

	sockfd = socket(PF_INET, SOCK_DGRAM, 0);
	const int broadcast=1;
	if((setsockopt(sockfd,SOL_SOCKET,SO_BROADCAST,&broadcast,sizeof broadcast)))
	{
		perror("setsockopt - SO_SOCKET ");
		exit(1); 
	} 

	while(1)  // loop forever broadcasting your aliveness to the local subnet
	{

		// send to broadcast address
		recv_addr.sin_addr.s_addr=broadcast_s_addr;		
		if (sendto(sockfd, send_line, strlen(send_line), 0, (struct sockaddr *)&recv_addr, sizeof(recv_addr))==(-1)) 
			perror("sendto"); 
		for (i=0;i<num_peers_unicast;i++)
		{
			recv_addr.sin_addr.s_addr=unicast_peers[i].sin_addr.s_addr;
			if (sendto(sockfd, send_line, strlen(send_line), 0, (struct sockaddr *)&recv_addr, sizeof(recv_addr))==(-1))
                        	perror("sendto");
		}
		sleep(1);
	}
}


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
		usleep(125000);
	}
}



//  this function cleans my table of known peers.   If a peer hasn't checked in with 5 seconds
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
			if (tmp.tv_sec>=5)  // if this record is too old, purge it
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


void* receiver(void* arg)
{
    int sockfd;
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
   for ( ; ; ) {
      send_len = sizeof(send_addr); /* MUST initialize this */
      mesglen = recvfrom(sockfd, message, MAX_MESG, 0, (struct sockaddr *) &send_addr, &send_len);
      if (mesglen>=0) {
         inet_ntop(PF_INET, (void *)&(send_addr.sin_addr.s_addr), send_dotted, INET_ADDRSTRLEN);
         message[mesglen]='\0';
         //  check and see if this is an existing host..  if so, update it's timestamp otherwise add an entry
         int found=0;
         int z;
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
      } else {
         perror("receive failed");
      }
   }
}


int main(int argc, char **argv)
{
	pthread_t pinger_thread,printer_thread,cleaner_thread,receiver_thread;
	int i,x,recv_port;

	if (argc < 2) { 
		fprintf(stderr, "%s usage: %s port [IPs]\n", argv[0], argv[0]); 
		exit(1);
	} 


	recv_port=atoi(argv[1]);
	port=recv_port; 
	if (recv_port<9000 || recv_port>32767) { 
		fprintf(stderr, "%s: port %d is not allowed\n", argv[0], recv_port); 
		exit(1); 
	} 
	char *target_dotted;
	for(i=0;i<argc-2;i++)
	{
		fprintf(stderr,"Setting ip for %d to %s\n",i,argv[i+2]);
		target_dotted = argv[i+2];
		inet_pton(AF_INET, target_dotted, &(unicast_peers[i].sin_addr));
		num_peers_unicast++;
	}

	for(i=0;i<num_peers_unicast;i++)
	{
		fprintf(stderr,"unicast peer: %d\t-\t%lu\n",i,unicast_peers[i].sin_addr.s_addr);
	}
	//sleep(3);	
	pthread_create(&pinger_thread,NULL,pinger,(void *) &x);
	pthread_create(&printer_thread,NULL,printer,(void *) &x);
	pthread_create(&cleaner_thread,NULL,cleaner,(void *) &x);
	pthread_create(&receiver_thread,NULL,receiver,(void *) &x);

	pthread_join(pinger_thread,NULL);
	pthread_join(printer_thread,NULL);
	pthread_join(cleaner_thread,NULL);
	pthread_join(receiver_thread,NULL);
	return(1);
}	
