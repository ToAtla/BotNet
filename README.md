# BotNet
A client and a server implementation for a BotNet.
Project for a Networking class at Reykjavik University Fall 2019

## Description
Two botnet servers can connect to one another, exchange information about active connections, send and recieve messages and disconnect from one another.
Each server 
  - is controlled by a single client
  - can only have one client but can connect to many servers.
  - is identified by a group_id

 Each client
  - can only connect to a single server
  - issues commands to its servers, or to other servers on the botnet through its server.

### Additional points
  - The server logs all sent and recieved messages into the file: ``` logfile.txt ```
  - Each connected server gets dropped after 600 seconds, if the server is at full capacity
  - Each newly connected server is greeted with a welcoming message
  - A VERBOSE variable controls the amount of informational logging to the server's terminal
  - A KEEPALIVE message is sent to each connected server every 60 seconds if no traffic has appeared
  - Each command needs to be encapsulated by a SOH and EOT to mark beginnings and ends of messages




## Compilation
Compile both the server and client with through the make file
``` make ```
Which runs the following commands behind the scenes
```g++ -Wall -std=c+=11 server.cpp -o server```
```g++ -Wall -std=c++11 client.cpp -o client```

Tested on Linux and MacOS

## Usage
The server is started using the following command
```./tsamgroup75 <port>```
where <port> indicates the port where incoming connections will be received.

Then the client can then be started and connected to the server 
```./client <server_ip> <server_port>```

The first command from the client must be a CONNECTTO command to connect the server to the BotNet:

![Server screenshot](images/server.png?raw=true)

From the server's side

![Client screenshot](images/client.png?raw=true)

## Command List
A Botnet consists of a network of server communicating with one another. 
To segregate other servers on the network from our server, we refer to other servers as ext_servers in this documentation.
### External Servers
The server can process the following commands from ext_servers

| Command | What it does | Example |
| ------ | ------ |------ |
| LISTSERVERS,<FROM_GROUP_ID> | Sends back a list of ext_servers connect to the server | LISTSERVERS,GROUP_62 |
| KEEPALIVE,<MSG_COUNT> | Maintains the connection between two directly connected servers, and indicates the number of messages waiting for the ext_server on the server| KEEPALIVE,0 |
| GET_MSG,<GROUP_ID> | Returns a single message for the given group, if any exist | GET_MSG,GROUP_15 |
| SEND_MSG,<FROM_GROUP_ID><TO_GROUP_ID>,<MESSAGE> | Stores the incoming message in a mailbox | SEND_MSG,GROUP_17,GROUP_15,You are divisible by three |
| LEAVE,<IP>,<PORT> | Disconnects from the specified server | LEAVE,130.208.243.4000 |
| STATUSREQ,<FROM_GROUP_ID> | Lists the groups we have messages for and how many | STATUSREQ,GROUP_2 |
| STATUSRESP,<FROM_GROUP_ID> | Sends GET_MSG commands to retrieve messages for each listed group | STATUSRESP,GROUP_6 |

### Client
The server can process the following commands from the client

| Command | What it does | Example |
| ------ | ------ |------ |
| CONNECTTO,<IP>,<PORT> | Connects the server to the given ext_server | CONNECTTO,130.208.243.61,4000 |
| LISTSERVERS | Displays the ext_servers connected to the server | LISTSERVERS |
| LISTREMOTE,<GROUP_ID>, | Displays the ext_servers connected to another ext_server | LISTREMOTE,P3_GROUP_13 |
| SENDMSG,<GROUP_ID>,<MSG> | Sends a message to the given group(ext_server) | SEND_MSG,GROUP_15,You Rock! |
| GETMSG,<GROUP_ID> | Displays the last message for the given group id | GET_MSG,GROUP_15 |
| CUSTOM,<TARGET_GROUP>,<COMMAND> | Sends the target command as is to the target server. (only the first two commas serve are command seperators) | CUSTOM,GROUP_66,LISTSERVERS,GROUP_55 |


# Created by
Þórður (Thor) Atlason - thorduratl17@ru.is
Þórður (Doddi) Friðriksson - thordurf17@ru.is