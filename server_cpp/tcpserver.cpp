#include <iostream>
#include <array>
#include <system_error>
#include <print>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <cerrno>
#include <csignal>
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

extern "C" void sigchild_handler([[maybe_unused]] int signo){
    int saved_errno = errno;
    while(waitpid(-1, nullptr, WNOHANG) > 0);
    errno = saved_errno;
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
    struct sockaddr_storage their_addr_storage;
    socklen_t sin_size;
    struct sigaction sa; //for sigchild handler
    int yes = 1;
    std::array<char, INET6_ADDRSTRLEN> s{};
    int rv;

    // std::memset(&hints, 0, sizeof(hints)); //not to be used here because we have already declared hints{} with empty braces.
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    rv = getaddrinfo(nullptr, NetworkConfig::Port, &hints, &servinfo);
    if (rv != 0){
        std::print(stderr, "getaddrinfo: {}\n", gai_strerror(rv)); //under the hood, std::print uses the std::format's formatting engine to write straingt to the console efficiently without unnecessary heap allocations.
        return 1; //exit the program
    }

    //servinfo is freed automatically when main exits //TAG: IMM: Immediatly because we are essentially telling that this smart object now owns the responsibility of cleaning up that raw resource. From now onwards no matter what happens in the code, the wrapper guarantees the cleanup.
    std::unique_ptr<addrinfo, AddrInfoDelete> res_guard(servinfo);

    for (p = servinfo; p != nullptr; p = p->ai_next){
        int raw_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (raw_fd == -1){
            std::string errMsg_socket = std::error_code(errno, std::system_category()).message();
            std::print(stderr, "server: socket: {}\n", errMsg_socket);
            continue;  //if this fails, move to the next iteration.
        }

        //Immediate wrapping to prevent leaks on any subsequent errors
        std::unique_ptr<int, SocketCloser> managed_sock(new int(raw_fd));

        if (setsockopt(raw_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
            std::string errMsg_setsockopt = std::error_code(errno, std::system_category()).message();
            std::print(stderr, "server: setsockopt: {}\n", errMsg_setsockopt);
            std::exit(EXIT_FAILURE); //exit the program immediatly.
        }

        //if all those goes well, we will bind the socket
        if (bind(raw_fd, p->ai_addr, p->ai_addrlen) == -1){
            std::string errMsg_bind = std::error_code(errno, std::system_category()).message();
            std::print(stderr, "server: bind: {}\n", errMsg_bind);
            continue; //managed_sock cleans up sockfd automatically here.
        }


        sockfd = raw_fd;
        managed_sock.release();
        delete managed_sock.get();
        break;
    }


    if (p == nullptr){
        std::print(stderr, "server: failed to bind\n");
        std::exit(EXIT_FAILURE);
    }
     
    if (listen(sockfd, NetworkConfig::BackLog) == -1){
        std::print(stderr, "listen\n");
        std::exit(EXIT_FAILURE);
    }
    
    //sigchild handler part
    sa.sa_handler = sigchild_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, nullptr) == -1){
        std::print(stderr, "sigaction\n");
        std::exit(EXIT_FAILURE);
    }

    std::print(stdout, "server: waiting for connections . . . . .\n");

    while(1){ //infinite loop to wait for and accept connections
        sin_size = sizeof(their_addr_storage);
        new_fd = accept(sockfd, reinterpret_cast<struct sockaddr*>(&their_addr_storage), &sin_size);
        if(new_fd == -1){ //if accept failed
            std::print(stderr, "accept: failed\n");
            continue; //to the next iteration
        }

        inet_ntop(their_addr_storage.ss_family, get_in_addr(reinterpret_cast<struct sockaddr*>(&their_addr_storage)), s.data(), s.size()); //converts the clients raw binary address to human readable format and stores in s. 
        std::print(stdout, "server: got connectionn from {}\n", s.data());

        pid_t pid = fork();
        if (pid == -1){
            std::print(stderr, "fork: fork failed\n");
            close(new_fd);
            continue;
        }

        if (pid == 0){
            close(sockfd);
            if (send(new_fd, "Hello from the server!", 22, 0) == -1){
                std::print(stderr, "send: send failed!\n");
            }
            close(new_fd);
            std::exit(EXIT_SUCCESS);
        }
        close(new_fd);
    }
    return 0;
}

//TAG: RAII: stands for Resource Acquisition Is Initialisation - the resource cleanup is immediatly tied to its lifetime. When the resource dies, the reosurce is automatically cleaned by the wrapper and we dont have to write close(), free() etc at the end of every function. Whenver that object goes out of scope, the compiler automatically calls its destructor.
