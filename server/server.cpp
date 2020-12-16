#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#include <fstream>
#include "unistd.h"
#include <ctime>
#include <mutex>

#define MAXLINE 4096

using namespace std;


void show_capability_list(void);
void show_file_list(void);

struct capability { // user(domain)'s capability
    string object;
    bool r, w;
};

struct file_detail { // file detail
    string permission;
    string owner;
    string group;
    int bytes;
    string date;
    string name;
    mutex locker;
    int writing = 0;
    int reading = 0;

    file_detail &operator=(const file_detail &o) { // for fear that mutex can't be assigned
        permission = o.permission;
        owner = o.owner;
        group = o.group;
        bytes = o.bytes;
        date = o.date;
        name = o.name;
        writing = 0;
        reading = 0;
        return *this;
    }
};

mutex R_LOCKER;

unordered_map<string, pair<string, vector<struct capability>> > CAPABILITY_LIST; // capability list
unordered_map<string, struct file_detail> FILE_LIST; // store file detail

const int MEMBER_SIZE = 6;
string MEMBER[MEMBER_SIZE] = {"Ian", "Mary", "Ken", "Bob", "Joy", "John"};
void init() { // for initializing 6 members
    CAPABILITY_LIST["Ian"];
    CAPABILITY_LIST["Ian"].first = "AOS";
    CAPABILITY_LIST["Ian"].second = vector<struct capability>();
    CAPABILITY_LIST["Mary"];
    CAPABILITY_LIST["Mary"].first = "AOS";
    CAPABILITY_LIST["Mary"].second = vector<struct capability>();
    CAPABILITY_LIST["Ken"];
    CAPABILITY_LIST["Ken"].first = "AOS";
    CAPABILITY_LIST["Ken"].second = vector<struct capability>();
    CAPABILITY_LIST["Bob"];
    CAPABILITY_LIST["Bob"].first = "CSE";
    CAPABILITY_LIST["Bob"].second = vector<struct capability>();
    CAPABILITY_LIST["Joy"];
    CAPABILITY_LIST["Joy"].first = "CSE";
    CAPABILITY_LIST["Joy"].second = vector<struct capability>();
    CAPABILITY_LIST["John"];
    CAPABILITY_LIST["John"].first = "CSE";
    CAPABILITY_LIST["John"].second = vector<struct capability>();

}

string create_file(string file_name, string user, string permission) { // for creating file
    fstream file;
    string path = "../file/" + file_name;

    struct stat buf;

    if(stat(path.c_str(), &buf) != -1) return "file already existed"; // determine if file already existed

    file.open(path, ios::out);
    file.seekg(0, ios::end);
    int file_size = file.tellg(); // find file size
    file.close();

    struct file_detail t; // temp file_detail structure
    t.permission = permission;
    t.owner = user;
    t.group = CAPABILITY_LIST[user].first;
    t.bytes = file_size;

    // the date of file to be created
    time_t now = time(0);
    char *dt = ctime(&now);
    t.date = dt;
    stringstream ss(t.date);
    string tmp;
    vector<string> da;
    while(ss >> tmp) { // split date with space
        da.push_back(tmp);
    }
    t.date = da[1] + " " + da[2] + " " + da[4];

    t.name = file_name;
    FILE_LIST[file_name] = t; // add a file into FILE_LIST
    cout << "Create file: " << FILE_LIST[file_name].permission << " " << FILE_LIST[file_name].owner << " " << FILE_LIST[file_name].group << " " << FILE_LIST[file_name].bytes << " " << FILE_LIST[file_name].date << " " << FILE_LIST[file_name].name << endl;

    // update CAPABILITY_LIST
    for(int i=0; i<MEMBER_SIZE; i++) { // for every member
        struct capability tmp;
        tmp.object = file_name;
        if(MEMBER[i] == user) { // if owner;
            tmp.r = t.permission[0] == 'r' ? true : false;
            tmp.w = t.permission[1] == 'w' ? true : false;
        } else if(CAPABILITY_LIST[MEMBER[i]].first == CAPABILITY_LIST[user].first) { // if same group
            tmp.r = t.permission[2] == 'r' ? true : false;
            tmp.w = t.permission[3] == 'w' ? true : false;
        } else { // if other
            tmp.r = t.permission[4] == 'r' ? true : false;
            tmp.w = t.permission[5] == 'w' ? true : false;
        }
        CAPABILITY_LIST[MEMBER[i]].second.push_back(tmp);

    }

    return "create file successfully";
}

string transfer_file(int connfd, string file_name, string user) { // for transferring file to user

    string path = "../file/" + file_name;

    struct stat buf;

    if(stat(path.c_str(), &buf) == -1) return "file doesn't exist"; // determine if file already existed

    int len = CAPABILITY_LIST[user].second.size();
    for(int i=0; i<len; i++) { //search capability
        if(CAPABILITY_LIST[user].second[i].object == file_name) { // find object
            if(CAPABILITY_LIST[user].second[i].r) {
                break;
            } else {
                return "Permission denial";
            }
        }
    }


    if(FILE_LIST[file_name].reading == 0) { // no one is reading
        FILE_LIST[file_name].locker.lock();
    }


    R_LOCKER.lock(); // mutex for reading
    FILE_LIST[file_name].reading += 1; // update the number of members reading the file
    R_LOCKER.unlock();
    sleep(5); // for test synchronization

    // critical section

    fstream file;
    file.open(path, ios::in | ios::binary); // open file with read and binary mode

    std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    send(connfd, contents.c_str(), sizeof(contents), 0);
    file.close();
    cout << "transfer" << " " << file_name << " to " << user << endl;

    R_LOCKER.lock(); // mutex for reading
    FILE_LIST[file_name].reading -= 1; // update the number of members reading the file
    R_LOCKER.unlock();

    if(FILE_LIST[file_name].reading == 0) {
        FILE_LIST[file_name].locker.unlock(); // unlock this file
    }

    return "";
}

string upload_file(string file_name, string user) { //determine file existence and permission

    string path = "../file/" + file_name;

    struct stat buf;

    if(stat(path.c_str(), &buf) == -1) return "file doesn't exist"; // determine if file already existed

    int len = CAPABILITY_LIST[user].second.size();
    for(int i=0; i<len; i++) { //search capability
        if(CAPABILITY_LIST[user].second[i].object == file_name) { // find object
            if(CAPABILITY_LIST[user].second[i].w) { // if user has writing permission for this file
                break;
            } else {
                return "Permission denial";
            }
        }
    }

    return "upload successfully";

}

void write_file(string file_name, string user, string mode, string content) {

    FILE_LIST[file_name].locker.lock(); // lock the file
    cout << user << ": write lock finished" << endl;
    sleep(5); // for synchronization

    // critical section

    string path = "../file/" + file_name;
    if(mode == "o") { // overwrite mode
        fstream file;
        file.open(path, ios::out | ios::trunc | ios::binary); // open file with write and binary mode
        file << content; // write content into file with overwriting mode
        file.close();

        file.open(path, ios::in);
        file.seekg(0, ios::end);
        int file_size = file.tellg(); // find file size
        file.close();

        FILE_LIST[file_name].bytes = file_size; // update file size in FILE_LIST
        cout << "overwrite " << file_name << " successfully" << endl;

    }else if(mode == "a") { // append mode
        fstream file;
        file.open(path, ios::out | ios::app | ios::binary); // open file with write and binary mode
        file << content; // write content into file with appending mode
        file.close();

        file.open(path, ios::in);
        file.seekg(0, ios::end);
        int file_size = file.tellg(); // find file size
        file.close();

        FILE_LIST[file_name].bytes = file_size; // update file size in FILE_LIST
        cout << "append " << file_name << " successfully" << endl;
    }

    FILE_LIST[file_name].locker.unlock(); // unlock this file

    return;
}

string changemode(string file_name, string user, string permission) { // for changing mode of file
    string path = "../file/" + file_name;

    struct stat buf;

    if(stat(path.c_str(), &buf) == -1) return "file doesn't exist"; // determine if file already existed

    if(FILE_LIST[file_name].owner != user) return "Permission denial"; // determine whether is the owner of file

    FILE_LIST[file_name].locker.lock(); // lock this file
    cout << user << ": changemode lock finished" << endl;
    sleep(5); // for synchronization

    // critical section

    FILE_LIST[file_name].permission = permission;

    for(int i=0; i<MEMBER_SIZE; i++) { // update CAPABILITY_LIST
        int len = CAPABILITY_LIST[MEMBER[i]].second.size();
        for(int j=0; j<len; j++) { // member's capabilities
            if(CAPABILITY_LIST[MEMBER[i]].second[j].object != file_name) {
                continue;
            }else {
                if(MEMBER[i] == user) { // if owner;
                    CAPABILITY_LIST[MEMBER[i]].second[j].r = permission[0] == 'r' ? true : false;
                    CAPABILITY_LIST[MEMBER[i]].second[j].w = permission[1] == 'w' ? true : false;
                } else if(CAPABILITY_LIST[MEMBER[i]].first == CAPABILITY_LIST[user].first) { // if same group
                    CAPABILITY_LIST[MEMBER[i]].second[j].r = permission[2] == 'r' ? true : false;
                    CAPABILITY_LIST[MEMBER[i]].second[j].w = permission[3] == 'w' ? true : false;
                } else { // if other
                    CAPABILITY_LIST[MEMBER[i]].second[j].r = permission[4] == 'r' ? true : false;
                    CAPABILITY_LIST[MEMBER[i]].second[j].w = permission[5] == 'w' ? true : false;
                }
            }
        }
    }

    FILE_LIST[file_name].locker.unlock(); // unlock this file

    return "change mode successfully";
}

void show_capability_list(void) { // show CAPABILITY_LIST detail
    for(int i=0; i<MEMBER_SIZE; i++) {
        cout << MEMBER[i] <<": ";
        int len = CAPABILITY_LIST[MEMBER[i]].second.size();
        for(int j=0; j<len; j++) {
            cout << CAPABILITY_LIST[MEMBER[i]].second[j].object << " " << CAPABILITY_LIST[MEMBER[i]].second[j].r << " " <<CAPABILITY_LIST[MEMBER[i]].second[j].w << " --- ";
        }
        cout << endl;
    }
}

void show_file_list(void) { // show FILE_LIST detail
    for(auto it = FILE_LIST.begin(); it != FILE_LIST.end(); it++) {
        cout << it->second.permission << " " << it->second.owner << " " << it->second.group << " " << it->second.bytes << " " << it->second.date << " " << it->second.name << endl;
    }
}

void thread_task(int connfd) { // the task which every thread should execute/
    int n, cnt=0;
    string user; // user name
    char buff[MAXLINE];
    while(1) { // receive user message and return response using loop.
        n = recv(connfd, buff, MAXLINE, 0);
        buff[n] = '\0';
        if(cnt++ == 0) user = buff;
        cout << user << " " <<": receive message from client: " << buff << endl;

        //divide user's command into vector
        stringstream ss(buff);
        string tmp;
        vector<string> command;
        while(ss >> tmp) {
            command.push_back(tmp);
        }

        //determine what user's command is
        if(command[0] == "create") { // create file
            string mes = create_file(command[1], user, command[2]);
            send(connfd, mes.c_str(), sizeof(mes), 0);
            continue;
        }else if(command[0] == "read") { // transfer file
            string mes =  transfer_file(connfd ,command[1], user);
            send(connfd, mes.c_str(), sizeof(mes), 0);
            continue;
        }else if(command[0] == "write") { // upload file
            string mes =  upload_file(command[1], user);
            send(connfd, mes.c_str(), sizeof(mes), 0);
            n = recv(connfd, buff, MAXLINE, 0);
            buff[n] = '\0';

            string content = buff;

            write_file(command[1], user, command[2], content);

            continue;
        }else if(command[0] == "changemode") { // change file mode
            string mes = changemode(command[1], user, command[2]);
            send(connfd, mes.c_str(), sizeof(mes), 0);
            continue;
        }else if(command[0] == "exit") { // exit connection
            send(connfd, "close connection", sizeof("close connection"), 0);
            break;
        }else if(command[0] == "show" && command[1] == "-f") { // show file detail
            show_file_list();
        }else if(command[0] == "show" && command[1] == "-c") { // show capability detail
            show_capability_list();
        }

        send(connfd, "ack!!", sizeof("ack!!"), 0);
    }

    cout << "close: " << user <<endl;
    close(connfd);
    return;
}



int main()
{
    int listenfd, connfd;
    struct sockaddr_in servaddr, clientaddr;
    char buff[MAXLINE];
    int n;

    init(); // initializing CAPABILITY_LIST

    if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1 ) { //create a socket for listening
        cout << "create socket error" << endl;
        return 0;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); // receive messages which comes from different interface
    servaddr.sin_port = htons(8888); // server port number

    if(bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1) { // bind socket with server socket pair
        cout << "bind socket error" << endl;
        return 0;
    }

    if(listen(listenfd, 10) == -1){ // listen for socket connection
        cout << "listen socket error" << endl;
        return 0;
    }

    cout << "======waiting for client‘s request======" << endl;

    thread t[10]; // the maximum connection number of user
    int cnt = 0;
    while(1) { // wait for user connection
        unsigned int clientaddrlen = sizeof(clientaddr);
        int connfd;
        if((connfd = accept(listenfd, (struct sockaddr*) &clientaddr, &clientaddrlen)) != -1) {
            cout << "thread enter!!" <<endl;
            t[cnt] = thread(thread_task, connfd);
            t[cnt++].detach(); // execute thread task without blocking main thread
        }
    }

    close(listenfd);


    cout <<"Server Hello world!" << endl;
    return 0;
}
