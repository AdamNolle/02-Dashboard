#include "serial_port.hpp"
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <iostream>

SerialPort::SerialPort(const std::string& portName, int baudRate) {
    fd = open(portName.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1) {
        std::cerr << "Failed to open serial port: " << portName << std::endl;
    } else {
        fcntl(fd, F_SETFL, 0);
        if (!configurePort(baudRate)) {
            std::cerr << "Failed to configure serial port." << std::endl;
            close(fd);
            fd = -1;
        }
    }
}

SerialPort::~SerialPort() {
    if (fd != -1) {
        close(fd);
    }
}

bool SerialPort::configurePort(int baudRate) {
    struct termios options;
    tcgetattr(fd, &options);

    // Set baud rates
    cfsetispeed(&options, baudRate);
    cfsetospeed(&options, baudRate);

    // 8N1 Mode
    options.c_cflag &= ~PARENB; // No parity
    options.c_cflag &= ~CSTOPB; // 1 stop bit
    options.c_cflag &= ~CSIZE;  // Mask data size
    options.c_cflag |= CS8;     // 8 data bits

    // Raw input/output mode
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // Raw input
    options.c_oflag &= ~OPOST; // Raw output
    options.c_cflag |= (CLOCAL | CREAD); // Enable receiver, Ignore modem control lines

    // Apply the settings
    if (tcsetattr(fd, TCSANOW, &options) != 0) {
        return false;
    }
    return true;
}

bool SerialPort::readLine(std::string& data) {
    char buf[256];
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n > 0) {
        data.assign(buf, n);
        return true;
    }
    return false;
}

bool SerialPort::writeData(const std::string& data) {
    ssize_t n = write(fd, data.c_str(), data.size());
    return n == (ssize_t)data.size();
}

bool SerialPort::isOpen() const {
    return fd != -1;
}
