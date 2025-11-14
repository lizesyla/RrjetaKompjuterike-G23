#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <string>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// PIKA 1: PORT & IP
const int PORT = 54000;
const char* SERVER_IP = "0.0.0.0";

// PIKA 2: LIMIT I LIDHJEVE
const int MAX_CLIENTS = 4;

vector<SOCKET> clients;
mutex clientsMutex;

// PIKA 3: MENAXHIMI I KLIENTIT (vetëm console)
void handleClient(SOCKET clientSocket, string clientIP) {
    char buffer[4096];

    while (true) {
        ZeroMemory(buffer, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);

        if (bytesReceived <= 0) {
            cout << "[DISCONNECT] " << clientIP << endl;
            break;
        }

        string msg(buffer, bytesReceived);
        cout << "[FROM " << clientIP << "] " << msg << endl;
    }

    closesocket(clientSocket);

    // hiqe nga lista e klienteve
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
    int wsOK = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsOK != 0) {
        cout << "Winsock init failed! Error: " << wsOK << endl;
        return 1;
    }

    SOCKET listening = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listening == INVALID_SOCKET) {
        cout << "Failed to create socket! Error: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(PORT);
    hint.sin_addr.s_addr = INADDR_ANY;

    if (bind(listening, (sockaddr*)&hint, sizeof(hint)) == SOCKET_ERROR) {
        cout << "Bind failed! Error: " << WSAGetLastError() << endl;
        closesocket(listening);
        WSACleanup();
        return 1;
    }

    if (listen(listening, SOMAXCONN) == SOCKET_ERROR) {
        cout << "Listen failed! Error: " << WSAGetLastError() << endl;
        closesocket(listening);
        WSACleanup();
        return 1;
    }

    cout << "Serveri po degjon ne portin " << PORT << " ..." << endl;

    while (true) {
        sockaddr_in client;
        int clientSize = sizeof(client);

        SOCKET clientSocket = accept(listening, (sockaddr*)&client, &clientSize);
        if (clientSocket == INVALID_SOCKET) {
            cout << "Error accepting client! Error: " << WSAGetLastError() << endl;
            continue;
        }

        string clientIP = inet_ntoa(client.sin_addr);

        {
            lock_guard<mutex> lock(clientsMutex);
            if ((int)clients.size() >= MAX_CLIENTS) {
                string msg = "Serveri eshte i mbushur. Provo perseri me vone.\n";
                send(clientSocket, msg.c_str(), (int)msg.size(), 0);
                closesocket(clientSocket);
                cout << "[REFUSED] klient i ri nga " << clientIP
                     << " (max clients = " << MAX_CLIENTS << ")" << endl;
                continue;
            }

            clients.push_back(clientSocket);
        }

        cout << "[CONNECT] " << clientIP << endl;

        // PIKA 3: krijo thread per kete klient
        thread t(handleClient, clientSocket, clientIP);
        t.detach();
    }

    closesocket(listening);
    WSACleanup();
    return 0;
}
