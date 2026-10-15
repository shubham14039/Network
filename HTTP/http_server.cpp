#include <map>
#include <algorithm>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <stdexcept>
#include <exception>
#include <fstream>
#include <sstream>
#include <print> 
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

//Exception classes
class SomeException : public std::excetption {

};

//HTTP request object
struct HttpRequest{
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    std::string body;
};

//Parser class object
class Parser{
    private: 
        //request line parsing
        //extract the method, path and  HTTP version separated by spaces
        bool parseRequestLine(const std::string& line, HttpRequest& request){
            std::istringstream line_stream(line);

            //extract first token
            if (!(line_stream >> request.method)){
                return false;
            }

            //second token
            if (!(line_stream >> request.path)){
                return false;
            }

            //third token
            if (!(line_stream >> request.version)){
                return false;
            }
        }

        //Header line parsing 
        //renormalize header keys to lowercase to make lookups reliable
        bool parseHeaderLine(const std::string& line, std::map<std::string, std::string>& header){
            size_t colon_pos = line.find(':');
            if (colon_pos == std::string::npos){
                return false; //missing colon. Invalid header format
            }

            //extract key and value
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);

            //trim leading and trailing whitespaces
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            //lowercase
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            
            headers[key] = value;
            return true;
        }

    public:
        bool parseRequest(const std::string& raw_data, HttpRequest& request){
            //Separate body from header
            std::string header_section;
            size_t header_end_pos = raw_data.find("\r\n\r\n");

            if (header_end_pos != std::string::npos){ //std::string::npos = string position not found. Returned by .find() if search fails
                header_section = raw_data.substr(0, header_end_pos);
                request.body = raw_data.substr(header_end_pos + 4);
            }

            else {
                header_end_pos = raw_data.find("\n\n"); //Unix style LF only boundary
                if (header_end_pos != std::string::npos){
                    header_section = raw_data.substr(0, header_end_pos);
                    request.body = raw_data.substr(header_end_pos + 2);
                }

                else {
                    header_section = raw_data;
                    request.body = "";
                }
            }

            //Process header stream line by line
            std::istringstream stream(header_section);
            std::string line;

            //parse the request line
            if (std::getline(stream, line)){
                if (!line.empty() && line.back() == "\r"){
                    line.pop_back();
                }

                if (parseRequestLine(line, request)){
                    return false; //malformed request line
                }
            }

            else {
                return false; //empty request
            }


            //parse subsequent lines until the end
            while (std::getline(stream, line)){
                if (!line.empty() && line.back() == "\r"){
                    line.pop_back();
                }

                //if immediate empty line occur unexpetedely
                if (line.empty()){
                    continue;
                }

                if (!parseHeaderLine(line, request.headers)){
                    return false;
                }
            }
            return false;
        }
};

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
                        throw std::runtime_error("SCOKET: Failed to create a socket!\n");
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
        const char* Port = nullptr; //To be passed by the executioner
        struct addrinfo hints{}, *servinfo;
        struct sockaddr_storage their_addr_storage;
        int yes = 1;
        socklen_t sin_size;
        std::array<char, INET6_ADDRSTRLEN> s{};
        std::array<char, MaxDataSize> buffer{}; //4KB of buffer size
        int rv;
        ScopedSocket sockfd; //Socket object 
        Parser parser; //TAG: PARSER: Parser object to be used to parse incoming stream using {Composition}. Other method includes {Dependency injection}

        int start(){
            if (!sockfd.isValid()){
                throw std::out_of_range("SOCKET: ");
                //TAG: CATCH: Catch the exception and get out of the function 
            }

            hints.ai_family = AF_INET6;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_flags = AI_PASSIVE;

            if ((rv = (getaddrinfo(nullptr, Port, &hints, &servinfo))) != 0){
                std::print(stderr, "[-] {}\n", gai_strerror(rv));
                return 1;
            }

            if (setsockopt(sockfd.get(), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0){
                throw std::system_error(errno, std::system_category(), "SOCKET: ");
                //TAG: CATCH: Catch the error and exit the whole program using std::exit(EXIT_FAILURE)
            }

            std::print("SOCKET: socket created successfully on port: {}\n", Port);

            if ((rv = (bind(sockfd.get(), servinfo->ai_addr, servinfo->ai_addrlen))) < 0){
                throw std::system_error(errno, std::system_category(), "SOCKET: ");
                //TAG: CATCH: Catch the error and get out of the fucntion 
            }
            freeaddrinfo(servinfo);

            if ((rv = (listen(sockfd.get(), BackLog))) < 0){
                throw std::system_error(errno, std::system_category(), "SOCKET: ");
                //TAG: CATCH: Catch the error and get out of the fucntion 
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

        bool run(){
            while(true){
                sin_size = sizeof(their_addr_storage);
                int acceptfd = accept(sockfd.get(), reinterpret_cast<struct sockaddr*>(&their_addr_storage), &sin_size);
                if (acceptfd < 0){
                    std::print(stderr, "[-] Accept failed from {}\n", Port);
                    continue;
                }

                ScopedSocket client_socket(acceptfd); //New client socket object spawned

                inet_ntop(their_addr_storage.ss_family, get_in_addr(reinterpret_cast<struct sockaddr*>(&their_addr_storage)), s.data(), s.size());
                std::print("[+] Got a connection from {} on {}\n", s.data(), Port);

                pid_t pid = fork();
                if (pid < 0){
                    std::print(stderr, "[-] Fork failed!\n");
                    continue;
                }

                if (pid == 0){
                    sockfd.close();
                    int numbytes = recv(client_socket.get(), buffer.data(), MaxDataSize - 1, 0);
                    if (numbytes < 0){
                        throw std::runtime_error("[+] Recieve failed from: {}\n", client_socket.get());
                        //TAG: CATCH: catch the exception and act accordingly
                        // std::print(stderr, "[+] Recieve failed!\n");
                        // std::exit(EXIT_FAILURE);
                    }
                    std::print(stdout, "[+] Recieved {}\n", std::string_view(buffer.data(), numbytes));

                    //sending data goes here
                    //What ever data is requested from the client
                    //that is parsed and handled according to the uri
                    //and then that particular data is sent

                    //sending via serve fucntion 
                    serve(filepath, client_socket.get());
                    std::exit(EXIT_SUCCESS);
                }
            }
            return true; //TAG: IDK:
        }

    public:
        void serve(const std::string& filepath, int send_fd){
            //check if the path provided contains valid file

            int file_fd = open(filepath, O_RDONLY);
            if (file_fd < 0){
                throw std::runtime_error("Error opening the file!");
            }

            struct stat file_stat;
            if (fstat(file_fd, &file_stat) < 0){
                std::string err = std::error_code(errno, std::system_category()).message();
                std::print(stderr, "Fstat failed: {}\n", err);
            }

            off_t len = 0;
            off_t remaining = file_stat.st_size;

            //opening the file
            // std::ifstream file(filepath, std::ios::binary | std::ios::ate){
            //     if (!file.is_open()){
            //         throw std::runtime_error("Failed to open the file");
            //     }
            // }

            // std::streamsize file_size = file.tellg();
            // file.seekg(0, std::ios::beg);

            // const size_t BUFFER_SIZE = 4096; //4kb of buffer size
            // std::vector<char> buffer(BUFFER_SIZE);

            //return response stream object
            std::string status_line =  "HTTP/1.1 200 OK\r\n";
            std::string content_type =  "Content-Type: text/html; charset=UTF-8\r\n";
            std::string content_len =  "Content-Length: " + remaining + "\r\n";
            std::string connection =  "Connection: close\r\n\r\n";

            //iovectors
            struct iovec iov[4];
            iov[0].iov_base = const_cast<char*>(status_line.data());
            iov[0].iov_len = status_line.size();
            iov[1].iov_base = const_cast<char*>(content_type.data());
            iov[1].iov_len = content_type.size();
            iov[2].iov_base = const_cast<char*>(content_len.data());
            iov[2].iov_len = content_len.size();
            iov[3].iov_base = const_cast<char*>(connection.data());
            iov[3].iov_len = connection.size();

            struct sf_hdtr hdtr;
            hdtr.headers = iov;
            hdtr.hdr_cnt = 4;
            hdtr.trailers = nullptr;
            hdtr.trl_cnt = 0;

            int nopush = 1;
            setsockopt(send_fd, IPPROTO_TCP, TCP_NOPUSH, &nopush, sizeof(nopush));

            write(send_fd, headers, strlen(headers)); //TAG: FIX: headers are of hdtr type and change strlen accordingly

            //stream loop
            while (remaining > 0){
                if (sendfile(file_fd, send_fd, 0, &len, nullptr, 0) < 0){ //TAG: FIX: chunking not happening here
                    throw std::runtime_error("Sending *this chunk of file failed!");
                    //TAG: CATCH: catch the exception and act accordingly
                }
                //sending chunk via sendfile()
                //errors should be handled for exceptions inproduction 
            }
            
            nopush = 0;
            setsockopt(send_fd, IPPROTO_TCP, TCP_NOPUSH, &nopush, sizeof(nopush));

            file.close();
            std::print("File streaming completed successfully!\n");
        }

        void terminate(){
            //terminate the whole server and call destructor 
        }

        //Other functionality to be given to the user to

        HTTP(const char* port) : sockfd(AF_INET6, SOCK_STREAM, IPPROTO_TCP), Port(port){
            //construcctor body
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
