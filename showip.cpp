#include <iostream>
#include <string>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define INET_ADDSTRLEN 16
#define INET6_ADDSTRLEN 46

int main(int argc, char *argv[]){ //The script gets inouts through the command line. Via arg count {int argc (auto)} and char *argv[].
    struct addrinfo hints{}, *res, *p; //This object is for ourselves. But the linked list (*res) returned by the getaddrinfo() has unknown size, so {res} must be a pointer. And *p is used as a flag when we visit every node sequentially.
    int status;
    char ipstr[INET6_ADDSTRLEN];

    if (argc != 2){
        std::cerr << "usage: shoip hostname\n";
        return 1;
    }

    hints.ai_family = AF_UNSPEC; //Do not care what type of address is it. IPv4(AF_INET) or IPv6(AF_INET6) 
    hints.ai_socktype = SOCK_STREAM; //TCP (SOCK_STREAM) or UDP (SOCK_DGRAM)

    //TAG: SIGN: Signature for getaddrinfo() function. This is not a system call but a function provided by standard library and it may perform however number of sys-calls to get the desired output.
    // int getaddrinfo(
    // const char *node,    //This is the host. It can be anything of hostname, IPv4 or IPv6 address. 
    // const char *service,    //This is usually the port. again it accepts char argv[2] from the command line. In this case, we do not privide the port number  
    // const struct addrinfo *hints,    //This is where you describe what type of address you want. See  lines 14, 23 and 24.
    // struct addrinfo **res    //This is the output. Where the function will write the output. 
    // )
    if ((status = getaddrinfo(argv[1], NULL, &hints, &res)) != 0){
        std::string error = std::format("getaddrinfo: {}\n", gai_strerror(status));
        std::cerr << error << std::endl;
        return 2;
    }

    //After all the guardrails are checked then only we would print the DNS name for that given IP address.
    std::string message = std::format("IP address for {}:\n", argv[1]);
    std::cout << message;

    for (p = res; p != NULL; p = p  -> ai_next){
        void *addr;
        char *ipver; 
        struct sockaddr_in *ipv4;
        struct sockaddr_in6 *ipv6;

        if (p -> ai_family == AF_INET){
            ipv4 = (struct sockaddr_in *)p -> ai_addr;
            addr = &(ipv4 -> sin_addr);
            ipver = "IPv4";
        } 
        else {
            ipv6 = (struct sockaddr_in6 *)p -> ai_addr;
            addr = &(ipv6 -> sin6_addr);
            ipver = "IPv6";
        }

        inet_ntop(p -> ai_family, addr, ipstr, sizeof ipstr);
        std::string output = std::format("{}, {} \n", ipver, ipstr);
        std::cout << output;
    }
    freeaddrinfo(res);
    

    return 0;

}
