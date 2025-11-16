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
#include <filesystem>
#include <sstream>
#include <chrono>
#include <sys/stat.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;
namespace fs = std::filesystem;

const int PORT = 54000;
const char* SERVER_IP = "0.0.0.0";
const int MAX_CLIENTS = 4;
const int TIMEOUT_MS = 30000;

struct ClientStats {
    SOCKET socket;       // Unik për çdo klient
    string ip;
    int messagesReceived = 0;
    int bytesReceived = 0;
    int bytesSent = 0;
    bool isAdmin = false;
};

vector<SOCKET> clients;
vector<ClientStats> clientStats;
mutex clientsMutex;
mutex logMutex;
mutex statsMutex;

set<string> fullAccessClients = {
    "192.168.1.100", // IP admin
};

const string SERVER_FILES_DIR = "server_files";

// --- LOG ---
void logMessage(const string& msg) {
    lock_guard<mutex> lock(logMutex);
    ofstream file("server_log.txt", ios::app);
    if (file.is_open()) {
        auto now = chrono::system_clock::to_time_t(chrono::system_clock::now());
        file << "[" << std::ctime(&now) << "] " << msg << endl;
    }
}

// --- SEND ALL ---
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

// --- SEND FILE ---
bool sendFile(SOCKET sock, const fs::path& filepath, ClientStats &stats) {
    ifstream in(filepath, ios::binary);
    if (!in.is_open()) return false;
    in.seekg(0, ios::end);
    auto size = (int)in.tellg();
    in.seekg(0, ios::beg);

    string header = "FILESIZE " + to_string(size) + "\n";
    if (!sendAll(sock, header.c_str(), (int)header.size())) return false;
    stats.bytesSent += (int)header.size();

    const int BUF = 4096;
    char buffer[BUF];
    while (!in.eof()) {
        in.read(buffer, BUF);
        streamsize r = in.gcount();
        if (r > 0) {
            if (!sendAll(sock, buffer, (int)r)) return false;
            stats.bytesSent += (int)r;
        }
    }
    return true;
}

// --- RECV ALL ---
bool recvAll(SOCKET sock, char* data, int totalBytes) {
    int received = 0;
    while (received < totalBytes) {
        int r = recv(sock, data + received, totalBytes - received, 0);
        if (r <= 0) return false;
        received += r;
    }
    return true;
}

// --- SERVER DIR ---
void ensureServerDir() {
    if (!fs::exists(SERVER_FILES_DIR)) {
        fs::create_directory(SERVER_FILES_DIR);
    }
}

// --- LIST FILES ---
string listFiles() {
    ensureServerDir();
    ostringstream out;
    for (auto& p : fs::directory_iterator(SERVER_FILES_DIR)) {
        out << p.path().filename().string() << "\n";
    }
    return out.str();
}

// --- FILE INFO ---
string fileInfo(const string& filename) {
    string fullPath = "server_files/" + filename;
    struct stat fileStat;
    if (stat(fullPath.c_str(), &fileStat) != 0) {
        return "ERROR: File-i nuk ekziston ose nuk mund te lexohet.\n";
    }
    off_t fileSize = fileStat.st_size;

    char modTime[30];
    strftime(modTime, sizeof(modTime), "%Y-%m-%d %H:%M:%S", localtime(&fileStat.st_mtime));
    char createTime[30];
    strftime(createTime, sizeof(createTime), "%Y-%m-%d %H:%M:%S", localtime(&fileStat.st_ctime));

    string info = "INFO per file: " + filename + "\n" +
                  "Madhesia: " + to_string(fileSize) + " bytes\n" +
                  "Data modifikimit: " + string(modTime) + "\n" +
                  "Data krijimit: " + string(createTime) + "\n";
    return info;
}

// --- SEARCH FILES ---
vector<string> searchFiles(const string& keyword) {
    ensureServerDir();
    vector<string> found;
    for (auto& p : fs::directory_iterator(SERVER_FILES_DIR)) {
        string name = p.path().filename().string();
        if (name.find(keyword) != string::npos) found.push_back(name);
    }
    return found;
}

// --- UPDATE STATS HELPER ---
void updateStats(ClientStats* st, int bytesReceived, int bytesSent) {
    lock_guard<mutex> lock(statsMutex);
    if (st) {
        st->bytesReceived += bytesReceived;
        st->bytesSent += bytesSent;
    }
}

// --- HANDLE CLIENT ---
void handleClient(SOCKET clientSocket, string clientIP) {
    DWORD timeout = TIMEOUT_MS;
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    bool isAdmin = false;
    {
        lock_guard<mutex> lock(statsMutex);
        bool presetAdmin = (fullAccessClients.find(clientIP) != fullAccessClients.end());
        clientStats.push_back({ clientSocket, clientIP, 0, 0, 0, presetAdmin });
    }

    auto getStatsRef = [&](ClientStats*& out)->bool {
        for (auto &s : clientStats) {
            if (s.socket == clientSocket) { out = &s; return true; }
        }
        return false;
    };

    char buffer[8192];

    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        string firstMsg(buffer, bytesReceived);
        if (firstMsg.rfind("ROLE:", 0) == 0) {
            string r = firstMsg.substr(5);
            r.erase(r.find_last_not_of("\r\n")+1);
            if (r == "admin") isAdmin = true;
        }
        lock_guard<mutex> lock(statsMutex);
        for (auto &s : clientStats) {
            if (s.socket == clientSocket) {
                if (isAdmin) s.isAdmin = true;
                break;
            }
        }
    }

    cout << "[SESSION START] " << clientIP << " admin=" << (isAdmin? "YES":"NO") << endl;
    logMessage("[CONNECT SESSION] " + clientIP + " admin=" + string(isAdmin? "YES":"NO"));

    while (true) {
        ZeroMemory(buffer, sizeof(buffer));
        int bytes = recv(clientSocket, buffer, sizeof(buffer)-1, 0);
        if (bytes <= 0) {
            cout << "[DISCONNECT] " << clientIP << endl;
            logMessage("[DISCONNECT] " + clientIP);
            break;
        }

        string msg(buffer, bytes);
        msg.erase(msg.find_last_not_of("\r\n")+1);
        msg.erase(0, msg.find_first_not_of(" \t"));

        ClientStats* st = nullptr;
        if (getStatsRef(st)) {
            lock_guard<mutex> lock(statsMutex);
            st->messagesReceived++;
            st->bytesReceived += bytes;
            isAdmin = st->isAdmin;
        }

        cout << "[FROM " << clientIP << "] " << msg << endl;
        logMessage("[FROM " + clientIP + "] " + msg);

        istringstream iss(msg);
        string cmd; iss >> cmd;

        if (cmd == "STATS") {
            if (st) {
                string statsMsg = "Mesazhet e pranuara: " + to_string(st->messagesReceived) +
                                  ", Bytes te pranuara: " + to_string(st->bytesReceived) +
                                  ", Bytes te derguara: " + to_string(st->bytesSent) + "\n";
                sendAll(clientSocket, statsMsg.c_str(), (int)statsMsg.size());
                st->bytesSent += (int)statsMsg.size();
            }
            continue;
        }
        else if (cmd == "/list") {
            string fl = listFiles();
            if (fl.empty()) fl = "(empty)\n";
            sendAll(clientSocket, fl.c_str(), (int)fl.size());
            updateStats(st, 0, (int)fl.size());
            continue;
        }
        else if (cmd == "/read" || cmd == "/download") {
            string filename; iss >> filename;
            if (filename.empty()) {
                string err = "ERROR: Duhet emri i file.\n";
                sendAll(clientSocket, err.c_str(), (int)err.size());
                updateStats(st, 0, (int)err.size());
                continue;
            }
            fs::path p = fs::path(SERVER_FILES_DIR) / filename;
            if (!fs::exists(p)) {
                string err = "ERROR: File nuk ekziston.\n";
                sendAll(clientSocket, err.c_str(), (int)err.size());
                updateStats(st, 0, (int)err.size());
                continue;
            }
            if (cmd == "/read") {
                ifstream in(p, ios::binary);
                in.seekg(0, ios::end);
                int size = (int)in.tellg();
                in.seekg(0, ios::beg);
                string header = "FILESIZE " + to_string(size) + "\n";
                sendAll(clientSocket, header.c_str(), (int)header.size());
                updateStats(st, 0, (int)header.size());

                const int BUF = 4096;
                char buf[BUF];
                while (!in.eof()) {
                    in.read(buf, BUF);
                    streamsize r = in.gcount();
                    if (r > 0) {
                        sendAll(clientSocket, buf, (int)r);
                        updateStats(st, 0, (int)r);
                    }
                }
            } else {
                sendFile(clientSocket, p, *st);
            }
            continue;
        }
        else if (cmd == "/info") {
            string filename; iss >> filename;
            if (filename.empty()) {
                string err = "ERROR: Duhet emri i file.\n";
                sendAll(clientSocket, err.c_str(), (int)err.size());
                updateStats(st, 0, (int)err.size());
                continue;
            }
            string info = fileInfo(filename);
            sendAll(clientSocket, info.c_str(), (int)info.size());
            updateStats(st, 0, (int)info.size());
            continue;
        }
        else if (cmd == "/search") {
            string keyword; iss >> keyword;
            if (keyword.empty()) {
                string err = "ERROR: Duhet keyword.\n";
                sendAll(clientSocket, err.c_str(), (int)err.size());
                updateStats(st, 0, (int)err.size());
                continue;
            }
            auto found = searchFiles(keyword);
            ostringstream out;
            if (found.empty()) out << "(no matches)\n";
            else for (auto &f : found) out << f << "\n";
            string outS = out.str();
            sendAll(clientSocket, outS.c_str(), (int)outS.size());
            updateStats(st, 0, (int)outS.size());
            continue;
        }
  else if (cmd == "/upload") {
    string filename; iss >> filename;
    if (filename.empty()) {
        string err = "ERROR: Duhet emri i file.\n";
        sendAll(clientSocket, err.c_str(), (int)err.size());
        updateStats(st, 0, (int)err.size());
        continue;
    }

    // Presim madhësinë
    int filesize = 0;
    ZeroMemory(buffer, sizeof(buffer));
    int br = recv(clientSocket, buffer, sizeof(buffer)-1, 0);
    if (br <= 0) {
        string err = "ERROR: Nuk mund te merrni madhësinë.\n";
        sendAll(clientSocket, err.c_str(), (int)err.size());
        continue;
    }
    buffer[br] = '\0';
    string header(buffer);
    if (header.rfind("FILESIZE ", 0) != 0) {
        string err = "ERROR: Header i FILESIZE nuk u pranuar.\n";
        sendAll(clientSocket, err.c_str(), (int)err.size());
        continue;
    }
    filesize = stoi(header.substr(9));

    // Hapim file-in ne server
    fs::path p = fs::path(SERVER_FILES_DIR) / filename;
    ofstream out(p, ios::binary);
    if (!out.is_open()) {
        string err = "ERROR: Nuk mund te hapni file ne server.\n";
        sendAll(clientSocket, err.c_str(), (int)err.size());
        continue;
    }

    int received = 0;
    while (received < filesize) {
        int r = recv(clientSocket, buffer, min((int)sizeof(buffer), filesize - received), 0);
        if (r <= 0) break;
        out.write(buffer, r);
        received += r;
    }
    out.close();

    string ok = "File u ngarkua me sukses: " + filename + "\n";
    sendAll(clientSocket, ok.c_str(), (int)ok.size());
}


        else {
            string err = "ERROR: Komande e panjohur.\n";
            sendAll(clientSocket, err.c_str(), (int)err.size());
            updateStats(st, 0, (int)err.size());
            continue;
        }
    }

    closesocket(clientSocket);
    {
        lock_guard<mutex> lock(clientsMutex);
        clients.erase(remove(clients.begin(), clients.end(), clientSocket), clients.end());
    }
    {
        lock_guard<mutex> lock(statsMutex);
        clientStats.erase(remove_if(clientStats.begin(), clientStats.end(),
                                    [&](const ClientStats& s){ return s.socket == clientSocket; }),
                          clientStats.end());
    }
}

// --- WRITE STATS ---
void writeStatsToFileAndConsole() {
    int activeClients;
    {
        lock_guard<mutex> lock(clientsMutex);
        activeClients = clients.size();
    }

    lock_guard<mutex> lock(statsMutex);
    ofstream statsFile("server_stats.txt");
    if (!statsFile.is_open()) {
        cerr << "[ERROR] Nuk mund të hap server_stats.txt" << endl;
        return;
    }

    statsFile << "Lidhjet aktive: " << activeClients << endl;
    statsFile << "IP klientëve aktivë dhe statistikat:\n";

    cout << "\n===== SERVER STATS =====" << endl;
    cout << "Lidhjet aktive: " << activeClients << endl;
    cout << "IP klienteve aktive dhe statistikat:" << endl;

    for (const auto& cs : clientStats) {
        statsFile << cs.ip
                  << ": Mesazhe=" << cs.messagesReceived
                  << ", BytesPranuar=" << cs.bytesReceived
                  << ", BytesDerguar=" << cs.bytesSent
                  << ", isAdmin=" << (cs.isAdmin ? "YES" : "NO") << endl;

        cout << cs.ip
             << ": Mesazhe=" << cs.messagesReceived
             << ", BytesPranuar=" << cs.bytesReceived
             << ", BytesDerguar=" << cs.bytesSent
             << ", isAdmin=" << (cs.isAdmin ? "YES" : "NO") << endl;
    }
    cout << "========================\n" << endl;
}

// --- MONITOR STATS THREAD ---
void monitorStats() {
    string command;
    while (true) {
        if (getline(cin, command)) {
            if (command == "STATS") {
                writeStatsToFileAndConsole();
            }
        }

        static auto lastUpdate = chrono::steady_clock::now();
        auto now = chrono::steady_clock::now();
        if (chrono::duration_cast<chrono::seconds>(now - lastUpdate).count() >= 10) {
            writeStatsToFileAndConsole();
            lastUpdate = now;
        }

        this_thread::sleep_for(chrono::milliseconds(100));
    }
}

// --- MAIN ---
int main() {
    ensureServerDir();

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
