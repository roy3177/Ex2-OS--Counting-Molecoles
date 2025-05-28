# include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sstream>
#include <algorithm>

int main(int argc, char* argv[]){

    if(argc!=2){
        std::cerr<<"Usage :" <<argv[0] << " <UDP_PORT>\n";
        return 1;
    }

    //Convert the port(as a string) to a number:
    int port=std::stoi(argv[1]);

    //Open the UDP socket:
    int udp_socket=socket(AF_INET,SOCK_DGRAM,0);
    if(udp_socket<0){
        perror("The creation of the UDP socket is failed");
        return 1;
    }

    //Define the address of the server:
    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET; // IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY; // Every local address
    server_addr.sin_port = htons(port); // convert the port to network byte order

    //Do the bind() action => connect our socket to the address and the port we define:
    int bind_result = bind(udp_socket, (sockaddr *)&server_addr, sizeof(server_addr));
    if (bind_result < 0){
        perror("bind failed");
        close(udp_socket);
        return 1;
    }

    std::cout << "UDP server listening on port " << port << "...\n";

    while(true){
        char buffer[1024]={0};
        sockaddr_in client_addr;
        socklen_t client_len=sizeof(client_addr);

        //recvfrom is waiting for getting message (UDP) from client:
        int bytes_received=recvfrom(udp_socket,buffer,sizeof(buffer)-1,0,(sockaddr*)&client_addr,&client_len);


        if(bytes_received<0){
            perror("recvfrom failed");
            continue;
        }


        buffer[bytes_received] = '\0'; // Ending our string
        std::string command(buffer); //Our response-->creating our string from the buffer

        /*
        /n --> next line
        /r-->go to the beginning of the line
        */

        command.erase(std::remove(command.begin(), command.end(), '\n'), command.end());
        command.erase(std::remove(command.begin(), command.end(), '\r'), command.end());


        std::cout << "[UDP Received] " << command << "\n";


        //Our respond to the client:
        //Using sendto (UDP):
        std::string response = "Command received: " + command + "\n";
        sendto(udp_socket, response.c_str(), response.size(), 0,
            (sockaddr*)&client_addr, client_len);

    }

    //Close the socket:
    close(udp_socket);
    return 0;



    }








