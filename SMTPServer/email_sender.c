// Allison Barricklow
// CSCI 4245
// Assign P1
// This file uses sockets to connect an smtp server and send an email

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <regex.h>
#include <string.h>

#define SMTP_PORT 25 // smtp port number
#define BUFFER_SIZE 1024 // setup a buffer

// Function declarations
// Explanations above function definitions after main
int socket_send(int sock, char *msg);
int socket_receive(int sock, char *buffer, int size);
void die(char *msg);
int validate_email(char *email);
int extract_sender(char *email_body, char*sender_email, int size);

// Main is the entry point
// Used to simulate the command line
// way of using SMTP
// Input SMTP_ADDR_IPV4>, <DEST_EMAIL_ADDR>, <EMAIL_FILENAME>
// Outputs the email sending through SMTP
int main(int argc, char *argv[]) {

    // Error if arguments are not valid
    if (argc != 4) {
        die("Use: %s <SMTP_ADDR_IPV4> <DEST_EMAIL_ADDR> <EMAIL_FILENAME>\n");
    }

    // Variables to access arguments passed
    char *smtp_addr = argv[1];
    char *dest_email = argv[2];
    char *filename = argv[3];
    
        // Validate destination email format
    if (!validate_email(dest_email)) {
        die("Invalid email address format: %s\n");
    }

    // Read email file into buffer
    FILE *fp = fopen(filename, "r");
    if (!fp)
        die("Failed to open email file");


    char email_body[BUFFER_SIZE * 8];   // Allots space for size of email
    // Reads email 1 byte at a time, stores bytes read
    size_t bytes_read = fread(email_body, 1, sizeof(email_body) - 1, fp);
    email_body[bytes_read] = '\0'; // Terminates the string
    fclose(fp);

    //Get sender email from email
    char sender_email[256] = {0};
    if (extract_sender(email_body, sender_email, sizeof(sender_email))) {
         printf("Sender email: %s\n", sender_email);
    } else {
        printf("Sender email not found\n");
    }

    if (sender_email[0] == '\0') {
        die("Sender email not found in email file");
    }

    // Creates the socket using C socket()
    // Takes the IPv4 address and socket type (TCP)
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        die("Socket creation failed");

    // Set up server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SMTP_PORT);
    // Using addr and AF_INET
    // https://www.cs.cmu.edu/~srini/15-441/S10/lectures/r01-sockets.pdf

    // Validate SMTP address
    if (inet_pton(AF_INET, smtp_addr, &server_addr.sin_addr) <= 0)
        die("Invalid SMTP IPv4 address");
     // Connect to the SMTP server
    // https://www.cs.cmu.edu/~srini/15-441/S10/lectures/r01-sockets.pdf
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        die("Failed to connect to SMTP server");


    char buffer[BUFFER_SIZE];


    // SMTP protocol sequence
    // Server greeting
    socket_receive(sock, buffer, sizeof(buffer));

    // Introduce client to server
    socket_send(sock, "HELO localhost\r\n");
    socket_receive(sock, buffer, sizeof(buffer));

    // Set sender email
    char mail_cmd[BUFFER_SIZE];
    snprintf(mail_cmd, sizeof(mail_cmd), "MAIL FROM:<%s>\r\n", sender_email);
    socket_send(sock, mail_cmd);
    socket_receive(sock, buffer, sizeof(buffer));

    // Set recipient email
    char rcpt_cmd[BUFFER_SIZE];
    snprintf(rcpt_cmd, sizeof(rcpt_cmd), "RCPT TO:<%s>\r\n", dest_email);
    socket_send(sock, rcpt_cmd);
    socket_receive(sock, buffer, sizeof(buffer));

    // Prepares to send email message
    socket_send(sock, "DATA\r\n");
    socket_receive(sock, buffer, sizeof(buffer));

    // Sends email message from file argument
    socket_send(sock, email_body);
    socket_send(sock, "\r\n.\r\n");
    socket_receive(sock, buffer, sizeof(buffer));

    // End SMTP session
    socket_send(sock, "QUIT\r\n");
    socket_receive(sock, buffer, sizeof(buffer));


    close(sock);
    printf("Email sent successfully to %s\n", dest_email);
    return 0;
}

// Sends data on TCP socket
// Input socket number, and message
// Returns number of bytes sent on TCP
int socket_send(int sock, char *msg) {
    return write(sock, msg, strlen(msg));
    // https://man7.org/linux/man-pages/man2/write.2.html
}


// Gets data being received on TCP socket
// puts it on char buffer
// My buffer length is 1024 bytes
// Input socket number, buffer char, and buffer size
// Returns number of bytes read
int socket_receive(int sock, char *buffer, int size) {
    return read (sock, buffer, size);
    // https://man7.org/linux/man-pages/man2/read.2.html
}

// Acts as a way of handling an error
// Will print system error and the
// error message passed
// Input an error message
// Output exits code
void die(char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// Validates the email address format in email message
// Finds the @ symbol and the . for .edu or .com
int validate_email(char *email) {
    char *at = strchr(email, '@');     // Find '@'
    if (!at || at == email) return 0;        // Must exist and not first char
    char *dot = strchr(at, '.');       // Find '.' after '@'
    if (!dot || dot == at + 1) return 0;     //  Must exist and not immediately after '@'
    return 1;                                // Otherwise valid
}

// Extracts the sender email from the "From :" line
// Input email message and sender_email variable and size
// Output 1 if found email, 0 if not found
int extract_sender(char *email_body, char *sender_email, int size) {

    // Checks for from line
    const char *from_line = strstr(email_body, "From :");
    if (!from_line) return 0; // "From :" not found

    // Finds <>
    const char *start = strchr(from_line, '<'); // Find '<'
    const char *end = strchr(from_line, '>');   // Find '>'

    // If <> aren't formatted correctly
    if (!start || !end || end <= start) return 0; // Invalid format

    // Get actual email from between <>
    start++; // Skip '<'
    size_t len = end - start;
    if (len >= size) len = size - 1;
    strncpy(sender_email, start, len);
    sender_email[len] = '\0';
    return 1;
}
