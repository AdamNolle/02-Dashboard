#include "serial_port.hpp"
#include "http_server.hpp"
#include <atomic>
#include <fstream>
#include <ctime>
#include <sstream>
#include <vector>
#include <mutex>
#include <iostream>
#include <thread>
#include <unistd.h>   // For usleep
#include <termios.h>  // For B9600

// Global variables
std::atomic<bool> recording(false);
std::atomic<bool> pauseRecording(false);
std::string currentFileName;
std::mutex dataMutex;
std::vector<std::string> dataFiles;
SerialPort serialPort("/dev/ttyUSB0", B9600); // Adjust as needed
std::atomic<bool> running(true); // Control flag for the data thread
std::string latestData; // Store the latest data read from the sensor
std::mutex latestDataMutex;

std::string readSensorData(SerialPort& serialPort) {
    // Send command to the sensor
    std::string command = "%\r\n";
    if (!serialPort.writeData(command)) {
        std::cerr << "Failed to write to serial port." << std::endl;
        return "";
    }

    // Give the sensor some time to respond
    usleep(100000); // Sleep for 100 ms

    // Read the response
    std::string data;
    if (serialPort.readLine(data)) {
        // Remove any trailing newlines or carriage returns
        data.erase(std::remove(data.begin(), data.end(), '\r'), data.end());
        data.erase(std::remove(data.begin(), data.end(), '\n'), data.end());
        return data;
    }
    return "";
}

std::string getCurrentTimestamp() {
    std::time_t now = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return buf;
}

std::string handleRequest(const std::string& request) {
    // Simple HTTP request parsing
    std::istringstream requestStream(request);
    std::string method, path, protocol;
    requestStream >> method >> path >> protocol;

    // Remove query parameters
    size_t pos = path.find('?');
    if (pos != std::string::npos) {
        path = path.substr(0, pos);
    }

    if (path == "/") {
        // Serve the main page
        std::ifstream htmlFile("index.html");
        if (!htmlFile.is_open()) {
            return "HTTP/1.1 500 Internal Server Error\r\n\r\n";
        }
        std::stringstream buffer;
        buffer << htmlFile.rdbuf();
        std::string content = buffer.str();
        std::ostringstream oss;
        oss << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length()
            << "\r\nContent-Type: text/html\r\n\r\n" << content;
        return oss.str();
    } else if (path == "/data") {
        // Serve the latest sensor data
        std::string data;
        {
            std::lock_guard<std::mutex> lock(latestDataMutex);
            data = latestData;
        }
        std::string content = "{\"data\":\"" + data + "\"}";
        std::ostringstream oss;
        oss << "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
            << content.length() << "\r\n\r\n" << content;
        return oss.str();
    } else if (path.find("/control") == 0) {
        // Handle control commands
        // Extract action from query parameters
        size_t actionPos = request.find("action=");
        std::string action;
        if (actionPos != std::string::npos) {
            action = request.substr(actionPos + 7);
            size_t endPos = action.find('&');
            if (endPos != std::string::npos) {
                action = action.substr(0, endPos);
            }
        }

        if (action == "start") {
            if (!recording) {
                recording = true;
                pauseRecording = false;
                currentFileName = "data/data_" + std::to_string(std::time(nullptr)) + ".csv";
                // Write CSV header
                std::ofstream outFile(currentFileName, std::ios::app);
                outFile << "Timestamp,O2 Level\n";
                outFile.close();
            }
        } else if (action == "pause") {
            pauseRecording = true;
        } else if (action == "resume") {
            pauseRecording = false;
        } else if (action == "stop") {
            if (recording) {
                recording = false;
                pauseRecording = false;
                dataFiles.push_back(currentFileName);
            }
        }
        return "HTTP/1.1 204 No Content\r\n\r\n";
    } else if (path == "/files") {
        // List saved files
        std::string content = "{\"files\":[";
        for (size_t i = 0; i < dataFiles.size(); ++i) {
            content += "\"" + dataFiles[i] + "\"";
            if (i != dataFiles.size() - 1) {
                content += ",";
            }
        }
        content += "]}";
        std::ostringstream oss;
        oss << "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
            << content.length() << "\r\n\r\n" << content;
        return oss.str();
    } else if (path.find("/view_file/") == 0) {
        // Serve the requested file
        std::string filename = path.substr(11); // Remove '/view_file/'
        std::ifstream file(filename);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string content = buffer.str();
            std::ostringstream oss;
            oss << "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "
                << content.length() << "\r\n\r\n" << content;
            return oss.str();
        } else {
            return "HTTP/1.1 404 Not Found\r\n\r\n";
        }
    } else {
        return "HTTP/1.1 404 Not Found\r\n\r\n";
    }
}

int main() {
    // Ensure data directory exists
    system("mkdir -p data");

    if (!serialPort.isOpen()) {
        std::cerr << "Failed to open serial port." << std::endl;
        return -1;
    }

    HTTPServer server(8080);
    server.setRequestHandler(handleRequest);
    server.start();

    std::thread dataThread([&]() {
        while (running) {
            // Read data from the sensor
            std::string data = readSensorData(serialPort);
            if (!data.empty()) {
                // Store the latest data
                {
                    std::lock_guard<std::mutex> lock(latestDataMutex);
                    latestData = data;
                }

                if (recording && !pauseRecording) {
                    std::lock_guard<std::mutex> lock(dataMutex);
                    std::ofstream outFile(currentFileName, std::ios::app);
                    outFile << getCurrentTimestamp() << "," << data << "\n";
                    outFile.close();
                }
            }

            usleep(1000000); // Sleep for 1 second
        }
    });

    std::cout << "Server started on port 8080" << std::endl;
    std::cout << "Press Enter to stop the server..." << std::endl;
    std::cin.get();

    running = false;
    server.stop();
    dataThread.join(); // Wait for the thread to finish

    return 0;
}
