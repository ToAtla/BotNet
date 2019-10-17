//
// A BotNet client for Computer Networking
//
// Command line: ./client <server_ip> <server_port>
//
// Author: Þórður Atlason (thorduratl17@ru.is) and Þórður Friðriksson (thordurf17@ru.is)
//
// Computer Networking at RU Fall 2019
//
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <iostream>
#include <ctime>
using namespace std;

string get_timestamp(){
    time_t now = time(0);
    string time_string(ctime(&now));
    time_string.pop_back();
    return time_string;
}

int main(int argc, char *argv[])
{

    string hardcoded_string = "CONNECTTO,130.208.243.61,4043";
    // user arguments should be exactly 3, the ip address of the server and the port number
    if (argc != 3)
    {
        printf("Usage: client <ip address> <ip port>\n");
        exit(0);
    }

    string ip_string(argv[1]);  // get ip address from user argument
    int portno = atoi(argv[2]); // get port number from user argument and convert to int
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd < 0)
    {
        perror("Failed to open socket");
        return (-1);
    }

    struct sockaddr_in server_socket_addr;                             // address of server
    memset(&server_socket_addr, 0, sizeof(server_socket_addr));        // Initialise memory
    server_socket_addr.sin_family = AF_INET;                           // pv4
    server_socket_addr.sin_addr.s_addr = inet_addr(ip_string.c_str()); // bind to server ip
    server_socket_addr.sin_port = htons(portno);                       // portno

    // connect to server
    if (connect(socketfd, (struct sockaddr *)&server_socket_addr, sizeof(server_socket_addr)) < 0)
    {
        perror("Failed to connect");
        return (-1);
    }
    string command = "";
    do
    {
        cout << endl;
        cout << "Enter a command to the server" << endl;
        getline(cin, command);

        if (command.size() > 0 && command.compare("exit")) // bigger then 0 and not exit
        {
            if (!command.compare("connect")) {
                command = hardcoded_string;
            }
            // send the server the command
            if (send(socketfd, command.c_str(), command.size() + 1, 0) < 0)
            {
                perror("Failed to send command to server");
                return (-1);
            }
            else
            {
                
                cout << get_timestamp() << " SENT     >> " << command << endl;
                int responseSize = 6000;
                char response[responseSize];
                memset(response, 0, responseSize); // zero initialize char array
                int byteCount = recv(socketfd, response, responseSize, 0); // this blocks and waits til it receives something from the server
                if (byteCount < 0)
                {
                    cout << "error receiving output from server" << endl;
                }
                else
                {   
                    response[byteCount] = '\0'; // make sure to end the string at the right spot so we dont read of out memory
                    
                    cout << get_timestamp() << " RECEIVED << " << response << endl;
                }
            }
        }
    } while (command.compare("exit")); // while command is not exit

    return 0;
}