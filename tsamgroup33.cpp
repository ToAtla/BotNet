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
#include <sstream>

#define BACKLOG 5 // how many pending connections queue will hold
#define CLIENTBUFFERSIZE 1025
int VERBOSE = 1;
std::string CLIENT_PREFIX = "CLI::";

// hardcoded global strings
std::string MY_IP = "130.243.201.63";      // skel ip address (we are only running the server on skel for now)
std::string MY_GROUP_NAME = "tsamgroup33"; // our groupname won't change a lot

void fatal(std::string message)
{
    std::cout << message << std::endl;
    exit(0);
}

/*
*   Sends a message through the given socket
*/
int reply_through_socket(const int communicationSocket, const std::string message)
{
    if (send(communicationSocket, message.c_str(), message.size(), 0) < 0)
    {
        fatal("Replying failed");
    }
    return 0;
}

/*
*   Tries to find matching server command to execute
*   then executes it
*/
int execute_server_command(const int communicationSocket, std::string command)
{
    // TODO
    // Maybe add SOH....EOT quality check here
    // Then proceed with command parsing

    //splitting command stream into word, seperated by ","
    std::istringstream command_stream(command);
    std::string word;
    std::getline(command_stream, word, ',');

    if (word == "LISTSERVERS")
    {
        std::cout << "listing servers" << std::endl;
        // the command has to be coming from a connected server
        reply_through_socket(communicationSocket, "No Servers currently connected to me, apart from you");
    }
    else
    {
        std::cout << "Command not recognized" << std::endl;
    }

    return 0;
}

/*
*   Tries to find a matching client command to execute
*   then executes it
*/
int execute_client_command(const int communicationSocket, std::string command)
{
    command = command.substr(CLIENT_PREFIX.size()); // removing CLI:: prefix
    //splitting command stream into word, seperated by ","
    std::istringstream command_stream(command);
    std::string word;
    std::getline(command_stream, word, ',');
    if (word == "LISTSERVERS")
    {
        std::cout << "listing servers" << std::endl;
        // the command is coming from the client
        reply_through_socket(communicationSocket, "No Servers currently connected to me at all");
    }
    else if (word == "CONNECT")
    {

        reply_through_socket(communicationSocket, "Connecting to specified server\n");
        try
        {
            // Form:
            // CONNECT,<IP>,<PORT>
            std::string ip;
            std::getline(command_stream, ip, ',');
            std::string port;
            std::getline(command_stream, port, ',');
            char SOH = 1;
            char EOT = 4;
            std::string connect_message;
            connect_message.push_back(SOH);
            connect_message += "CONNECT," + ip + "," + port;
            connect_message.push_back(EOT);
            std::cout << connect_message << std::endl;

            // TODO
            // Figure out how to send this message to the specified ip through our listening port

            reply_through_socket(communicationSocket, "Connected to the given server... theoretically\n");
        }
        catch (const std::exception &e)
        {
            std::cerr << e.what() << '\n';
            reply_through_socket(communicationSocket, "Failed to connect this server to specified server");
        }
    }
    else
    {
        std::cout << "Command not recognized" << std::endl;
        reply_through_socket(communicationSocket, "Command not recognized");
        }

    return 0;
}

int is_client_command(const std::string command)
{
    bool is_server_command = command.rfind(CLIENT_PREFIX, 0);
    // To seperate client and server commands
    // we assume that client commands are prefixed with a CLIENT_PREFIX
    if (is_server_command)
    {
        if (VERBOSE)
        {
            std::cout << "Server command recieved" << std::endl;
        }
        return 0;
    }
    else
    {
        if (VERBOSE)
        {

            std::cout << "Client command received" << std::endl;
        }
        return 1;
    }
    return 0;
}

/*
* Accepts a socket as a parameter
* Opens a communication socket and returns it 
* The socket needs to be closed by the calling process
*/
int accept_command(const int listeningSocket, int &communicationSocket, char *buffer)
{

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
    if (VERBOSE)
    {
        printf("%i bytes recieved\n", n);
        printf("%s\n", buffer);
    }
    return 0;
}
/*
*   Sets up a listening socket to listen to client and server commands
*/
int establish_server(char *port, int &listeningSocket)
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

int run_server(char *port)
{
    int listeningSocket;
    establish_server(port, listeningSocket);

    // --------------------The following bit can be threaded
    int communicationSocket;
    char buffer[CLIENTBUFFERSIZE];
    memset(buffer, 0, sizeof(buffer));

    accept_command(listeningSocket, communicationSocket, buffer);
    if (is_client_command(buffer))
    {
        execute_client_command(communicationSocket, buffer);
    }
    else
    {
        execute_server_command(communicationSocket, buffer);
    }
    close(communicationSocket);
    // --------------------Threading section ends
    return 0;
}

int main(int argc, char *argv[])
{
    VERBOSE = 1;
    if (argc != 2)
    {
        std::cout << "Usage: ./serverfile <portnumber>" << std::endl;
    }
    char *port = argv[1];
    run_server(port);

    return 0;
}