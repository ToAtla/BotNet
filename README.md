# BotNet
A client and a server implementation for a BotNet.
Project for a Networking class at Reykjavik University Fall 2019

## Examples

Add screenshots and stuff

## Compilation
g++ -std=c++11 client.cpp -lpthread -o client
g++ -std=c+=11 server.cpp o server

## Usage
To run (on same machine):

    ./server 10000
    ./client 127.0.0.1 10000

## Command List
A Botnet consists of a network of server communicating with one another. To segregate other servers on the network from our server, we refer to other servers as ext_servers in this documentation.
### External Servers
The server can process the following commands from ext_servers

| Command | What it does | Example |
| ------ | ------ |------ |
| LISTSERVERS,<FROM_GROUP_ID> | Sends back a list of ext_servers connect to the server | LISTSERVERS,GROUP_62 |
| KEEPALIVE,<MSG_COUNT> | Maintains the connection between two directly connected servers, and indicates the number of messages waiting for the ext_server on the server| KEEPALIVE,0 |
| GET_MSG,<GROUP_ID> | Gets a message for the given group | GET_MSG,GROUP_15 |
| LEAVE,<IP>,<PORT> | Disconnects from the specified server | LEAVE,130.208.243.4000 |

### Client
The server can process the following commands from the client

| Command | What it does | Example |
| ------ | ------ |------ |
| CONNECTTO,<IP>,<PORT> | Connects the server to the given ext_server | CONNECTTO,130.208.243.61,4000 |
| LISTSERVERS | Displays the ext_servers connected to the server | LISTSERVERS |
| SEND_MSG,<GROUP_ID>,<MSG> | Sends a message to the given group(ext_server) | SEND_MSG,GROUP_15,You Rock! |
| GET_MSG,<GROUP_ID> | Displays the last message for the given group id | GET_MSG,GROUP_15 |
| WHO | Displays all connected ext_servers | Example |
