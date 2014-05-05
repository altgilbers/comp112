#ifndef __HELP_H__
#define __HELP_H__ 1

/*=============================
  helper routines for Assignment 5: strong consistency  
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

#ifdef __linux
	#include <values.h> 
#elif __APPLE__
	#include <limits.h>
#endif

#include <signal.h> 
#include <openssl/md5.h> 

/*=============================
  protocol definitions 
  =============================*/ 

#define MAGIC_PUT 1	/* put a key/value pair */ 
#define MAGIC_ACK 2	/* put ack: that it was put */ 
#define MAGIC_GET 3	/* get a key/value pair */ 
#define MAGIC_GOT 4	/* get ack: found it */ 
#define MAGIC_NOT 5	/* get nak: no value */ 
#define MAGIC_DEL 6	/* Extra credit: delete a key/value pair */ 
#define MAGIC_DAK 7	/* Extra credit: ack that it was deleted */ 
#define MAGIC_PING 8	/* I'm alive! */
#define MAGIC_SPUT 9 /* Server put... includes timestamp  */
#define KEYLEN 128	/* key length */ 
#define VALLEN 1024	/* value length */ 

// one message to be forwarded
struct packet { 
    uint32_t magic; // discriminant determines whether packet is 
                    // put, (put) ack, get, or got (get ack). 
    union un { 
	struct put { // when magic==MAGIC_PUT
	    char key[KEYLEN]; 
	    char value[VALLEN]; 
        } put; 
	struct ack { // when magic==MAGIC_ACK
	    char key[KEYLEN]; 
	    char value[VALLEN]; 
        } ack; 
        struct get { // when magic==MAGIC_GET
	    char key[KEYLEN]; 
	} get; 
	struct got { // when magic==MAGIC_GOT
	    char key[KEYLEN]; 
	    char value[VALLEN]; 
        } got; 
        struct not { // when magic==MAGIC_NOT
	    char key[KEYLEN]; 
	} not; 
        struct del { // when magic==MAGIC_DEL
	    char key[KEYLEN]; 
	} del; 
        struct dak { // when magic==MAGIC_DAK
	    char key[KEYLEN]; 
	} dak;

////////   My discriminants ////
        struct ping { // when magic==MAGIC_DAK
            struct timeval t;
        } ping;
	struct sput {
	   char key[KEYLEN];
	   char value[VALLEN];
	   struct timeval t;
	} sput;
	 
    } u; 
} ; 

/*=============================
   low-level protocol help 
  =============================*/ 

// network order => local order 
extern void packet_network_to_local(struct packet *local, const struct packet *net); 
// local order => network order 
extern void packet_local_to_network(const struct packet *local, struct packet *net); 

/*=============================
   high-level protocol help 
  =============================*/ 


// my request 
extern ssize_t send_put(int sockfd, const struct sockaddr *to_addr, 
	const char *key, const char *value) ; 
// your response 
extern ssize_t send_ack(int sockfd, const struct sockaddr *to_addr, 
	const char *key, const char *value) ; 

// my request 
extern ssize_t send_get(int sockfd, const struct sockaddr *to_addr, 
	const char *key) ; 
// your responses
extern ssize_t send_got(int sockfd, const struct sockaddr *to_addr, 
	const char *key, const char *value) ; 
extern ssize_t send_not(int sockfd, const struct sockaddr *to_addr, 
	const char *key) ; 

// Extra credit: my request 
extern ssize_t send_del(int sockfd, const struct sockaddr *to_addr, 
	const char *key) ; 
// Extra credit: your response 
extern ssize_t send_dak(int sockfd, const struct sockaddr *to_addr, 
	const char *key) ; 


extern ssize_t send_ping(int sockfd, const struct sockaddr *to_addr);

extern ssize_t send_sput(int sockfd, const struct sockaddr *to_addr,
        const char *key, const char *value, const struct timeval *t) ;

/* receive a packet from a server */ 
extern ssize_t recv_packet(int sockfd, struct packet *p,
	struct sockaddr *from_addr); 

/*=============================
  client-specific routines 
  =============================*/ 
extern void session_usage(); 
extern int session_arguments_valid(int server_port); 

/*=============================
   halligan-specific routines
  =============================*/ 
/* read the primary IP address for an ECE/CS host */ 
extern int get_primary_addr(struct in_addr *a); 

#endif /* __HELP_H__ */ 
