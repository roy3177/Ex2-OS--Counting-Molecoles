#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h> // Required for getaddrinfo

int main(int argc, char *argv[])
{

    // With the address of the server and our port:
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <host> <port>\n";
        return 1;
    }

    const char *server_ip = argv[1]; // Our server's address
    const char *port_str = argv[2];  // Our port (as string for getaddrinfo)

    // Define the server's address using getaddrinfo:
    struct addrinfo hints{}, *res;
    hints.ai_family = AF_INET;       // IPv4 only
    hints.ai_socktype = SOCK_STREAM; // TCP

    int status = getaddrinfo(server_ip, port_str, &hints, &res);
    if (status != 0)
    {
        std::cerr << "getaddrinfo error: " << gai_strerror(status) << "\n";
        return 1;
    }

    // Creating the socket TCP:
    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0)
    {
        perror("socket failed");
        freeaddrinfo(res);
        return 1;
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0)
    {
        perror("Connection failed");
        freeaddrinfo(res);
        return 1;
    }

    std::cout << "Connected to server at " << server_ip << ":" << port_str << "\n";
    freeaddrinfo(res); // We no longer need the addrinfo result

    while (true)
    {
        // Request a command from the user:
        std::cout << "Enter command (e.g., ADD HYDROGEN 3 or EXIT): ";
        std::string command;
        std::getline(std::cin, command);

        // EXIT --> Terminates the program:
        if (command == "EXIT")
        {
            std::cout << "Closing connection. Bye!\n";
            break;
        }

        command += '\n'; // Adding newline to match server expectations

        // Sending command to the server:
        ssize_t bytes_sent = send(sock, command.c_str(), command.size(), 0);
        if (bytes_sent < 0)
        {
            perror("Send failed");
            break;
        }

        // Getting response from the server:
        char buffer[1024] = {0};
        ssize_t bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            std::cerr << "Failed to receive response from server\n";
            break;
        }
        buffer[bytes_received] = '\0';
        std::cout << "Server returned: " << buffer << "\n";

            }

    close(sock);
    return 0;
}
