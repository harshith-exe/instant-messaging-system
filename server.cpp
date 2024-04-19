
#include "server.h"

using namespace std;
mutex clientMutex;
map<uint32, string> clientSockets;

class Encryption {
public:
    const uint8 XOR_KEY = 'S'; // You can change this key

    string xorEncryptDecrypt(const string& data) {
        string encrypted;
    	for (uint8 c : data) {
       	    encrypted += c ^ XOR_KEY;
    	}
    	return encrypted;
	}

    string xorDecrypt(const string& data) {
    	return xorEncryptDecrypt(data); 
	}


    static string encryptDecrypt(const string& input) {
        uint8 key = 'K';
        string output = input;
        for (uint32 i = 0; i < input.size(); i++) {
            output[i] = input[i] ^ key;
        }
        return output;
    }
};


class UserManagement : public Encryption {
    public:
    map<string, string> userPasswords; 
    map<string, set<string>> userFriends;

    void loadUsers() {
        ifstream file("users.txt");
        string username, encryptedPassword;
        while (file >> username >> encryptedPassword) {
            string password = xorDecrypt(encryptedPassword);
            userPasswords[username] = password;
        }
    }

    void saveUsers() {
        ofstream file("users.txt");
        for (const auto& pair : userPasswords) {
            string encryptedPassword = xorEncryptDecrypt(pair.second);
            file << pair.first << " " << encryptedPassword << endl;
        }
    }

    void loadFriends() {
        ifstream friendsFile("friends.txt");
        string line;
        while (getline(friendsFile, line)) {
            istringstream iss(line);
            string username, friendName;
            iss >> username;
            while (iss >> friendName) {
                userFriends[username].insert(friendName);
            }
        }
        friendsFile.close();
    }

    void saveFriends() {
        ofstream friendsFile("friends.txt");
        for (const auto& pair : userFriends) {
            friendsFile << pair.first;
            for (const auto& friendName : pair.second) {
                friendsFile << " " << friendName;
            }
            friendsFile << "\n";
        }
        friendsFile.close();
    }

    void registerUserHandler(uint32 clientSocket, istringstream& iss) {
        string line;
        getline(iss, line); 
        istringstream lineStream(line);
        string username, password, fullname, dob, email;
        lineStream >> username >> password >> fullname >> dob >> email;
        if (userPasswords.find(username) == userPasswords.end()) {
            userPasswords[username] = Encryption::encryptDecrypt(password); /* Encrypt the password */
            ofstream userInfoFile("user_info.txt", ios::app);
            userInfoFile << username << " " << fullname << " " << dob << " " << email << endl;
            userInfoFile.close();
            saveUsers();
            send(clientSocket, "Registered\n", 11, 0);
        } 
        else {
            send(clientSocket, "Username already exists\n", 25, 0);
        }
    }

    void loginUserHandler(uint32 clientSocket, istringstream& iss) {
        string username, password;
        iss >> username >> password;
        if (userPasswords.find(username) != userPasswords.end() && userPasswords[username] == Encryption::encryptDecrypt(password)) {
            clientMutex.lock();
            clientSockets[clientSocket] = username;
            clientMutex.unlock();
            send(clientSocket, "Logged in\n", 10, 0);
        } 
        else {
            send(clientSocket, "Login failed\n", 13, 0);
        }
    }
};

class NetworkHandler : public UserManagement {
private:
    map<string, set<string>> userFriends;
    map<uint32, tuple<uint32, string>> pendingFriendRequests;

public:

    void handleAddFriendAndResponse(uint32 clientSocket, istringstream& iss, const string& cmd) {
        if (cmd == "addfriend") {
            string friendUsername;
    	    iss >> friendUsername;
      	    clientMutex.lock();
            bool friendFound = false;
            uint32 friendSocket = -1;
            for (const auto& pair : clientSockets) {
                if (pair.second == friendUsername) {
                    friendSocket = pair.first;
                    friendFound = true;
                    break;
                }
            }
            clientMutex.unlock();
            if (friendFound && friendSocket != -1) {
                string requestMessage = "Friend request from " + clientSockets[clientSocket] + ". Do you accept? (yes/no)";
                pendingFriendRequests[friendSocket] = std::make_tuple(clientSocket, clientSockets[clientSocket]);
                send(friendSocket, requestMessage.c_str(), requestMessage.length(), 0);
                return;
            }		
        } 
        else if (cmd == "yes" || cmd == "no") {
            auto it = pendingFriendRequests.find(clientSocket);
            if (it != pendingFriendRequests.end()) {
                auto [requesterSocket, requesterUsername] = it->second;
                if (cmd == "yes") {
                    userFriends[clientSockets[clientSocket]].insert(requesterUsername);
                    userFriends[requesterUsername].insert(clientSockets[clientSocket]);
                    string successMsg = requesterUsername + " and " + clientSockets[clientSocket] + " are now friends.";
                    send(clientSocket, successMsg.c_str(), successMsg.length(), 0);
                    send(requesterSocket, successMsg.c_str(), successMsg.length(), 0);
                    saveFriends();
                    pendingFriendRequests.erase(it);
                } 
                else {
                    string msg = "Friend request declined.";
                    send(requesterSocket, msg.c_str(), msg.length(), 0);
                    pendingFriendRequests.erase(it);
                }
            }
        }     
    }


    void sendMessage(uint32 clientSocket, istringstream& iss) {
        string recipientUsername, message;
        iss >> recipientUsername;
        getline(iss, message);
        /* Debugging statement */
        cout << "Received message from " << clientSockets[clientSocket] << " to " << recipientUsername << ": " << message << endl; 
        clientMutex.lock();
        string messageFileName = "messages/" + clientSockets[clientSocket] + "_" + recipientUsername + ".txt";
        ofstream messageFile(messageFileName, ios::app);
        if (messageFile.is_open()) {
            messageFile << clientSockets[clientSocket] << ": " << message << endl;
            messageFile.close();
        } 
        else {
            cerr << "Error opening message file for " << clientSockets[clientSocket] << " and " << recipientUsername << endl;
        }
        bool recipientFound = false;
        for (const auto& pair : clientSockets) {
            if (pair.second == recipientUsername) {
                string fullMessage = clientSockets[clientSocket] + ": " + message;
                send(pair.first, fullMessage.c_str(), fullMessage.length(), 0);
                recipientFound = true;
                break;
            }
        }
        if (!recipientFound) {
            string errorMessage = "Recipient " + recipientUsername + " does not exist or is not online.\n";
            send(clientSocket, errorMessage.c_str(), errorMessage.length(), 0);
        }
        clientMutex.unlock();
    }

    void listFriends(uint32 clientSocket) {
        clientMutex.lock();
        string friendsList = "Your friends:";
        for (const auto& friendName : userFriends[clientSockets[clientSocket]]) {
            friendsList += " " + friendName;
        }
        friendsList += "\n";
        send(clientSocket, friendsList.c_str(), friendsList.length(), 0);
        clientMutex.unlock();
    }

    void viewMessageHistory(uint32 clientSocket, istringstream& iss) {
        string friendName;
        iss >> friendName;
        clientMutex.lock();
        if (userFriends[clientSockets[clientSocket]].count(friendName) > 0) {
            string messageFileName1 = "messages/" + clientSockets[clientSocket] + "_" + friendName+".txt";
            string messageFileName2 = "messages/" + friendName +"_"+ clientSockets[clientSocket]+".txt";
            ifstream messageFile1(messageFileName1);
            ifstream messageFile2(messageFileName2);
            string messageHistory;
            if (messageFile1.is_open() && messageFile2.is_open()) {
                string line;
                messageHistory += "Your messages to " + friendName + ":\n";
                while (getline(messageFile1, line)) {
                    messageHistory += line + "\n";
                }
                messageHistory += "\n" + friendName + "'s messages to you:\n";
                while (getline(messageFile2, line)) {
                    messageHistory += line + "\n";
                }
                send(clientSocket, messageHistory.c_str(), messageHistory.length(), 0);
                messageFile1.close();
                messageFile2.close();
            } 
            else {
                string errorMessage = "No message history with " + friendName + "\n";
                send(clientSocket, errorMessage.c_str(), errorMessage.length(), 0);
            }
        } 
        else {
            string errorMessage = "You are not friends with " + friendName + "\n";
            send(clientSocket, errorMessage.c_str(), errorMessage.length(), 0);
        }
        clientMutex.unlock();
    }

    void viewProfileHandler(uint32 clientSocket, const string& friendName) {
        ifstream userInfoFile("user_info.txt");
        if (!userInfoFile) {
            cerr << "Error opening user_info.txt\n";
            return;
        }
        string username, fullname, dob, email;
        bool profileFound = false;
        while (userInfoFile >> username >> fullname >> dob >> email) {
            cout << "Reading user: " << username << endl; 
            if (username == friendName) {
                string profileInfo = "Full Name: " + fullname + "\n";
                profileInfo += "Date of Birth (DOB): " + dob + "\n";
                profileInfo += "email: " + email + "\n";
                send(clientSocket, profileInfo.c_str(), profileInfo.length(), 0);
                profileFound = true;
                break;
            }
        }
        userInfoFile.close();
        if (!profileFound) {
            string errorMessage = "Profile for " + friendName + " not found\n";
            send(clientSocket, errorMessage.c_str(), errorMessage.length(), 0);
        }
    }

    void handleClient(uint32 clientSocket) {
        char buffer[1024];
        string username;
        while (true) {
            memset(buffer, 0, 1024);
            uint32 bytesReceived = recv(clientSocket, buffer, 1024, 0);
            if (bytesReceived <= 0) {
                cout << "Client disconnected or error\n";
                break;
            }
            string command(buffer);
            istringstream iss(command);
            string cmd;
            iss >> cmd;
            if (cmd == "register") {
                registerUserHandler(clientSocket, iss);
            } 
            else if (cmd == "login") {
                loginUserHandler(clientSocket, iss);
            } 
            else if (cmd == "send") {
                sendMessage(clientSocket, iss);
            } 
            else if (cmd == "addfriend" || cmd == "yes" || cmd == "no") {
                handleAddFriendAndResponse(clientSocket, iss, cmd);
            } 
            else if (cmd == "listfriends") {
                listFriends(clientSocket);
            } 
            else if (cmd == "viewprofile") {
                string friendName;
                iss >> friendName;
                viewProfileHandler(clientSocket, friendName);
            } 
            else if (cmd == "viewhistory") {
                viewMessageHistory(clientSocket, iss);
            }
        }
        close(clientSocket);
        clientMutex.lock();
        clientSockets.erase(clientSocket);
        clientMutex.unlock();
    }
};

int main() {
    UserManagement userManager;
    NetworkHandler networkHandler;

    userManager.loadUsers();
    networkHandler.loadFriends();

    uint32 serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(12345);
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(serverSocket, 5);

    while (true) {
        sockaddr_in clientAddr;
        socklen_t clientAddrSize = sizeof(clientAddr);
        uint32 clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrSize);
        thread(&NetworkHandler::handleClient, &networkHandler, clientSocket).detach();
    }

    return 0;
}