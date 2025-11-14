#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <fstream>
#include <string>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

const int PORT = 54000;
const char* SERVER_IP = "0.0.0.0";
const int MAX_CLIENTS = 4;

vector<SOCKET> clients;
mutex clientsMutex;
mutex logMutex;

void logMessage(const string& msg) {
    lock_guard<mutex> lock(logMutex);
    ofstream file("server_log.txt", ios::app);
    file << msg << endl;
}

void handleClient(SOCKET clientSocket, string clientIP) {
    char buffer[4096];

    while (true) {
        ZeroMemory(buffer, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);

        if (bytesReceived <= 0) {
            cout << "[DISCONNECT] " << clientIP << endl;
            logMessage("[DISCONNECT] " + clientIP);
            break;
        }

        string msg(buffer, bytesReceived);
        cout << "[FROM " << clientIP << "] " << msg << endl;
        logMessage("[FROM " + clientIP + "] " + msg);
    }

    closesocket(clientSocket);

    {
        lock_guard<mutex> lock(clientsMutex);
        for (auto it = clients.begin(); it != clients.end(); ++it) {
            if (*it == clientSocket) {
                clients.erase(it);
                break;
            }
        }
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return 1;

    SOCKET listening = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listening == INVALID_SOCKET) {
        WSACleanup();
        return 1;
    }

    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(PORT);
    hint.sin_addr.s_addr = INADDR_ANY;

    if (bind(listening, (sockaddr*)&hint, sizeof(hint)) == SOCKET_ERROR) {
        closesocket(listening);
        WSACleanup();
        return 1;
    }

    if (listen(listening, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(listening);
        WSACleanup();
        return 1;
    }

    cout << "Serveri po degjon ne portin " << PORT << " ..." << endl;

    while (true) {
        sockaddr_in client;
        int clientSize = sizeof(client);

        SOCKET clientSocket = accept(listening, (sockaddr*)&client, &clientSize);
        if (clientSocket == INVALID_SOCKET) continue;

        string clientIP = inet_ntoa(client.sin_addr);

        {
            lock_guard<mutex> lock(clientsMutex);
            if ((int)clients.size() >= MAX_CLIENTS) {
                string msg = "Serveri eshte i mbushur. Provo perseri me vone.\n";
                send(clientSocket, msg.c_str(), (int)msg.size(), 0);
                closesocket(clientSocket);
                logMessage("[REFUSED] " + clientIP);
                continue;
            }

            clients.push_back(clientSocket);
        }

        cout << "[CONNECT] " << clientIP << endl;
        logMessage("[CONNECT] " + clientIP);

        thread t(handleClient, clientSocket, clientIP);
        t.detach();
    }

    closesocket(listening);
    WSACleanup();
    return 0;
}
