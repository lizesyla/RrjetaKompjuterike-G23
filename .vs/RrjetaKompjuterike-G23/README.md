Rrjetat Kompjuterike – Projekti Server/Client

Implementimi i një sistemi server–klient me komunikim TCP, role, qasje në file dhe monitorim trafiku.

Përshkrimi i projektit

Ky projekt implementon një arkitekturë Server–Client duke përdorur Winsock2 (C++), me funksionalitet të plotë sipas kërkesave të lëndës Rrjetat Kompjuterike.

Sistemi përfshin:

Server i aftë të menaxhojë më shumë klientë.

Klientë me role të ndryshme (admin dhe user).

Qasje në file në server.

Monitorim të trafikut dhe aktiviteteve të klientëve.

Përpunim komandash.

1. Funksionaliteti i Serverit
1. Variablat kryesore të konfigurimit

PORT = 54000

SERVER_IP = 0.0.0.0

2. Menaxhimi i lidhjeve

Serveri dëgjon më shumë klientë në të njëjtën kohë.

Limiti maksimal është përcaktuar me MAX_CLIENTS.

Nëse limiti tejkalohet, lidhjet e reja refuzohen me mesazh përkatës.

3. Përpunimi i kërkesave

Çdo klient mund të dërgojë kërkesa dhe serveri i trajton ato me thread të ndara.

4. Monitorimi i komunikimit

Serveri ruan për secilin klient:

IP adresën

Numrin e mesazheve të pranuara

Bytes të dërguar / pranuar

Statusin admin/user

5. Timeout

Nëse klienti nuk dërgon asnjë mesazh për më shumë se:

TIMEOUT_MS = 30000 ms


serveri e mbyll lidhjen.

6. Qasje në file në server

Serveri lejon:

Lexim file

Listim folderësh

Upload

Download

Fshirje

Kërkim

Informata për file

7. Monitorim i trafikut

Komanda:

STATS


shfaq në server:

Numrin e lidhjeve aktive

IP adresat e klientëve aktivë

Statistikat e dërgim/pranimit të mesazheve

Gjendjen admin/user të secilit klient

Statistikat shkruhen edhe automatikisht në:

server_stats.txt

2. Funksionaliteti i Klientit
1. Lidhja me serverin

Klienti lidhet përmes TCP socket në portin 54000.

2. Role të ndryshme

admin: qasje e plotë (lexim, shkrim, fshirje, upload, download, search…)

user: vetëm lexim

3. Komandat që ekzekuton klienti admin
Komanda	Përshkrimi
/list	Liston file-t në server
/read <file>	Lexon përmbajtjen e file-it në server
/upload <file>	Dërgon file në server
/download <file>	Shkarkon file nga serveri
/delete <file>	Fshin file në server
/search <keyword>	Kërkon file sipas fjalës kyçe
/info <file>	Shfaq madhësinë dhe datat e krijimit/modifikimit
4. Komandat për user

User ka vetëm:

/list

/read

/info

3. Si të ekzekutohet projekti
A. Kompajlimi i serverit (MSYS2 MINGW64)
g++ server.cpp -o server.exe -lws2_32 -std=c++17


Ekzekutimi:

./server.exe

B. Kompajlimi i klientit
g++ client.cpp -o client.exe -lws2_32 -std=c++17


Ekzekutimi:

./client.exe

4. Struktura e projektit
RrjetaKompjuterike-G23/
│── Server/
│   ├── server.cpp
│   ├── server.exe
│   ├── server_log.txt
│   ├── server_stats.txt
│   └── server_files/
│       └── ...
│
│── Client/
│   ├── client.cpp
│   └── client.exe
│
└── README.md

5. Autore / Kontribut

Projekti është zhvilluar si pjesë e detyrës së dytë për lëndën Rrjetat Kompjuterike.

6. Licenca

Ky projekt është krijuar vetëm për qëllime arsimore.