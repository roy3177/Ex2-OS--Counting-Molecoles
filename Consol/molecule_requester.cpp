#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sstream>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <host> <port>\n";
        return 1;
    }

    const char* server_ip = argv[1];
    int port = std::stoi(argv[2]);

    //Create the UDP socket:
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket failed");
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address / Address not supported\n";
        return 1;
    }

    while (true) {
        std::cout << "Enter command (DELIVER <MOLECULE> <AMOUNT> | EXIT): ";
        std::string command;
        std::getline(std::cin, command);

        if (command == "EXIT") {
            std::cout << "Exiting.\n";
            break;
        }

        // Send the command to the server:
        ssize_t sent = sendto(sock, command.c_str(), command.size(), 0,
                              (sockaddr*)&server_addr, sizeof(server_addr));
        if (sent < 0) {
            perror("sendto failed");
            break;
        }

        //Get response from the server:
        char buffer[1024] = {0};
        socklen_t len = sizeof(server_addr);
        ssize_t received = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                                    (sockaddr*)&server_addr, &len);
        if (received < 0) {
            perror("recvfrom failed");
            break;
        }

        buffer[received] = '\0';
        std::cout << "[Server Reply] " << buffer << "\n";
    }

    close(sock);
    return 0;
}
