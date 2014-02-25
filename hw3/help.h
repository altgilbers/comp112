#ifndef __HELP_H__
#define __HELP_H__ 1

/*=============================
  helper routines for Assignment 3: clandestine fabrics  
  =============================*/ 

#include <stdlib.h> 
#include <stdint.h> 
#include <unistd.h> 
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <values.h> 
#include <signal.h> 
#include <openssl/md5.h> 

/*=============================
  protocol definitions 
  =============================*/ 

#define MAXMESSAGE 140		/* file bytes per packet */ 
#define MAXREDIRECTS 12		/* redirects in one command */ 
#define MAXTARGETS 10		/* maximum number of relay targets */ 
#define MAGIC_MESSAGE 1		/* code that packet contains a message */ 
#define MAGIC_REGISTER 2	/* code that packet registers an instance */ 
#define MAGIC_REDIRECT 3	/* code that packet contains a redirect */ 

// one message to be forwarded
struct packet { 
    uint32_t magic; // discriminant determines whether packet is 
                    // message, registration, or redirect 
    union un { 
        struct message { 
	    uint32_t magic; 
	    unsigned char digest[MD5_DIGEST_LENGTH]; 
	    char message[MAXMESSAGE]; 
	} mes; 
        struct registration { 
	    uint32_t unused; 
        } reg; 
	struct redirect { 
	    uint32_t magic; 
	    uint32_t nredirects; 
	    struct sockaddr redirects[MAXREDIRECTS]; 
	} red;
    } u; 
} ; 

/*=============================
   low-level protocol help 
  =============================*/ 

// network order => local order 
extern void packet_network_to_local(struct packet *local, 
	struct packet *net); 
// local order => network order 
extern void packet_local_to_network(struct packet *local, 
	struct packet *net); 

/*=============================
   high-level protocol help 
  =============================*/ 

/* send a message to a server */ 
extern ssize_t send_message(int sockfd, const struct sockaddr *to_addr, 
	const unsigned char *digest, const char *message) ; 

extern ssize_t send_register(int sockfd, const struct sockaddr *to_addr) ; 

extern ssize_t send_redirect(int sockfd, const struct sockaddr *to_addr,
	const struct sockaddr *redir) ; 

/* receive a packet from a server */ 
extern ssize_t recv_packet(int sockfd, struct packet *p,
	struct sockaddr *from_addr); 

/*=============================
  client-specific routines 
  =============================*/ 
extern void client_usage(); 
extern int client_arguments_valid(const char *server_dotted, int server_port); 

/*=============================
   halligan-specific routines
  =============================*/ 

/* read the primary IP address for an ECE/CS host */ 
extern int get_primary_addr(struct in_addr *a); 

#endif /* __HELP_H__ */ 
