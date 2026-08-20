//Simple stream socket
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

//TAG: ARCHITECTURE: 
                 /* SERVER STARTS */
                 /*       │ */
                 /*       ↓ */
                 /*  socket() */
                 /*       │ */
                 /*       ↓ */
                 /*    bind() */
                 /*       │ */
                 /*       ↓ */
                 /*   listen() */
                 /*       │ */
                 /*       ↓ */
             /* listening socket exists */
                 /*       │ */
                 /*       ↓ */
                 /*    accept() */
                 /*       │ */
                 /*       ↓ */
              /* connected socket created */
                 /*       │ */
                 /*       ↓ */
                 /*    fork() */
                 /*  /       \ */
                 /* /         \ */
            /* Parent         Child */
              /* │              │ */
              /* │              └── handles client */
              /* │                         │ */
              /* │                      recv() */
              /* │                         │ */
              /* │                    client disconnects */
              /* │                         │ */
              /* │                    close(clientfd) */
              /* │                         │ */
              /* │                       exit() */
              /* │                         │ */
              /* │                    child terminates */
              /* │                         │ */
              /* │                    kernel sends */
              /* │                     SIGCHLD */
              /* │                         │ */
              /* ←─────────────────────────┘ */
              /* │ */
              /* ↓ */
      /* sigchld_handler(int s) */
              /* │ */
              /* │ s = SIGCHLD */
              /* │ but we don't need it */
              /* │ */
              /* ↓ */
      /* save errno */
              /* │ */
              /* ↓ */
      /* waitpid(-1, NULL, WNOHANG) */
              /* │ */
              /* ├── reap terminated child */
              /* ├── reap another if available */
              /* └── stop when none remain */
              /* │ */
              /* ↓ */
      /* restore errno */
              /* │ */
              /* ↓ */
      /* return from handler */
              /* │ */
              /* ↓ */
      /* parent continues */
              /* │ */
              /* ↓ */
          /* accept() again */


#define PORT "3490" //The port that the users will be connecting to 

#define BACKLOG 10 //number of pending connections


void sigchld_handler(int s){ //int s is required by the signal handler interface.
    (void) s; //specify that the parameter would be unused. We can technically use it. But for this specific function we are not using it.
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0); //This operation may modify errno. so we save the errno before and then assign it again.

    errno = saved_errno;

    //TAG: EXP: TCP does create a new socket (kernel does this internally) for each new connection, but it does not create a new process to handle that connection. Thats why we are using fork(), to create a new process execution context. Now for each child socket, we have a child process. That child process is related to that specific connection. When the connection is dropped, that connected socket is closed and not necessarily that child process that was being used to handle that. Now the parent has to reap that child process. This function does that.  And fork() is used for concurrency. We can use same process for multiple connected sockets, and in fact we do in high frequency networking, but that would introduce other advanced methods.
    
}
 
//Get sockaddr, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET){
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void){
    int sockfd, new_fd;
    struct addrinfo hints, *servinfo, *p; //*servinfo and *p are pointers to addrinfo objects. Think of them like a empty box which will contain addresses to addrinfo objects in the future. getaddrinfo() writes addresses (of that linked list that it has created somewhere in the memory) to these boxes (only servinfo).
    struct sockaddr_storage their_addr; //sockaddr_storage is a generic container to hold socket addresses of different types without knowing about them beforehand.
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN]; //for storing the clients address in human readable format. Done by inet_ntop().
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next){ //for each address in that servinfo linked list
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){ //socket() returns a file descriptor and that value is assigned to sockfd. if it cannot create a socket, it returns -1. In that case, we leave this iteration entirely and continue with the next one.
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd ,SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){ //SOL_SOCKET -> socket level option (other optioins include protocol level options such as IPPROTO_TCP), SO_REUSEADDR -> This option, &yes -> enable it (we have declared yes = 1 previously)
            perror("setsockopt");
            exit(EXIT_FAILURE); //Failure is FATAL. Exit the whole program. Why -> because we have chosen not to run the server in potentially undesirable configuration.
        }

        //If all those goes well, now we bind the socket
        if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1){
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (p == NULL){
        fprintf(stderr, "server: failed to bind\n");
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, BACKLOG) == -1){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1){
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    printf("server: waiting for connections.....\n");

    while(1){
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1){
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        printf("server: got connection from %s\n",s);

        if (!fork()){
            close(sockfd);
            if (send(new_fd, "Hello, world!", 13, 0) == -1){
                perror("send");
            }
            close(new_fd);
            exit(EXIT_SUCCESS);
        }
        close(new_fd);
    }

    return 0;
}

