# BotNet
To compile:

g++ -std=c++11 client.cpp -lpthread -o client
g++ -std=c+=11 server.cpp o server

To run (on same machine):

    ./server 10000
    ./client 127.0.0.1 10000


Commands on client:

CONNECT <name>   : Connect to server as <name>
WHO              : Show connections to server
MSG <name> <message> : Send message to name
MSG ALL <message>    : Send message to all connected
