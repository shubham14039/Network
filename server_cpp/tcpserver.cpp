#include <iostream>
#include <print>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view> //for non modifying strings
#include <cerrno>
#include <csignal>
#include <atomic> //multi threaded C++ uses atomic
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace NetworkConfig{
    static inline constexpr const char* Port = "3490";
    inline constexpr int BackLog = 10;
}

void *get_in_addr(struct sockaddr *sa){
    if (sa -> sa_family == AF_INET){
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);

}

struct SocketCloser{
    void operator()(int* sock_fd) const{
        if (sock_fd && *sock_fd >= 0){
            close(*sock_fd);
            delete sock_fd;
        }
    }
};

struct AddrInfoDelete{
    void operator()(addrinfo* ptr) const{
        if (ptr){
            freeaddrinfo(ptr);
        }
    }
};

int main(void){
    int sockfd, new_fd;
    struct addrinfo hints{}, *servinfo, *p;
    struct sockaddr_storage their_storage;
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    std::array<char, INET6_ADDRSTRLEN> s{};
    int rv;

    // std::memset(&hints, 0, sizeof(hints)); //not to be used here because we have already declared hints{} with empty braces.
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(nullptr, NetworkConfig::Port, &hints, &servinfo)) != 0){
        std::print(stderr, "getaddrinfo: {}\n", gai_strerror(rv)); //under the hood, std::print uses the std::format's formatting engine to write straingt to the console efficiently without unnecessary heap allocations.
        return 1; //exit the program
    }


    for (p = servinfo; p != nullptr; p = p->ai_next){
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            std::string errMsg_socket = std::error_code(errno, std::system_category()).message();
            std::print(stderr, "server: socket: {}\n", errMsg_socket);
            continue;  //if this fails, move to the next iteration.
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
            std::string errMsg_setsockopt = std::error_code(errno, std::system_category()).message();
            std::print(stderr, "server: setsockopt: {}\n", errMsg_setsockopt);
            std::exit(EXIT_FAILURE); //exit the program immediatly.
        }

        //if all those goes well, we will bind the socket
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1){
            std::unique_ptr<int, SocketCloser> managed_sock(new int(sockfd));
            std::string errMsg_bind = std::error_code(errno, std::system_category()).message();
            std::print(stderr, "server: bind: {}\n", errMsg_bind);
            continue;
        }


        break;
    }

    std::unique_ptr<addrinfo, AddrInfoDelete> res(servinfo);

    if (p == nullptr){
        std::print(stderr, "server: failed to bind\n");
        std::exit(EXIT_FAILURE);
    }
     
    if (listen(sockfd, NetworkConfig::BackLog) == -1){
        std::print(stderr, "listen\n");
        std::exit(EXIT_FAILURE);
    }
    
    //sigchild handler part
    return 0;
}
