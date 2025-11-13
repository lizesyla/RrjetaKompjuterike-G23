#include <iostream>
#include <winsock2.h>  // për funksionet e socket-it
#pragma comment(lib, "ws2_32.lib") // lidh me bibliotekën e Winsock

using namespace std;

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
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // localhost
    serverAddr.sin_port = htons(1200); 


    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "Lidhja me serverin deshtoi!" << endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    cout << "Jeni lidhur me serverin!\n";
    cout << "Shkruani kerkesen tuaj: ";
    getline(cin, message);


    send(clientSocket, message.c_str(), message.length(), 0);


    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        cout << "Pergjigja nga serveri: " << buffer << endl;
    } else {
        cout << "Nuk u mor asnje pergjigje nga serveri." << endl;
    }

    closesocket(clientSocket);
    WSACleanup();
    cout << "Lidhja u mbyll me sukses.\n";

    return 0;
}