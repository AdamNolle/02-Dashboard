// http_server.cpp
#include "http_server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include <cstring>
#include <iostream>

HTTPServer::HTTPServer(int port) : port(port), running(false) {}

HTTPServer::~HTTPServer() {
    stop();
}

void HTTPServer::setRequestHandler(const std::function<std::string(const std::string&)>& handler) {
    requestHandler = handler;
}

void HTTPServer::start() {
    running = true;
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        std::cerr << "Socket failed" << std::endl;
        running = false;
        return;
    }

    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // 0.0.0.0
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0) {
        std::cerr << "Bind failed" << std::endl;
        running = false;
        return;
    }

    if (listen(server_fd, 3) < 0) {
        std::cerr << "Listen failed" << std::endl;
        running = false;
        return;
    }

    std::thread([this, addrlen]() {
        while (running) {
            int new_socket = accept(server_fd, (struct sockaddr *)NULL, NULL);
            if (new_socket < 0) {
                if (running) {
                    std::cerr << "Accept failed" << std::endl;
                }
                continue;
            }

            std::thread([this, new_socket]() {
                char buffer[1024] = {0};
                read(new_socket, buffer, 1024);
                std::string request(buffer);
                std::string response = requestHandler ? requestHandler(request) : "HTTP/1.1 404 Not Found\r\n\r\n";
                send(new_socket, response.c_str(), response.length(), 0);
                close(new_socket);
            }).detach();
        }
    }).detach();
}

void HTTPServer::stop() {
    running = false;
    if (server_fd != -1) {
        close(server_fd);
    }
}
