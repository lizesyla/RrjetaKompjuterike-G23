#include <iostream>
#include <winsock2.h>
#include <map>
#include <fstream>
#include <sstream>
#include <string>
#pragma comment(lib, "ws2_32.lib") 

using namespace std;

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

void printResponse(SOCKET clientSocket, const string& message) {
    char buffer[8192];

    // Timeout 5 sekonda
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived <= 0) {
        cout << "Serveri u shkeput ose nuk ka pergjigje.\n";
        return;
    }
    buffer[bytesReceived] = '\0';
    string resp(buffer, bytesReceived);

    if (resp.rfind("FILESIZE ", 0) == 0) {
        istringstream iss(resp);
        string tag; int size;
        iss >> tag >> size;
        cout << "[INFO] Server do te dergoje file me size = " << size << " bytes.\n";

        // Emri i file-it nxirret nga komanda
        string filename = message.substr(message.find(' ') + 1);
        ofstream out(filename, ios::binary);
        int received = 0;
        while (received < size) {
            int r = recv(clientSocket, buffer, min((int)sizeof(buffer), size - received), 0);
            if (r <= 0) { cout << "Gabim ne marrjen e file.\n"; break; }
            out.write(buffer, r);
            received += r;
        }
        out.close();
        cout << "File u shkarkua si: " << filename << " (" << received << " bytes)\n";
    } else {
        cout << "Pergjigja nga serveri: " << resp << endl;
    }
}

int main() {
    WSADATA wsaData;
    SOCKET clientSocket;
    struct sockaddr_in serverAddr;

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
    serverAddr.sin_port = htons(54000); 

    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "Lidhja me serverin deshtoi!" << endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    cout << "Jeni lidhur me serverin!\n";

    string roli;
    bool validRole = false;
    bool isAdmin = false;

    while (!validRole) {
        cout << "Zgjedh rolin tend (admin ose user): ";
        getline(cin, roli);

        if (roli == "admin") {
            isAdmin = true;
            cout << "Jeni kycur si ADMIN - keni qasje te plote\n";
            validRole = true;
        } else if (roli == "user") {
            isAdmin = false;
            cout << "Jeni kycur si USER - vetem read lejohet\n";
            validRole = true;
        } else {
            cout << "Roli nuk ekziston. Shkruani 'admin' ose 'user'.\n";
        }
    }

    // Dërgo ROLE me newline
    string privInfo = "ROLE:" + roli + "\n";
    if (!sendAll(clientSocket, privInfo.c_str(), (int)privInfo.size())) {
        cerr << "Gabim ne dergimin e role.\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    // Presim konfirmimin e serverit
    char buf[512];
    int br = recv(clientSocket, buf, sizeof(buf)-1, 0);
    if (br > 0) {
        buf[br] = '\0';
        cout << "Server: " << buf << endl;
    }

    map<string, bool> allowedCommands;
    if (isAdmin) {
        allowedCommands = {
            {"/list", true}, {"/read", true}, {"/upload", true},
            {"/download", true}, {"/delete", true}, {"/search", true},
            {"/info", true}, {"STATS", true}
        };
    } else {
        allowedCommands = {
            {"/read", true}, {"/list", true}, {"/download", true},
            {"/info", true}, {"/search", true}
        };
    }

    string message;
    while (true) {
        cout << "Shkruani kerkesen tuaj: ";
        getline(cin, message);

        if (message.empty()) {
            cout << "Komanda nuk mund te jete bosh. Provoni perseri.\n";
            continue;
        }
        if (message == "/exit") {
            cout << "Duke u shkeputur nga serveri...\n";
            break;
        }

        istringstream iss(message);
        string cmd; iss >> cmd;

        if (allowedCommands.find(cmd) == allowedCommands.end()) {
            cout << "Kjo komande nuk lejohet per rolin tuaj ose nuk ekziston.\n";
            continue;
        }

        // Shtojmë newline për të gjitha komandat
        message += "\n";

        // --- UPLOAD ---
        if (cmd == "/upload") {
            string localPath, remoteName;
            iss >> localPath >> remoteName;
            if (localPath.empty() || remoteName.empty()) {
                cout << "Usage: /upload <localPath> <remoteName>\n";
                continue;
            }

            ifstream in(localPath, ios::binary);
            if (!in.is_open()) {
                cout << "ERROR: Nuk mund te hap file lokal: " << localPath << endl;
                continue;
            }

            in.seekg(0, ios::end);
            int size = (int)in.tellg();
            in.seekg(0, ios::beg);

            string header = "/upload " + remoteName + "\n";
            if (!sendAll(clientSocket, header.c_str(), (int)header.size())) {
                cout << "ERROR ne dergimin e header.\n";
                in.close();
                continue;
            }

            string fsizeStr = "FILESIZE " + to_string(size) + "\n";
            if (!sendAll(clientSocket, fsizeStr.c_str(), (int)fsizeStr.size())) {
                cout << "ERROR ne dergimin e FILESIZE.\n";
                in.close();
                continue;
            }

            const int BUF = 4096;
            char buf[BUF];
            while (!in.eof()) {
                in.read(buf, BUF);
                streamsize r = in.gcount();
                if (r > 0) {
                    if (!sendAll(clientSocket, buf, (int)r)) {
                        cout << "ERROR during upload.\n";
                        break;
                    }
                }
            }
            in.close();

            br = recv(clientSocket, buf, sizeof(buf)-1, 0);
            if (br > 0) {
                buf[br] = '\0';
                cout << "Server: " << buf << endl;
            }
            continue;
        }

        // --- DOWNLOAD / READ ---
        else if (cmd == "/download" || cmd == "/read") {
            if (message.find(' ') == string::npos) {
                cout << "Usage: " << cmd << " <filename>\n";
                continue;
            }
            if (!sendAll(clientSocket, message.c_str(), (int)message.size())) {
                cout << "ERROR sending command\n";
                continue;
            }
            printResponse(clientSocket, message);
            continue;
        }

        // --- OTHER COMMANDS (text) ---
        else {
            if (!sendAll(clientSocket, message.c_str(), (int)message.size())) {
                cout << "ERROR sending command\n";
                continue;
            }
            int bytesReceived = recv(clientSocket, buf, sizeof(buf)-1, 0);
            if (bytesReceived <= 0) {
                cout << "Serveri u shkeput.\n";
                continue;
            }
            buf[bytesReceived] = '\0';
            cout << "Server: " << buf << endl;
        }
    }

    closesocket(clientSocket);
    WSACleanup();
    cout << "Lidhja u mbyll me sukses.\n";
    return 0;
}
