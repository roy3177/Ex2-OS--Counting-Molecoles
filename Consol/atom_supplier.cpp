#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main(int argc, char* argv[]) {

    // With the address of the server and our port:
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <host> <port>\n";
        return 1;
    }

    const char* server_ip = argv[1]; // Our server's address
    int port = std::stoi(argv[2]);   // Our port

    // Creating the socket TCP:
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket failed");
        return 1;
    }

    // Define the server's address:
    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    int conversion_result = inet_pton(AF_INET, server_ip, &server_addr.sin_addr);
    if (conversion_result <= 0) {
        std::cerr << "Invalid address / Address not supported\n";
        return 1;
    }

    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        return 1;
    }

    std::cout << "Connected to server at " << server_ip << ":" << port << "\n";

    while (true) {
        // Request a command from the user:
        std::cout << "Enter command (e.g., ADD HYDROGEN 3 or EXIT): ";
        std::string command;
        std::getline(std::cin, command);

        // EXIT --> Terminates the program:
        if (command == "EXIT") {
            std::cout << "Closing connection. Bye!\n";
            break;
        }

        command += '\n'; // Adding newline to match server expectations

        // Sending command to the server:
        ssize_t bytes_sent = send(sock, command.c_str(), command.size(), 0);
        if (bytes_sent < 0) {
            perror("Send failed");
            break;
        }

        // Getting response from the server:
        char response_buffer[1024] = {0};
        ssize_t bytes_received = recv(sock, response_buffer, sizeof(response_buffer) - 1, 0);
        if (bytes_received <= 0) {
            std::cerr << "Failed to receive response from server\n";
            break;
        }

        response_buffer[bytes_received] = '\0'; // Null-terminate the string

        // Try to parse it as a number
        try {
            unsigned int count = std::stoul(response_buffer);
            std::cout << "Server returned count: " << count << "\n";
        } catch (...) {
            std::cout << "[Server Message] " << response_buffer << "\n";
        }
    }

    close(sock);
    return 0;
}
