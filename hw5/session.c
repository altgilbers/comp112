#include "help.h" 

#define TRUE  1
#define FALSE 0

#define DEFAULT_PORT 9960
#define MAX_UNICAST_PEERS 10
#define MAX_MESG 80

int port=DEFAULT_PORT;


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
				send_line = "my message"; 
				sockfd = socket(PF_INET, SOCK_DGRAM, 0);
				const int broadcast=1;
				if((setsockopt(sockfd,SOL_SOCKET,SO_BROADCAST,&broadcast,sizeof broadcast)))
				{	
								perror("setsockopt - SO_SOCKET ");
								exit(1);
				} 
				sleep(10);
				while(1)  // loop forever broadcasting your aliveness to the local subnet
				{
								recv_addr.sin_addr.s_addr=broadcast_s_addr;		
								if (sendto(sockfd, send_line, strlen(send_line), 0, (struct sockaddr *)&recv_addr, sizeof(recv_addr))==(-1)) 
												perror("sendto"); 
								sleep(10);
				}
}


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


												switch (local.magic) { 
																case MAGIC_PUT: 
																				fprintf(stderr, "%s:%d: put (%s=>{%s})\n",
																												send_dotted,send_port,
																												local.u.put.key, local.u.put.value);
																				rbtree_put(local.u.put.key, local.u.put.value);
																				rbtree_print(); 
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

int main(int argc, char **argv)
{
				int i; 

				int x;
				pthread_t pinger_thread,printer_thread,cleaner_thread,receiver_thread;
				pthread_create(&pinger_thread,NULL,pinger,(void *) &x);
				pthread_create(&receiver_thread,NULL,receiver,(void *) &x);
				pthread_join(pinger_thread,NULL);
				pthread_join(receiver_thread,NULL);
				return(1);
}	
