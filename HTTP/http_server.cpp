//TAG: TODO: Implement error handling
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <stdexcept>
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


class HTTP {
    private:
        class ScopedSocket{
            private:
                int sock_fd;

            public:
                //Acquire the socket in the constructor
                ScopedSocket(int domain, int type, int protocol){
                    sock_fd = socket(domain, type, protocol);
                    if (sock_fd < 0){
                        throw std::runtime_error("[-] Failed to create a socket!\n");
                    }
                }

                //Constructor overload for an already existing file descriptor
                explicit ScopedSocket(int fd) : sock_fd(fd){}

                //Release the socket in the destructor
                ~ScopedSocket(){
                    close();
                }

                //Delete copy semantics to double deleting the same file descriptor
                ScopedSocket(const ScopedSocket&) = delete;
                ScopedSocket& operator = (const ScopedSocket&) = delete;

                //Implement move constructor (Transfer ownership & prevent dangling state)
                ScopedSocket(ScopedSocket&& other) noexcept : sock_fd(other.sock_fd){
                    other.sock_fd = -1;
                }

                //Implmentation of move assignment operator
                ScopedSocket& operator = (ScopedSocket&& other) noexcept{
                    if (this != &other){
                        close();
                        sock_fd = other.sock_fd;
                        other.sock_fd = -1;
                    }
                    return *this;
                }

                //Close function 
                void close(){
                    if (sock_fd >= 0){
                        ::close(sock_fd);
                        sock_fd = -1;
                    }
                }

                //Accessor
                int get() const {
                    return sock_fd;
                }

                //Check if the socket wrapper holds the valid resource
                bool isValid() const {
                    return sock_fd >= 0;
                }

        };

        static constexpr int BackLog = 10;
        static constexpr int MaxDataSize = 4096;
        const char* Port = nullptr; //To be passed by the program
        struct addrinfo hints{}, *servinfo;
        struct sockaddr_storage their_addr_storage;
        int yes = 1;
        socklen_t sin_size;
        std::array<char, INET6_ADDRSTRLEN> s{};
        std::array<char, MaxDataSize> buffer{}; //4KB of buffer size
        int rv;
        ScopedSocket sockfd;

        //TAG: TODO: Write a wrapper class for managing socket. This wrapper class owns the resources about sockfd.
        int start(){
            if (!sockfd.isValid()){
                std::print(stderr, "[-] Error: Socket is not valid!\n");
                return 1;
            }

            hints.ai_family = AF_INET6;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_flags = AI_PASSIVE;

            if ((rv = (getaddrinfo(nullptr, Port, &hints, &servinfo))) != 0){
                std::print(stderr, "[-] {}\n", gai_strerror(rv));
                return 1;
            }


            if (setsockopt(sockfd.get(), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0){
                std::string err_msg_setsock = std::error_code(errno, std::system_category()).message();
                std::print(stderr, "[-] {}\n", err_msg_setsock);
                std::exit(EXIT_FAILURE); //TAG: EXIT: Exit the whole program. Would terminate for a specific HTTP object or the whole program. TEST!
                //TAG: TODO: Throw the error code and pass it to exception handler
            }

            std::print("[+]: socket created successfully on port: {}\n", Port);

            if ((rv = (bind(sockfd.get(), servinfo->ai_addr, servinfo->ai_addrlen))) < 0){
                std::string err_msg_bind = std::error_code(errno, std::system_category()).message();
                std::print(stderr, "[-] Binding failed: {}\n", err_msg_bind);
                return 1;
                //TAG: TODO: Therow the error code and pass it to exception handler
            }
            freeaddrinfo(servinfo);

            if ((rv = (listen(sockfd.get(), BackLog))) < 0){
                std::string err_msg_listen = std::error_code(errno, std::system_category()).message();
                std::print(stderr, "[-] Listening failed");
                return 1;
                //TAG: TODO: Throw the error code and pass it to exception handler
            }

            std::print(stdout, "[+] Server listening for incoming connections on port {} . . .\n", Port);
            return sockfd.get();
        }


        void *get_in_addr(struct sockaddr *sa){ //void because it has to handle two different kinds of structures
            if (sa->sa_family == AF_INET){
                return &((reinterpret_cast<struct sockaddr_in*>(sa))->sin_addr);
            }
            return &((reinterpret_cast<struct sockaddr_in6*>(sa))->sin6_addr);
        }

        bool run(ScopedSocket sockfd){
            while(true){
                sin_size = sizeof(their_addr_storage);
                int acceptfd = accept(sockfd.get(), reinterpret_cast<struct sockaddr*>(&their_addr_storage), &sin_size);
                if (acceptfd < 0){
                    std::print(stderr, "[-] Accept failed from {}\n", Port);
                    continue;
                }

                ScopedSocket client_socket(acceptfd)

                inet_ntop(their_addr_storage.ss_family, get_in_addr(reinterpret_cast<struct sockaddr*>(&their_addr_storage)), s.data(), s.size());
                std::print(stdout, "[+] Got a connection from {} on {}\n", s.data(), Port);

                pid_t pid = fork();
                if (pid < 0){
                    std::print(stderr, "[-] Fork failed!\n");
                    continue;
                }

                if (pid == 0){
                    sockfd.close();
                    int numbytes = recv(client_socket.get(), buffer.data(), MaxDataSize - 1, 0);
                    if (numbytes < 0){
                        std::print(stderr, "[+] Recieve failed!\n");
                        std::exit(EXIT_FAILURE);
                    }
                    std::print(stdout, "[+] Recieved {}\n", std::string_view(buffer.data(), numbytes));

                    //sending data goes here
                    //What ever data is requested from the client
                    //that is parsed and handled according to the uri
                    //and then that particular data is sent
                    //TAG: HOW: How to implement that
                    //
                    //
                    //
                    //
                    //
                    //
                    std::exit(EXIT_SUCCESS);
                }
            }
            return true; //TAG: IDK:
        }

        void stop(){
            //stopping the server file descriptor and releasing its resources
            //using the custom wrapper class
        }


    public:

        void parse_incoming(){
            //TAG: TODO: parse the incoming HTTP request stream
        }

        void parse_outgoing(){
            //TAG: TODO: header pieces for return response stream
        }

        HTTP(const char* port) : sockfd(AF_INET6, SOCK_STREAM, IPPROTO_TCP), Port(port){
            //construcctor body
            //TAG: TODO: check if the port is available for use or not. we cannot check that on ourselves. We have to trust on the kernel to specify that
            //TAG: TODO: If not available then throw error or suggestion 
            //TAG: TODO: If available then initialise the server on that port
        }

        ~HTTP(){ //destructor body
            stop(); //calling the stop function.
        }


};

int main(int argc, char* argv[]){
    if (argc != 2){
        std::print(stderr, "USAGE: provide valid port number\n");
        std::exit(EXIT_FAILURE);
    }
    return 0;
}
