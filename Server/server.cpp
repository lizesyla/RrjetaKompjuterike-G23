#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <fstream>
#include <string>

#pragma comment(lib, "ws2_32.lib")   // lidh automatikisht winsock librarin

using namespace std;

// -------------------- PIKA 1: PORT & IP --------------------
const int PORT = 54000;               // numri i portit (mund ta ndryshosh)
const char* SERVER_IP = "0.0.0.0";    // 0.0.0.0 => dëgjon në të gjitha interfacet

// -------------------- PIKA 2: LIMIT I LIDHJEVE -------------
const int MAX_CLIENTS = 4;            // max lidhje aktive

vector<SOCKET> clients;
mutex clientsMutex;
mutex logMutex;

// -------------------- PIKA 4: LOG I MESAZHEVE --------------
void logMessage(const string& msg) {
    lock_guard<mutex> lock(logMutex);
    ofstream file("server_log.txt", ios::app);  // ruhet ne folderin e projektit
    file << msg << endl;
}

// -------------------- PIKA 3 & 4: MENAXHIMI I KLIENTIT ----
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

        // mesazhi i marre
        string msg(buffer, bytesReceived);

        // shfaqe ne server
        cout << "[FROM " << clientIP << "] " << msg << endl;

        // ruaje ne log file (PIKA 4)
        logMessage("[FROM " + clientIP + "] " + msg);

        // (opsionale) mundesh me i kthye pergjigje klientit:
        // string reply = "Serveri mori mesazhin: " + msg;
        // send(clientSocket, reply.c_str(), (int)reply.size(), 0);
    }

    // mbyll socket-in e klientit
    closesocket(clientSocket);

    // hiqe nga lista globale e klienteve
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

// -------------------- FUNKSIONI KRYESOR --------------------
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

    // Lidhja me IP & PORT (PIKA 1)
    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(PORT);
    hint.sin_addr.s_addr = INADDR_ANY;   // sepse SERVER_IP = 0.0.0.0

    if (bind(listening, (sockaddr*)&hint, sizeof(hint)) == SOCKET_ERROR) {
        cout << "Bind failed! Error: " << WSAGetLastError() << endl;
        closesocket(listening);
        WSACleanup();
        return 1;
    }

    // Filloi listen (PIKA 2)
    if (listen(listening, SOMAXCONN) == SOCKET_ERROR) {
        cout << "Listen failed! Error: " << WSAGetLastError() << endl;
        closesocket(listening);
        WSACleanup();
        return 1;
    }

    cout << "Serveri po degjon ne portin " << PORT << " ..." << endl;

    // Loop kryesor i serverit – pranon klientë (PIKA 2 & 3)
    while (true) {
        sockaddr_in client;
        int clientSize = sizeof(client);

        SOCKET clientSocket = accept(listening, (sockaddr*)&client, &clientSize);
        if (clientSocket == INVALID_SOCKET) {
            cout << "Error accepting client! Error: " << WSAGetLastError() << endl;
            continue;
        }

        string clientIP = inet_ntoa(client.sin_addr);

        // kontrollo MAX_CLIENTS (PIKA 2)
        {
            lock_guard<mutex> lock(clientsMutex);
            if ((int)clients.size() >= MAX_CLIENTS) {
                string msg = "Serveri eshte i mbushur. Provo perseri me vone.\n";
                send(clientSocket, msg.c_str(), (int)msg.size(), 0);
                closesocket(clientSocket);
                cout << "[REFUSED] klient i ri nga " << clientIP
                     << " (max clients = " << MAX_CLIENTS << ")" << endl;
                logMessage("[REFUSED] " + clientIP + " (too many clients)");
                continue;
            }

            // shtoje klientin ne liste
            clients.push_back(clientSocket);
        }

        cout << "[CONNECT] " << clientIP << endl;
        logMessage("[CONNECT] " + clientIP);

        // PIKA 3: krijo thread per kete klient
        thread t(handleClient, clientSocket, clientIP);
        t.detach();
    }

    // nuk arrin kurrë këtu normalisht, por per siguri:
    closesocket(listening);
    WSACleanup();
    return 0;
}
