// Allison Barricklow
// CSCI 4245
// Programming Assign 2
// Hosting an HTTP server

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#define PORT 8080
#define BUFFER_SIZE 4092

// Worker thread pool
class ThreadPool {
public:
    // Starts worker threads that will wait for tasks
    ThreadPool(size_t n) : stop_flag(false){
        for (size_t i = 0; i < n; ++i) {
            workers.emplace_back([this] {this->worker_loop();});
        }
    }

    // Deconstructor to shut down worker threads
    ~ThreadPool() {
        {
            // Locks queue
            std::unique_lock<std::mutex> lk(queue_mutex);
            stop_flag = true;
        }
        cv.notify_all();  // Notifies all workers
        for (auto &t : workers){
            // If thread stil running, wait before deconstruction
            if (t.joinable()){
                t.join();
            }
        }
    }

    // Add task
    void enqueue(std::function<void()> task){
        {
            // Locks queue
            std::unique_lock<std::mutex> lk(queue_mutex);
            // Add new task to queue
            tasks.push(std::move(task));
        }
        cv.notify_one(); // Calls a worker thread for task
    }
private:
    // Worker thread's life loop
    void worker_loop(){
        while (true){
            std::function<void()> task;
            {
                // Locks queue
                std::unique_lock<std::mutex> lk(queue_mutex);
                cv.wait(lk, [this] { return stop_flag || !tasks.empty(); }); // Wait for task
                // If shutdown, and not running a task
                if (stop_flag && tasks.empty()){
                    return;
                }
                task = std::move(tasks.front()); // Take task at front of queue
                tasks.pop();
            }
            try{
                task(); // Do task
            }
            catch (const std::exception &e){
                std::cerr<<"Task exception: "<<e.what()<<"\n";
            }
        }
    }

    std::vector<std::thread> workers;  // Worker threads
    std::queue<std::function<void()>> tasks; // Task queue
    std::mutex queue_mutex; // Protect queue
    std::condition_variable cv; // Notifies worker threads
    bool stop_flag;  // Shutdown flag
};

// Read until something is read
static ssize_t super_read(int fd, void *buf, size_t count){
    ssize_t r;
    do{
        r = read(fd, buf, count);
    } while (r < 0 && errno == EINTR); //
    // EINTR errors are interrupted system calls, usually can just try again
    // https://medium.com/@agadallh5/understanding-eintr-the-error-interrupt-signal-in-unix-systems-670a1bedc121 
    
    return r;
}

// Write until all bytes are written
static ssize_t super_write(int fd, const void *buf, size_t count){
    ssize_t w = 0; // Total bytes written
    const char *p = (const char *)buf;
    size_t left = count; // Bytes to be written
    // Keeps calling write()
    while (left > 0){
        ssize_t n = write(fd, p + w, left);
        if (n < 0){
            if (errno == EINTR) continue; // Interrupted writes
            return -1;
        } 
        left -= n;
        w += n;
    }
    return w;
}

// Decodes form-encoded data for POST
static std::string url_decode(const std::string &s){
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i){
        char c = s[i];
        if (c == '+'){
            out.push_back(' '); // form: + is a space
        }
        // form: %xx is followed by 2 digits of hex
        else if (c == '%' && i + 2 < s.size()){
            int hi, lo;
            // Hex dictionary
            auto hex = [](char ch) -> int {
                if (ch >= '0' && ch <= '9') return ch - '0';
                if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
                if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
                return -1;
            };
            hi = hex(s[i + 1]);
            lo = hex(s[i + 2]);
            // Return decoded characters
            if (hi >= 0 && lo >= 0){
                out.push_back((char)((hi << 4) | lo));
                i += 2;
            }
            else{
                out.push_back('%');
            }
        }
        else out.push_back(c); 
    }
    return out;
}

// Parse POST query encoded data: key=value&key2=value2
static bool parse_POST(const std::string &s, const std::string &key, std::string &value) {
    size_t pos = s.find(key + "="); // Find first value
    if (pos == std::string::npos) return false; 
    pos += key.size() + 1; // pos at value
    size_t end = s.find('&', pos); // end of first value
    value = s.substr(pos, (end == std::string::npos) ? std::string::npos : end - pos);
    value = url_decode(value); // Get actual value
    return true;
}

// Helper to read until CRLFCRLF
struct HttpRequest {
    std::string method; // GET/POST/DELTE
    std::string uri; // /index /multiply
    std::string version; // HTTP/1,1
    std::vector<std::pair<std::string, std::string>> headers; // Header data
    std::string body; // POST data
};

static bool parse_request(int client_fd, HttpRequest &req){
    std::string data;
    data.reserve(8192);
    char buf[BUFFER_SIZE];
    ssize_t n;
    // Read until we have headers end
    while (true){
        n = super_read(client_fd, buf, sizeof(buf)); // Read from client
        if (n < 0) return false;
        if (n == 0) break; // Connection closed
        data.append(buf, buf + n); // Add to data
        if (data.find("\r\n\r\n") != std::string::npos) break; // Until end characters
        if (data.size() > 65536) break; // Too much data
    }

    size_t hdr_end = data.find("\r\n\r\n");
    if (hdr_end == std::string::npos) return false; // invalid request

    std::string headers_block = data.substr(0, hdr_end);
    size_t pos = 0;
    // Reads each line at a time
    auto next_line = [&](std::string &line) -> bool {
        // Checks if more lines
        if (pos >= headers_block.size()){
            return false;
        }
        size_t lf = headers_block.find("\r\n", pos);
        // If final line
        if (lf == std::string::npos){
            line = headers_block.substr(pos);
            pos = headers_block.size();
            return true;
        }
        // If more lines, take substring, advance pos
        line = headers_block.substr(pos, lf - pos);
        pos = lf + 2;
        return true;
    };

    // Request line
    std::string line;
    if (!next_line(line)){
        return false;
    }
    {
        std::istringstream iss(line); // Stream to parse line
        // If request doesn't have method/uri/version invalid
        if (!(iss >> req.method >> req.uri >> req.version)){
            return false;
        }
    }

    // Headers
    while (next_line(line)){
        size_t c = line.find(':'); // format- key: value
        if (c != std::string::npos) {
            // Get key and value
            std::string key = line.substr(0, c);
            std::string val = line.substr(c + 1);
            // Get rid of spaces
            while (!val.empty() && (val[0] == ' ' || val[0] == '\t')) val.erase(0, 1);
            while (!val.empty() && (val.back() == ' ' || val.back() == '\t')) val.pop_back();
            req.headers.emplace_back(key, val); // Store header
        }
    }

    // Body (POST)
    size_t body_start = hdr_end + 4; // Skips end characters
    size_t content_length = 0;
    for (auto &h : req.headers){
        // Get content length 
        if (strcasecmp(h.first.c_str(), "Content-Length") == 0){
            content_length = (size_t)std::stoul(h.second);
        }
    }
    // Copy bytes that were already received
    if (data.size() > body_start){
        req.body = data.substr(body_start);
    }
    // Read remaining bytes
    while (req.body.size() < content_length){
        ssize_t r = super_read(client_fd, buf, sizeof(buf));
        if (r <= 0) return false; // Connection closed or error
        req.body.append(buf, buf + r);
    }

    return true;
}

// HTTP Response
static void send_response(int client_fd, int code, const std::string &reason,
                          const std::string &content_type, const std::string &body,
                          const std::vector<std::pair<std::string, std::string>> &extra_headers = {}) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << code << " " << reason << "\r\n";
    oss << "Content-Type: " << content_type << "\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    // For requests with location
    for (auto &h : extra_headers){
        oss << h.first << ": " << h.second << "\r\n";
    }
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << body;
    std::string resp = oss.str();
    super_write(client_fd, resp.data(), resp.size()); // Write to the socket
}

// Default HTML page
static void default_html(int client_fd){
    const std::string page =
        "<!doctype html>\n<html><head><meta charset=\"utf-8\"><title>Index</title></head>\n"
        "<body><h1>Hello from Allison's server :)</h1>\n"
        "</body></html>\n";
    send_response(client_fd, 200, "OK", "text/html", page);
}

// Handles a HTTP request
static void handle_client(int client_fd){
    // Get peer info for logging
    struct sockaddr_in peer_addr;
    socklen_t peer_len = sizeof(peer_addr);
    char peerbuf[INET_ADDRSTRLEN] = "?";
    // Get IP of client
    if (getpeername(client_fd, (struct sockaddr *)&peer_addr, &peer_len) == 0){
        inet_ntop(AF_INET, &peer_addr.sin_addr, peerbuf, sizeof(peerbuf));
        // https://man7.org/linux/man-pages/man3/inet_ntop.3.html
        // Turning binary IP into readable IP
    }

    HttpRequest req;
    // Get method/uri/version, header, and body
    if (!parse_request(client_fd, req)){
        // Malformed request
        std::cerr << "Invalid request from " << peerbuf << "\n";
        close(client_fd);
        return;
    }

    // Log and output request
    std::cout << "[" << peerbuf << "] " << req.method << " " << req.uri << " " << req.version << "\n";

    // Route handling
    std::string method = req.method;
    std::string uri = req.uri;

    // Separate path and query
    std::string path = uri;
    std::string query;
    size_t qpos = uri.find('?');
    if (qpos != std::string::npos){
        path = uri.substr(0, qpos);
        query = uri.substr(qpos + 1);
    }

    // Implement request functions
    // Return default page
    if ((path == "/" || path == "/index.html")){
        if(method == "GET"){
            default_html(client_fd);
        }
        else{
            send_response(client_fd, 405, "Method Not Allowed", "text/plain", "Method Not Allowed");
        }
    }
    // GET /google
    else if (path == "/google"){
        if(method == "GET"){
            // 301 Redirect
            std::vector<std::pair<std::string, std::string>> extra;
            extra.emplace_back("Location", "https://google.com");
            send_response(client_fd, 301, "Moved Permanently", "text/plain", "Moved Permanently", extra);
        }
        else{
            send_response(client_fd, 405, "Method Not Allowed", "text/plain", "Method Not Allowed");
        }
    }
    // DELETE /database.php?data=all
    else if (path == "/database.php" && method == "DELETE"){
        send_response(client_fd, 403, "Forbidden", "text/plain", "Forbidden");
    }
    //non-DELETE database.php
    else if(path == "/database.php"){
        send_response(client_fd, 405, "Method Not Allowed", "text/plain", "Method Not Allowed");
    }
    // POST /multiply
    else if (path == "/multiply"){
        if (method != "POST"){
            send_response(client_fd, 405, "Method Not Allowed", "text/plain", "Method Not Allowed");
        }
        else{
            // Form-encoded body a=INT&b=INT
            std::string a_str, b_str;
            bool ok_a = parse_POST(req.body, "a", a_str);
            bool ok_b = parse_POST(req.body, "b", b_str);
            if (!ok_a || !ok_b){
                send_response(client_fd, 400, "Bad Request", "text/plain", "Bad Request: expected a=INT&b=INT");
            }
            else{
                // Validate integers
                std::regex int_re("^[+-]?[0-9]+$");
                if (!std::regex_match(a_str, int_re) || !std::regex_match(b_str, int_re)) {
                    send_response(client_fd, 400, "Bad Request", "text/plain", "Bad Request: a and b must be integers");
                } else {
                    // Compute product
                    long long a = atoll(a_str.c_str());
                    long long b = atoll(b_str.c_str());
                    long long prod = a * b;
                    std::ostringstream body;
                    body << prod << "\n";
                    send_response(client_fd, 200, "OK", "text/plain", body.str());
                }
            }
        }
    }
    else{
        // Unknown 404
        send_response(client_fd, 404, "Not Found", "text/plain", "Not Found");
    }

    close(client_fd);
}

// Entry point
int main() {
    // Create listening socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("Socket couldn't listen");
        return 1;
    }

    // Refresh rate of socket
    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Failure setting refresh");
        // https://linuxjournal.rubdos.be/ljarchive/LJ/298/12538.html
        // Let socket be reopened without waiting
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    // Bind to socket
    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Failed to bind");
        close(listen_fd);
        return 1;
    }

    // Listen to port open
    if (listen(listen_fd, SOMAXCONN) < 0) {
        perror("Failed to listen");
        close(listen_fd);
        return 1;
    }

    unsigned int hw = std::thread::hardware_concurrency(); // Number of CPU cores on the machine running
    size_t threads = (hw == 0) ? 8 : std::max<unsigned int>(4, hw * 2); // Use number of CPUs to calc number of worker threads
    std::cout << "Starting server on port " << PORT << " with " << threads << " worker threads\n";

    ThreadPool pool(threads); // Starts worker threads

    while (true) {
        struct sockaddr_in client_addr; // Client
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len); // Accept clients 

        // If call interupted 
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            break;
        }

        // For each accepted connection, enqueue a job to handle it
        pool.enqueue([client_fd]() { handle_client(client_fd); });
    }

    close(listen_fd);
    return 0;
}

