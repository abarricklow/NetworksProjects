// Allison Barricklow
// CSCI 4245
// Programming Assign 2
// Hosting an HTTP server

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 4096

int get_request(int newsockfd, char uri[], char version[]);
int post_request(int newsockfd, char buffer[]);

int main() {
    char buffer[BUFFER_SIZE];

    // Create socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Failed to create socket :(\n");
        return 1;
    }
    printf("Socket created successfully!\n");

    // Create host address
    struct sockaddr_in host_addr;
    int host_addrlen = sizeof(host_addr);

    // Create client address
    struct sockaddr_in client_addr;
    int client_addrlen = sizeof(client_addr);

    // Configure host address
    host_addr.sin_family = AF_INET;
    host_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    host_addr.sin_port = htons(PORT);


    // Bind socket
    if (bind(sockfd, (struct sockaddr *)&host_addr, host_addrlen) != 0) {
        perror("Failed to bind to socket :(\n)");
        return 1;
    }
    printf("Socket successfully bound!\n");

    // Listen on socket
    if (listen(sockfd, SOMAXCONN) != 0) {
        perror("Failed to listen :(\n");
        return 1;
    }
    printf("Server listening!\n");

    while(1) {
        // Accept incoming connections
        int newsockfd = accept(sockfd, (struct sockaddr *)&host_addr,
                               (socklen_t *)&host_addrlen);
        if (newsockfd < 0) {
            perror("Webserver declined :(\n");
            continue;
        }
        printf("Connection accepted!\n");

        // Get client address
        int sockn = getsockname(newsockfd, (struct sockaddr *)&client_addr,
                                (socklen_t *)&client_addrlen);
        if (sockn < 0) {
            perror("Coudln't get socket name\n");
            continue;
        }

        // Read from the socket
        int valread = read(newsockfd, buffer, BUFFER_SIZE);
        if (valread < 0) {
            perror("Couldn't read socket name\n");
            continue;
        }

        // Read the request
        char method[BUFFER_SIZE], uri[BUFFER_SIZE], version[BUFFER_SIZE];
        sscanf(buffer, "%s %s %s", method, uri, version);
        printf("[%s:%u] %s %s %s\n", inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port), method, uri, version);

        if(strcmp(method, "GET") == 0){
            get_request(newsockfd, buffer);
        }
        else if(strcmp(method, "PUSH") == 0){
            push_request(newsockfd, buffer);
        }
        else{
            printf("Failed BOOOO");
        }

        close(newsockfd);
    }

    return 0;
}

int get_request(int newsockfd, char uri[], char version[]){
    char resp[] = "HTTP/1.0 200 OK\r\n"
                  "Server: webserver-c\r\n"
                  "Content-type: text/html\r\n\r\n"
                  "<html>hello, world</html>\r\n";

    // Write to the socket
    int valwrite = write(newsockfd, resp, strlen(resp));
    if (valwrite < 0) {
        perror("Couldn't write to socket\n");
    }
}


int post_request(int newsockfd, char buffer[]){
    
}