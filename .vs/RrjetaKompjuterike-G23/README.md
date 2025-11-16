README – Projekt Server–Client (Rrjetat Kompjuterike)
Përshkrimi i projektit

Ky projekt implementon një sistem komunikimi Server–Klient duke përdorur WinSock2 në gjuhën C++. Sistemi mbështet shumë klientë njëkohësisht (multithreading), menaxhim të privilegjeve, transferim të file-ve, dhe mekanizëm monitorimi të trafikut në kohë reale.

Serveri pranon kërkesa nga klientët dhe i përpunon ato sipas komandave të specifikuara. Njëri klient (admin) ka privilegje të plota, ndërsa klientët tjerë kanë vetëm akses leximi.

Funksionaliteti i Serverit
1. Parametrat kryesorë

Porti i serverit: 54000

IP Adresa: 0.0.0.0

Numri maksimal i klientëve: 4

Timeout për klientët joaktivë: 30 sekonda

2. Menaxhimi i lidhjeve

Serveri dëgjon në portin e caktuar.

Klientët pranohen deri në kufirin e paracaktuar.

Nëse numri i lidhjeve tejkalohet, lidhjet e reja refuzohen automatikisht.

3. Përpunimi i kërkesave nga klientët

Serveri pranon dhe përpunon komandat e mëposhtme:

Komanda	Përshkrimi
/list	Liston të gjitha file-t në direktoriumin server_files
/read <emri>	Kthen përmbajtjen e file-it
/download <emri>	Shkarkon file nga serveri
/upload <emri>	Ngarkon file në server (vetëm admin)
/delete <emri>	Fshin file nga serveri (vetëm admin)
/search <keyword>	Kërkon file sipas fjalës kyçe
/info <emri>	Shfaq madhësinë dhe datat e file-it
STATS	Kthen statistikat e klientit
4. Log dhe monitorim

Serveri gjeneron dy file logimi:

server_log.txt – regjistron çdo lidhje, shkëputje dhe kërkesë.

server_stats.txt – përditëson statistikat çdo 10 sekonda:

Numri i klientëve aktivë

IP adresat e tyre

Mesazhet e pranuara

Bytes të dërguar dhe të pranuar

5. Privilegjet

Admin-i ka leje të plotë: lexon, shkruan, fshin dhe ngarkon file.

User-i ka vetëm leje leximi.

Funksionaliteti i Klientit
1. Lidhja me serverin

Klienti lidhet me serverin përmes socket-it TCP, duke përdorur portin dhe IP adresën e caktuar.

2. Zgjedhja e rolit

Në startim, klienti zgjedh:

admin
user

3. Ekzekutimi i komandave

Klienti lejon ekzekutimin e komandave të mbështetura nga serveri, sipas rolit dhe privilegjeve.

4. Leximi i përgjigjeve

Klienti lexon përgjigjet e serverit në formë tekstuale dhe i paraqet ato në terminal.

5. Diferencimi i privilegjeve

Admin-i merr përgjigje më shpejt, ndërsa përdoruesit e zakonshëm kanë vonesë artificiale të lehtë, për të simuluar prioritet.

Struktura e projektit
RrjetaKompjuterike-G23/
│── Client/
│   └── client.cpp
│
│── Server/
│   ├── server.cpp
│   ├── server_files/
│   │   ├── test.txt
│   │   ├── ...
│   ├── server_log.txt
│   ├── server_stats.txt
│
│── README.md
│── .vscode/

Udhëzime për ekzekutim
1. Ekzekutimi i serverit:

Në terminal:

cd Server
server.exe


Serveri duhet të shfaqë:

Serveri po degjon ne portin 54000 ...

2. Ekzekutimi i klientit:

Hap një dritare tjetër terminali:

cd Client
client.exe

3. Zgjedhja e rolit:
admin


ose

user

4. Ekzekutimi i komandave:
/list
/read test.txt
/download test.txt
/upload file.txt
/delete file.txt
/info test.txt
/search test
STATS

