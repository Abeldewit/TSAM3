# TSAM3 - Group100 
# Abel de Wit & Hartmann Ingvarsson 
### Botnet project

Source control = Github, 

OS = Mac and Ubuntu 

However later in the project we ran into some difficulties running on my Linux distro, mainly running an STD Logic error because of a  null reference error, something to to with the tokens?. However working on skel.ru.is didn't bring us any problems, and the build is stable on MAC OS.


*To compile:*

Use the makefile, open a terminal and write make
Or if you don't wanna use the make file, you can compile the server and client separately. 
g++ -std=c++11 client.cpp -lpthread -o client
g++ -std=c++11 server.cpp -o server

*To run* 
To run (on same machine):

    ./tsamvgroup100 4100
    ./client 127.0.0.1 4100


### Commands on client:

To be able to send commands that are executed on and to the server, a user most login with the password 
"100!"  Commands are excuted as: COMMAND,<parameter> and ar accepted as such.

##### Finished commands: 

``PASS,<password>``  : To be able to run server commands, and connect to the botnet, a client needs to be logged in.

``CONNECT,<ipaddress>,<portnumber>`` : Connect to server from its ip address and portnumber. 

``SENDMSG, GROUP ID``: Send a message to the server for the GROUP ID.

`` LISTSERVERS ``    : List servers your server is connected to.

##### Unfinished commands: 

``GETMSG, GROUP ID `` Get a single message from the server for the GROUP ID.

### Commands on server:
Messages between servers are executes as follows: 
<SOH><Command>,< comma separated parameters ><EOT>

##### Finished commands:

`` LISTSERVERS,<FROM GROUP ID> `` :  Reply with servers response (below).

`` SERVERS  `` :  Provide a list of directly connected - i.e. 1-hop, servers to this server.
    The first IP address in the list should be the IP of the connected server.
    Servers should be specified as GROUP ID, the HOST IP,
    and PORT they will accept connections on, comma sepa-
    rated within the message, each server separated by ;
    SERVERS,P_GROUP_100,130.208.243.61,8888;P_GROUP_2,10.2.132.12,888;.
    
`` KEEPALIVE,<No. of Messages>``  Periodic message to 1-hop connected servers, indicating still
    alive and the no. of messages waiting for the server at the
    other end. Do not send more than once/minute.
    
##### Unfinished commands:


`` GET MSG,<GROUP ID>``  Get messages for the specified group. This may be for your
own group, or another group.
    
`` SEND MSG,<FROM GROUP ID>,<TO GROUP ID>,<Message content>`` 
Send message to another group
    
`` LEAVE,SERVER IP,PORT``  Disconnect from server at specified port.

`` STATUSREQ,FROM GROUP``  Reply with STATUSRESP as below

`` STATUSRESP,FROM GROUP,TO GROUP,<server, msgs held>,...`` 
Reply with comma separated list of servers and no. of messages you have for them
eg. 

`` STATUSRESP,V GROUP 2,I 1,V GROUP4,20,V GROUP71,2``  server at the
other end. Do not send more than once/minute.

### Our goals:

1. Implement client and server as described above. All local commands to the server must be
implemented by a separate client over the network. (4 points)

2. Provide a wireshark trace of communication between your client and server for all commands
implemented (2 points)

3. Have been successfully connected to by an Instructor’s server. (1 point)

6. Code is submitted as a single tar file, with README and Makefile. (Do not include
hg/git/etc. repositories.) (1 point)

##### Bonus: 
• Provide a time stamped plaintext communication from the Oracle. (Time stamp should be
when you received it. The oracle will respond to any messages addressed to it and delivered.
It may move.)


• Decode a hashed message from the oracle (1 point)

By sending a SEND_MSG to the Oracle after we found it we recieved a MD5 Hash. 
 ***efb5803993a803fe4414fa2ca001a84f***

Decrypting that gives us a supersecretpassword: **"away"**



