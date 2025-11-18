// DONT USE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <regex.h>
#include <string.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <errno.h>
#include <unistd.h>

#define PORT 8080  // hosting on TCP port 8080
#define BUFFER 4096

/*
// Functions to communicate with socket / terminal
int socket_send(int sock, char *msg);
int socket_receive(int sock, char *buffer, int size);
*/ 
// Functions to handle requests
int get_request(int *client_fd);

/*
int post_request(int a, int b);
// int get_request(char *html);
int delete_request(char *request);
*/
void die(char *msg);


// Entry point
// Used to script command line actions
// TODO Input / Request: ________
// TODO Output: __________
int main(int argc, char *argv[]) {
    char buffer[BUFFER];
    int server_fd;
    struct sockaddr_in server_addr;

    // Socket creation
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0){
        die("Socket creation failed!\n");
    }
    printf("Socket created!\n");

    // So port 8080 can be reused quickly
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Socket configuration
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    if(bind(server_fd, (struct sockaddr *) & server_addr, sizeof(server_addr)) < 0){
        die("Socket binding failed!\n");
    }
    printf("Socket bound\n");

    if((listen(server_fd, 10)) < 0){
        die("Socket listening failed!\n");
    }
    printf("Socket listening\n");

    printf("Serving HTTP on port %d\n", PORT);

    while(1){
        // Set up client connection
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int *client_fd = malloc(sizeof(int));

        if((*client_fd = accept(server_fd, (struct sockaddr *) & client_addr, & client_addr_len)) < 0 ){
            die("Client accept failed!\n");
        }

        memset(buffer, 0, BUFFER);
        read(*client_fd, buffer, BUFFER - 1);
        printf("Received Request: %s\n", buffer);

        printf("ALLISON (WHILE) BEFORE IF\n");
        // Send request to correct function
        if(strncmp(buffer, "GET /", 6) == 0 || strncmp(buffer, "GET /index.html", 15) == 0){
            printf("ALLISON IN (WHILE) IF BRANCH \n");
            get_request(client_fd);
        }
        //other requests
        //char *body = strstr(buffer, "\r\n\r\n"); // find end of header
        else {
            //printf("ALLISON IN (WHILE) ELSE BRANCH \n");
            char htmlbad[] = "HTTP/1.0 200 OK\r\n"
                  "Server: webserver-c\r\n"
                  "Content-type: text/html\r\n\r\n"
                  "<html>h404 Not Found</html>\r\n";
            write(*client_fd, htmlbad, strlen(htmlbad));
        }

       close(*client_fd);
    }

    close(server_fd);
    return 0;
}

// Terminate function
// Prints error message
void die(char *msg){
    perror(msg);
    exit(EXIT_FAILURE);
}

int get_request(int *client_fd){
    printf("ALLISON GET REQUEST FUNCT\n");

    char html[] = "HTTP/1.0 200 OK\r\n"
                  "Server: webserver-c\r\n"
                  "Content-type: text/html\r\n\r\n"
                  "<html>hello, world</html>\r\n";
    write(*client_fd, html, strlen(html));

    return 0;
}