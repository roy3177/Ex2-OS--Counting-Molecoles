#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sstream>
#include <netdb.h>

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <host> <port>\n";
        return 1;
    }

    const char *server_ip = argv[1];
    int port = std::stoi(argv[2]);

    // Create the UDP socket:
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        perror("socket failed");
        return 1;
    }

    struct addrinfo hints{}, *res;
    hints.ai_family = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_DGRAM; // UDP

    int status = getaddrinfo(server_ip, std::to_string(port).c_str(), &hints, &res);
    if (status != 0)
    {
        std::cerr << "getaddrinfo error: " << gai_strerror(status) << "\n";
        return 1;
    }

    sockaddr_storage server_addr{};
    std::memcpy(&server_addr, res->ai_addr, res->ai_addrlen);
    socklen_t addr_len = res->ai_addrlen;

    freeaddrinfo(res); // Clean up

    while (true)
    {
        std::cout << "Enter command (DELIVER <MOLECULE> <AMOUNT> | EXIT): ";
        std::string command;
        std::getline(std::cin, command);

        if (command == "EXIT")
        {
            std::cout << "Exiting.\n";
            break;
        }

        // Send the command to the server:
        ssize_t sent = sendto(sock, command.c_str(), command.size(), 0,
                              (sockaddr *)&server_addr, sizeof(server_addr));
        if (sent < 0)
        {
            perror("sendto failed");
            break;
        }

        // Get response from the server:
        char buffer[1024] = {0};
        socklen_t len = sizeof(server_addr);
        ssize_t received = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                                    (sockaddr *)&server_addr, &len);
        if (received < 0)
        {
            perror("recvfrom failed");
            break;
        }

        buffer[received] = '\0';
        std::cout << "[Server Reply] " << buffer << "\n";
    }

    close(sock);
    return 0;
}
