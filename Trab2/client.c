/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

#define BUFSIZE 1024

/* 
 * error - wrapper for perror
 */
void error(char *msg) 
{
    perror(msg);
    exit(0);
}

int main(int argc, char **argv) 
{
    int sockfd, portno, n;
    unsigned int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];

    /* check command line arguments */
    if (argc != 3) 
    {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) 
    {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    while(1) 
    {
        printf("-------------------------------\n\n");
        /* get a message from the user */
        bzero(buf, BUFSIZE);
        printf("Enter command:\n");
        if(!fgets(buf, BUFSIZE, stdin))
        {
            break;
        }
        printf("Command: %s\n", buf);
        if(buf[0] == '#') {
            printf("Ignoring command\n");
            continue;
        }


        /* send the message to the server */
        serverlen = sizeof(serveraddr);
        n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &serveraddr, serverlen);
        if (n < 0) 
            error("ERROR in sendto");
        
        /* print the server's reply */
        n = recvfrom(sockfd, buf, 1024, 0, (struct sockaddr *) &serveraddr, &serverlen);
        if (n < 0) 
            error("ERROR in recvfrom");
        //buf[n] = '\0';

        printf("Server reply:\n%s\n\n", buf);

        printf("-------------------------------");

        sleep(2);
    }

    return 0;
}