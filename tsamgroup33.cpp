/*
** A server that listens for TCP connections and excecuted recognized commands
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <iostream>

#define BACKLOG 5  // how many pending connections queue will hold
#define CLIENTBUFFERSIZE 1025


int accept_commands(int listeningSocket){

    int new_fd;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    sin_size = sizeof their_addr;
    char buffer[CLIENTBUFFERSIZE];
    memset(buffer, 0, sizeof(buffer));
    printf("Accepting\n");
    
    new_fd = accept(listeningSocket, (struct sockaddr *)&their_addr, &sin_size);
    if (new_fd == -1)
    {
        printf("accept");
    }
    printf("Accepted\n");
    
    int n = recv(new_fd, buffer, CLIENTBUFFERSIZE, 0);
    printf("%i bytes recieved\n", n);
    buffer[CLIENTBUFFERSIZE - 1] = '\0';
    printf("%s\n", buffer);
    close(new_fd); // parent doesn't need this
    return 0;
}

int establish_server(char *port, int &listeningSocket){
    struct addrinfo hints, *servinfo, *p;
    int yes=1;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ( (rv = getaddrinfo(NULL, port, &hints, &servinfo) ) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
   

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((listeningSocket = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
            perror("server: socket");
            continue;
        }
        if (setsockopt(listeningSocket, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1)
        {
            perror("setsockopt");
            exit(1);
        }
        if (bind(listeningSocket, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(listeningSocket);
            perror("server: bind");
            continue;
        }
        break;
    }
    freeaddrinfo(servinfo); // all done with this structure
    if (p == NULL)
    {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }
    if (listen(listeningSocket, BACKLOG) == -1)
    {
        perror("listen");
        exit(1);
    }
    
    printf("server: waiting for connections...\n");

    
    
    return 0;
}

int run_server(char *port){
    int listeningSocket;
    establish_server(port, listeningSocket);
    accept_commands(listeningSocket);
    //reply_to_command();
    return 0;
}


int main(int argc, char* argv[])
{
    if(argc != 2){
        std::cout << "Usage: ./serverfile <portnumber>" << std::endl;
    }
    char *port = argv[1];
    run_server(port);

    return 0;
}