#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <set>
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
const int TIMEOUT_MS = 30000;

struct ClientStats {
    string ip;
    int messagesReceived = 0;
    int bytesReceived = 0;
    int bytesSent = 0;
};

vector<SOCKET> clients;
vector<ClientStats> clientStats;
mutex clientsMutex;
mutex logMutex;
mutex statsMutex;

set<string> fullAccessClients = {
    "192.168.1.100",
};

void logMessage(const string& msg) {
    lock_guard<mutex> lock(logMutex);
    ofstream file("server_log.txt", ios::app);
    if(file.is_open()){
    file << msg << endl;
    }
}

bool sendAll(SOCKET socket, const char* data, int totalBytes) {
    int bytesSent = 0;
    while (bytesSent < totalBytes) {
        int result = send(socket, data + bytesSent, totalBytes - bytesSent, 0);
        if (result == SOCKET_ERROR) {
            int err = WSAGetLastError();
            cerr << "[ERROR] send failed: " << err << endl;
            return false;
        }
        bytesSent += result;
    }
    return true;
}

void handleClient(SOCKET clientSocket, string clientIP) {
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&TIMEOUT_MS, sizeof(TIMEOUT_MS));
    bool fullAccess = (fullAccessClients.find(clientIP) != fullAccessClients.end());

    {
        lock_guard<mutex> lock(statsMutex);
        clientStats.push_back({ clientIP, 0, 0, 0 });
    }
    
    char buffer[4096];

    while (true) {
        ZeroMemory(buffer, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);

        if(bytesReceived == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAETIMEDOUT) {
                cout << "[TIMEOUT] " << clientIP << endl;
                logMessage("[TIMEOUT] " + clientIP);
            } else {
                cout << "[ERROR] Receiving nga " << clientIP << " Error: " << err << endl;
                logMessage("[ERROR] Receiving nga " + clientIP + " Error: " + to_string(err));
            }
            break;
        }

        if (bytesReceived <= 0) {
            cout << "[DISCONNECT] " << clientIP << endl;
            logMessage("[DISCONNECT] " + clientIP);
            break;
        }

        string msg(buffer, bytesReceived);
        cout << "[FROM " << clientIP << "] " << msg << endl;
        logMessage("[FROM " + clientIP + "] " + msg);

        if (msg == "STATS") {
            string statsMsg;
            {
                lock_guard<mutex> lock(statsMutex);
                for (auto& stats : clientStats) {  
                    if (stats.ip == clientIP) {
                        statsMsg = "Mesazhet e pranuara: " + to_string(stats.messagesReceived) +
                                ", Bytes te pranuara: " + to_string(stats.bytesReceived) +
                                ", Bytes te derguara: " + to_string(stats.bytesSent) + "\n";
                        stats.bytesSent += (int)statsMsg.size();
                        break;
                    }
                }
            }
            if(!sendAll(clientSocket, statsMsg.c_str(), (int)statsMsg.size())) {
                break;
            }
            // send(clientSocket, statsMsg.c_str(), (int)statsMsg.size(), 0);
            continue;
        }


        {
            lock_guard<mutex> lock(statsMutex);
            for (auto& stats : clientStats) {
                if (stats.ip == clientIP) {
                    stats.messagesReceived++;
                    stats.bytesReceived += bytesReceived;
                    break;
                }
            }
        }
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



    {
        lock_guard<mutex> lock(statsMutex);
        for (auto it = clientStats.begin(); it != clientStats.end(); ++it) {
            if (it->ip == clientIP) {
                clientStats.erase(it);
                break;
            }
        } 
    }
}


void monitorStats() {
    string command;
    while (true) {
        getline(cin, command);
        if (command == "STATS") {
            
            int activeClients;
            {
                lock_guard<mutex> lock(clientsMutex);
                activeClients = clients.size();
            }

            lock_guard<mutex> lock(statsMutex);
            ofstream statsFile("server_stats.txt");

            if (statsFile.is_open()) {
                statsFile << "Lidhjet aktive: " << activeClients << endl;
                statsFile << "IP klientëve aktivë dhe statistikat:\n";
                for (const auto& cs : clientStats) {
                    statsFile << cs.ip << ": Mesazhe=" << cs.messagesReceived
                              << ", BytesPranuar=" << cs.bytesReceived
                              << ", BytesDeruar=" << cs.bytesSent << endl;
                }
            }

            cout << "Lidhjet aktive: " << activeClients << endl;
            cout << "IP klientëve aktivë dhe statistikat:\n";

            for (const auto& cs : clientStats) {
                cout << cs.ip << ": Mesazhe=" << cs.messagesReceived
                    << ", BytesPranuar=" << cs.bytesReceived
                    << ", BytesDerguar=" << cs.bytesSent << endl;
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

    thread statsThread(monitorStats);
    statsThread.detach();

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