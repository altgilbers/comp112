/******************************************************************************* 
ialtgi01 - Ian Altgilbers
Comp 112 HW1

Simple Web Server 

This web server takes one command line argument (the port it will bind to).  

For each client connection made, the server records:
1. timestamp of request
2. ip of client
3. hostname of client (if in DNS)
4. the User-Agent reported
5. the method requested
6. the URI requested
7. the status of the resonse

These are stored in a "rolling database" and the last 10 are displayed.

The server responds to different GET/HEAD requests with different responses...

/ or /status gives a summary of the last 10 requests
/info gives a generic information page
/redirect sends you off to youtube with a 302 redirect
any other GET will get a 404...

If you provide OPTIONS or POST, then I give you a 405 Method Not Allowed..

If you provide any other method, I give you a 400
The code is based off the server2.c code provided in class.

*******************************************************************************/

#define MAX_MESG 8192
#define MAX_ADDR 500
#define NUM_ENTRIES 10

#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h> 
#include <stdlib.h>
#include <unistd.h> 
#include <ctype.h> 
#include <time.h>



enum METHOD{GET,HEAD,POST,OPTIONS};

// sanitize method - take a string and replaces problematic characters with their
// HTML escaped versions.  
void sanitize(char *suspect)
{
	char* subs[3];
	subs[0]="&quot;"; subs[1]="&lt;"; subs[2]="&gt;";
	int i=0, j=0, rep=-1, l, dirty=0;

	while(i<MAX_MESG && suspect[i]!='\0') 
	{
		switch(suspect[i])
		{
			case '\"': rep=0;break;	
			case '<': rep=1;break;	
			case '>': rep=2;break;	
			default: rep=-1;
		}
		if (rep > -1)
		{
			dirty++;
			l=strlen(subs[rep]);
			for(j=MAX_MESG-1; j>i+l-1;j--)
				suspect[j]=suspect[j-l+1];			
			for(j=i;j<i+l;j++)
				suspect[j]=subs[rep][j-i];
		}
		i++;
	}
	if (dirty)
		fprintf(stderr,"WARNING. %d characters escaped\n", dirty);
	return;
}


// struct to keep records of recent connection vitals
struct record{
	char ip[INET_ADDRSTRLEN];
	char host[80];
	char user_agent[1024];
	char method[80];
	char URI[80];
	int status;
	time_t time_stamp;
};
typedef struct record record;

// simple helper to initialize log records
void reset_record(record* R){
	strcpy(R->ip,"undefined");
	strcpy(R->host,"undefined");
	strcpy(R->user_agent,"undefined");
	strcpy(R->method,"undefined");
	strcpy(R->URI,"undefined");
	R->status=-1;
	R->time_stamp=0;
	return;
}
void print_record(record* R){
	fprintf(stderr,"ip:[%s]\n",R->ip);
	fprintf(stderr,"host:[%s]\n",R->host);
	fprintf(stderr,"user_agent:[%s]\n",R->user_agent);
	fprintf(stderr,"method:[%s]\n",R->method);
	fprintf(stderr,"URI:[%s]\n",R->URI);
	fprintf(stderr,"status:[%d]\n",R->status);
	fprintf(stderr,"time_stamp:[%ld]\n",R->time_stamp);

}

int main(int argc, char *argv[])
{
	int sockfd, newsockfd;
	struct sockaddr_in serv_addr;
	int serv_port =9060;
	unsigned int mesglen; int i; 
	struct sockaddr_in cli_addr;		/* raw client address */
	char cli_dotted[MAX_ADDR];		/* message ip address */
	unsigned long cli_ulong;		/* address in binary */ 
	struct hostent *cli_hostent;		/* host entry */

	if(argc != 2)
	{
		fprintf(stderr, " %s usage: %s port\n",argv[0], argv[0]); 
		exit(1); 
	} 
	sscanf(argv[1], "%d", &serv_port); /* read the port number if provided */


	// set up my bookkeeping 
	record state[NUM_ENTRIES];
	for (i=0;i<NUM_ENTRIES;i++){
		reset_record(&state[i]);
	}


	if((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("can't open stream socket");
		exit(1);
	}
	
	// zero, then set our sockaddr_in with appropriate values 
	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = PF_INET;	/* internet domain addressing */ 
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
	serv_addr.sin_port = htons(serv_port); /* port is given one */ 
	
	if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("can't bind local address");
		exit(1);
	}
	  
	listen(sockfd, 5);	// listen for connections, queueing 5

	fprintf(stderr,"%s: server listening on port %d\n", argv[0], serv_port); 
	int j,c=0;
	
	while(1) {    /* loop forever, handling connections as they arrive */

		j=c%NUM_ENTRIES;    // make sure we don't go out of range...
		//fprintf(stderr,"Servicing request number: %d, j=%d\n",c,j);
                reset_record(&state[j]);  // clear old data
		mesglen = sizeof(cli_addr);
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &mesglen);
		  
		if(newsockfd < 0) { perror("can't bind local address"); }

		inet_ntop(PF_INET, (void *)&(cli_addr.sin_addr),  cli_dotted, MAX_MESG); 
		fprintf(stderr, "%s: server: connection from %s\n", argv[0],cli_dotted); 

		cli_ulong = cli_addr.sin_addr.s_addr; 
		cli_hostent = gethostbyaddr((char *)&cli_ulong, sizeof(cli_ulong),PF_INET);

	  	if (cli_hostent) { 
		   fprintf(stderr, "%s: server: host name is %s\n", argv[0],cli_hostent->h_name); 
		} else { 
			fprintf(stderr, "%s: server: no name for host\n", argv[0]); 
		}

		FILE *r = fdopen(newsockfd, "r");   // open socket connection for reading
		FILE *w = fdopen(newsockfd, "w");   // open socket connection for writing (different fd's because I'm not sure about offsets)
		char buffer[8192];
		char OUT[100000];
		int line_number=0;
		int method=-1;
		
		// record time stamp for this connection
		time(&(state[j].time_stamp));
		 printf("recorded time stamp for connection %d: %s\n",c, ctime(&(state[j].time_stamp)));
		strcpy(state[j].ip,cli_dotted);
		if(cli_hostent)
		strcpy(state[j].host,cli_hostent->h_name);
	
		while(1) 	// keep reading uetil we're done with headers
		{

			// echo the request information on server console
			fprintf(stderr,"CLIENT:%s",buffer);

			int d1,d2; 
			fgets(buffer, 8192, r);
			if (line_number==0)  		// first line is key.. it defines the method and URI
			{
				if (!strncmp(buffer,"GET ",4))
				{
			 		fprintf(stderr,"SERVER: GET method detected.\n");
					method=GET;
					strcpy(state[j].method,"GET");
				}
				else if (!strncmp(buffer,"HEAD ",5))
				{
					fprintf(stderr,"SERVER: HEAD method detected.\n");
					method=HEAD;
                                        strcpy(state[j].method,"HEAD");
				}
				else if (!strncmp(buffer,"POST ",5))
				{
					fprintf(stderr,"SERVER: POST method detected.\n");
					method=POST;
                                        strcpy(state[j].method,"POST");
				}
				else if (!strncmp(buffer,"OPTIONS",3))
				{
					fprintf(stderr,"SERVER: OPTIONS method detected.\n");
					method=OPTIONS;
                                        strcpy(state[j].method,"Options");
				}
				else
				{
					fprintf(stderr,"SERVER: Unsupported Method detected\n");
                                        strcpy(state[j].method,"Unsupported");
				}

				// extract the URI.. There's surely a better way to do this.. maybe with strtok...
				d1=0,d2=0;
				char *pch;
				pch=strchr(buffer,' ');
				if (pch!=NULL)  // we found the first space
				{
					d1=pch-buffer;  // location of first space
				 	pch=strchr(pch+1,' ');
					if (pch!=NULL)  // found second space
					{
						d2=pch-buffer;
                                		//fprintf(stderr,"found a space at %d and %d, URI length=%d\n",d1,d2,d2-d1-1);
                              			strncpy(state[j].URI,buffer+d1+1,d2-d1-1);
                                		state[j].URI[d2-d1-1]='\0';
                                		//fprintf(stderr,"URI=[%s]\n",state[j].URI);
					}
				}
				if (pch==NULL)
					strcpy(state[j].URI,"Malformed Request");
			}
			
			//  process headers... 
			if (!strncmp(buffer,"User-Agent:",10))
			{
				sanitize(buffer);
				strncpy(state[j].user_agent,buffer, 1024);
			}
		if((buffer[0]=='\r' && buffer[1]=='\n')||buffer[0]=='\n') // this is an empty line, so the request headers are over..
		break;
		line_number++;
	}
        fprintf(stderr,"Record for state[j]=%d\n",j);
	print_record(&state[j]);

	fprintf(stderr,"Number of headers provided: %d\n\n",line_number);

	// build my OUT buffer, so I can set Content-Length response header properly
	memset((char *)OUT,0,100000);
	
	if(method==GET || method==HEAD)
	{
		if(!strcmp(state[j].URI,"/") || !strcmp(state[j].URI,"/status")){
			state[j].status=200;
	        	sprintf(OUT+strlen(OUT),"<html><head><style type=\"text/css\">\n");
	        	sprintf(OUT+strlen(OUT),"tr:nth-child(odd){ background-color:#eee; }tr:nth-child(even){ background-color:#fff; }");
	        	sprintf(OUT+strlen(OUT),"</style></head><body>\n");
			sprintf(OUT+strlen(OUT),"<h1>You are visitor number %d, since the server was started</h1>",c+1);
			sprintf(OUT+strlen(OUT),"<table>\n");
	       		sprintf(OUT+strlen(OUT),"<thead><tr><th>Req#</th><th>Timestamp</th><th>IP</th><th>hostname</th><th>status</th><th>Method</th><th>URI</th><th>User-Agent</th></tr></thead>\n");
	        	sprintf(OUT+strlen(OUT),"<tbody>\n");
	
	        	for(int k=NUM_ENTRIES;k>0;k--)
      			{
				if(c-NUM_ENTRIES+k < 0) // don't print out negative request numbers
					break;
	        	        int index=(j+k)%NUM_ENTRIES;  // adjust index so the most recent request is first
				//fprintf(stderr,"c=%d,j=%d,k=%d,c-k=%d,index=%d\n",c,j,k,c-k,index);
			        sprintf(OUT+strlen(OUT),"<tr>\n");
	        	        sprintf(OUT+strlen(OUT),"<td>%d</td>",c-NUM_ENTRIES+k+1);
	        	        sprintf(OUT+strlen(OUT),"<td>%s</td>",ctime(&(state[index].time_stamp)));
	        	        sprintf(OUT+strlen(OUT),"<td>%s</td>",state[index].ip);
	        	        sprintf(OUT+strlen(OUT),"<td>%s</td>",state[index].host);
	        	        sprintf(OUT+strlen(OUT),"<td>%d</td>",state[index].status);
	        	        sprintf(OUT+strlen(OUT),"<td>%s</td>",state[index].method);
             			sprintf(OUT+strlen(OUT),"<td>%s</td>",state[index].URI);
             			sprintf(OUT+strlen(OUT),"<td>%s</td>",state[index].user_agent);
             			sprintf(OUT+strlen(OUT),"</tr>\n");
        		}
      			sprintf(OUT+strlen(OUT),"</tbody>\n");
      			sprintf(OUT+strlen(OUT),"</table>\n");
			sprintf(OUT+strlen(OUT),"</body></html>\n");
		}
		else if (!strcmp(state[j].URI,"/info"))  // info page
		{
			state[j].status=200;
			sprintf(OUT+strlen(OUT),"<html><head><title>Information Page</title></head><body>\n");
			sprintf(OUT+strlen(OUT),"<p>You have reached Ian's super sweet web server.</p>\n");
			sprintf(OUT+strlen(OUT),"<p>This page is poorly named, as there is no information here.</p>\n");
			sprintf(OUT+strlen(OUT),"</body></html>\n");
		}
                else if (!strcmp(state[j].URI,"/redirect")) // This URL sends a redirect 
                {
                        state[j].status=302;
                        sprintf(OUT+strlen(OUT),"<html><body>\n");
                        sprintf(OUT+strlen(OUT),"<p>This is a redirect page, which will not be seen!</p>\n");
                        sprintf(OUT+strlen(OUT),"</body></html>\n");
                }
		else	// A GET or a HEAD to an unknown URL, 404 for you.
		{
			state[j].status=404;
			sprintf(OUT+strlen(OUT),"<html><body>\n");
			sprintf(OUT+strlen(OUT),"<h1>Error 404</h1>\n");
			sprintf(OUT+strlen(OUT),"<p>I'm sorry I only recognize GET requests for a very small set of URLs:</p>");
			sprintf(OUT+strlen(OUT),"<p><a href=\"/\">/</a></p>");
			sprintf(OUT+strlen(OUT),"<p><a href=\"/status\">/status</a></p>");
			sprintf(OUT+strlen(OUT),"<p><a href=\"/info\">/info</a></p>");
			sprintf(OUT+strlen(OUT),"<p><a href=\"/redirect\">/redirect</a></p>");
			sprintf(OUT+strlen(OUT),"</body></html>\n");
		}
	}

	else if(method==POST || method==OPTIONS) // I recognize these methods, but didn't implement them, so 405 it is...
	{
		state[j].status=405;
		sprintf(OUT+strlen(OUT),"<html><body>\n");
		sprintf(OUT+strlen(OUT),"<h1>405 Method Not Allowed</h1>\n");
		sprintf(OUT+strlen(OUT),"<p>I'm sorry, I'm a pathetic little webserver, and I only respond to GET and HEAD requests</p>");
		sprintf(OUT+strlen(OUT),"</body></html>\n");
	}
	else  // I didn't recognize the method, so I'll give you a 400
	{
		state[j].status=400;
                sprintf(OUT+strlen(OUT),"<html><body>\n");
                sprintf(OUT+strlen(OUT),"<h1>400 Bad Request</h1>\n");
                sprintf(OUT+strlen(OUT),"<p>I'm sorry, your request may be malformed.  I was unable to determine the HTTP method.</p>");
                sprintf(OUT+strlen(OUT),"</body></html>\n");
	}

	// send headers
	switch(state[j].status){
 		case 200:
			fprintf(w,"HTTP/1.1 200 OK\n");
			fprintf(w,"Content-Type: text/html\n");
			fprintf(w,"Content-Length: %lu\n",strlen(OUT));
			fprintf(w,"\n");
			break;
		case 302:
			fprintf(stderr,"HTTP/1.1 302 Found\n");
			fprintf(w,"HTTP/1.1 302 Found\n");
			fprintf(w,"Location: http://www.youtube.com/watch?v=oHg5SJYRHA0\n");
                        fprintf(w,"\n");
			break;
		case 404:
                        fprintf(w,"HTTP/1.1 404 Not Found\n");
                        fprintf(w,"Content-Type: text/html\n");
                        fprintf(w,"Content-Length: %lu\n",strlen(OUT));
                        fprintf(w,"\n");
			break;
                case 405:
                        fprintf(w,"HTTP/1.1 405 Method Not Allowed\n");
                        fprintf(w,"Content-Type: text/html\n");
                        fprintf(w,"Content-Length: %lu\n",strlen(OUT));
                        fprintf(w,"\n");
                        break;
		default:
			fprintf(w,"HTTP/1.1 400 Bad Request\n");
			fprintf(w,"Content-type: text/html\n\n");
                        fprintf(w,"Content-Length: %lu\n",strlen(OUT));
                        fprintf(w,"\n");
	}
	// send content, unless it's a HEAD request or the response is a 302
	if(method!=HEAD && state[j].status !=302)
		fprintf(w,"%s",OUT);

	// flush and close up
	fflush(w);  // flush it out to the client
	fclose(r);  // close the connection for reading (could be done as soon as we have read \n\n, unless we're dealing with a POST
	fclose(w);  // we're done writing, so we close
	close(newsockfd);
	// next!
	c++;
	}  
}
