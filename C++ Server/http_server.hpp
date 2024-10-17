// http_server.hpp
#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include <string>
#include <functional>

class HTTPServer {
public:
    HTTPServer(int port);
    ~HTTPServer();
    void start();
    void stop();
    void setRequestHandler(const std::function<std::string(const std::string&)>& handler);

private:
    int server_fd;
    int port;
    bool running;
    std::function<std::string(const std::string&)> requestHandler;
};

#endif // HTTP_SERVER_HPP
