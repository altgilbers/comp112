#include <stdio.h> 
#include <openssl/md5.h>
#include <string.h>

//  unsigned char *MD5(const unsigned char *d, unsigned long n,
//                    unsigned char *md);
// 
//  int MD5_Init(MD5_CTX *c);
//  int MD5_Update(MD5_CTX *c, const void *data,
//                          unsigned long len);
//  int MD5_Final(unsigned char *md, MD5_CTX *c);

main() { 

    unsigned char signature[MD5_DIGEST_LENGTH]; 
    char *stuff = "this is a test\n"; 
    int i; 
    MD5((const unsigned char *)stuff, strlen(stuff), signature); 
    for (i=0; i<16; i++) { 
	printf("%2x",signature[i]); 
    } 
    printf("\n"); 
} 
