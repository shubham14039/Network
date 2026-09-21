//TAG: TODO: Implement error handling
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/stat.h>
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
        inline constexpr int Backlog = 10;
        inline constexpr int MaxDataSize = 4096;
        static inline constexpr const char* Port = nullptr; //To be passed by the program
        struct addrinfo hints{}, *servinfo;
        struct sockaddr_storage their_addr_storage;
        int yes = 1;
        socklen_t sin_size;
        std::array<char, INET6_ADDRSTRLEN> s{};
        std::array<char, MaxDataSize> buffer{}; //4KB of buffer size
        int rv;


        //TAG: TODO: Write a wrapper class for managing socket. This wrapper class owns the resources about sockfd.
        bool start(){
            hints.ai_family = AF_INET6
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_flags = AI_PASSIVE;

            if (rv = (getaddrinfo(nullptr, Port, &hints, &servinfo)) != 0){
                std::print(stderr, "[-] {}\n", gai_strerror(rv));
                return false;
            }

            int sockfd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
            if (sockfd < 0){
                std::string err_msg_sock = std::error_code(errno, std::system_category()).message();
                std::print(stderr, "[-] Socket creation failed on port {}, due to: {}\n", Port, err_msg_sock);
                return false;

                //TAG: TODO: Throw the error code and pass it to exception handler
            }

            if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0){
                std::string err_msg_setsock = std::error_code(errno, std::system_category()).message();
                std::print(stderr, "[-] {}\n", err_msg_setsockopt);
                std::exit(EXIT_FAILURE); //TAG: EXIT: Exit the whole program. Would terminate for a specific HTTP object or the whole program. TEST!
                //TAG: TODO: Throw the error code and pass it to exception handler
            }

            std::print("[+]: socket created successfully on port: {}\n", Port);

            if (rv = (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen)) < 0){
                std::string err_msg_bind = std::error_code(errno, std::system_category()).message();
                std::print(stderr, "[-] Binding failed: {}\n", err_msg_bind);
                return false;
                //TAG: TODO: Therow the error code and pass it to exception handler
            }

            if (rv = (listen(sockfd, BackLog)) < 0){
                std::string err_msg_listen = std::error_code(errno, std::system_category()).message();
                std::print(stderr, "[-] Listening failed");
                return false;
                //TAG: TODO: Throw the error code and pass it to exception handler
            }

            std::print(stdout, "[+] Server listening for incoming connections on port {} . . .\n", Port);
            return true;
        }


        void *get_in_addr(struct sockaddr *sa){ //void because it has to handle two different kinds of structures
            if (sa->sa_family == AF_INET){
                return &((reinterpret_cast<struct sockaddr_in*>(sa))->sin_addr);
            }
            return &((reinterpret_cast<struct sockaddr_in6*>(sa))->sin6_addr);
        }

        bool run(int sockfd){
            while(1){
                sin_size = sizeof(their_addr_storage);
                int acceptfd = accept(sockfd, reinterpret_cast<struct sockaddr*>(&their_addr_storage), &sin_size);
                if (acceptfd < 0){
                    std::print(stderr, "[-] Accept failed from {}\n", Port);
                    continue;
                }

                inet_ntop(their_addr_storage.ss_family, get_in_addr(reinterpret_cast<struct sockaddr*>(&their_addr_storage)), s.data(), s.size());
                std::print(stdout, "[+] Got a connection from {} on {}\n", s.data(), Port);

                pid_t pid = fork();
                if (pid < 0){
                    std::print(stderr, "[-] Fork failed!\n");
                    close(acceptfd);
                    continue;
                }

                if (pid == 0){
                    close(sockfd);
                    int numbytes = recv(acceptfd, buffer.data(), MaxDataSize - 1, 0);
                    if (numbytes < 0){
                        std::print(stderr, "[+] Recieve failed!\n");
                        std::exit(EXIT_FAILURE);
                    }
                    std::print(stdout, "[+] Recieved {}\n", std::string_view(buffer.data(), numbytes));
                }
                //sending data goes here
                //What ever data is requested from the client
                //that is parsed and handled according to the uri
                //and then that particular data is sent
                //
                //TAG: HOW: How to implement that
                //
                //
                //
                //
                //
                //
                close(acceptfd);
                std::exit(EXIT_SUCCESS);
            }
            close(acceptfd);

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

        HTTP(char &port) : Port(port){
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
