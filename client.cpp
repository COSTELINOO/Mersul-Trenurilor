#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cerrno>
#include <unistd.h>
#include <cstring>
#include <vector>

std::vector<std::string> vector_comenzi;

extern int errno;

int main(int argc, char *argv[]) {
    // descriptorul de socket
    int sd = 0;

    // structura folosita pentru realizarea conexiunii
    sockaddr_in server{};


    if (argc != 3) {
        std::cout << "\n[client] Sintaxa: " << argv[0] << " <adresa_server> <port>\n";
        return -1;
    }

    // stabilim portul
    int port = std::stoi(argv[2]);

    // cream socketul
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1) {
        perror("\n[client] Eroare la creare socket().\n");
        return errno;
    }

    // umplem structura pentru realizarea conexiunii cu serverul
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons(port);

    // ne conectam la server
    if (connect(sd, (sockaddr *) &server, sizeof(server)) == -1) {
        perror("\n[client] Eroare la connectare server.\n");
        return errno;
    }

    bool mesaj_trimis = false;
    std::cout << "\n[client] Introduceti comenzile: \n";
    while (true) {
        std::string msg = "";

        // citirea comenzilor
        std::getline(std::cin, msg);

        //trimiterea comenzilor la server

        if (msg == "Trimite") {
            vector_comenzi.emplace_back("Gata");

            if (vector_comenzi.size() > 1) {
                int cnt = 0;
                for (const auto &comanda: vector_comenzi) {
                    usleep(10000);
                    if (comanda.size() > 0) {
                        cnt++;

                        if (send(sd, comanda.c_str(), comanda.length() + 1, 0) <= 0) {
                            perror("\n[client] Eroare la trimitearea comenzilor catre server .\n");
                            return errno;
                        }
                        mesaj_trimis = true;
                    }
                }
                std::cout << "\n[client]: Au fost trimise catre : " << cnt - 1 << " comenzi\n";
            }
            vector_comenzi.clear();
        } else {
            if (msg.size() > 0 && msg != "\n") {
                {
                    vector_comenzi.push_back(msg);
                }
            }
        }

        //citire raspuns de la server
        char buffer[100000];
        bzero(buffer, 100000);
        bool comanda_terminata = false;

        if (mesaj_trimis == true)
            while (comanda_terminata == false) {
                bzero(buffer, 100000);
                ssize_t bytes_read = read(sd, buffer, 100000);
                if (bytes_read < 0) {
                    perror("\n[client] Eroare la citirea mesajului transmis de server.\n");
                    return errno;
                }
                if (bytes_read == 0) {
                    // serverul a închis conexiunea
                    std::cout << "\n[client] Deconectare de la server...\n";
                    std::cout << "[client] Iesire din aplicatie...\n";
                    close(sd);
                    return 0;
                }
                if (strncmp(buffer, "quit", 4) == 0) {
                    std::cout << "[client] Iesire din aplicatie...\n";
                    //close(sd);
                    return 0;
                }


                //verificam daca serverul a terminat
                if (strncmp(buffer, "Gata", 4) == 0) {
                    comanda_terminata = true;
                    std::cout << "\n[client] Introduceti comenzile: \n";
                    mesaj_trimis = false;
                    vector_comenzi.clear();
                } else {
                    std::cout << buffer << "\n";
                }
            }
    }
}
