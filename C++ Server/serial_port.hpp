#ifndef SERIAL_PORT_HPP
#define SERIAL_PORT_HPP

#include <string>

class SerialPort {
public:
    SerialPort(const std::string& portName, int baudRate);
    ~SerialPort();
    bool readLine(std::string& data);
    bool writeData(const std::string& data); // Added method
    bool isOpen() const;

private:
    int fd; // File descriptor for the port
    bool configurePort(int baudRate);
};

#endif // SERIAL_PORT_HPP
