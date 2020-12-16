#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <arpa/inet.h>
#include <sstream>
#include <fstream>
#include <vector>
#include <sys/stat.h>

#define MAXLINE 4096 // maximum buffer size

using namespace std;

int main()
{
    int   sockfd, n;
    char  recvline[MAXLINE], sendline[MAXLINE];
    struct sockaddr_in  servaddr;

    if((sockfd =socket(AF_INET, SOCK_STREAM, 0)) == -1) { // create socket
        cout << "create socket error" << endl;
    }

    memset(&servaddr, 0, sizeof(servaddr)); // clean servaddr

    servaddr.sin_family = AF_INET; // for IPv4
    servaddr.sin_port = htons(8888); // destination port
    servaddr.sin_addr.s_addr = inet_addr("10.0.2.15"); // destination ipv4


    if(connect(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr)) == -1){ // connect to server
        cout << "Connection error" << endl;
    }

    cout << "Please enter user name: ";

    while(1) { // loop for conversation
        //Send a message to server
        char message[100] = {};
        char receiveMessage[100] = {};
        cin.getline(message, 100); // get input (including space)

        //divide user's command into vector
        stringstream ss(message);
        string tmp;
        vector<string> command;
        while(ss >> tmp) { // split string with space
            command.push_back(tmp);
        }
        if(command[0] == "read") { // request download file
            send(sockfd, message, sizeof(message), 0); // send message to server
            recv(sockfd, receiveMessage, sizeof(receiveMessage),0); // receive message from server
            if(strcmp(receiveMessage, "Permission denial") == 0) { // permisssion denial
                cout << receiveMessage << endl;
                continue;
            }else if(strcmp(receiveMessage, "file doesn't exist") == 0) { // file doesn't exitst
                cout << receiveMessage << endl;
                continue;
            }

            fstream file;
            file.open(command[1], ios::out | ios::trunc | ios::binary); // open file(overwrite if file exists)
            file << receiveMessage; // input received message into file
            file.close();
            cout << "download successfully" << endl;
            continue;

        }else if(command[0] == "write") {
            struct stat buf;
            if(stat(command[1].c_str(), &buf) == -1) { // determine if file already existed
                cout << "file doesn't existed" << endl;
                continue;
            }

            send(sockfd, message, sizeof(message), 0); // send message to server
            recv(sockfd, receiveMessage, sizeof(receiveMessage),0); // receive message from server
            if(receiveMessage != "") cout << receiveMessage << endl;

            fstream file;
            file.open(command[1], ios::in | ios::binary); // open file with readable and binary mode

            std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            send(sockfd, contents.c_str(), sizeof(contents), 0); // send file content to server
            file.close();
            continue;
        }

        send(sockfd, message, sizeof(message), 0); // send message to server
        recv(sockfd, receiveMessage, sizeof(receiveMessage),0); // receive message from server
        cout << receiveMessage << endl;
        if(strcmp(message, "exit") == 0) break; // if user input exit, then close connection
    }


    close(sockfd);
    return 0;
}
