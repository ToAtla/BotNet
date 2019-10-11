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
#include <string>
#include <iostream>
#include <sstream>
#include <map>
#include <chrono>

using namespace std;

#define BACKLOG 5             // Allowed length of queue of waiting connections
#define MAXCONNECTEDSERVERS 5 // Allowed amount of connected external servers
#define BUFFERSIZE 1025

string OUR_GROUP_ID = "P3_GROUP_75";
string STANDIN_GROUPID = "XXX";
int OUR_PORTNR;
int OUR_IP;
bool VERBOSE = false;
char SOH = 1;                         // beg symbol
char EOT = 4;                         // end symbol
map<string, vector<string>> mail_box; // maps group ids to  their stored messages

class Botnet_server
{
public:
    int sock; // socket of server connection
    string group_id;
    string ip_address;
    int portnr;

    Botnet_server(int socket)
    {
        this->sock = socket;
    }

    Botnet_server(int socket, string group_id, string ip_address, int portnr)
    {
        this->sock = socket;
        this->group_id = group_id;
        this->portnr = portnr;
        this->ip_address = ip_address;
    }

    string to_string()
    {
        string return_str;
        return_str = group_id + "," + ip_address + "," + std::to_string(portnr) + ";";
        return return_str;
    }

    ~Botnet_server() {} // Virtual destructor defined for base class
};

class Command
{
public:
    string command;
    vector<string> arguments;
    Command(string raw_command)
    {
        stringstream ss(raw_command);
        getline(ss, command, ',');

        string argument;
        while (getline(ss, argument, ','))
        {
            arguments.push_back(argument);
        }
    }

    Command()
    {
    }

    string to_string()
    {

        string return_str;
        return_str.push_back(SOH);
        return_str += command;
        for (auto i = arguments.begin(); i != arguments.end(); ++i)
            return_str += "," + *i;
        return_str.push_back(EOT);

        return return_str;
    }
};

/*
*   Prints commands verbosly
*/
void if_verbose(string text)
{
    if (VERBOSE)
    {
        cout << text << endl
             << endl;
    }
}
/**
 * Returns the current local time of the form
 * Fri Oct 11 16:56:47 2019
 */
string get_timestamp()
{
    time_t now = time(0);
    string time_string(ctime(&now));
    time_string.pop_back();
    return time_string;
}

void log_incoming(string message)
{
    cout << get_timestamp() << " INCOMING    << " << message << endl;
}

void log_outgoing(string message)
{
    cout << get_timestamp() << " OUTGOING    >> " << message << endl;
}

/*
*   Sends a message through the given socket
*   Logs the messages to the servers terminal
*/
int send_and_log(const int communicationSocket, const string message)
{
    if (send(communicationSocket, message.c_str(), message.size(), 0) < 0)
    {
        cout << "Sending '" + message + "' failed" << endl;
        return -1;
    }
    log_outgoing(message);
    return 0;
}

/*
*   Sets up a listening socket to listen to client and server commands
*/
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
        // :: prefix used to escape from the std namespace and thus avoiding the std::bind() function
        if (::bind(listeningSocket, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(listeningSocket);
            perror("server: bind");
            continue;
        }
        break;
    }
    freeaddrinfo(servinfo); // all done with this structure

    // Make sure the socket was established correctly
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
* this function waits for beginning connection to the botnet
* waits for client to send a valid CONNECTTO,<ip>,<port>,<groupId> command
* extracts the ip and port and returns it via output parameters.
*/
void client_botnet_connect_cmd(int clientSock, string &outIp, int &outPort)
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

        string string_command(server_connect_command); // char array to string
        // TODO: check if command is correct
        if (true)
        {
            // TODO: use Command class to do this better
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
            send_and_log(clientSock, message);
        }
    }
}

void split(string &str, vector<string> &cont, char delim = ' ')
{
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delim))
    {
        cont.push_back(token);
    }
}

/**
 * Gives the Client a way to display the running servers on the same machine
 * with ports starting with 4
 */
string get_parallel_server()
{

    FILE *fp;
    string command = "ps -aux | grep \\./ | grep ' 4'";
    // Large arbitrary size chosen
    char var[6000];

    // Run a command and save the output to a string
    fp = popen(command.c_str(), "r");
    std::string output_string;
    while (fgets(var, sizeof(var), fp) != NULL)
    {
        output_string = output_string + var;
    }
    pclose(fp);
    return output_string;
}

/*
* takes SERVERS,<groupid,ip,port>;<groupid,ip,port>; and converts to vector of Botnet_server objects
*/
void servers_response_to_vector(string servers_response, vector<Botnet_server> &servers)
{
    // first remove "SERVERS" from string
    servers_response = servers_response.substr(8);

    vector<string> server_strings;
    split(servers_response, server_strings, ';');

    for (auto i = server_strings.begin(); i != server_strings.end(); ++i)
    {
        vector<string> server_parts;
        split(*i, server_parts, ',');

        if (server_parts.size() == 3)
        {
            Botnet_server server = Botnet_server(-1, server_parts[0], server_parts[1], stoi(server_parts[2]));
            servers.push_back(server);
        }
    }
}

void send_list_servers_cmd(int socketfd)
{
    Command command = Command();
    command.command = "LISTSERVERS";
    command.arguments.push_back(OUR_GROUP_ID);

    // TODO: error handling
    send_and_log(socketfd, command.to_string());
}

/*
* connects to server and returns socket file descriptor
* incrament botner-server connection count
*/
int connect_to_botnet_server(string botnet_ip, int botnet_port, map<int, Botnet_server *> &botnet_servers, int &maxfds, fd_set &open_sockets)
{
    if_verbose("inside connect_to_botnet_server");
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd < 0)
    {
        perror("Failed to open socket");
        exit(0);
    }

    struct sockaddr_in server_socket_addr;                             // address of botnet server
    memset(&server_socket_addr, 0, sizeof(server_socket_addr));        // Initialise memory
    server_socket_addr.sin_family = AF_INET;                           // pv4
    server_socket_addr.sin_addr.s_addr = inet_addr(botnet_ip.c_str()); // bind to server ip
    server_socket_addr.sin_port = htons(botnet_port);                  // portno

    // connect to server
    if (connect(socketfd, (struct sockaddr *)&server_socket_addr, sizeof(server_socket_addr)) < 0)
    {
        if_verbose("inside botnet connection error");
        perror("Failed to connect");
        exit(0);
    }
    if_verbose("attemtping to add to botnet server list");

    // send LISTSERVERS command to learn the server id.
    send_list_servers_cmd(socketfd);

    botnet_servers[socketfd] = new Botnet_server(socketfd, STANDIN_GROUPID, botnet_ip, botnet_port);

    maxfds = max(maxfds, socketfd);
    FD_SET(socketfd, &open_sockets);
    return 0;
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

        // send LISTSERVERS command to learn the server id.
        send_list_servers_cmd(server_socket);

        // TODO: havent been able to test if the port and ip extraction worked
        char ip_address[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(server_addr.sin_addr), ip_address, INET_ADDRSTRLEN);
        botnet_servers[server_socket] = new Botnet_server(server_socket, STANDIN_GROUPID, string(ip_address), server_addr.sin_port);

        if_verbose("after new connection this is the stored server: " + botnet_servers[server_socket]->to_string());

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

/*
* extract the beginning of the message to see if it is for example LISTSERVERS or KEEPALIVE or 
*/
string get_message_type(string message)
{
    string type;
    stringstream ss(message);
    getline(ss, type, ',');

    return type;
}

string get_connected_servers(const map<int, Botnet_server *> botnet_servers)
{
    if_verbose("inside get_connected_servers");
    string message = "";
    message.push_back(SOH);
    message += "SERVERS,";

    //  TODO: have the ip dynamic like OUR_PORTNR
    Botnet_server my_server = Botnet_server(-1, OUR_GROUP_ID, "130.208.243.61", OUR_PORTNR);

    message += my_server.to_string();

    for (auto const &pair : botnet_servers)
    {
        Botnet_server *botnet_server = pair.second;

        message += botnet_server->to_string();
    }
    message.push_back(EOT);
    return message;
}

// TODO: add timeout and if we reach buffersize+ then we drop the ignore the message and move on
void deal_with_server_command(map<int, Botnet_server *> &botnet_servers, Botnet_server *botnet_server, fd_set &open_sockets, int &maxfds)
{
    char buffer[BUFFERSIZE];
    memset(buffer, 0, BUFFERSIZE);

    int byteCount = recv(botnet_server->sock, buffer, sizeof(buffer), MSG_DONTWAIT);
    // if_verbose("after first receive in deal_with_server_command");
    // recv() == 0 means server has closed connection
    if (byteCount == 0)
    {
        printf("Botnet server closed connection: %d", botnet_server->sock);
        close(botnet_server->sock);

        close_botnet_server(botnet_server->sock, open_sockets, botnet_servers, maxfds);
    }
    // We don't check for -1 (nothing received) because select()
    // only triggers if there is something on the socket for us.
    else
    {
        // continue to receive until we have a full message

        while (buffer[byteCount - 1] != EOT)
        {
            if_verbose("inside while gathering response");
            byteCount += recv(botnet_server->sock, buffer + byteCount, sizeof(buffer) - byteCount, MSG_DONTWAIT);
        }
        // remove EOT and SOH from buffer and convert to string.
        buffer[byteCount - 1] = '\0';
        string incoming_string(buffer);
        incoming_string = incoming_string.substr(1);
        log_incoming(incoming_string);

        string message_type = get_message_type(incoming_string);
        if (message_type == "SERVERS")
        {
            if_verbose("inside SERVERS if");
            // but the received servers into a more organized form
            vector<Botnet_server> servers;
            servers_response_to_vector(incoming_string, servers);

            // update the the values int the botnet list, this is mostly for the group_id.
            botnet_server->group_id = servers[0].group_id;
            botnet_server->ip_address = servers[0].ip_address;
            botnet_server->portnr = servers[0].portnr;

            if_verbose("server that is sending message: " + botnet_server->to_string());
            //TODO: kannski reyna að tengjast helling af fólki hér automatically
        }
        else if (message_type == "LISTSERVERS")
        {
            if_verbose("inside LISTSERVERS if");
            Command command = Command(incoming_string);
            botnet_server->group_id = command.arguments[0];
            string server_list = get_connected_servers(botnet_servers);
            send_and_log(botnet_server->sock, server_list);
        }
        else if (message_type == "SEND_MSG")
        {
            Command send_msg_command = Command(incoming_string);
            cout << "Message recieved FROM " + send_msg_command.arguments[1] + " TO " + send_msg_command.arguments[2] + " MSG: " + send_msg_command.arguments[3] << endl;
        }
        else if (message_type == "KEEPALIVE")
        {
            send_and_log(botnet_server->sock, "Keepalive confirmed, thanks for the update");
        }
        else
        {
            // TODO: dont think we should have this because if they do the same then this will just be a back and forth loop
            // Not a valid command
            string error("Command " + incoming_string + " not recognized");
            if_verbose(error);
            send_and_log(botnet_server->sock, "Command not recognized, did you remember the SOH and EOT characters?");
        }
    }
}

void deal_with_client_command(int &clientSocket, map<int, Botnet_server *> &botnet_servers, fd_set &open_sockets, int &maxfds)
{
    char buffer[BUFFERSIZE];
    memset(buffer, 0, BUFFERSIZE);
    if_verbose("Dealing with client command");
    // recv() == 0 means client has closed connection
    int byteCount = recv(clientSocket, buffer, sizeof(buffer), MSG_DONTWAIT);
    if (byteCount == 0)
    {
        printf("Client closed connection: %d", clientSocket);
        close(clientSocket);
        clientSocket = -1;
    }
    // We don't check for -1 (nothing received) because select()
    // only triggers if there is something on the socket for us.
    else
    {
        buffer[byteCount + 1] = '\0';
        Command command = Command(string(buffer));

        log_incoming(command.to_string());
        //  TODO: handle if we already have 5 connections
        if (command.command == "CONNECTTO")
        {
            if_verbose("Received CONNECTTO command from client");
            string ip = command.arguments[0];
            int port = stoi(command.arguments[1]);

            if (connect_to_botnet_server(ip, port, botnet_servers, maxfds, open_sockets) == 0)
            {
                send_and_log(clientSocket, "Connected successfully to indicated server");
            }
            else
            {
                send_and_log(clientSocket, "Failed to connect to indicated server");
            }
        }
        else if (command.command == "LISTSERVERS")
        {
            if_verbose("Listing servers for client");
            string server_list = get_connected_servers(botnet_servers);
            send_and_log(clientSocket, server_list);
        }
        else if (command.command == "SEND_MSG")
        {
            if_verbose("Sending message from client");
            send_and_log(clientSocket, "Sending message - not implemented");
        }
        else if (command.command == "GET_MSG")
        {
            if_verbose("Getting message for client");
            send_and_log(clientSocket, "Getting message - not implemented");
        }
        else if (command.command == "WHO")
        {
            if_verbose("Listing parallel servers for client");
            send_and_log(clientSocket, get_parallel_server());
        }
        else
        {
            if_verbose("Non recognized client command recieved");
            send_and_log(clientSocket, "Sorry, I don't recognize that commmand");
        }
    }
}

void send_keep_alive_messages(map<int, Botnet_server *> &botnet_servers)
{
    Command command = Command();
    command.command = "KEEPALIVE";
    command.arguments.push_back("0");

    for (auto const &pair : botnet_servers)
    {
        Botnet_server *botnet_server = pair.second;

        // if the group_id is not in the mailbox then just send message count = 0.
        map<string, vector<string>>::iterator it = mail_box.find(botnet_server->group_id);
        if (it != mail_box.end())
        {
            int message_count = mail_box[botnet_server->group_id].size();
            command.arguments[0] = to_string(message_count);
        }
        else
        {
            command.arguments[0] = "0";
        }

        // TODO: error handling
        send_and_log(botnet_server->sock, command.to_string());

        if_verbose("Sent keepalive: " + command.to_string() + " to server " + botnet_server->to_string());
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
    map<int, Botnet_server *> botnet_servers; // Lookup table for per Client information // TODO: maybe move this to be global?

    if (argc != 2)
    {
        printf("Usage: chat_server <ip port>\n");
        exit(0);
    }

    // Setup socket for server to listen to connections
    char *port = argv[1];
    OUR_PORTNR = atoi(port);
    // establish listening socket, initalize maxfds, add to open_sockets
    establish_server(port, listenSocket, maxfds, open_sockets);

    // wait for the client to connect to the server, set clientSock, maybe change maxfds, add to open_sockets
    wait_for_client_connection(listenSocket, clientSock, maxfds, open_sockets);

    // get ip and port of first server to connect to
    string botnet_server_ip;
    int botnet_server_port;
    client_botnet_connect_cmd(clientSock, botnet_server_ip, botnet_server_port);
    if_verbose(botnet_server_ip);
    if_verbose(to_string(botnet_server_port));

    // connect to botnet, maybe change maxfds, add to open_sockets, add to botnet_servers
    if (connect_to_botnet_server(botnet_server_ip, botnet_server_port, botnet_servers, maxfds, open_sockets) == 0)
    {
        if_verbose("Connected to botnet server");
        send_and_log(clientSock, "Connected to indicated server");
    }

    auto start = chrono::high_resolution_clock::now();
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
        if (FD_ISSET(listenSocket, &read_sockets))
        {
            // Check if we have room
            if (botnet_servers.size() < MAXCONNECTEDSERVERS)
            {
                // accepts connection, maybe changes maxfds, add to botnet_servers
                new_connections(listenSocket, maxfds, open_sockets, botnet_servers);
                n--;
            }
            else
            {
                // Can't accept
                send_and_log(listenSocket, "Sorry, this server is at full capacity");
            }
        }

        if (FD_ISSET(clientSock, &read_sockets))
        {
            // check for commands from client
            deal_with_client_command(clientSock, botnet_servers, open_sockets, maxfds);
            n--;
        }

        // for each connected server check for any incomming commands/messages
        for (auto const &pair : botnet_servers)
        {
            Botnet_server *botnet_server = pair.second;
            if (FD_ISSET(botnet_server->sock, &read_sockets))
            {
                deal_with_server_command(botnet_servers, botnet_server, open_sockets, maxfds);
                n--;
            }
        }

        // if time between start and end is bigger then 60 seconds then send keep alive
        auto end = chrono::high_resolution_clock::now();
        auto time_elapsed = chrono::duration_cast<chrono::seconds>(end - start);
        if (time_elapsed.count() >= 60)
        {
            send_keep_alive_messages(botnet_servers);
            start = chrono::high_resolution_clock::now();
        }
    }
}
