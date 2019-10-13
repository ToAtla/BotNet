//
// BotNet server for Computer Networking
//
// Command line: ./chat_server 4000
//
// Author: Þórður Atlason (thorduratl17@ru.is) and Þórður Friðriksson (thordurf17@ru.is)
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
#include <fstream>
#include <map>
#include <chrono>
#include <utility> // std::pair
#include <thread>

using namespace std;

#define BACKLOG 5             // Allowed length of queue of waiting connections
#define MAXCONNECTEDSERVERS 5 // Allowed amount of connected external servers
#define BUFFERSIZE 1025
#define LOGFILE "logfile.txt"

string OUR_GROUP_ID     = "P3_GROUP_75";
string STANDIN_GROUPID  = "UNKOWNGROUP";
string CLIENT_IP = "111.222.333.444";
int CLIENT_PORT = 1337;
int OUR_PORTNR;
string OUR_IP;
bool VERBOSE = true;
char SOH = 1;                                       // beg symbol
char EOT = 4;                                       // end symbol
map<string, vector<pair<string, string>>> mail_box; // maps group ids to a pair of the sender and the stored message

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

map<int, Botnet_server *> botnet_servers; // Lookup table for per Client information

class Message
{
public:
    string type;
    vector<string> arguments;
    // TODO:    make the constructor robust and capable of
    //          creating a message from a string containing SOH & EOT
    Message(string raw_message)
    {
        stringstream ss(raw_message);
        getline(ss, type, ',');

        string argument;
        while (getline(ss, argument, ','))
        {
            arguments.push_back(argument);
        }
    }

    Message()
    {
    }

    string to_string() const
    {

        string return_str;
        return_str.push_back(SOH);
        return_str += type;
        for (auto i = arguments.begin(); i != arguments.end(); ++i)
            return_str += "," + *i;
        return_str.push_back(EOT);

        return return_str;
    }
};

/*
*   Prints verbosly
*/
void if_verbose(string text)
{
    if (VERBOSE)
    {
        cout << text << endl
             << endl;
    }
}

void sleep(int milliseconds)
{
    this_thread::sleep_for(chrono::milliseconds(milliseconds));
}

/*
* iterate through botnet list and return the socket the matches the id.
*/
int get_socket_from_id(string server_id)
{
    for (auto const &pair : botnet_servers)
    {
        if (server_id == pair.second->group_id)
        {
            return pair.first;
        }
    }
    return -1; // if id doesnt exist in list
}

bool in_mailbox(string group_id)
{
    map<string, vector<pair<string, string>>>::iterator it = mail_box.find(group_id);

    return (it != mail_box.end());
}

string reconstruct_message_from_vector(const vector<string> &container, int start_index)
{
    string return_str = container[start_index];
    for (unsigned int i = start_index + 1; i < container.size(); i++)
    {
        return_str += "," + container[i];
    }
    return return_str;
}

string fd_set_to_string(const fd_set &socketset, int maxfds)
{
    string return_str = "";
    for (int i = 0; i <= maxfds; i++)
    {
        if (FD_ISSET(i, &socketset))
        {
            map<int, Botnet_server *>::iterator it = botnet_servers.find(i);

            if (it != botnet_servers.end())
            {
                return_str += to_string(i) + " -> " + botnet_servers[i]->group_id + ", ";
            }
            else
            {
                return_str += to_string(i) + ", ";
            }
        }
    }
    return return_str;
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
    return time_string.substr(4,15);
}

void initialize_log_file()
{
    ifstream f(LOGFILE);
    if (!f.good())
    {
        string top_header = "TIME             TO/FROM   GROUP_ID     IP ADDRESS         PORT    MESSAGE";
        string sub_header = "-------------------------------------------------------------------------";
        ofstream outfile(LOGFILE);
        outfile << top_header << endl;
        outfile << sub_header << endl;
        outfile.close();
    }
}

void append_to_log_file(string message)
{
    ofstream outfile(LOGFILE, ios_base::app);
    outfile << message << endl;
    outfile.close();
}


string make_log_string(const int source_socket, string message, int out){

    string rtn = get_timestamp() + "  ";
    if(out){
        rtn = rtn + "TO    >>";
    }
    else{
        rtn = rtn + "FROM  <<";
    }
    // if the socket corresponds to a botnet_server
    if(botnet_servers.count(source_socket)){
        Botnet_server *sender = botnet_servers[source_socket];
        rtn = rtn + "  " + sender->group_id + "  " + sender->ip_address + "     " + to_string(sender->portnr) + "    " + message;
    }else{
        rtn = rtn + "  " + "CLIENT     " + "  " + CLIENT_IP + "     " + to_string(CLIENT_PORT) + "    " + message;
    }
    
    return rtn;
}

void log_incoming(const int source_socket, string message)
{
    string log_string = make_log_string(source_socket, message, 0);
    cout << log_string << endl;
    append_to_log_file(log_string);
}

/*
*   Sends a message through the given socket
*   Logs the messages to the servers terminal
*/
int send_and_log(const int communicationSocket, const Message message)
{
    if (send(communicationSocket, message.to_string().c_str(), message.to_string().size(), 0) < 0)
    {
        cout << "Sending '" + message.to_string() + "' failed" << endl;
        return -1;
    }
    string log_string = make_log_string(communicationSocket, message.to_string(), 1);
    cout << log_string << endl;
    append_to_log_file(log_string);
    return 0;
}

/*
*   Sets up a listening socket to listen to client and server messages
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
    struct sockaddr_in *client_info = (struct sockaddr_in*) &client_addr;
    CLIENT_PORT = client_info->sin_port;
    char temp_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_info->sin_addr), temp_ip, INET_ADDRSTRLEN);
    CLIENT_IP = to_string(client_info->sin_addr.s_addr);

    if_verbose("-- client connected successfully on socket: " + to_string(clientSocket) + " --");
    if_verbose("-- client ip: " + CLIENT_IP + " client port: " + to_string(CLIENT_PORT) + " --");
    if_verbose("-- maxfds is: " + to_string(maxfds) + " --");
}

/*
* this function waits for beginning connection to the botnet
* waits for client to send a valid CONNECTTO,<ip>,<port>,<groupId> message
* extracts the ip and port and returns it via output parameters.
*/
void client_botnet_connect_cmd(int clientSock, string &outIp, int &outPort)
{
    printf("waiting for client server connect message: CONNECTTO,<server_ip>,<port>\n");

    char server_connect_command[BUFFERSIZE];
    memset(server_connect_command, 0, BUFFERSIZE);

    int byteCount = recv(clientSock, server_connect_command, BUFFERSIZE, 0);
    if (byteCount < 0)
    {
        cout << "failed to receive from client" << endl;
        exit(0);
    }
    server_connect_command[BUFFERSIZE - 1] = '\0';

    Message connection_command(server_connect_command);
    outIp = connection_command.arguments[0];
    outPort = atoi(connection_command.arguments[1].c_str());
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
    Message message = Message();
    message.type = "LISTSERVERS";
    message.arguments.push_back(OUR_GROUP_ID);

    // TODO: error handling
    send_and_log(socketfd, message);
}

/*
* connects to server and returns socket file descriptor
* incrament botner-server connection count
*/
int connect_to_botnet_server(string botnet_ip, int botnet_port, int &maxfds, fd_set &open_sockets)
{
    if_verbose("-- inside connect_to_botnet_server --");
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
        if_verbose("--inside botnet connection error--");
        perror("Failed to connect");
        return -1;
    }
    if_verbose("--attemtping to add to botnet server list--");

    botnet_servers[socketfd] = new Botnet_server(socketfd, STANDIN_GROUPID, botnet_ip, botnet_port);

    // send LISTSERVERS command to learn the server id.
    send_list_servers_cmd(socketfd);

    if_verbose("-- server added to list: " + botnet_servers[socketfd]->to_string() + " --");
    if_verbose("-- socket added to  open_socekts: " + to_string(socketfd) + " --");

    maxfds = max(maxfds, socketfd);
    FD_SET(socketfd, &open_sockets);
    return 0;
}

void send_messages_from_mailbox(int socketfd)
{
    string message_owner = botnet_servers[socketfd]->group_id; // group id of message owner

    map<string, vector<pair<string, string>>>::iterator it = mail_box.find(message_owner);

    // construct the sending  message: SEND_MSG,<from_group_id>,<to_group_id>,<message content>
    Message message = Message();
    message.type = "SEND_MSG";
    message.arguments.push_back("");
    message.arguments.push_back(message_owner);
    message.arguments.push_back("");

    // if there are messages in the mailbox
    if (it != mail_box.end())
    {
        vector<pair<string, string>> messages = mail_box[message_owner]; // messages that the message_owner owns

        // send all messages in the mail box that are for the message_owner
        for (unsigned int i = 0; i < messages.size(); i++)
        {
            pair<string, string> message_pair = messages[i];
            message.arguments[0] = message_pair.first;
            message.arguments[2] = message_pair.second;

            send_and_log(socketfd, message);
        }

        // remove messages from mail_box since they have been delivered to their rightful place
        mail_box.erase(message_owner);
    }

    // also send message from us to them.
    message.arguments[0] = OUR_GROUP_ID;
    message.arguments[2] = "Hello friend";
    send_and_log(socketfd, message);
}

int send_welcome_message(int socket, string to_group_id)
{
    // construct SEND_MSG,<FROM_GROUP_ID>,<TO_GROUP_ID>,<message content>
    Message message = Message();
    message.type = "SEND_MSG";
    message.arguments.push_back(OUR_GROUP_ID);   // from group id
    message.arguments.push_back(to_group_id);    // to group id
    message.arguments.push_back("hello friend"); // message content

    return send_and_log(socket, message);
}

/*
* Accepts incoming connection.
* Adds socket to botnet_server map
*/
void new_connections(int listenSocket, int &maxfds, fd_set &open_sockets)
{
    struct sockaddr_in server_addr;
    socklen_t server_addr_len;

    if_verbose("--inside new_connections--");
    int server_socket = accept(listenSocket, (struct sockaddr *)&server_addr,
                               &server_addr_len);

    if (server_socket < 0)
    {
        perror("error in accepting new connection\n");
    }
    else
    {
        FD_SET(server_socket, &open_sockets);

        //TODO: multiple messages problem
        //send_messages_from_mailbox(server_socket, botnet_servers);

        // TODO: havent been able to test if the port and ip extraction worked
        char ip_address[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(server_addr.sin_addr), ip_address, INET_ADDRSTRLEN);
        botnet_servers[server_socket] = new Botnet_server(server_socket, STANDIN_GROUPID, string(ip_address), server_addr.sin_port);

        // send LISTSERVERS command to learn the server id.
        send_list_servers_cmd(server_socket);

        if_verbose("-- after new connection this is the stored server: " + botnet_servers[server_socket]->to_string() + " --");

        // And update the maximum file descriptor
        maxfds = max(maxfds, server_socket);
        printf("botnet-server connected on server: %d\n", server_socket);
        
    }
}

// Close a botnet server's connection, remove it from the botnet_server list, and
// tidy up select sockets afterwards.

void close_botnet_server(int botnet_server_sock, fd_set &open_sockets, int &maxfds)
{
    delete botnet_servers[botnet_server_sock];
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
    close(botnet_server_sock);
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

/**
 * Returns the servers external IP address of the form 192.168.1.2
 * Modified from: https://www.binarytides.com/tcp-syn-portscan-in-c-with-linux-sockets/
 */
string get_local_address()
{
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));

    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    const char *kGoogleDnsIp = "8.8.8.8";
    int dns_port = 53;

    struct sockaddr_in serv;

    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr(kGoogleDnsIp);
    serv.sin_port = htons(dns_port);

    connect(sock, (const struct sockaddr *)&serv, sizeof(serv));

    struct sockaddr_in name;
    socklen_t namelen = sizeof(name);
    getsockname(sock, (struct sockaddr *)&name, &namelen);
    close(sock);
    const char *p = inet_ntop(AF_INET, &name.sin_addr, buffer, 100);

    string address(p);
    return address;
}

string get_connected_servers()
{
    if_verbose("-- inside get_connected_servers --");
    string message = "";
    message += "SERVERS,";

    Botnet_server my_server = Botnet_server(-1, OUR_GROUP_ID, OUR_IP, OUR_PORTNR);

    message += my_server.to_string();

    for (auto const &pair : botnet_servers)
    {
        Botnet_server *botnet_server = pair.second;

        message += botnet_server->to_string();
    }
    return message;
}

string get_status_response(string to_group_id)
{
    string return_str;
    return_str += "STATUSRESP," + OUR_GROUP_ID + "," + to_group_id;
    for (auto const &pair : mail_box)
    {
        string server_id = pair.first;
        int message_count = pair.second.size();
        // According to Piazza, we don't include groups, that we have no messages for
        if (message_count > 0)
        {
            return_str += "," + server_id + "," + to_string(message_count);
        }
    }
    return return_str;
}

/*
    takes in a pointer to a null-terminating char array and splits them on the EOT char
    also removes the SOH char.
*/
vector<string> split_to_multiple_commands(char *buffer)
{
    vector<string> incoming_strings;

    stringstream ss(buffer);
    string incoming_string;
    while (getline(ss, incoming_string, EOT))
    {
        incoming_strings.push_back(incoming_string.substr(1));
    }

    return incoming_strings;
}

/**
 * Manages and replies to all commands sent from peer servers
 */
void deal_with_server_command(Botnet_server *botnet_server, fd_set &open_sockets, int &maxfds)
{

    if_verbose("-- inside deal_with_server_command --");
    char buffer[BUFFERSIZE];
    memset(buffer, 0, BUFFERSIZE);

    if_verbose("-- receiving from server: " + botnet_server->to_string() + " --");

    int byteCount = recv(botnet_server->sock, buffer, sizeof(buffer), MSG_DONTWAIT);
    // if_verbose("after first receive in deal_with_server_command");
    // recv() == 0 means server has closed connection
    if (byteCount == 0)
    {
        printf("Botnet server closed connection: %d", botnet_server->sock);
        close(botnet_server->sock);

        close_botnet_server(botnet_server->sock, open_sockets, maxfds);
    }
    // We don't check for -1 (nothing received) because select()
    // only triggers if there is something on the socket for us.
    else
    {
        auto start = chrono::high_resolution_clock::now();
        auto end = chrono::high_resolution_clock::now();
        auto time_elapsed = chrono::duration_cast<chrono::seconds>(end - start);
        // continue to receive until we have a full message ending with EOT
        while (buffer[byteCount - 1] != EOT)
        {
            // if_verbose("-- inside while gathering response --");
            byteCount += recv(botnet_server->sock, buffer + byteCount, sizeof(buffer) - byteCount, MSG_DONTWAIT);

            end = chrono::high_resolution_clock::now();
            time_elapsed = chrono::duration_cast<chrono::seconds>(end - start);

            if (time_elapsed.count() > 4)
            {
                // Timeout, we ignore the command
                return;
            }
        }

        buffer[byteCount - 1] = '\0';
        vector<string> incoming_strings = split_to_multiple_commands(buffer);

        // go through all the commands that we got.
        for (unsigned int i = 0; i < incoming_strings.size(); i++)
        {
            string incoming_string = incoming_strings[i];


            log_incoming(botnet_server->sock, incoming_string);

            // TODO: simplify with command class
            Message incoming_message(incoming_string);

            if (incoming_message.type == "SERVERS")
            {
                if_verbose("-- inside SERVERS if --");
                // but the received servers into a more organized form
                vector<Botnet_server> servers;
                servers_response_to_vector(incoming_string, servers);

                //update the the values int the botnet list, this is mostly for the group_id.
                if (botnet_server->group_id == STANDIN_GROUPID)
                {
                    botnet_server->group_id = servers[0].group_id;
                }
                
                // update the the values int the botnet list, this is mostly for the group_id.
                //botnet_server->group_id = servers[0].group_id;
                //botnet_server->ip_address = servers[0].ip_address;
                //botnet_server->portnr = servers[0].portnr;

                //TODO: kannski reyna að tengjast helling af fólki hér automatically
            }
            else if (incoming_message.type == "LISTSERVERS")
            {
                if_verbose("-- inside LISTSERVERS if --");
                botnet_server->group_id = incoming_message.arguments[0];

                Message server_list_answer(get_connected_servers());
                send_and_log(botnet_server->sock, server_list_answer);
            }
            //  TODO: untested
            else if (incoming_message.type == "SEND_MSG")
            {
                string from_group_id = incoming_message.arguments[0];                                    // group who message is from
                string to_group_id = incoming_message.arguments[1];                                      // group who message is to
                string message_content = reconstruct_message_from_vector(incoming_message.arguments, 2); // if there were commas in message then we need to reconstruct.
                if_verbose("-- Message recieved FROM " + from_group_id + " TO " + to_group_id + " MSG: " + message_content + " --");

                mail_box[to_group_id].push_back(pair<string, string>(from_group_id, message_content)); // add message to  mailbox

                if_verbose("-- see if it got added to mailbox, size of mailbox: " + to_string(mail_box[to_group_id].size()) + ", first item: <" + mail_box[to_group_id][0].first + ", " + mail_box[to_group_id][0].second + "> --");
            }
            // TODO: untested
            else if (incoming_message.type == "GET_MSG")
            {
                string to_group_id = incoming_message.arguments[0]; // id of the group whos message is for

                if (in_mailbox(to_group_id))
                {
                    int message_count = mail_box[to_group_id].size();
                    pair<string, string> message_pair = mail_box[to_group_id][message_count - 1]; // newest message

                    // construct command to send and then for each message send to server.
                    Message outgoing_message = Message();
                    outgoing_message.type = "SEND_MSG";

                    outgoing_message.arguments.push_back(message_pair.first);  // who the message is from
                    outgoing_message.arguments.push_back(to_group_id);         // who the message is for
                    outgoing_message.arguments.push_back(message_pair.second); // message

                    send_and_log(botnet_server->sock, outgoing_message);

                    if_verbose("-- sending GET_MSG: " + outgoing_message.to_string() + "--");

                    // if we are sending the message to its rightful server then remove message
                    // and if all messages are gone then remove from the mail_box
                    if (get_socket_from_id(to_group_id) == botnet_server->sock)
                    {
                        mail_box[to_group_id].pop_back();
                        if (mail_box[to_group_id].size() == 0)
                        {
                            mail_box.erase(to_group_id);
                        }
                    }
                }
                else
                {
                    if_verbose("-- no messages for this group_id --");
                }

                // TODO: multiple message problem
                /*for (unsigned int i = 0; i < messages.size(); i++)
            {
                outgoing_message.arguments.push_back(messages[i].first);  // who the message is from
                outgoing_message.arguments.push_back(to_group_id);        // who the message is for
                outgoing_message.arguments.push_back(messages[i].second); // message

                send_and_log(botnet_server->sock, outgoing_message);

                if_verbose("-- sending GET_MSG: " + outgoing_message.to_string() + "--");
            }*/
            }
            else if (incoming_message.type == "KEEPALIVE")
            {
                // TODO: do something here
                if_verbose("-- received keep alive --");
                if (atoi(incoming_message.arguments[0].c_str()) != 0)
                {
                    if_verbose("-- This server has messages for us. Let's do something--");
                }
            }
            // TODO: untested
            else if (incoming_message.type == "LEAVE")
            {
                close_botnet_server(botnet_server->sock, open_sockets, maxfds);
            }
            // TODO: untested
            else if (incoming_message.type == "STATUSREQ")
            {
                string from_group = incoming_message.arguments[0];

                Message outgoing_message(get_status_response(from_group));
                send_and_log(botnet_server->sock, outgoing_message);
            }
            else if (incoming_message.type == "STATUSRESP")
            {
                cout << incoming_string << endl;
                // TODO: maybe automate getting messages here.
            }
            else
            {
                // Not a valid command
                string error("-- Unknown server message: " + incoming_string + " --");
                if_verbose(error);
            }
        }
    }
}

void deal_with_client_command(int &clientSocket, fd_set &open_sockets, int &maxfds)
{
    char buffer[BUFFERSIZE];
    memset(buffer, 0, BUFFERSIZE);
    if_verbose("-- Dealing with client command --");
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
        Message message = Message(string(buffer));

        log_incoming(clientSocket, message.to_string());
        //  TODO: handle if we already have 5 connections
        if (message.type == "CONNECTTO")
        {
            if_verbose("-- Received CONNECTTO command from client --");
            string ip = message.arguments[0];
            int port = stoi(message.arguments[1]);

            // Cyclic check
            if (port == OUR_PORTNR && ip == OUR_IP)
            {
                Message success("FAILIURE,We can't connect to ourselves");
                send_and_log(clientSocket, success);
                return;
            }

            if (connect_to_botnet_server(ip, port, maxfds, open_sockets) == 0)
            {
                Message success("SUCCESS,Connected successfully to indicated server");
                send_and_log(clientSocket, success);
            }
            else
            {
                Message failure("FAILURE,Connection attmept failed");
                send_and_log(clientSocket, failure);
            }
        }
        else if (message.type == "LISTSERVERS")
        {
            if_verbose("-- Listing servers for client --");
            Message server_list_answer(get_connected_servers());
            send_and_log(clientSocket, server_list_answer);
        }
        else if (message.type == "LISTREMOTE")
        {
            // The answer won't reach the client
            string group = message.arguments[0];
            if_verbose("-- Listing remote servers on " + group + " --");

            string message = "LISTSERVERS," + OUR_GROUP_ID;
            Message listservers_message(message);
            int group_socket = get_socket_from_id(group);
            send_and_log(group_socket, message);

            Message report("SUCCESS,queried a remote server");
            send_and_log(clientSocket, report);
        }
        else if (message.type == "SENDMSG")
        {
            if_verbose("-- Sending message from client --");

            string to_group_id = message.arguments[0];

            // store the message in the mailbox
            mail_box[to_group_id].push_back(pair<string, string>(OUR_GROUP_ID, reconstruct_message_from_vector(message.arguments, 1)));

            Message report("SUCCESS, message waits in mailbox");
            send_and_log(clientSocket, report);
        }
        else if (message.type == "GETMSG")
        {
            if_verbose("-- Getting message for client --");
            string message_owner_id = message.arguments[0]; // group id whos message we are trying to retrieve

            Message outgoing_message = Message();
            outgoing_message.type = "SEND_MSG";
            string outgoing_string = "";

            // get from our own mailbox
            // if there  is mail for the the message owner
            if (in_mailbox(message_owner_id))
            {
                // TODO: multiple message problem
                outgoing_string = mail_box[message_owner_id][0].second;
            }
            else
            {
                outgoing_string = "no message for this group_id";
            }
            outgoing_message.arguments.push_back(outgoing_string);
            send_and_log(clientSocket, outgoing_message);
        }
        else if (message.type == "WHO")
        {
            if_verbose("-- Listing parallel servers for client --");
            Message parallel_stuff(get_parallel_server());
            send_and_log(clientSocket, parallel_stuff);
        }
        // expected format: CUSTOM,<GROUP_ID>,<server command>
        else if (message.type == "CUSTOM")
        {

            string group_id = message.arguments[0];

            int communicationSocket = get_socket_from_id(group_id);
            if (communicationSocket != -1)
            {
                
                string server_command = reconstruct_message_from_vector(message.arguments, 1);
                Message new_message = Message(server_command);
                if_verbose("-- sending following custom command to server --");
                if_verbose(server_command);
                send_and_log(communicationSocket, new_message);
            }
            else
            {
                if_verbose("-- no such group connected --");
            }
            Message report("SUCCESS,custom command");
            send_and_log(clientSocket, report);
        }
        else
        {
            if_verbose("-- Non recognized client command recieved --");
            Message error_msg("ERROR,Sorry I don't recognize that command");
            send_and_log(clientSocket, error_msg);
        }
    }
}

/**
 * Sends keepalive messages to each connected server
 * disconnects a server if the keepalive does not reach them
 */
void send_keep_alive_messages(fd_set &open_sockets, int &maxfds)
{
    Message message = Message();
    message.type = "KEEPALIVE";
    message.arguments.push_back("0");

    for (auto const &pair : botnet_servers)
    {
        Botnet_server *botnet_server = pair.second;

        // if the group_id is not in the mailbox then just send message count = 0.
        map<string, vector<std::pair<string, string>>>::iterator it = mail_box.find(botnet_server->group_id);

        if (it != mail_box.end())
        {
            int message_count = mail_box[botnet_server->group_id].size();
            message.arguments[0] = to_string(message_count);
        }
        else
        {
            message.arguments[0] = "0";
        }

        if (send_and_log(botnet_server->sock, message) < 0)
        {
            string error("-- Keepalive message to " + botnet_server->group_id + " failed. Disconnecting them --");
            if_verbose(error);
            close_botnet_server(botnet_server->sock, open_sockets, maxfds);
        }
        else
        {
            if_verbose("-- Successfully sent keepalive: " + message.to_string() + " to server " + botnet_server->to_string() + " --");
        }
    }
}

int main(int argc, char *argv[])
{
    int listenSocket;                 // Socket for connections to server
    int clientSock;                   // Socket of connecting client
    fd_set open_sockets;              // Current open sockets
    fd_set read_sockets;              // Exception socket list
    fd_set except_sockets;            // Exception socket list
    int maxfds;                       // Passed to select() as max fd in set
    struct timeval keepalive_timeout; // Time between keepalives
    OUR_IP = get_local_address();
    initialize_log_file();
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
    if (connect_to_botnet_server(botnet_server_ip, botnet_server_port, maxfds, open_sockets) == 0)
    {
        if_verbose("Connected to botnet server");
        Message success("SUCCESS,Connected to indicated server");
        send_and_log(clientSock, success);
    }
    else
    {
        //  TODO: maybe have this in a loop so if the initial client connection string is wrong then we dont have to restart everything?
        Message failure("FAILURE,Failed to connect to indicated server");
        send_and_log(clientSock, failure);
        exit(0);
    }

    if_verbose("-- maxfds is: " + to_string(maxfds) + " --");

    while (true)
    {
        if_verbose("-- open sockets: " + fd_set_to_string(open_sockets, maxfds) + " --");
        // Get modifiable copy of readSockets
        read_sockets = except_sockets = open_sockets;

        if_verbose("-- read sockets before select: " + fd_set_to_string(read_sockets, maxfds) + " --");

        if_verbose("-- selecting --");

        // Look at sockets and see which ones have something to be read()
        keepalive_timeout.tv_sec = 60;
        int n = select(maxfds + 1, &read_sockets, NULL, &except_sockets, &keepalive_timeout);

        if_verbose("-- read sockets after select: " + fd_set_to_string(read_sockets, maxfds) + " --");

        if (n < 0)
        {
            perror("select failed - closing down\n");
            break;
        }
        else if (n == 0)
        {
            // select timed out
            // we send keepalive
            send_keep_alive_messages(open_sockets, maxfds);
        }

        if_verbose("-- outside if new_connections --");

        // if there is an incoming connection and we have room for more connections.
        if (FD_ISSET(listenSocket, &read_sockets))
        {
            if_verbose("-- inside if new_connections --");
            // Check if we have room
            if (botnet_servers.size() < MAXCONNECTEDSERVERS)
            {
                // accepts connection, maybe changes maxfds, add to botnet_servers
                new_connections(listenSocket, maxfds, open_sockets);
                
            }
            else
            {
                if_verbose("-- the botnet is full --");
            }
            n--;
        }

        if_verbose("-- outside if client commands --");

        if (FD_ISSET(clientSock, &read_sockets))
        {
            // check for commands from client
            deal_with_client_command(clientSock, open_sockets, maxfds);
            n--;
        }

        // for each connected server check for any incomming commands/messages
        for (auto const &pair : botnet_servers)
        {
            if_verbose("-- outside if server commands --");

            Botnet_server *botnet_server = pair.second;
            if (FD_ISSET(botnet_server->sock, &read_sockets))
            {
                deal_with_server_command(botnet_server, open_sockets, maxfds);
                n--;
            }
        }
    }
}
