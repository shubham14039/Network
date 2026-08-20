#include <print>
#include <memory>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <cstdint>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <array>


namespace NetworkConfig{
    static inline constexpr const char* Port = "3490";
    inline constexpr int MaxDataSize = 100;
}

struct AddrInfoDelete{
    void operator()(addrinfo* ptr) const{
        if (ptr){
            freeaddrinfo(ptr);
        }
    }
};

struct SocketCloser{
    void operator()(int* sock_fd) const{
        if (sock_fd && *sock_fd >= 0){
            close(*sock_fd);
            delete sock_fd;
        }
    }
};

void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET){
        return &(reinterpret_cast<struct sockaddr_in *>(sa)->sin_addr);
    }
    return &(reinterpret_cast<struct sockaddr_in6 *>(sa)->sin6_addr);
}

int main(int argc, char* argv[]){
    int sockfd, numbytes;
    std::array<char, NetworkConfig::MaxDataSize> buff{};
    struct addrinfo hints{}, *servinfo, *p;
    int rv;
    std::array<char, INET6_ADDRSTRLEN> s{};

    if (argc != 2){
        std::print(stderr, "Usage: client hostname\n");
        std::exit(EXIT_FAILURE);
    }

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;


    rv = getaddrinfo(argv[1], NetworkConfig::Port, &hints, &servinfo);
    if (rv != 0){
        std::print(stderr, "getaddrinfo: {}\n", gai_strerror(rv));
        return 1;
    }

    std::unique_ptr<addrinfo, AddrInfoDelete> res_guard(servinfo);

    for (p = servinfo; p != nullptr; p = p->ai_next){
        int rawfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (rawfd == -1){
            std::print(stderr, "client: socket\n");
            continue; 
        }
        std::unique_ptr<int, SocketCloser> managed_sock(new int(rawfd));

        inet_ntop(p->ai_family, get_in_addr(reinterpret_cast<struct sockaddr *>(p->ai_addr)), s.data(), s.size());
        std::print(stdout, "client: attempting connetion to {}\n", s.data());

        if(connect(rawfd, p->ai_addr, p->ai_addrlen) == -1){
            std::print(stderr, "client: connect\n");
            continue;
        }

        sockfd = rawfd;
        managed_sock.release(); //TAG: FAULT: I believe memory leak is happening hare. Would work on later. 
        delete managed_sock.get();
        break;
    }

    if (p == nullptr){
        std::print(stderr, "client: failed to connect!\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr(reinterpret_cast<struct sockaddr *>(p->ai_addr)), s.data(), s.size());
    std::print(stdout, "client: connected to {}\n", s.data());
    

    if ((numbytes = recv(sockfd, buff.data(), NetworkConfig::MaxDataSize -1, 0)) == -1){
        std::print(stderr, "recv: \n");
        std::exit(EXIT_FAILURE);
    }

    std::print("client: recieved {}\n", std::string_view(buff.data(), numbytes));
    close(sockfd);

    return 0;
}
