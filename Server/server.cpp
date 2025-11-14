#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// PIKA 1: PORT & IP
const int PORT = 54000;
const char* SERVER_IP = "0.0.0.0";

// PIKA 2: LIMIT I LIDHJEVE
const int MAX_CLIENTS = 4;

int main() {
    // Inicializo Winsock
    WSADATA wsaData;
    int wsOK = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsOK != 0) {
        cout << "Winsock init failed! Error: " << wsOK << endl;
        return 1;
    }

    // Krijo socket-in e serverit (TCP)
    SOCKET listening = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listening == INVALID_SOCKET) {
        cout << "Failed to create socket! Error: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    // Lidhja me IP & PORT
    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(PORT);
    hint.sin_addr.s_addr = INADDR_ANY;   // SERVER_IP = 0.0.0.0

    if (bind(listening, (sockaddr*)&hint, sizeof(hint)) == SOCKET_ERROR) {
        cout << "Bind failed! Error: " << WSAGetLastError() << endl;
        closesocket(listening);
        WSACleanup();
        return 1;
    }

    // Filloi listen
    if (listen(listening, SOMAXCONN) == SOCKET_ERROR) {
        cout << "Listen failed! Error: " << WSAGetLastError() << endl;
        closesocket(listening);
        WSACleanup();
        return 1;
    }

    cout << "Serveri po degjon ne portin " << PORT << " ..." << endl;

    int activeClients = 0;

    while (true) {
        sockaddr_in client;
        int clientSize = sizeof(client);

        SOCKET clientSocket = accept(listening, (sockaddr*)&client, &clientSize);
        if (clientSocket == INVALID_SOCKET) {
            cout << "Error accepting client! Error: " << WSAGetLastError() << endl;
            continue;
        }

        string clientIP = inet_ntoa(client.sin_addr);

        if (activeClients >= MAX_CLIENTS) {
            string msg = "Serveri eshte i mbushur. Provo perseri me vone.\n";
            send(clientSocket, msg.c_str(), (int)msg.size(), 0);
            closesocket(clientSocket);
            cout << "[REFUSED] klient i ri nga " << clientIP
                 << " (max clients = " << MAX_CLIENTS << ")" << endl;
            continue;
        }

        activeClients++;
        cout << "[CONNECT] " << clientIP
             << " (numri i lidhjeve aktive: " << activeClients << ")" << endl;

        // Tash për V1 vetëm e mbyllim direkt klientin
        closesocket(clientSocket);
        activeClients--;
    }

    closesocket(listening);
    WSACleanup();
    return 0;
}
