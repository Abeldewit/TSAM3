# TSAM3 - Group100 
# Abel de Wit & Hartmann Ingvarsson 
### Botnet project


*To compile:*
Use the makefile, open a terminal and write make


g++ -std=c++11 client.cpp -lpthread -o client
g++ -std=c++11 server.cpp -o server

To run (on same machine):

    ./tsamvgroup100 4100
    ./client 127.0.0.1 4100


Commands on client:

``PASS <password>``  : To be able to run server commands, and connect to the botnet, a client needs to be logged in.
``CONNECT <ipaddress> <portnumber>`` : Connect to server from its ip address and portnumber. 
``SENDMSG, GROUP ID``: Send a message to the server for the GROUP ID
`` LISTSERVERS ``    : List servers your server is connected to

Commands on server:

`` LISTSERVERS,<FROM GROUP ID> `` :  Reply with servers response (below)
`` SERVERS  `` :  Provide a list of directly connected - i.e. 1-hop, servers to this server.
    The first IP address in the list should be the IP of the connected server.
    Servers should be specified as GROUP ID, the HOST IP,
    and PORT they will accept connections on, comma sepa-
    rated within the message, each server separated by ;
    SERVERS,P_GROUP_100,130.208.243.61,8888;P_GROUP_2,10.2.132.12,888;
`` KEEPALIVE,<No. of Messages> `` : 
    Periodic message to 1-hop connected servers, indicating still
    alive and the no. of messages waiting for the server at the
other end. Do not send more than once/minute
