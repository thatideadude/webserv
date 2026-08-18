#include <iostream>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

int main() {
    // Create socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "socket() failed" << std::endl;
        return 1;
    }
    
    // Set SO_REUSEADDR
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Set non-blocking
    fcntl(server_fd, F_SETFL, O_NONBLOCK);
    
    // Bind
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "bind() failed: " << strerror(errno) << std::endl;
        close(server_fd);
        return 1;
    }
    
    if (listen(server_fd, 10) < 0) {
        std::cerr << "listen() failed: " << strerror(errno) << std::endl;
        close(server_fd);
        return 1;
    }
    
    std::cout << "Server listening on port 8080 (fd " << server_fd << ")" << std::endl;
    
    // Setup poll
    struct pollfd pfd;
    pfd.fd = server_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    
    std::cout << "Waiting for connections..." << std::endl;
    
    while (true) {
        int ret = poll(&pfd, 1, 5000);  // 5 second timeout
        
        std::cout << "poll() returned: " << ret << std::endl;
        
        if (ret < 0) {
            std::cerr << "poll() error: " << strerror(errno) << std::endl;
            break;
        }
        
        if (ret == 0) {
            std::cout << "Timeout, no connections" << std::endl;
            continue;
        }
        
        if (pfd.revents & POLLIN) {
            std::cout << "🎉 Got connection!" << std::endl;
            int client = accept(server_fd, NULL, NULL);
            std::cout << "Accepted client: " << client << std::endl;
            close(client);
            break;
        }
    }
    
    close(server_fd);
    return 0;
}
