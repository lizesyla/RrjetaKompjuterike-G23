#include <iostream>
#include <winsock2.h>
#include <map>
#pragma comment(lib, "ws2_32.lib") 

using namespace std;
// Funksion për të dërguar komandën dhe pritur përgjigjen nga serveri
void sendCommand(SOCKET clientSocket, const string& message, bool isAdmin) {
    char buffer[4096];

        // Simulimi i prioritetit: admin merr përgjigje më shpejt
    if (!isAdmin) Sleep(100); // 100 milisekonda

    if (send(clientSocket, message.c_str(), message.length(), 0) == SOCKET_ERROR) {
        cerr << "Gabim ne dergimin e komandes.\n";
        return;
    }

    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived <= 0) {
        cout << "Serveri u shkëput.\n";
        return;
    }

    buffer[bytesReceived] = '\0';
    cout << "Pergjigja nga serveri: " << buffer << endl;
}


int main() {
    WSADATA wsaData;
    SOCKET clientSocket;
    struct sockaddr_in serverAddr;
    char buffer[1024];
    string message;


    cout << "Inicializimi i Winsock..." << endl;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "Gabim ne inicializimin e Winsock!" << endl;
        return 1;
    }

 
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        cerr << "Gabim ne krijimin e socketit!" << endl;
        WSACleanup();
        return 1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    serverAddr.sin_port = htons(1200); 


    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "Lidhja me serverin deshtoi!" << endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }
    //test

    cout << "Jeni lidhur me serverin!\n";
   string roli;
    bool validRole = false;
    bool isAdmin = false;

    while (!validRole) {
        cout << "Zgjedh rolin tend (admin ose user): ";
        getline(cin, roli);

        if (roli == "admin") {
            isAdmin = true;
            cout << "✅ Jeni kycur si ADMIN - keni qasje te plote\n";
            validRole = true;
        } else if (roli == "user") {
            isAdmin = false;
            cout << "ℹ️  Jeni kycur si USER - vetem read lejohet\n";
            validRole = true;
        } else {
            cout << "⚠️ Roli nuk ekziston. Shkruani 'admin' ose 'user'.\n";
        }
    }
    string privInfo = "ROLE:" + roli;
    send(clientSocket, privInfo.c_str(), privInfo.length(), 0);

    map<string, bool> allowedCommands;

    if (isAdmin) {
        allowedCommands = {
            {"/list", true}, {"/read", true}, {"/upload", true},
            {"/download", true}, {"/delete", true}, {"/search", true},
            {"/info", true}
        };
    } else {
        allowedCommands = {
            {"/read", true}
        };
    }


    while (true) {

        cout << "Shkruani kerkesen tuaj: ";
        getline(cin, message);

       
        if (message.empty()) {
            cout << "⚠️ Komanda nuk mund të jetë bosh. Provoni përsëri.\n";
            continue;
        }

        if (message == "/exit") {
            cout << "Duke u shkëputur nga serveri...\n";
            break;
        }

        bool commandValid = false;
        for (auto const& cmd : allowedCommands) {
            if (message.find(cmd.first) == 0) {
                commandValid = true;
                break;
            }
        }

        if (!commandValid) {
            cout << "🚫 Kjo komandë nuk lejohet për rolin tuaj ose nuk ekziston.\n";
            continue;
        }

     
       sendCommand(clientSocket, message, isAdmin);
    }

    closesocket(clientSocket);
    WSACleanup();
    cout << "Lidhja u mbyll me sukses.\n";

    return 0;
}