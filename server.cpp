#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <fstream>
#include <string>

#pragma comment(lib, "ws2_32.lib")   // me u linku Winsock automatikisht

using namespace std;

// -------------------- PIKA 1: PORT & IP --------------------
const int PORT = 54000;          // porti i serverit
const int MAX_CLIENTS = 4;       // ----------------- PIKA 2: max lidhje
const char* SERVER_IP = "0.0.0.0";  // 0.0.0.0 = degjo ne te gjitha interfacet

// lista globale e klienteve
vector<SOCKET> clients;
mutex clientsMutex;
mutex logMutex;

// -------------------- PIKA 4: LOG I MESAZHEVE --------------------
void logMessage(const string& msg) {
    lock_guard<mutex> lock(logMutex);
    ofstream file("server_log.txt", ios::app);
    file << msg << endl;
}

// -------------------- PIKA 3 & 4: MENAXHIMI I KLIENTIT --------------------
void handleClient(SOCKET clientSocket, string clientIP) {
    char buffer[4096];

    while (true) {
        ZeroMemory(buffer, 4096);
        int bytesReceived = recv(clientSocket, buffer, 4096, 0);

        if (bytesReceived <= 0) {
            cout << "[DISCONNECT] " << clientIP << endl;
            logMessage("[DISCONNECT] " + clientIP);
            break;
        }

        string msg(buffer, bytesReceived);

        cout << "[FROM " << clientIP << "] " << msg << endl;
        logMessage("[FROM " + clientIP + "] " + msg);

        // ketu ma vone mundesh me shtua pergjigje ose komanda /list, /read, ...
    }

    closesocket(clientSocket);

    // hiqe nga lista globale e klienteve
    lock_guard<mutex> lock(clientsMutex);
    for (auto it = clients.begin(); it != clients.end(); ++it) {
        if (*it == clientSocket) {
            clients.erase(it);
            break;
        }
    }
}

// -------------------- MAIN: PIKAT 1, 2, 3 --------------------
int main() {
    // inicializo Winsock
    WSADATA wsaData;
    int wsOK = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsOK != 0) {
        cout << "Winsock init failed! Error: " << wsOK << endl;
        return 1;
    }

    // krijo socket-in e serverit
    SOCKET listening = socket(AF_INET, SOCK_STREAM, 0);
    if (listening == INVALID_SOCKET) {
        cout << "Failed to create socket!" << endl;
        WSACleanup();
        return 1;
    }

    // lidhe socket-in me IP & port (PIKA 1)
    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(PORT);
    hint.sin_addr.s_addr = INADDR_ANY;   // degjo ne te gjitha IP-t

    if (bind(listening, (sockaddr*)&hint, sizeof(hint)) == SOCKET_ERROR) {
        cout << "Bind failed!" << endl;
        closesocket(listening);
        WSACleanup();
        return 1;
    }

    // fillo me listen (PIKA 2)
    if (listen(listening, SOMAXCONN) == SOCKET_ERROR) {
        cout << "Listen failed!" << endl;
        closesocket(listening);
        WSACleanup();
        return 1;
    }

    cout << "Serveri po degjon ne portin " << PORT << " ..." << endl;

    // loop qe pranon kliente (PIKA 2 & 3)
    while (true) {
        sockaddr_in client;
        int clientSize = sizeof(client);

        SOCKET clientSocket = accept(listening, (sockaddr*)&client, &clientSize);
        if (clientSocket == INVALID_SOCKET) {
            cout << "Error accepting client!" << endl;
            continue;
        }

        // kontrollo max lidhjet (PIKA 2)
        {
            lock_guard<mutex> lock(clientsMutex);
            if ((int)clients.size() >= MAX_CLIENTS) {
                string msg = "Serveri eshte i mbushur. Provo perseri me vone.\n";
                send(clientSocket, msg.c_str(), (int)msg.size(), 0);
                closesocket(clientSocket);
                cout << "[REFUSED] klient i ri, max clients = " << MAX_CLIENTS << endl;
                logMessage("[REFUSED] klient - max clients reached");
                continue;
            }

            clients.push_back(clientSocket);
        }

        // merr IP e klientit
        string clientIP = inet_ntoa(client.sin_addr);

        cout << "[CONNECT] " << clientIP << endl;
        logMessage("[CONNECT] " + clientIP);

        // krijo nje thread per kete klient (PIKA 3 & 4)
        thread t(handleClient, clientSocket, clientIP);
        t.detach();
    }

    closesocket(listening);
    WSACleanup();
    return 0;
}
