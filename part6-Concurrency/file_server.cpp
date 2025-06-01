#include <iostream>     // For input/output
#include <cstring>      // For memset
#include <unistd.h>     // For close(), read(), write()
#include <arpa/inet.h>  // For inet_ntoa and sockaddr_in
#include <sys/socket.h> // For socket functions
#include <sys/select.h> // For select()
#include <set>          // For managing connected clients
#include <algorithm>    // For std::remove
#include <signal.h>     // For handling Ctrl+C (SIGINT)
#include <sstream>      // For parsing commands (e.g., istringstream)
#include <getopt.h>     // For getopt to parse command-line arguments
#include <errno.h>      // For errno and error reporting
#include <sys/un.h>     // For UNIX domain sockets
#include <sys/file.h>   // for file locking -> flock() function
#include <sys/stat.h>   // For struct stat, fstat, ftruncate
#include <sys/mman.h>   // For mmap, munmap, PROT_*, MAP_*

// Structure to hold the stock of atoms
struct Stock
{
    unsigned long long hydrogen;
    unsigned long long oxygen;
    unsigned long long carbon;
};

int tcp_port = -1;       // TCP port for the server
int udp_port = -1;       // UDP port for the server
int timeout_seconds = 0; // Timeout in seconds for the server to wait for a request(initialized to 0)

// Variables for saving the pathes of the UDS:
std::string stream_path;
std::string datagram_path;

std::string save_file_path; // Path to save the stock of atoms

bool shutdown_requested = false; // Global flag to indicate shutdown

// Signal handler to request shutdown (triggered by SIGINT)
void handle_shutdown(int)
{
    shutdown_requested = true; // Will stop the main loop
}

// Signal handler for timeout expiration (triggered by SIGALRM)
void handle_timeout(int)
{
    std::cout << "[INFO] Timeout reached. Shutting down.\n";
    shutdown_requested = true;
}

int main(int argc, char *argv[])
{
    signal(SIGPIPE, SIG_IGN);        // Ignore broken pipe (prevents crash when sending to closed socket)
    signal(SIGINT, handle_shutdown); // Handle Ctrl+C (SIGINT) to gracefully shut down the server

    // Initialize atom counts:
    unsigned long long hydrogen_count = 0;
    unsigned long long oxygen_count = 0;
    unsigned long long carbon_count = 0;

    // Parse command-line arguments
    int opt;
    while ((opt = getopt(argc, argv, "o:c:h:t:T:U:s:d:f:")) != -1)
    {
        switch (opt)
        {
        case 'o':
            oxygen_count = std::stoull(optarg);
            break;
        case 'c':
            carbon_count = std::stoull(optarg);
            break;
        case 'h':
            hydrogen_count = std::stoull(optarg);
            break;
        case 't':
            timeout_seconds = std::stoi(optarg);
            break;
        case 'T':
            tcp_port = std::stoi(optarg);
            break;
        case 'U':
            udp_port = std::stoi(optarg);
            break;
        case 's':
            stream_path = optarg;
            break;
        case 'd':
            datagram_path = optarg;
            break;
        case 'f':
            save_file_path = optarg;
            break; // Path to save the stock of atoms
        default:
            // Print usage and exit if arguments are invalid
            std::cerr << "Usage: " << argv[0] << " -T <tcp_port> -U <udp_port> [-t <timeout>] [-o <oxygen>] [-c <carbon>] [-h <hydrogen>]\n";
            return 1;
        }
    }

    // Set an alarm for timeout if specified
    if (timeout_seconds > 0)
    {
        signal(SIGALRM, handle_timeout);
        alarm(timeout_seconds);
    }
    // Ensure at least one form of communication is available
    if ((tcp_port == -1 || udp_port == -1) && stream_path.empty() && datagram_path.empty())
    {
        std::cerr << "[ERROR] Must provide at least TCP/UDP or UDS stream/datagram options.\n";
        return 1;
    }

    ///////////////////////////////////////////////////////////////
    Stock *stock_ptr = nullptr;
    int stock_fd = -1;
    bool use_save_file = !save_file_path.empty();

    if (use_save_file)
    {
        // Open or create the file
        stock_fd = open(save_file_path.c_str(), O_RDWR | O_CREAT, 0666);
        if (stock_fd < 0)
        {
            perror("Failed to open save file");
            return 1;
        }

        // Check if file exists and is the right size
        struct stat st;
        bool file_exists = (fstat(stock_fd, &st) == 0 && st.st_size == sizeof(Stock));
        if (!file_exists)
        {
            // Set file size
            if (ftruncate(stock_fd, sizeof(Stock)) < 0)
            {
                perror("Failed to set save file size");
                close(stock_fd);
                return 1;
            }
        }

        // Map the file to memory
        stock_ptr = (Stock *)mmap(nullptr, sizeof(Stock), PROT_READ | PROT_WRITE, MAP_SHARED, stock_fd, 0);
        if (stock_ptr == MAP_FAILED)
        {
            perror("mmap failed");
            close(stock_fd);
            return 1;
        }

        // If file was just created, initialize values
        if (!file_exists)
        {
            stock_ptr->hydrogen = hydrogen_count;
            stock_ptr->oxygen = oxygen_count;
            stock_ptr->carbon = carbon_count;
            msync(stock_ptr, sizeof(Stock), MS_SYNC);
        }
        else
        {
            // Load from file, ignore command-line initialization
            hydrogen_count = stock_ptr->hydrogen;
            oxygen_count = stock_ptr->oxygen;
            carbon_count = stock_ptr->carbon;
        }
    }
    //////////////////////////////////////////////////////////////

    // Create the socket TCP:
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("Socket failed");
        return 1;
    }

    // Create the UDP socket:
    int udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_fd < 0)
    {
        perror("UDP socket failed");
        return 1;
    }

    // Create UDS STREAM socket
    int uds_stream_fd = -1;
    if (!stream_path.empty())
    {
        uds_stream_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (uds_stream_fd < 0)
        {
            perror("UDS stream socket failed");
            return 1;
        }

        // Prepare the address structure for UDS STREAM
        sockaddr_un stream_addr;
        std::memset(&stream_addr, 0, sizeof(stream_addr));
        stream_addr.sun_family = AF_UNIX;
        std::strncpy(stream_addr.sun_path, stream_path.c_str(), sizeof(stream_addr.sun_path) - 1);

        unlink(stream_path.c_str());                                                // remove any existing socket file
        if (bind(uds_stream_fd, (sockaddr *)&stream_addr, sizeof(stream_addr)) < 0) // Bind the socket
        {
            // print an error message if the bind failed:
            perror("UDS stream bind failed");
            return 1;
        }

        // Listen for incoming connections on the UDS STREAM socket
        if (listen(uds_stream_fd, 5) < 0)
        {
            perror("UDS stream listen failed");
            return 1;
        }

        std::cout << "[INFO] UDS STREAM socket bound to " << stream_path << "\n";
    }

    // Create UDS DATAGRAM socket
    int uds_dgram_fd = -1;
    if (!datagram_path.empty())
    {
        uds_dgram_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (uds_dgram_fd < 0)
        {
            perror("UDS datagram socket failed");
            return 1;
        }

        sockaddr_un dgram_addr;
        std::memset(&dgram_addr, 0, sizeof(dgram_addr));
        dgram_addr.sun_family = AF_UNIX;
        std::strncpy(dgram_addr.sun_path, datagram_path.c_str(), sizeof(dgram_addr.sun_path) - 1);

        unlink(datagram_path.c_str());
        if (bind(uds_dgram_fd, (sockaddr *)&dgram_addr, sizeof(dgram_addr)) < 0)
        {
            perror("UDS datagram bind failed");
            return 1;
        }

        std::cout << "[INFO] UDS DATAGRAM socket bound to " << datagram_path << "\n";
    }

    // Set up TCP bind address
    sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(tcp_port);

    // Set up UDP bind address
    sockaddr_in udp_addr;
    std::memset(&udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons(udp_port); // same port as TCP

    // bind to the TCP :
    int bind_result = bind(server_fd, (sockaddr *)&address, sizeof(address));
    if (bind_result < 0)
    {
        perror("TCP bind failed");
        close(server_fd);
        return 1;
    }

    // bind to the UDP :
    if (bind(udp_fd, (sockaddr *)&udp_addr, sizeof(udp_addr)) < 0)
    {
        perror("UDP bind failed");
        close(server_fd);
        close(udp_fd);
        return 1;
    }

    // Start listening on TCP socket
    int listen_result = listen(server_fd, 5);
    if (listen_result < 0)
    {
        perror("Listen failed");
        close(server_fd);
        return 1;
    }
    std::cout << "Server is listening on TCP port " << tcp_port
              << " and UDP port " << udp_port << "...\n";

    // Set of clients:
    std::set<int> clients;

    // Infinite loop-for IO MUX:
    while (!shutdown_requested)
    {
        fd_set readfds;              // Struct that includes the sockets's list
        FD_ZERO(&readfds);           // Starting from empty list
        FD_SET(server_fd, &readfds); // Adding the listening socket to the readfds
        int max_fd = server_fd;
        FD_SET(udp_fd, &readfds);

        // Add UDS STREAM
        if (uds_stream_fd != -1)
        {
            FD_SET(uds_stream_fd, &readfds);
            if (uds_stream_fd > max_fd)
            {
                max_fd = uds_stream_fd;
            }
        }

        // Add UDS DGRAM
        if (uds_dgram_fd != -1)
        {
            FD_SET(uds_dgram_fd, &readfds);
            if (uds_dgram_fd > max_fd)
            {
                max_fd = uds_dgram_fd;
            }
        }

        if (udp_fd > max_fd)
        {
            max_fd = udp_fd;
        }

        // Adding all the clients:
        for (int client_fd : clients)
        {
            FD_SET(client_fd, &readfds);
            if (client_fd > max_fd)
            {
                max_fd = client_fd;
            }
        }

        // Active waiting until one of the sockets will be ready for reading:
        int activity = select(max_fd + 1, &readfds, nullptr, nullptr, nullptr);
        if (activity < 0 && shutdown_requested)
        {
            break;
        }
        if (activity < 0)
        {
            perror("Select error");
            break;
        }

        std::set<int> disconnected;

        // Accept TCP clients
        if (FD_ISSET(server_fd, &readfds))
        {
            sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int new_client = accept(server_fd, (sockaddr *)&client_addr, &client_len);
            if (new_client < 0)
            {
                perror("Accept failed");
                continue;
            }
            clients.insert(new_client);
            std::cout << "[INFO] New client connected: " << inet_ntoa(client_addr.sin_addr) << "\n";
        }

        // Accept UDS STREAM clients
        if (uds_stream_fd != -1 && FD_ISSET(uds_stream_fd, &readfds))
        {
            sockaddr_un client_addr;
            socklen_t client_len = sizeof(client_addr);
            int new_client = accept(uds_stream_fd, (sockaddr *)&client_addr, &client_len);
            if (new_client < 0)
            {
                perror("UDS stream accept failed");
            }
            else
            {
                clients.insert(new_client);
                std::cout << "[INFO] New UDS STREAM client connected.\n";
            }
        }

        // If a new UDP request is coming:
        if (FD_ISSET(udp_fd, &readfds))
        {
            char buffer[1024] = {0};
            sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            ssize_t bytes_received = recvfrom(udp_fd, buffer, sizeof(buffer) - 1, 0,
                                              (sockaddr *)&client_addr, &client_len);

            if (bytes_received > 0)
            {
                buffer[bytes_received] = '\0';
                std::string command(buffer);

                if (timeout_seconds > 0)
                {
                    alarm(timeout_seconds);
                }

                command.erase(std::remove(command.begin(), command.end(), '\n'), command.end());
                command.erase(std::remove(command.begin(), command.end(), '\r'), command.end());

                std::istringstream iss(command);
                std::string action, molecule;
                unsigned long long amount;

                // Parse UDP command (DELIVER <molecule> <amount>)
                if (!(iss >> action >> molecule >> amount) || action != "DELIVER" || amount <= 0)
                {
                    std::string response = "CANNOT_DELIVER";
                    sendto(udp_fd, response.c_str(), response.size(), 0,
                           (sockaddr *)&client_addr, client_len);
                    continue;
                }

                // The required molecules:
                unsigned long long need_H = 0, need_O = 0, need_C = 0;
                if (molecule == "WATER")
                {
                    need_H = 2 * amount;
                    need_O = 1 * amount;
                }
                else if (molecule == "CARBON DIOXIDE")
                {
                    need_C = 1 * amount;
                    need_O = 2 * amount;
                }
                else if (molecule == "GLUCOSE")
                {
                    need_C = 6 * amount;
                    need_H = 12 * amount;
                    need_O = 6 * amount;
                }
                else if (molecule == "ALCOHOL")
                {
                    need_C = 2 * amount;
                    need_H = 6 * amount;
                    need_O = 1 * amount;
                }
                else
                {
                    std::string response = "CANNOT_DELIVER";
                    sendto(udp_fd, response.c_str(), response.size(), 0,
                           (sockaddr *)&client_addr, client_len);
                    continue;
                    ;
                }

                // Check if we have enough atoms to deliver the requested molecule:
                if (hydrogen_count >= need_H && oxygen_count >= need_O && carbon_count >= need_C)
                {
                    hydrogen_count -= need_H;
                    oxygen_count -= need_O;
                    carbon_count -= need_C;

                    std::cout << "[INFO] UDP request received: DELIVER " << molecule << "\n";
                    std::cout << "[INFO] Atoms: H=" << hydrogen_count
                              << ", O=" << oxygen_count
                              << ", C=" << carbon_count << "\n";

                    std::string response = "DELIVERED";
                    sendto(udp_fd, response.c_str(), response.size(), 0,
                           (sockaddr *)&client_addr, client_len);
                }

                else
                {
                    std::string response = "CANNOT_DELIVER";
                    sendto(udp_fd, response.c_str(), response.size(), 0,
                           (sockaddr *)&client_addr, client_len);
                }
            }
        }

        // If a new UDS DATAGRAM request is coming:
        if (uds_dgram_fd != -1 && FD_ISSET(uds_dgram_fd, &readfds))
        {
            char buffer[1024] = {0};
            sockaddr_un client_addr;
            socklen_t client_len = sizeof(client_addr);
            ssize_t bytes_received = recvfrom(uds_dgram_fd, buffer, sizeof(buffer) - 1, 0,
                                              (sockaddr *)&client_addr, &client_len);

            if (bytes_received > 0)
            {
                buffer[bytes_received] = '\0';
                std::string command(buffer);

                if (timeout_seconds > 0)
                {
                    alarm(timeout_seconds);
                }

                // Clean up input:
                command.erase(std::remove(command.begin(), command.end(), '\n'), command.end());
                command.erase(std::remove(command.begin(), command.end(), '\r'), command.end());

                std::istringstream iss(command);
                std::string action, molecule;
                unsigned long long amount;

                // Parse UDS DATAGRAM command (DELIVER <molecule> <amount>)
                if (!(iss >> action >> molecule >> amount) || action != "DELIVER" || amount <= 0)
                {
                    std::string response = "CANNOT_DELIVER";
                    sendto(uds_dgram_fd, response.c_str(), response.size(), 0,
                           (sockaddr *)&client_addr, client_len);
                    continue;
                }

                // The required molecules:
                unsigned long long need_H = 0, need_O = 0, need_C = 0;
                if (molecule == "WATER")
                {
                    need_H = 2 * amount;
                    need_O = 1 * amount;
                }
                else if (molecule == "CARBON DIOXIDE")
                {
                    need_C = 1 * amount;
                    need_O = 2 * amount;
                }
                else if (molecule == "GLUCOSE")
                {
                    need_C = 6 * amount;
                    need_H = 12 * amount;
                    need_O = 6 * amount;
                }
                else if (molecule == "ALCOHOL")
                {
                    need_C = 2 * amount;
                    need_H = 6 * amount;
                    need_O = 1 * amount;
                }
                else
                {
                    std::string response = "CANNOT_DELIVER";
                    sendto(uds_dgram_fd, response.c_str(), response.size(), 0,
                           (sockaddr *)&client_addr, client_len);
                    continue;
                    ;
                }

                // Check if we have enough atoms to deliver the requested molecule:
                if (hydrogen_count >= need_H && oxygen_count >= need_O && carbon_count >= need_C)
                {
                    hydrogen_count -= need_H;
                    oxygen_count -= need_O;
                    carbon_count -= need_C;

                    std::cout << "[INFO] UDS DATAGRAM request received: " << command << "\n";
                    std::cout << "[INFO] Atoms: H=" << hydrogen_count
                              << ", O=" << oxygen_count
                              << ", C=" << carbon_count << "\n";

                    std::string response = "DELIVERED";
                    sendto(uds_dgram_fd, response.c_str(), response.size(), 0,
                           (sockaddr *)&client_addr, client_len);
                }

                ////////////////////////////////////////////////////////////
                if (use_save_file && stock_ptr && stock_fd != -1)
                {
                    flock(stock_fd, LOCK_EX);
                    stock_ptr->hydrogen = hydrogen_count;
                    stock_ptr->oxygen = oxygen_count;
                    stock_ptr->carbon = carbon_count;
                    msync(stock_ptr, sizeof(Stock), MS_SYNC);
                    flock(stock_fd, LOCK_UN);
                }

                ///////////////////////////////////////////////////////////////

                else
                {
                    std::string response = "CANNOT_DELIVER";
                    sendto(uds_dgram_fd, response.c_str(), response.size(), 0,
                           (sockaddr *)&client_addr, client_len);
                }
            }
        }

        // Handle message from clients:
        for (int client_fd : clients)
        {
            if (FD_ISSET(client_fd, &readfds))
            {
                char buffer[1024] = {0};
                int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

                if (bytes_received <= 0)
                {
                    std::cout << "[INFO] Client disconnected.\n";
                    close(client_fd);
                    disconnected.insert(client_fd);
                    continue;
                }

                std::string command(buffer);

                if (timeout_seconds > 0)
                {
                    alarm(timeout_seconds);
                }
                //  Clean up input:
                command.erase(std::remove(command.begin(), command.end(), '\n'), command.end());
                command.erase(std::remove(command.begin(), command.end(), '\r'), command.end());

                // Checking if the client request to exit;
                if (command == "EXIT")
                {
                    std::cout << "[INFO] Client requested EXIT.\n";
                    close(client_fd);
                    disconnected.insert(client_fd);
                    continue;
                }

                // Checking if the client request to close all of th network:
                if (command == "SHUTDOWN")
                {
                    std::cout << "[INFO] Shutdown command received from client.\n";
                    shutdown_requested = true;
                    close(client_fd);
                    disconnected.insert(client_fd);
                    break;
                }

                std::istringstream iss(command);
                std::string action, atom_type;
                long long amount = -1;

                if (!(iss >> action >> atom_type >> amount))
                {
                    std::string error_msg = "ERROR: Usage: ADD <ATOM_TYPE> <POSITIVE_AMOUNT>\n";
                    send(client_fd, error_msg.c_str(), error_msg.size(), 0);
                    continue;
                }
                // Validate the command:
                if (action != "ADD" || amount <= 0)
                {
                    std::string error_msg = "ERROR: Invalid command or non-positive amount.\n";
                    send(client_fd, error_msg.c_str(), error_msg.size(), 0);
                    continue;
                }
                // Choose Atom:
                unsigned long long *target = nullptr;
                if (atom_type == "HYDROGEN")
                {
                    target = &hydrogen_count;
                }
                else if (atom_type == "OXYGEN")
                {
                    target = &oxygen_count;
                }
                else if (atom_type == "CARBON")
                {
                    target = &carbon_count;
                }
                else // If the atom type is not recognized:
                {
                    std::string error_msg = "ERROR: Unknown atom type. Use HYDROGEN, OXYGEN, or CARBON.\n";
                    send(client_fd, error_msg.c_str(), error_msg.size(), 0);
                    continue;
                }

                *target += static_cast<unsigned long long>(amount);
                std::cout << "[INFO] Atoms: H=" << hydrogen_count
                          << ", O=" << oxygen_count
                          << ", C=" << carbon_count << "\n";

                std::string response = "SUCCESS: Atom added successfully.\n";
                // Update and respond:
                send(client_fd, response.c_str(), response.size(), 0);
            }
            ///////////////////////////////////////
            if (use_save_file && stock_ptr && stock_fd != -1)
            {
                flock(stock_fd, LOCK_EX);
                stock_ptr->hydrogen = hydrogen_count;
                stock_ptr->oxygen = oxygen_count;
                stock_ptr->carbon = carbon_count;
                msync(stock_ptr, sizeof(Stock), MS_SYNC);
                flock(stock_fd, LOCK_UN);
            }
            ///////////////////////////////////////
        }
        // Disconnect Clients:
        for (int fd : disconnected)
        {
            clients.erase(fd);
        }
    }

    // Clean Shutdown:
    for (int fd : clients)
    {
        close(fd);
    }

    close(server_fd);
    close(udp_fd);

    if (uds_stream_fd != -1)
    {
        close(uds_stream_fd);
        unlink(stream_path.c_str());
    }

    if (uds_dgram_fd != -1)
    {
        close(uds_dgram_fd);
        unlink(datagram_path.c_str());
    }

    if (use_save_file && stock_ptr)
    {
        msync(stock_ptr, sizeof(Stock), MS_SYNC);
        munmap(stock_ptr, sizeof(Stock));
        close(stock_fd);
    }

    std::cout << "[INFO] Server shut down.\n";
    return 0;
}
