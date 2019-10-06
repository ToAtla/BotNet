//
// Simple chat server for TSAM-409
//
// Command line: ./chat_server 4000
//
// Author: Jacky Mallett (jacky@ru.is)
//
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <vector>
#include <regex>
#include <string>
#include <iostream>
#include <sstream>
#include <map>

using namespace std;

#define BACKLOG 5 // Allowed length of queue of waiting connections
#define BUFFERSIZE 1025

string OUR_GROUP_ID = "P3_GROUP_75";

int establish_server(char *port)
{
    int listeningSocket;
    struct addrinfo hints, *servinfo, *p;
    int yes = 1;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // TODO: should this be unspecified?
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) // TODO: what does this do?
    {
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

    return listeningSocket;
}

int wait_for_client_connection(int listeningSocket)
{
    struct sockaddr_storage client_addr; // connector's address information
    socklen_t sin_size;
    sin_size = sizeof(client_addr);
    int clientSocket;

    printf("Waiting for client to connect\n");

    clientSocket = accept(listeningSocket, (struct sockaddr *)&client_addr, &sin_size);

    return clientSocket;
}

/*
* this function waits for beginnig connection to the botnet
* waits for client to send a valid CONNECTTO,<ip>,<port> command
* extracts the ip and port and returns it via output parameters.
*/
int client_botnet_connect_cmd(int clientSock, string &outIp, int &outPort)
{
    char server_connect_command[BUFFERSIZE];
    memset(server_connect_command, 0, BUFFERSIZE);
    // while we dont have a valid connection command
    while (true)
    {
        int byteCount = recv(clientSock, server_connect_command, BUFFERSIZE, 0);
        if (byteCount < 0)
        {
            cout << "failed to receive from client" << endl;
            exit(0);
        }
        server_connect_command[BUFFERSIZE - 1] = '\0';

        string string_command(server_connect_command);                  // char array to string
        regex e("(CONNECTTO,)(\\d{1,3}(\\.\\d{1,3}){3})(,)(\\d{1,4})"); // regular expression for CONNECTTO,<ip>,<portnr>
        if (regex_match(string_command, e))
        {
            // extract ip and port
            size_t firstCommaIndex = string_command.find(",", 0);
            size_t secondCommaIndex = string_command.find(",", firstCommaIndex + 1);

            outIp = string_command.substr(firstCommaIndex + 1, secondCommaIndex - firstCommaIndex - 1);
            outPort = atoi(string_command.substr(secondCommaIndex + 1).c_str());
            break;
        }
        else
        {
            // send to client and try again
            string message("Begin by connecting server to botnet server");
            send(clientSock, message.c_str(), message.length(), 0);
        }
    }
}

/*
* connects to server and returns socket file descriptor
*/
int connect_to_botnet_server(string botnet_ip, int botnet_port)
{
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd < 0)
    {
        perror("Failed to open socket");
        return (-1);
    }

    struct sockaddr_in server_socket_addr;                             // address of botnet server
    memset(&server_socket_addr, 0, sizeof(server_socket_addr));        // Initialise memory
    server_socket_addr.sin_family = AF_INET;                           // pv4
    server_socket_addr.sin_addr.s_addr = inet_addr(botnet_ip.c_str()); // bind to server ip
    server_socket_addr.sin_port = htons(botnet_port);                  // portno

    // connect to server
    if (connect(socketfd, (struct sockaddr *)&server_socket_addr, sizeof(server_socket_addr)) < 0)
    {
        perror("Failed to connect");
        return (-1);
    }

    // testing send to server
    string message = "";
    char SOH = 1;
    char EOT = 1;
    message = SOH + "LISTSERVERS," + OUR_GROUP_ID + EOT;

    send(socketfd, message.c_str(), message.length(), 0);

    char response[5000];
    int byteCount = recv(socketfd, response, 5000, 0);

    response[byteCount] = '\0';
    cout << response << endl;

    return socketfd;
}

int new_connections()
{
}

int check_for_server_commands()
{
}

int check_for_client_commands()
{
}

int main(int argc, char *argv[])
{
    int listenSocket;      // Socket for connections to server
    int clientSock;        // Socket of connecting client
    fd_set botnet_servers; // connected botnet servers

    if (argc != 2)
    {
        printf("Usage: chat_server <ip port>\n");
        exit(0);
    }

    // Setup socket for server to listen to
    char *port = argv[1];
    listenSocket = establish_server(port);
    // TODO: fallegra ef þetta er partur af establish_server?:
    if (listen(listenSocket, BACKLOG) < 0)
    {
        printf("Listen failed on port %s\n", port);
        exit(0);
    }

    printf("Listening on port: %s\n", port);

    // wait for the client to connect to the server
    clientSock = wait_for_client_connection(listenSocket);
    // TODO: fallegra ef þetta er partur af wait_for_client_connection?:
    if (clientSock < 0)
    {
        printf("failed to accept from client\n");
        exit(0);
    }

    printf("client connected successfully\n");
    printf("waiting for client server connect command: CONNECTTO,<server_ip>,<port>\n");

    // get ip and port of first server to connect to
    string botnet_server_ip;
    int botnet_server_port;
    client_botnet_connect_cmd(clientSock, botnet_server_ip, botnet_server_port);

    // connect to botnet
    int server_socket = connect_to_botnet_server(botnet_server_ip, botnet_server_port);
    FD_SET(server_socket, &botnet_servers);

    while (true)
    {
        // check for new connections
        new_connections();

        // check for commands from connected servers
        check_for_server_commands();

        // check for commands from client
        check_for_client_commands();
    }
}