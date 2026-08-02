// The system call functions:
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>

//Signature for socket() syscall
int socket(int domain, int type, int protocol);
// On success, the socket() function returns a non negative integer - socket descriptor. or -1 on error.

// domain specifies the protocol family and it is one of the follwing:
// AF_INET for IPv4
// AF_INET6 for IPv6
// AF_LOCAL for Unix domain protocols
// AF_ROUTE for routing sockets
// AF_KEY for Key socket

// type is type of socket and of one of the follwing:
// SOCK_STREAM for stream socket
// SOCK_DGRAM for datagram socket
// SOCK_SEQPACKET for sequenced packet socket
// SOCK_RAW for raw socket

// protocol of sockets for AF_INET and AF_INET6: 
// IPPROTO_TCP for TCP transport protocol
// IPPROTO_UDP for UDP transport protocol
// IPPROTO_SCTP for SCTP transport protocol



//Signature for bind() syscall.
int bind(int sockfd, struct sockaddr *my_addr, int addrlen);
// sockfd is the socket file descriptor returned by the socket() syscall
// my_addr is a pointer to a struct sockaddr that contains information about the address
// addrlen is the length in bytes of that address. TAG: EXP: Why do we need length of that address (in bytes)? -> First of all, that *my_addr is a pointer so the kernel cannot infer anything about that address. It is just a normal pointer. Secondly, bind() verifies if the length provided and 

//TAG: EX: Example usage.
struct addrinfo hints{}, *res; //hints -> what type of address we want. res -> where to write the fetched addresses. To be given to getaddrinfo().
int sockfd;
hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
hints.ai_socktype = SOCK_STREAM;
hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

getaddrinfo(NULL, "3490", &hints, &res); //getaddrinfo() wrote the memory res. Also, port below 1024 are reserved (Unless you are the superuser). Can have any port above that, upto 65535. Provided, they are not being used by any other program.
// here getaddrinfo() is not just performing a DNS lookup. But acting as a configuration engine too
sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol); //No hardcoding. Use whatever getaddrinfo() determined. 

// bind it to the port we passed in to getaddrinfo():
bind(sockfd, res->ai_addr, res->ai_addrlen);
//bind also returns -1 on error.



//Signature for connect() syscall
int connect(int sockfd, struct sockaddr *serv_addr, int addrlen);
//sockfd is the file descriptor for the socket.
//*serv_addr is the destination server address. 
//addrlen is the length of the address in bytes. Provided because the kernel needs to know how many bytes to read (where the server address is stored).
//All these values can be gleaned from the results of getaddrinfo(), which writes the results to {res}.

//TAG: EX: Example usage
getaddrinfo("www.example.com", "3490", &hints, &res);
//create a socket
sockfd = socekt(res->ai_family, res->ai_socktype, res->ai_protocol);

//connect
connect(sockfd, res->ai_addr, res->ai_addrlen);


//Signature for listen() syscall
int listen(int sockfd, int backlog);
//sockfd is the socket file descriptor. The socket that we created and it is being used for listening to incoming connections.
//backlog is the number of connectons allowed on the incoming queue. Incoming connections would be waiting in this queue until we accept() (next syscall) them; and this is the number of connections allowed in the queue. Most systems silently limit this to around 20ish connections.

//Signature for accept() syscall
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
//TAG: EXPLANATION: accept() does not accepts packets. Instead it accepts the connection requests, that are queued up using listen() syscall. When a new connection is accepted, one request is removed from the queue. And another socket is created internally by the kernel for that particular connection. And that new socket has many attributes including, but not limited to, Remote IP, Remote port, TCP sequence numbers etc. Alongwith copied attributes like Local IP, Local port, Protocol, and other socket options like SO_REUSEADDR (Reusing same port number for multiple sockets).
//sockfd is the listening socket file descriptor that we created.
//*addr is the pointer to the address 
//
//TAG: MENTAL_MODEL: socket() -> Ask for a socket from the kernel. Now we have a socket with no label. bind() -> Gives that label to that newly assigned socket. We are essentially telling the kernel that 'if packets arrive for this particular address (that we specified in the bind syscall), handle them to this particular socket'. Now we want our socekt to listen to incoming connections. That why we use listen(). It listens to incoming connection requests. accept() syscall accepts those incoming connections and removes the first connection from the queue (that listen() formed while waiting for incoming connecitons). Behind the scenes, the kernel already performed the hadnshake and returned a file descriptor. The accept() claims the ownership of the a connection the kernel has already established, and removes the first request from the queue. Now after the handshake, the kernel creates another socket,instead modifying the original one, with same attributes and additional attrubutes like remote address. The original, listening, socket has no remote address. The listening socket never turned into the connected socket. It is still listening for incoming connection requests.



//Signature for send() and recv() syscalls
int send(int sockfd, const void *msg, int len, int flags);
//sockfd is the socket you want to send data to
//*msg is the pointer to the data you want to send. Notice that we are providing the pointer to the address and not the actual message content. Actually, this is for kernel. The kernel reads data from that address and sends raw bytes (not the actual characters or numbers). And that's why we also provide len. Why const? -> so that the data remains read only and not modifiable.
//len is the length of the data stream in bytes. This is porvided to prevent the kernel to send extra or less data. If we dont provide len, the kernel would not know how many bytes of data to send.
//flags lets us to modify the behaviour of this particular send operation. falgs is not a part of socket attributes.

int recv(int sockfd, void *buf, int len, int flags);
//sockfd is the socket descriptor to read from
//*buf is the buffer to read information into
//len is the maximum length of the buffer
//flags are the behaviour flags

//TAG: IMP: Data arrives at the Network card -> kernel -> socket recieve queue. That means the data is already in our machine but not written in the memory. It is stored in the socket's recieve queue. Only when we recv(), that data is copied into our program. 
