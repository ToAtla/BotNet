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
#include <string>
#define BACKLOG 5  // how many pending connections queue will hold
#define CLIENTBUFFERSIZE 1025
int VERBOSE = 1;

void fatal(std::string message){
    std::cout << message << std::endl;
    exit(0);
}

int reply_through_socket(int communicationSocket, std::string message){
    if( send(communicationSocket, message.c_str(), message.size(), 0) < 0){
        fatal("Replying failed");
    }
    return 0;
}
/*
* Accepts a socket as a parameter
* Opens a communication socket and returns it 
* The socket needs to be closed by the calling process
*/
int accept_commands(const int listeningSocket, int &communicationSocket, char *buffer){

    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    sin_size = sizeof their_addr;
    
    printf("Accepting\n");
    
    communicationSocket = accept(listeningSocket, (struct sockaddr *)&their_addr, &sin_size);
    if (communicationSocket == -1)
    {
        printf("accept");
    }
    printf("Accepted\n");
    
    int n = recv(communicationSocket, buffer, CLIENTBUFFERSIZE, 0);
    buffer[CLIENTBUFFERSIZE - 1] = '\0';
    if(VERBOSE){
        printf("%i bytes recieved\n", n);
        printf("%s\n", buffer);
    }
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

    // --------------------The following bit can be threaded
    int communicationSocket;
    char buffer[CLIENTBUFFERSIZE];
    memset(buffer, 0, sizeof(buffer));

    accept_commands(listeningSocket, communicationSocket, buffer);
    
    reply_through_socket(communicationSocket, "Got your command");
    close(communicationSocket);
    // --------------------Threading section ends
    return 0;
}


int main(int argc, char* argv[])
{
    VERBOSE = 1;
    if(argc != 2){
        std::cout << "Usage: ./serverfile <portnumber>" << std::endl;
    }
    char *port = argv[1];
    run_server(port);

    return 0;
}