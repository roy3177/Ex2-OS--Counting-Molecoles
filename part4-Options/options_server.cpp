#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <set>
#include <algorithm>
#include <signal.h>
#include <sstream>
#include <getopt.h>
#include <errno.h>

int tcp_port=-1;
int udp_port=-1;
int timeout_seconds=0;

bool shutdown_requested = false;

void handle_shutdown(int) {
    shutdown_requested = true;
}

void handle_timeout(int) {
    std::cout << "[INFO] Timeout reached. Shutting down.\n";
    shutdown_requested = true;
}


int main(int argc, char* argv[]) {
    signal(SIGPIPE, SIG_IGN); 
    signal(SIGINT, handle_shutdown);

  
    unsigned long long hydrogen_count = 0;
    unsigned long long oxygen_count = 0;
    unsigned long long carbon_count = 0;

   int opt;
    while ((opt = getopt(argc, argv, "o:c:h:t:T:U:")) != -1) {
        switch (opt) {
            case 'o': oxygen_count = std::stoull(optarg); break;
            case 'c': carbon_count = std::stoull(optarg); break;
            case 'h': hydrogen_count = std::stoull(optarg); break;
            case 't': timeout_seconds = std::stoi(optarg); break;
            case 'T': tcp_port = std::stoi(optarg); break;
            case 'U': udp_port = std::stoi(optarg); break;
            default:
                std::cerr << "Usage: " << argv[0] << " -T <tcp_port> -U <udp_port> [-t <timeout>] [-o <oxygen>] [-c <carbon>] [-h <hydrogen>]\n";
                return 1;
        }
    }

    if (timeout_seconds > 0) {
        signal(SIGALRM, handle_timeout);
        alarm(timeout_seconds);
    }

    if (tcp_port == -1 || udp_port == -1) {
        std::cerr << "[ERROR] Both -T <tcp_port> and -U <udp_port> are required.\n";
        return 1;
    }


    // Create the socket TCP:
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket failed");
        return 1;
    }

    // Create the UDP socket:
    int udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_fd < 0) {
        perror("UDP socket failed");
        return 1;
    }

   

    // Define the address's structure of the server:
    sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(tcp_port);

    sockaddr_in udp_addr;
    std::memset(&udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons(udp_port); // same port as TCP

    //bind to the TCP :
    int bind_result = bind(server_fd, (sockaddr *)&address, sizeof(address));
    if (bind_result < 0){
        perror("TCP bind failed");
        close(server_fd);
        return 1;
    }

    //bind to the UDP :
    if (bind(udp_fd, (sockaddr*)&udp_addr, sizeof(udp_addr)) < 0) {
        perror("UDP bind failed");
        close(server_fd);
        close(udp_fd);
        return 1;
    }


    // Starts the listen action:
    int listen_result = listen(server_fd, 5);
    if (listen_result < 0){
        perror("Listen failed");
        close(server_fd);
        return 1;
    }
    std::cout << "Server is listening on TCP port " << tcp_port
          << " and UDP port " << udp_port << "...\n";

    // Set of clients:
    std::set<int> clients;

    // Infinite loop-for IO MUX:
    while (!shutdown_requested) {
        fd_set readfds; // Struct that includes the sockets's list
        FD_ZERO(&readfds); // Starting from empty list
        FD_SET(server_fd, &readfds); // Adding the listening socket to the readfds
        int max_fd = server_fd;

        FD_SET(udp_fd, &readfds);
        if (udp_fd > max_fd) {
            max_fd = udp_fd;
        }

        FD_SET(STDIN_FILENO, &readfds);
        if (STDIN_FILENO > max_fd) {
            max_fd = STDIN_FILENO;
        }

        // Adding all the clients:
        for (int client_fd : clients) {
            FD_SET(client_fd, &readfds);
            if (client_fd > max_fd){
                max_fd = client_fd;
            }
        }

        // Active waiting until one of the sockets will be ready for reading:
       int activity = select(max_fd + 1, &readfds, nullptr, nullptr, nullptr);
        if (activity < 0 && shutdown_requested) {
            break; 
        }
        if (activity < 0) {
            perror("Select error");
            break;
        }


        std::set<int> disconnected;

        // If a new connection is coming:
        if (FD_ISSET(server_fd, &readfds)) {
            sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int new_client = accept(server_fd, (sockaddr*)&client_addr, &client_len);
            if (new_client < 0) {
                perror("Accept failed");
                continue;
            }
            clients.insert(new_client);
            std::cout << "[INFO] New client connected: " << inet_ntoa(client_addr.sin_addr) << "\n";
        }

       if (FD_ISSET(udp_fd, &readfds)) {
            char buffer[1024] = {0};
            sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            ssize_t bytes_received = recvfrom(udp_fd, buffer, sizeof(buffer) - 1, 0,
                                            (sockaddr*)&client_addr, &client_len);

            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';
                std::string command(buffer);

                if (timeout_seconds > 0) {
                    alarm(timeout_seconds);
                }


                command.erase(std::remove(command.begin(), command.end(), '\n'), command.end());
                command.erase(std::remove(command.begin(), command.end(), '\r'), command.end());

                std::istringstream iss(command);
                std::string action, molecule;
                unsigned long long amount;

                if (!(iss >> action >> molecule >> amount) || action != "DELIVER" || amount <= 0) {
                    std::string response = "CANNOT_DELIVER";
                    sendto(udp_fd, response.c_str(), response.size(), 0, 
                        (sockaddr*)&client_addr, client_len);
                    continue;
                }

                // The requiers molecules:
                unsigned long long need_H = 0, need_O = 0, need_C = 0;
                if (molecule == "WATER") {
                    need_H = 2 * amount;
                    need_O = 1 * amount;
                }
                else if (molecule == "CARBON_DIOXIDE") {
                    need_C = 1 * amount;
                    need_O = 2 * amount;
                } 
                else if (molecule == "GLUCOSE") {
                    need_C = 6 * amount;
                    need_H = 12 * amount;
                    need_O = 6 * amount;
                } 
                else if (molecule == "ALCOHOL") {
                    need_C = 2 * amount;
                    need_H = 6 * amount;
                    need_O = 1 * amount;
                } 
                else {
                    std::string response = "CANNOT_DELIVER";
                    sendto(udp_fd, response.c_str(), response.size(), 0, 
                        (sockaddr*)&client_addr, client_len);
                    continue;;
                }

                if (hydrogen_count >= need_H && oxygen_count >= need_O && carbon_count >= need_C) {
                    hydrogen_count -= need_H;
                    oxygen_count -= need_O;
                    carbon_count -= need_C;

                    std::cout << "[INFO] UDP request received: DELIVER " << molecule << "\n";
                    std::cout << "[INFO] Atoms: H=" << hydrogen_count 
                            << ", O=" << oxygen_count 
                            << ", C=" << carbon_count << "\n";

                    std::string response = "DELIVERED";
                    sendto(udp_fd, response.c_str(), response.size(), 0, 
                        (sockaddr*)&client_addr, client_len);
                } else {
                    std::string response = "CANNOT_DELIVER";
                    sendto(udp_fd, response.c_str(), response.size(), 0, 
                        (sockaddr*)&client_addr, client_len);
                }
            }
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            std::string line;
            std::getline(std::cin, line);
            std::istringstream iss(line);

            std::string cmd, drink;
            if (!(iss >> cmd >> drink) || cmd != "GEN") {
                std::cout << "[ERROR] Invalid command. Use: GEN <SOFT DRINK|VODKA|CHAMPAGNE>\n";
                continue;  
            }

            int can_make = 0;
            if (drink == "SOFT" || drink == "SOFT_DRINK" || drink == "SOFTDRINK") {
                unsigned long long h = hydrogen_count / 14;
                unsigned long long o = oxygen_count / 9;
                unsigned long long c = carbon_count / 7;
                can_make = std::min({h, o, c});
                std::cout << "Can make " << can_make << " SOFT DRINK(s)\n";
            } else if (drink == "VODKA") {
                unsigned long long h = hydrogen_count / 20;
                unsigned long long o = oxygen_count / 8;
                unsigned long long c = carbon_count / 8;
                can_make = std::min({h, o, c});
                std::cout << "Can make " << can_make << " VODKA(s)\n";
            } else if (drink == "CHAMPAGNE") {
                unsigned long long h = hydrogen_count / 8;
                unsigned long long o = oxygen_count / 4;
                unsigned long long c = carbon_count / 3;
                can_make = std::min({h, o, c});
                std::cout << "Can make " << can_make << " CHAMPAGNE(s)\n";
            } else {
                std::cout << "[ERROR] Unknown drink type.\n";
            }
        }



        for (int client_fd : clients) {
            if (FD_ISSET(client_fd, &readfds)) {
                char buffer[1024] = {0};
                int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

                if (bytes_received <= 0) {
                    std::cout << "[INFO] Client disconnected.\n";
                    close(client_fd);
                    disconnected.insert(client_fd);
                    continue;
                }

                std::string command(buffer);

                if (timeout_seconds > 0) {
                    alarm(timeout_seconds);
                }

                command.erase(std::remove(command.begin(), command.end(), '\n'), command.end());
                command.erase(std::remove(command.begin(), command.end(), '\r'), command.end());

                // Checking if the client request to exit;
                if (command == "EXIT") {
                    close(client_fd);
                    disconnected.insert(client_fd);
                    continue;
                }

                // Checking if the client request to close all of th network:
                if (command == "SHUTDOWN") {
                    std::cout << "[INFO] Shutdown command received from client.\n";
                    shutdown_requested = true;
                    close(client_fd);
                    disconnected.insert(client_fd);
                    break;
                }

                std::istringstream iss(command);
                std::string action, atom_type;
                long long amount = -1;

                if (!(iss >> action >> atom_type >> amount)) {
                    std::string error_msg = "ERROR: Usage: ADD <ATOM_TYPE> <POSITIVE_AMOUNT>\n";
                    send(client_fd, error_msg.c_str(), error_msg.size(), 0);
                    continue;
                }

                if (action != "ADD" || amount <= 0) {
                    std::string error_msg = "ERROR: Invalid command or non-positive amount.\n";
                    send(client_fd, error_msg.c_str(), error_msg.size(), 0);
                    continue;
                }

                unsigned long long* target = nullptr;
                if (atom_type == "HYDROGEN") {
                    target = &hydrogen_count;
                }
                else if (atom_type == "OXYGEN") {
                    target = &oxygen_count;
                }
                else if (atom_type == "CARBON") {
                    target = &carbon_count;
                }
                else {
                    std::string error_msg = "ERROR: Unknown atom type. Use HYDROGEN, OXYGEN, or CARBON.\n";
                    send(client_fd, error_msg.c_str(), error_msg.size(), 0);
                    continue;
                }

                *target += static_cast<unsigned long long>(amount);
                std::cout << "[INFO] Atoms: H=" << hydrogen_count
                          << ", O=" << oxygen_count
                          << ", C=" << carbon_count << "\n";

                unsigned int response = static_cast<unsigned int>(*target);
                send(client_fd, &response, sizeof(response), 0);
            }
        }

        for (int fd : disconnected) {
            clients.erase(fd);
        }
    }

    for (int fd : clients){
        close(fd);
    }
    close(server_fd);
    close(udp_fd);

    std::cout << "[INFO] Server shut down.\n";
    return 0;
}
