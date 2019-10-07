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

class Botnet_server
{
public:
    int sock; // socket of server connection
    string group_id;

    Botnet_server(int socket) : sock(socket) {}
    Botnet_server(int socket, string group_id)
    {
        this->sock = socket;
        this->group_id = group_id;
    }

    ~Botnet_server() {} // Virtual destructor defined for base class
};

void establish_server(char *port, int &listeningSocket, int &maxfds, fd_set &open_sockets)
{
    struct addrinfo hints, *servinfo, *p;
    int yes = 1;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
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

    if (listen(listeningSocket, BACKLOG) < 0)
    {
        printf("Listen failed on port %s\n", port);
        exit(0);
    }

    FD_SET(listeningSocket, &open_sockets);

    maxfds = listeningSocket;
    printf("Listening on port: %s\n", port);
}

void wait_for_client_connection(int listeningSocket, int &clientSocket, int &maxfds, fd_set &open_sockets)
{
    struct sockaddr_storage client_addr; // connector's address information
    socklen_t sin_size;
    sin_size = sizeof(client_addr);

    printf("Waiting for client to connect\n");

    clientSocket = accept(listeningSocket, (struct sockaddr *)&client_addr, &sin_size);
    if (clientSocket < 0)
    {
        printf("failed to accept from client\n");
        exit(0);
    }

    maxfds = max(maxfds, clientSocket);
    FD_SET(clientSocket, &open_sockets);

    printf("client connected successfully\n");
}

/*
* this function waits for beginnig connection to the botnet
* waits for client to send a valid CONNECTTO,<ip>,<port> command
* extracts the ip and port and returns it via output parameters.
*/
int client_botnet_connect_cmd(int clientSock, string &outIp, int &outPort)
{
    printf("waiting for client server connect command: CONNECTTO,<server_ip>,<port>\n");

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
* incrament botner-server connection count
*/
int connect_to_botnet_server(string botnet_ip, int botnet_port, map<int, Botnet_server *> &botnet_servers, int &maxfds, fd_set &open_sockets)
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

    botnet_servers[socketfd] = new Botnet_server(socketfd);

    maxfds = max(maxfds, socketfd);
    FD_SET(socketfd, &open_sockets);
}

/*
* Accepts incoming connection.
* Adds socket to botnet_server map
*/
void new_connections(int listenSocket, int maxfds, fd_set &open_sockets, map<int, Botnet_server *> &botnet_servers)
{
    struct sockaddr_in server_addr;
    socklen_t server_addr_len;

    int server_socket = accept(listenSocket, (struct sockaddr *)&server_addr,
                               &server_addr_len);

    if (server_socket < 0)
    {
        perror("error in accepting new connection\n");
    }
    else
    {
        FD_SET(server_socket, &open_sockets);

        botnet_servers[server_socket] = new Botnet_server(server_socket);

        // And update the maximum file descriptor
        maxfds = max(maxfds, server_socket);
        printf("botnet-server connected on server: %d\n", server_socket);
    }
}

// Close a botnet server's connection, remove it from the botnet_server list, and
// tidy up select sockets afterwards.

void close_botnet_server(int botnet_server_sock, fd_set &open_sockets, map<int, Botnet_server *> &botnet_servers, int &maxfds)
{
    // Remove server from the botnet server list
    botnet_servers.erase(botnet_server_sock);

    // If this botnet-server's socket is maxfds then the next lowest
    // one has to be determined. Socket fd's can be reused by the Kernel,
    // so there aren't any nice ways to do this.

    if (maxfds == botnet_server_sock)
    {
        for (auto const &p : botnet_servers)
        {
            maxfds = max(maxfds, p.second->sock);
        }
    }

    // And remove from the list of open sockets.
    FD_CLR(botnet_server_sock, &open_sockets);
}

int server_commands(map<int, Botnet_server *> &botnet_servers, fd_set &open_sockets, fd_set &read_sockets, int &maxfds)
{
    for (auto const &pair : botnet_servers)
    {
        Botnet_server *botnet_server = pair.second;

        if (FD_ISSET(botnet_server->sock, &read_sockets))
        {
            char buffer[BUFFERSIZE];
            memset(buffer, 0, BUFFERSIZE);

            // recv() == 0 means client has closed connection
            if (recv(botnet_server->sock, buffer, sizeof(buffer), MSG_DONTWAIT) == 0)
            {
                printf("Botnet server closed connection: %d", botnet_server->sock);
                close(botnet_server->sock);

                close_botnet_server(botnet_server->sock, open_sockets, botnet_servers, maxfds);
            }
            // We don't check for -1 (nothing received) because select()
            // only triggers if there is something on the socket for us.
            else
            {
                cout << buffer << endl;
            }
        }
    }
}

int client_commands(int& clientSocket)
{
    char buffer[BUFFERSIZE];
    memset(buffer, 0, BUFFERSIZE);

    // recv() == 0 means client has closed connection
    if (recv(clientSocket, buffer, sizeof(buffer), MSG_DONTWAIT) == 0)
    {
        printf("Client closed connection: %d", clientSocket);
        close(clientSocket);
        clientSocket = -1;
    }
    // We don't check for -1 (nothing received) because select()
    // only triggers if there is something on the socket for us.
    else
    {
        cout << buffer << endl;
    }
}

int main(int argc, char *argv[])
{
    int listenSocket;                         // Socket for connections to server
    int clientSock;                           // Socket of connecting client
    fd_set open_sockets;                      // Current open sockets
    fd_set read_sockets;                      // Exception socket list
    fd_set except_sockets;                    // Exception socket list
    int maxfds;                               // Passed to select() as max fd in set
    map<int, Botnet_server *> botnet_servers; // Lookup table for per Client information

    if (argc != 2)
    {
        printf("Usage: chat_server <ip port>\n");
        exit(0);
    }

    // Setup socket for server to listen to connections
    char *port = argv[1];
    // establish listening socket, initalize maxfds, add to open_sockets
    establish_server(port, listenSocket, maxfds, open_sockets);

    // wait for the client to connect to the server, set clientSock, maybe change maxfds, add to open_sockets
    wait_for_client_connection(listenSocket, clientSock, maxfds, open_sockets);

    // get ip and port of first server to connect to
    string botnet_server_ip;
    int botnet_server_port;
    client_botnet_connect_cmd(clientSock, botnet_server_ip, botnet_server_port);

    // connect to botnet, maybe change maxfds, add to open_sockets, add to botnet_servers
    connect_to_botnet_server(botnet_server_ip, botnet_server_port, botnet_servers, maxfds, open_sockets);

    while (true)
    {
        // Get modifiable copy of readSockets
        read_sockets = except_sockets = open_sockets;

        // Look at sockets and see which ones have something to be read()
        int n = select(maxfds + 1, &read_sockets, NULL, &except_sockets, NULL);

        if (n < 0)
        {
            perror("select failed - closing down\n");
            break;
        }

        // if there is an incoming connection and we have room for more connections.
        if (FD_ISSET(listenSocket, &read_sockets) && botnet_servers.size < 5)
        {
            // accepts connection, maybe changes maxfds, add to botnet_servers
            new_connections(listenSocket, maxfds, open_sockets, botnet_servers);
            n--;
        }

        if (FD_ISSET(clientSock, &read_sockets))
        {
            // check for commands from client
            client_commands(clientSock);
            n--;
        }

        while (n-- > 0)
        {
            // check for commands from connected servers
            server_commands(botnet_servers, open_sockets, read_sockets, maxfds);
        }
    }
}