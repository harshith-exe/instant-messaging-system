
#include "client.h"

using namespace std;

class Client {
private:
    uint32 sock;

public:
    Client() {
            sock = socket(AF_INET, SOCK_STREAM, 0);
            sockaddr_in serverAddr;
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(12345);
            inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
            if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
                cerr << "Can't connect to the server\n";
                exit(-1);
        }
    }


    void sendCommand(const string& command, const string& username = "", const string& password = "", const string& fullname = "", const string& dob = "", const string& email = "") {
        ostringstream oss;
        oss << command;
        if (!username.empty()) {
            oss << " " << username;
        }
        if (!password.empty()) {
            oss << " " << password;
        }
        if (!fullname.empty()) {
            oss << " " << fullname;
        }
        if (!dob.empty()) {
            oss << " " << dob;
        }
        if (!email.empty()) {
            oss << " " << email;
        }
        string toSend = oss.str();
        send(sock, toSend.c_str(), toSend.length(), 0);
    }

    void receiveMessages() {
        char buffer[1024];
        while (true) {
            memset(buffer, 0, 1024);
            uint32 bytesReceived = recv(sock, buffer, 1024, 0);
            if (bytesReceived <= 0) {
                cout << "Disconnected from server\n";
                break;
            }
            string receivedMsg(buffer);
            cout << endl << receivedMsg << endl;
        }
    }

    ~Client() {
        close(sock);
    }
};


int main() {
    cout << "Connected to the chat server!\n\n";
    cout << "Welcome! Please register or login to continue.\n\n";

    Client client;
    thread(&Client::receiveMessages, &client).detach();

    string command;
    cout << "Enter command (register, login, send, addfriend, listfriends, viewprofile, viewhistory, exit): ";
    while (true) {
        getline(cin, command);
        if (command == "exit") {
            break;
        } 
        else if (command == "register") {
            string username, password, fullname, dob, email;
            cout << "Username: ";
            getline(cin, username);
            cout << "Password: ";
            getline(cin, password);
            cout << "Full Name: ";
            getline(cin, fullname);
            cout << "Date of Birth (DOB): ";
            getline(cin, dob);
            cout << "email: ";
            getline(cin, email);
            client.sendCommand(command, username, password, fullname, dob, email);
        } 
        else if (command == "login") {
            string username, password;
            cout << "Username: ";
            getline(cin, username);
            cout << "Password: ";
            getline(cin, password);
            client.sendCommand(command, username, password);
        } 
        else if (command == "send") {
            string recipient, message;
            cout << "Recipient: ";
            getline(cin, recipient);
            cout << "Message: ";
            getline(cin, message);
            cout << "Sending message to " << recipient << ": " << message << endl; 
            client.sendCommand(command, "", "", recipient, message);
        } 
        else if (command == "addfriend") {
            string friendName;
            cout << "Friend Username: ";
            getline(cin, friendName);
            client.sendCommand(command, friendName);
        } 
        else if (command == "yes" || command == "no") {
            client.sendCommand(command);
            continue; 
        } 
        else if (command == "listfriends" ) {
            client.sendCommand(command);
        } 
        else if (command == "viewprofile") {
            string friendName;
            cout << "Enter friend's username: ";
            getline(cin, friendName);
            client.sendCommand(command, friendName);
        } 
        else if (command == "viewhistory") {
            string friendName;
            cout << "Enter friend's username to view message history: ";
            getline(cin, friendName);
            client.sendCommand(command, friendName);
        } 
        else {
            cout << "Invalid command. Please try again.\n";
        }
    }

    return 0;
}
