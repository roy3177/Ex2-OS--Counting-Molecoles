#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sstream>
#include <getopt.h>
#include <sys/un.h>
#include <netdb.h>

std::string client_path; // For later cleanup

int main(int argc, char *argv[])
{
    // Handling arguments: either <host> <port> OR -f <uds_path>
    std::string host;
    int port = -1;
    std::string uds_path;
    int opt;

    while ((opt = getopt(argc, argv, "f:")) != -1)
    {
        switch (opt)
        {
        case 'f':
            uds_path = optarg;
            break;
        default:
            std::cerr << "Usage: " << argv[0] << " <host> <port> | -f <uds_path>\n";
            return 1;
        }
    }

    bool use_uds = !uds_path.empty();
    if (!use_uds)
    {
        if (optind + 2 > argc)
        {
            std::cerr << "Usage: " << argv[0] << " <host> <port> | -f <uds_path>\n";
            return 1;
        }
        host = argv[optind];
        port = std::stoi(argv[optind + 1]);
    }

    int sock;
    sockaddr_storage server_addr{}; // Can be sockaddr_in or sockaddr_un
    socklen_t addr_len;

    if (use_uds)
    {
        // Create UNIX domain datagram socket (AF_UNIX + SOCK_DGRAM)
        sock = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (sock < 0)
        {
            perror("socket failed");
            return 1;
        }

        // Step 1: Bind to a temporary client path so server can reply
        sockaddr_un client_addr{};
        client_addr.sun_family = AF_UNIX;

        // Unique client socket path using the process ID
        client_path = "/tmp/molecule_requester_" + std::to_string(getpid()) + ".sock";
        std::strncpy(client_addr.sun_path, client_path.c_str(), sizeof(client_addr.sun_path) - 1);

        // Make sure the path doesn't already exist
        unlink(client_path.c_str());

        if (bind(sock, (sockaddr *)&client_addr, sizeof(client_addr)) < 0)
        {
            perror("bind failed");
            return 1;
        }

        // Step 2: Define server address
        sockaddr_un *addr = (sockaddr_un *)&server_addr;
        addr->sun_family = AF_UNIX;
        std::strncpy(addr->sun_path, uds_path.c_str(), sizeof(addr->sun_path) - 1);
        addr_len = sizeof(sockaddr_un);

        std::cout << "Using UNIX Domain Socket at " << uds_path << "\n";
        ;
    }
    else
    {
        // Create the UDP socket:
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0)
        {
            perror("socket failed");
            return 1;
        }

        struct addrinfo hints{}, *res;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;

        int status = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res);
        if (status != 0)
        {
            std::cerr << "getaddrinfo error: " << gai_strerror(status) << "\n";
            return 1;
        }

        std::memcpy(&server_addr, res->ai_addr, res->ai_addrlen);
        addr_len = res->ai_addrlen;

        std::cout << "Using UDP to " << host << ":" << port << "\n";

        freeaddrinfo(res); // cleanup
    }

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
                              (sockaddr *)&server_addr, addr_len);
        if (sent < 0)
        {
            perror("sendto failed");
            break;
        }

        // Get response from the server:
        //  Get response from the server:
        char buffer[1024] = {0};
        ssize_t received = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, nullptr, nullptr);
        if (received < 0)
        {
            perror("recvfrom failed");
            break;
        }

        buffer[received] = '\0';
        std::cout << "[Server Reply] " << buffer << "\n";
    }

    close(sock);

    if (use_uds)
    {
        unlink(client_path.c_str()); // Cleanup the temporary UDS client socket
    }

    return 0;
}
