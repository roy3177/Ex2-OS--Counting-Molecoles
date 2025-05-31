#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <getopt.h>
#include <sys/un.h>
#include <netdb.h>

int main(int argc, char* argv[]) {

    std::string host;
    int port=-1;
    std::string uds_path;
    int opt;

    while((opt=getopt(argc,argv,"f:")) != -1){
        switch (opt){
            case 'f':
                uds_path=optarg;
                break;
        
            default:
                std::cerr << "Usage: " << argv[0] << " <host> <port> | -f <uds_path>\n";
                return 1;
        }
    }
   
    bool use_uds = !uds_path.empty();
    if (!use_uds) {
        if (optind + 2 > argc) {
            std::cerr << "Usage: " << argv[0] << " <host> <port> | -f <uds_path>\n";
            return 1;
        }
        host = argv[optind];
        port = std::stoi(argv[optind + 1]);
    }

 

    int sock;
    if (use_uds) {
        sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock < 0) {
            perror("socket failed");
            return 1;
        }

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, uds_path.c_str(), sizeof(addr.sun_path) - 1);

        if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("connect failed");
            return 1;
        }

        std::cout << "Connected to server via UDS at " << uds_path << "\n";
    } 

    else {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            perror("socket failed");
            return 1;
        }

        addrinfo hints{}, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        int status = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res);
        if (status != 0) {
            std::cerr << "getaddrinfo: " << gai_strerror(status) << "\n";
            return 1;
        }

        sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sock < 0) {
            perror("socket failed");
            freeaddrinfo(res);
            return 1;
        }

        if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
            perror("Connection failed");
            freeaddrinfo(res);
            return 1;
        }

        std::cout << "Connected to server at " << host << ":" << port << "\n";
        freeaddrinfo(res);


    }

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
        char buffer[1024] = {0};
        ssize_t bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            perror("recv failed");
            break;
        }
        buffer[bytes_received] = '\0';
        std::cout << "Server returned: " << buffer << "\n";

    }

    close(sock);
    return 0;
}
