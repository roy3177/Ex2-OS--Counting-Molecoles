#include <iostream>          // For standard input/output (cin, cout, cerr)
#include <cstring>           // For functions like memset
#include <unistd.h>          // For close(), read(), write(), etc.
#include <arpa/inet.h>       // For htons(), inet_pton(), etc. (network address conversion)
#include <sys/socket.h>      // For socket(), connect(), send(), recv(), etc.
#include <getopt.h>          // For getopt() to parse command-line options
#include <sys/un.h>          // For Unix domain sockets (AF_UNIX)
#include <netdb.h>           // For getaddrinfo() and related structures

int main(int argc, char* argv[]) 
{

    std::string host;        // To hold the hostname or IP address
    int port = -1;           // To hold the port number (default -1 means not set)
    std::string uds_path;    // To hold the Unix Domain Socket path
    int opt;                 // Option variable for getopt()

    // Parse command-line arguments using getopt() for -f <uds_path>
    while((opt=getopt(argc,argv,"f:")) != -1)
    {
        switch (opt)
        {
            case 'f':
                uds_path=optarg; // Store UDS path
                break;
        
            default:
                // If incorrect usage, print usage message and exit
                std::cerr << "Usage: " << argv[0] << " <host> <port> | -f <uds_path>\n";
                return 1;
        }
    }
   
    // Determine if Unix Domain Socket should be used (if uds_path is not empty)
    bool use_uds = !uds_path.empty();
    if (!use_uds) 
    {
        // Check that <host> and <port> were provided as positional arguments
        if (optind + 2 > argc) 
        {
            std::cerr << "Usage: " << argv[0] << " <host> <port> | -f <uds_path>\n";
            return 1;
        }
        host = argv[optind];
        port = std::stoi(argv[optind + 1]);
    }

 

    int sock; // Socket file descriptor
    if (use_uds) 
    {
        // Create a Unix domain socket
        sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock < 0) 
        {
            perror("socket failed");
            return 1;
        }
        
        // Set up the socket address structure for UDS
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, uds_path.c_str(), sizeof(addr.sun_path) - 1);
        
        // Connect to the Unix domain socket
        if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) 
        {
            perror("connect failed");
            return 1;
        }

        std::cout << "Connected to server via UDS at " << uds_path << "\n";
    } 

    else 
    {
        // Create an INET (IPv4) socket
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) 
        {
            perror("socket failed");
            return 1;
        }

        // Set up the address resolution hints
        addrinfo hints{}, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;  // Use IPv4
        hints.ai_socktype = SOCK_STREAM; // TCP stream

        // Resolve the hostname and port into an address
        int status = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res);
        if (status != 0) 
        {
            std::cerr << "getaddrinfo: " << gai_strerror(status) << "\n";
            return 1;
        }

        // Create socket using resolved address info
        sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sock < 0) 
        {
            perror("socket failed");
            freeaddrinfo(res);
            return 1;
        }

        // Connect to the remote server
        if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) 
        {
            perror("Connection failed");
            freeaddrinfo(res);
            return 1;
        }

        std::cout << "Connected to server at " << host << ":" << port << "\n";
        freeaddrinfo(res); // Free the linked list returned by getaddrinfo
    }

    // Command loop: allows user to interact with the server
    while (true) 
    {
        // Request a command from the user:
        std::cout << "Enter command (e.g., ADD HYDROGEN 3 or EXIT): ";
        std::string command;
        std::getline(std::cin, command); // Read entire line from user

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
        if (bytes_received <= 0) 
        {
            perror("recv failed");
            break;
        }
        buffer[bytes_received] = '\0'; // Null-terminate the received string
        std::cout << "Server returned: " << buffer << "\n"; // Display response

    }

    close(sock); // Close the socket before exiting
    return 0;
}