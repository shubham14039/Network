// TAG: HELP: I dont have any idea about what is this or i cant picture what it does.
#include <iostream>
#include <cstdint>
#include <cstddef>

struct addrinfo{
    int ai_flags;
    int ai_family; //This specifies the address family. Could be anything form AF_INET (for IPv4), AF_INET6 (for IPv6) or AF_UNSPEC (IPv4 or IPv6)
    int ai_socktype; //Socket type. Stream (SOCK_STREAM) or Datagram (SOCK_DGRAM)
    int ai_protocol; //Specific protocol to be used. Often you don't need to explicitely specify it.  
    std::size_t ai_addrlen; //How many bytes does the structure pointed by ai_addr occupy?
    struct sockaddr *ai_addr; 
    char *ai_canonname; //Points to the canonical host name of the host.
    struct addrinfo *ai_next; //This makes the structure a linked list. And getaddrinfo() function returns a full linked list of addresses. So that the program runs each one of them one by one until one succeds.
};
 
struct sockaddr{
    unsigned short sa_family;
    char sa_data[14];
};


struct sockaddr_in{
    short int sin_family;
    unsigned short int sin_port;
    struct in_addr sin_addr; //TAG: NOTE: we can theoretically just use unint32_t sin_addr inplace of using struct, but that object would just be a 32 bit integer. On the other hand this struct object is a guaranteed ipv4 object. Both for readability purposes, and acts as documentation and error checking. Future expansion is another reason. maybe, we could add something to the struct in_addr which wouldnt need us to change every implementation. 
    unsigned char sin_zero[8];
};

struct in_addr{
    std::uint32_t s_addr;
};

struct sockaddr_storage{
    sa_family ss_family; 
    //all this is padding. Implementation specifc, ignore it for now
    // char __ss_pad1[_SS_PAD1SIZE];
    // std::int64_t __ss_align;
    // char __ss_pad2[_SS_PAD2SIZE];
};

struct sockaddr_in sa;

inet_pton(AF_INET, "10.12.110.57", &(sa.sin_addr)); //TAG: NOTE: The inet_pton returns -1 for error and 0 for messed up address so check before using the results.

char ip4[INET_ADDRSTRLEN];

inet_ntop(AF_INET, &(sa.sin_addr), ip4, INET_ADDRSTRLEN);
std::cout << "The IP address is: " << ip4 << std::endl;
