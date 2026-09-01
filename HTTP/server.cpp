#include <fstream>
#include <sstream>
#include <print> 
#include <iostream>
#include <system_error>
#include <string_view>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <array>
#include <cstdlib>

struct SocketCloser{
    void operator()(int* sock_fd) const{
        if (sock_fd && *sock_fd){
            close(*sock_fd);
            delete sock_fd;
        }
    std::print(stdout, "socket: resource released!\n");
    }
};

void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET){
        // return &(((struct sockaddr_in *)sa)->sin_addr);
        return &((reinterpret_cast<struct sockaddr_in *>(sa))->sin_addr);
    }
    // return &(((struct sockaddr_in6 *)sa)->sin6_addr);
    return &((reinterpret_cast<struct sockaddr_in6 *>(sa))->sin6_addr);
}

namespace NetworkConfig{
    static inline constexpr const char* Port = "3490";
    inline constexpr int BackLog = 10;
    inline constexpr int MaxDataSize = 8192;
}

int main(void){
    struct addrinfo hints{}, *servinfo;
    int yes = 1;
    struct sockaddr_storage their_addr_storage; //this is populated by the kernel with the address information of the connecting client.
    std::array<char, INET6_ADDRSTRLEN> s{};
    socklen_t sin_size;
    std::array<char, NetworkConfig::MaxDataSize> buffer{};

    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int rv = getaddrinfo(nullptr, NetworkConfig::Port, &hints, &servinfo);
    if (rv != 0){
        std::print(stderr, "getaddrinfo: {}\n", gai_strerror(rv));
        return 1;
    }

    //Socket creation
    int sockfd  = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP); //IPPROTO_TCP is same as passing 0 or 6(the default code for tcp connecitions)
    if (sockfd == -1){
        std::string err_msgSocket = std::error_code(errno, std::system_category()).message(); //TAG: MORE: Talk about more on this specific way of printing errror code.
        std::print(stderr, "server: socket: {}\n", err_msgSocket);
        return 1;
    }

    std::unique_ptr<int, SocketCloser> managed_sock(new int(sockfd)); //TAG: MORE: TAlk more about RAII and its methods (how it is used here specifically);
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0){ //we are changing some values of the newly created socket.
        std::string errMsg_setsockopt = std::error_code(errno, std::system_category()).message();
        std::print(stderr, "server: setsockopt: {}\n", errMsg_setsockopt);
    }
    std::print(stdout, "server: socket created successfully!\n");

    //Binding
    int bind_fd = bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
    if (bind_fd < 0){ //bind failed
        std::string err_msgBind = std::error_code(errno, std::system_category()).message();
        std::print(stderr, "server: bind: {}/n", err_msgBind);
        return 1;
    }
    std::print(stdout, "server: bound successfully!\n");


    //listening
    int listen_fd = listen(sockfd, NetworkConfig::BackLog);
    if (listen_fd < 0){
        std::string err_msgListen = std::error_code(errno, std::system_category()).message();
        std::print(stderr, "server: listen: {}\n",err_msgListen);
        return 1;
    }
    std::print(stdout, "server: listening for incoming connections . . . \n");

    //acceptig connections
    while(1){
        sin_size = sizeof(their_addr_storage);
        int accept_fd = accept(sockfd, reinterpret_cast<struct sockaddr*>(&their_addr_storage), &sin_size); //if we do not care who is conneting, we can set the second argument as nullptr.
        if (accept_fd < 0){
            std::print(stderr, "server: accept: failed!\n");
            continue;
        }

        inet_ntop(their_addr_storage.ss_family, get_in_addr(reinterpret_cast<struct sockaddr *>(&their_addr_storage)), s.data(), s.size());
        std::print(stdout, "server: got a connection from {}\n", s.data());

        pid_t pid = fork();
        if (pid < 0){
            std::print(stderr, "fork: fork failed!\n");
            close(accept_fd);
            continue;
        }

        if (pid == 0){
            close(sockfd); //Child does not need to accept connections.
            int numbytes = recv(accept_fd, buffer.data(), NetworkConfig::MaxDataSize - 1, 0);
            if (numbytes < 0){
                std::print(stderr, "server: recv: recieve failed!\n");
                std::exit(EXIT_FAILURE);
            }
            std::print(stdout, "server: recieved {}\n", std::string_view(buffer.data(), numbytes));

            //Reading the file from the disk in binary format
            std::ifstream file("index.html", std::ios::binary);
            std::string file_content;
            size_t file_size = 0;

            if (file.is_open()) {
                file.seekg(0, std::ios::end);
                file_size = file.tellg();
                file.seekg(0, std::ios::beg);

                std::stringstream buffer_stream;
                buffer_stream << file.rdbuf();
                file_content = buffer_stream.str();
                file.close();
            } else {
                // Fallback if index.html is missing
                file_content = "<h1>404 Not Found</h1><p>index.html missing on server.</p>";
                file_size = file_content.size();
            }
            std::ostringstream response;
            response << "HTTP/1.1 200 OK\r\n"
                     << "Content-Type: text/html; charset=UTF-8\r\n"
                     << "Content-Length: " << file_size << "\r\n"
                     << "Connection: close\r\n\r\n" // The double CRLF (\r\n\r\n) separates headers from body
                     << file_content;

            std::string http_response = response.str();
            if (send(accept_fd, http_response.c_str(), http_response.size(), 0) < 0){
                std::print(stderr, "server: send: failed!\n");
            }

            close(accept_fd);
            std::exit(EXIT_SUCCESS);
        }
        close(accept_fd); //parent's version is closed
    }
    return 0;
}
