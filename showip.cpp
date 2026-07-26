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

int main(int argc, char *argv[]){
    struct addrinfo hints{}, *res, *p;
    int status;
    char ipstr[INET6_ADDSTRLEN];

    if (argc != 2){
        std::cerr << "usage: shoip hostname\n";
        return 1;
    }

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(argv[1], NULL, &hints, &res)) != 0){
        std::string error = std::format("getaddrinfo: {}\n", gai_strerror(status));
        std::cerr << error << std::endl;
        return 2;
    }

    //After all the guardrails are checked then only we would print the DNS name for that given IP address.
    std::string message = std::format("IP address for {}:\n", argv[1]);
    std::cout << message;

    //TAG: ERROR: Infinite loop entered. Dont know why
    for (p = res; p != NULL; p -> ai_next){
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
