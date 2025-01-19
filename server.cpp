#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <pthread.h>
#include <condition_variable>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include "libxml/parser.h"
#include "libxml/tree.h"
#include <sys/stat.h>
#include <iomanip>
using namespace std;

#define PORT 2728
#define MAX_CLIENTS 100
#define MAX_COMMAND_LENGTH 10000

extern int errno;
pthread_mutex_t mutexx = PTHREAD_MUTEX_INITIALIZER;

string nume_xml = "trenuri.xml";

// lista comenzilor valide
string comenzi_valide =
        "Comenzile valide sunt urmatoarele(17 comenzi):\n"
        "->adaugare tren: [id_tren] [ruta] [status_plecare] [status_sosire] [intarziere] \n"
        "->adaugare statie: [id_tren] [nume_statie] [status_sosire] [status_plecare] [intarziere]\n"
        "->modificare statie: [id_tren] [nume_statie] [status_sosire] [status_plecare] [intarziere]\n"
        "->status ruta tren: [id_tren]\n"
        "->status statie: [id_tren] [nume_statie]\n"
        "->actualizare tren: [id_tren] [ruta] [status_plecare] [status_sosire] [intarziere]\n"
        "->actualizare status plecare: [id_tren] [status_plecare_nou]\n"
        "->actualizare status sosire: [id_tren] [status_sosire_nou]\n"
        "->actualizare intarziere tren: [id_tren] [intarziere]\n"
        "->plecari urmatoarea ora: [ora(HH:MM)]\n"
        "->sosiri urmatoarea ora: [ora(HH:MM)]\n"
        "->intarzierea trenurilor\n"
        "->intarziere: [id_tren]\n"
        "->mersul trenurilor\n"
        "->ruta trenurilor\n"
        "->help\n"
        "->quit\n"
        "->statii tren: [id_tren]\n\n\n"


        "Observatii privind corectitudinea comenzilor:\n"
        "->            [ruta]:              [STATIE1]-[STATIE2]\n"
        "->            [STATIE1]:           sir_de_caractere_doar_din_litere\n"
        "->            [STATIE2]:           sir_de_caractere_doar_din_litere\n"
        "->            [id_statie]:         sir_de_caractere\n"
        "->            [id_tren]:           sir_de_caractere\n"
        "->            [status_plecare]:    (HH:MM)\n"
        "->            HH:                  numar_intreg din intervalul 0-23\n"
        "->            MM:                  numar_intreg din intervalul 0-59\n"
        "->            [status_sosire]:     (HH:MM)\n"
        "->            [intarziere]:        (HH:MM)\n"
        "->                                -(HH:MM)\n"
        "->            [nume_statie]:       sir_de_caractere_din_litere\n";

void string_to_ora(const string &interval, int &ora, int &minut) {
    const bool negative = (interval[0] == '-');

    std::string time_str = interval;

    if (negative) {
        time_str = interval.substr(1);
    }

    size_t poz = time_str.find(':');

    if (poz != std::string::npos) {
        ora = std::stoi(time_str.substr(0, poz));
        minut = std::stoi(time_str.substr(poz + 1));
    }

    if (negative) {
        ora = -ora;
        minut = -minut;
    }
}

int compara_ore(const std::string &ora1, const std::string &ora2) {
    int h1, m1, h2, m2;
    string_to_ora(ora1, h1, m1);
    string_to_ora(ora2, h2, m2);

    if (h1 < (h2)) return -1;
    if (h1 > (h2 + 1)) return -1;
    if (h1 == h2 && m1 < m2) return -1;
    if (h1 == (h2 + 1) && m1 > m2) return -1;

    return 1;
}

struct statii {
    string id_tren;
    string nume;
    string status_sosire;
    string status_plecare;
    string intarziere;
    string estimare_plecare;
};

struct trenuri {
    string id_tren;
    string ruta;
    string status_plecare;
    string status_sosire;
    string intarziere;
    string plecari_urmatoare;
    string sosiri_urmatoare;
    string estimare_sosire;
    std::vector<statii> statie;
};

bool validare_ora(const std::string &time) {
    int hour, minute;

    if (sscanf(time.c_str(), "%2d:%2d", &hour, &minute) != 2) {
        cout << "[server] Eroare la validarea orei: \n" << time;
        return false;
    }

    return !(hour < 0 || hour > 23 || minute < 0 || minute > 59);
}

//validare ora intarziere(poate fi si negativa daca trenul ajunge mai devreme)
bool validare_ora_intarziere(const std::string &timee) {
    int hour, minute;
    string time = timee;

    if (time[0] == '-') {
        time.erase(0, 1);
    }

    if (sscanf(time.c_str(), "%2d:%2d", &hour, &minute) != 2) return false;
    return !(hour < 0 || hour > 23 || minute < 0 || minute > 59);
}

bool validare_alfabet(string nume) {
    for (int i = 0; i < nume.length(); i++) {
        if (!std::isalpha(nume[i])) {
            return false;
        }
    }

    return true;
}

bool validare_ruta(const std::string &ruta) {
    size_t pos = ruta.find('-');

    if (pos == std::string::npos || pos == 0 || pos == ruta.length() - 1) {
        return false;
    }

    if (ruta.find('-', pos + 1) != std::string::npos) {
        return false;
    }

    std::string substr1 = ruta.substr(0, pos);
    std::string substr2 = ruta.substr(pos + 1);

    if (validare_alfabet(substr1) == false) {
        return false;
    }
    if (validare_alfabet(substr2) == false) {
        return false;
    }

    return true;
}

// functia care valideaza o comanda
bool validare_comanda(const std::string &comanda_full, std::string &raspuns, trenuri &tren, statii &statie,
                      int &id_comanda) {
    if (comanda_full.empty()) {
        raspuns =
                "[server] Nu s-a inserat o comanda! Pentru a consulta lista comenzilor valide, folositi comanda: \"help\".";
        return false;
    }

    std::vector<std::string> parametri;
    size_t start = 0, end = 0;

    //parsare comanda in cuvinte
    while ((end = comanda_full.find(' ', start)) != std::string::npos) {
        parametri.push_back(comanda_full.substr(start, end - start));
        start = end + 1;
    }

    parametri.push_back(comanda_full.substr(start));


    if (comanda_full.find("adaugare tren:") == 0) {
        if (parametri.size() == 7) {
            if (validare_ora(parametri[4]) && validare_ora(parametri[5]) && validare_ora_intarziere(parametri[6]) && (
                    (parametri[3].length() >= 3) && validare_ruta(parametri[3]))) {
                id_comanda = 1;
                tren.id_tren = parametri[2];
                tren.ruta = parametri[3];
                tren.status_plecare = parametri[4];
                tren.status_sosire = parametri[5];
                tren.intarziere = parametri[6];

                raspuns = "[server] Comanda este valida si e in curs de procesata. ";
                return true;
            }
            raspuns =
                    "[server] Argumentele comenzii nu sunt valide; Foloseste comanda \"help\" pentru mai multe detalii; ";
            return false;
        }
        raspuns = "[server] Comanda necesita 7 argumente";
        return false;
    }


    if (comanda_full.find("adaugare statie:") == 0) {
        if (parametri.size() == 7) {
            if (validare_ora(parametri[4]) && validare_ora(parametri[5]) && validare_ora_intarziere(parametri[6]) &&
                validare_alfabet(parametri[3])) {
                id_comanda = 2;
                statie.id_tren = parametri[2];
                statie.nume = parametri[3];
                statie.status_sosire = parametri[4];
                statie.status_plecare = parametri[5];
                statie.intarziere = parametri[6];
                raspuns = "[server] Comanda este valida si e in curs de procesata. ";
                return true;
            }
            raspuns =
                    "[server] Argumentele comenzii nu sunt valide; Foloseste comanda \"help\" pentru mai multe detalii; ";


            return false;
        }
        raspuns = "Comanda necesita 7 argumente.";
        return false;
    }


    if (comanda_full.find("modificare statie:") == 0) {
        if (parametri.size() == 7) {
            if (validare_ora(parametri[4]) && validare_ora(parametri[5]) && validare_ora_intarziere(parametri[6]) &&
                validare_alfabet(parametri[3])) {
                id_comanda = 3;
                statie.id_tren = parametri[2];
                statie.nume = parametri[3];
                statie.status_sosire = parametri[4];
                statie.status_plecare = parametri[5];
                statie.intarziere = parametri[6];
                cout << "este bine";
                raspuns = "[server] Comanda este valida si e in curs de procesata. ";
                return true;
            }
            raspuns =
                    "[server] Argumentele comenzii nu sunt valide; Foloseste comanda \"help\" pentru mai multe detalii; ";


            return false;
        }
        raspuns = "Comanda necesita 6 argumente: ";
        return false;
    }


    if (comanda_full.find("status ruta tren:") == 0) {
        if (parametri.size() == 4) {
            id_comanda = 4;
            tren.id_tren = parametri[3];
            raspuns = "[server] Comanda este valida si e in curs de procesata. ";
            return true;
        }
        raspuns = "Comanda necesita 4 argumente: ";
        return false;
    }

    if (comanda_full.find("status statie:") == 0) {
        if (parametri.size() == 4) {
            if (validare_alfabet(parametri[3])) {
                id_comanda = 5;
                statie.id_tren = parametri[2];
                statie.nume = parametri[3];
                raspuns = "[server] Comanda este valida si e in curs de procesata. ";
                return true;
            }
            raspuns =
                    "[server] Argumentele comenzii nu sunt valide; Foloseste comanda \"help\" pentru mai multe detalii; ";


            return false;
        }
        raspuns = "Comanda necesita 4 argumente: ";
        return false;
    }

    if (comanda_full.find("actualizare tren:") == 0) {
        if (parametri.size() == 7) {
            if (validare_ora(parametri[4]) && validare_ora(parametri[5]) && validare_ora_intarziere(parametri[6]) &&
                validare_ruta(parametri[3])) {
                id_comanda = 6;
                tren.id_tren = parametri[2];
                tren.ruta = parametri[3];
                tren.status_plecare = parametri[4];
                tren.status_sosire = parametri[5];
                tren.intarziere = parametri[6];
                raspuns = "[server] Comanda este valida si e in curs de procesata. ";
                return true;
            }
            raspuns =
                    "[server] Argumentele comenzii nu sunt valide; Foloseste comanda \"help\" pentru mai multe detalii; ";


            return false;
        }
        raspuns = "Comanda necesita 7 argumente: ";
        return false;
    }


    if (comanda_full.find("actualizare status plecare:") == 0) {
        if (parametri.size() == 5) {
            if (validare_ora(parametri[4])) {
                id_comanda = 7;
                tren.id_tren = parametri[3];
                tren.status_plecare = parametri[4];
                raspuns = "[server] Comanda este valida si e in curs de procesata. ";
                return true;
            }
            raspuns =
                    "[server] Argumentele comenzii nu sunt valide; Foloseste comanda \"help\" pentru mai multe detalii; ";


            return false;
        }
        raspuns = "Comanda necesita 5 argumente: ";
        return false;
    }


    if (comanda_full.find("actualizare status sosire:") == 0) {
        if (parametri.size() == 5) {
            if (validare_ora(parametri[4])) {
                id_comanda = 8;
                tren.id_tren = parametri[3];
                tren.status_sosire = parametri[4];
                raspuns = "[server] Comanda este valida si e in curs de procesata. ";

                return true;
            }
            raspuns =
                    "[server] Argumentele comenzii nu sunt valide; Foloseste comanda \"help\" pentru mai multe detalii; ";


            return false;
        }
        raspuns = "Comanda necesita 5 argumente: ";
        return false;
    }


    if (comanda_full.find("actualizare intarziere tren:") == 0) {
        if (parametri.size() == 5) {
            if (validare_ora_intarziere(parametri[4])) {
                id_comanda = 9;
                tren.id_tren = parametri[3];
                tren.intarziere = parametri[4];
                raspuns = "[server] Comanda este valida si e in curs de procesata. ";

                return true;
            }
            raspuns =
                    "[server] Argumentele comenzii nu sunt valide; Foloseste comanda \"help\" pentru mai multe detalii; ";


            return false;
        }
        raspuns = "Comanda necesita 5 argumente: ";
        return false;
    }

    if (comanda_full.find("plecari urmatoarea ora:") == 0) {
        if (parametri.size() == 4) {
            if (validare_ora(parametri[3])) {
                id_comanda = 10;
                tren.plecari_urmatoare = parametri[3];
                raspuns = "[server] Comanda este valida si e in curs de procesata. ";

                return true;
            }
            raspuns =
                    "[server] Argumentele comenzii nu sunt valide; Foloseste comanda \"help\" pentru mai multe detalii; ";


            return false;
        }
        raspuns = "Comanda necesita 4 argumente: ";
    }

    if (comanda_full.find("sosiri urmatoarea ora:") == 0) {
        if (parametri.size() == 4) {
            if (validare_ora(parametri[3])) {
                id_comanda = 11;
                tren.sosiri_urmatoare = parametri[3];
                raspuns = "[server] Comanda este valida si e in curs de procesata. ";

                return true;
            }
            raspuns =
                    "[server] Argumentele comenzii nu sunt valide; Foloseste comanda \"help\" pentru mai multe detalii; ";


            return false;
        }
        raspuns = "Comanda necesita 4 argumente: ";
    }

    if (comanda_full == "intarzierea trenurilor") {
        id_comanda = 12;
        raspuns = "[server] Comanda este valida si e in curs de procesata. ";

        return true;
    }

    if (comanda_full.find("intarziere:") == 0) {
        if (parametri.size() == 2) {
            raspuns = "[server] Comanda este valida si e in curs de procesata. ";

            id_comanda = 13;
            tren.id_tren = parametri[1];
            return true;
        }

        raspuns = "Comanda necesita 2 argumente: ";
        return false;
    }


    if (comanda_full == "mersul trenurilor") {
        id_comanda = 14;
        raspuns = "[server] Comanda este valida si e in curs de procesata. ";

        return true;
    }

    if (comanda_full == "ruta trenurilor") {
        id_comanda = 15;
        raspuns = "[server] Comanda este valida si e in curs de procesata. ";

        return true;
    }


    if (comanda_full == "help") {
        id_comanda = 16;
        raspuns = "[server] Comanda este valida si e in curs de procesata. ";

        return true;
    }

    if (comanda_full == "quit") {
        id_comanda = 17;
        raspuns = "[server] Comanda este valida si e in curs de procesata. ";

        raspuns = "quit";
        return true;
    }
    if (comanda_full.find("statii tren:") == 0) {
        if (parametri.size() == 3) {
            raspuns = "[server] Comanda este valida si e in curs de procesata. ";

            id_comanda = 18;
            tren.id_tren = parametri[2];
            return true;
        }

        raspuns = "Comanda necesita 2 argumente: ";
        return false;
    }
    raspuns =
            "Comanda nu este recunoscuta. Pentru a consulta lista comenzilor valide si observatiile parametrilor acestora, folositi comanda: \"help\".";
    return false;
}

//verificam daca fisierul xml exista
bool file_exists(const std::string &filename) {
    struct stat buffer;
    return (stat(filename.c_str(), &buffer) == 0);
}

// creare fisier xml
void create_xml_file(const std::string &filename) {
    // creare document
    xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
    xmlNodePtr root_node = xmlNewNode(nullptr, BAD_CAST "trenuri");

    xmlDocSetRootElement(doc, root_node);

    // salvare
    if (xmlSaveFormatFileEnc(filename.c_str(), doc, "UTF-8", 1) == -1) {
        std::cerr << "Eroare la crearea fisierului XML: " << filename << std::endl;
    } else {
        std::cout << "Fisierul XML a fost creat: " << filename << std::endl;
    }


    xmlFreeDoc(doc);
    xmlCleanupParser();
}

// verificare existenta si creare fisier xml
void verifica_si_creeaza_fisier(const std::string &filename) {
    if (!file_exists(filename)) {
        std::cout << "Fisierul XML nu exista. Creare fisier..." << std::endl;
        create_xml_file(filename);
    }
}

bool mesaj_to_char(int fd, std::string &comanda_full) {
    char msg[MAX_COMMAND_LENGTH];
    int total_bytes_read = 0;
    int bytes;

    // citim datele pana la sfarsitul mesajului (NULL terminator sau EOF)
    while ((bytes = read(fd, msg + total_bytes_read, sizeof(msg) - total_bytes_read - 1)) > 0) {
        total_bytes_read += bytes;

        if (total_bytes_read >= sizeof(msg) - 1) {
            std::cerr << "Eroare: mesajul este prea lung pentru buffer." << std::endl;
            return false;
        }

        if (msg[total_bytes_read - 1] == '\n' || msg[total_bytes_read - 1] == '\0') {
            break;
        }
    }

    if (bytes < 0) {
        perror("Eroare la citire de la client.");
        return false;
    }

    if (bytes == 0) {
        return false;
    }


    msg[total_bytes_read] = '\0';
    comanda_full = std::string(msg);
    return true;
}

bool id_existent(xmlNodePtr root, const std::string &id_tren) {
    xmlNodePtr trenNode = root->xmlChildrenNode;

    while (trenNode) {
        if (trenNode->type == XML_ELEMENT_NODE) {
            if (xmlStrEqual(trenNode->name, BAD_CAST "tren")) {
                xmlChar *id_attr = xmlGetProp(trenNode, BAD_CAST "id");
                if (id_attr && strcmp((const char *) id_attr, id_tren.c_str()) == 0) {
                    xmlFree(id_attr);
                    return true;
                }

                xmlFree(id_attr);
            }
        }
        trenNode = trenNode->next;
    }

    //trenul nu a fost gasit
    return false;
}

bool statie_existent(xmlNodePtr trenNode, const std::string &nume_statie) {
    xmlNodePtr statieNode = trenNode->xmlChildrenNode;

    while (statieNode) {
        if (statieNode->type == XML_ELEMENT_NODE && xmlStrEqual(statieNode->name, BAD_CAST "statie")) {
            xmlNodePtr numeNode = statieNode->xmlChildrenNode;

            while (numeNode) {
                if (numeNode->type == XML_ELEMENT_NODE && xmlStrEqual(numeNode->name, BAD_CAST "nume")) {
                    std::string nume_statie_existent = (const char *) xmlNodeGetContent(numeNode);
                    if (nume_statie_existent == nume_statie) {
                        return true;
                    }
                }

                numeNode = numeNode->next;
            }
        }
        statieNode = statieNode->next;
    }
    return false;
}

std::string trim_whitespace(const std::string &str) {
    size_t first = str.find_first_not_of(' ');

    if (first == std::string::npos) return "";

    size_t last = str.find_last_not_of(' ');

    return str.substr(first, (last - first + 1));
}

bool adauga_tren(xmlDocPtr doc, xmlNodePtr root, trenuri &tren) {
    if (id_existent(root, tren.id_tren)) {
        std::cerr << "Eroare: Trenul cu ID-ul " << tren.id_tren << " exista deja.\n";
        return false;
    }

    int x1 = 0, y1 = 0, x2 = 0, y2 = 0, x3 = 0, y3 = 0;
    string_to_ora(tren.status_sosire, x1, y1);
    string_to_ora(tren.intarziere, x2, y2);

    x3 = x1 + x2;
    y3 = y1 + y2;
    if (y3 >= 60) {
        x3 += 1;
        y3 = y3 % 60;
    }

    string str_1 = to_string(x3);
    string str_2 = to_string(y3);
    tren.estimare_sosire = str_1 + ":" + str_2;


    xmlNodePtr trenNode = xmlNewChild(root, NULL, BAD_CAST "tren", NULL);


    xmlNewProp(trenNode, BAD_CAST "id", BAD_CAST tren.id_tren.c_str());
    xmlNewProp(trenNode, BAD_CAST "ruta", BAD_CAST tren.ruta.c_str());
    xmlNewProp(trenNode, BAD_CAST "status_plecare", BAD_CAST tren.status_plecare.c_str());
    xmlNewProp(trenNode, BAD_CAST "status_sosire", BAD_CAST tren.status_sosire.c_str());
    xmlNewProp(trenNode, BAD_CAST "intarziere", BAD_CAST tren.intarziere.c_str());
    xmlNewProp(trenNode, BAD_CAST "estimare_sosire", BAD_CAST tren.estimare_sosire.c_str());


    for (const auto &statie: tren.statie) {
        if (statie_existent(trenNode, statie.nume)) {
            std::cerr << "Eroare: Stația cu numele " << statie.nume << " există deja în trenul cu ID-ul " << tren.
                    id_tren << ".\n";
            continue;
        }

        xmlNodePtr statieNode = xmlNewChild(trenNode, NULL, BAD_CAST "statie", NULL);
        xmlNewProp(statieNode, BAD_CAST "nume", BAD_CAST statie.nume.c_str());
        xmlNewProp(statieNode, BAD_CAST "status_sosire", BAD_CAST statie.status_sosire.c_str());
        xmlNewProp(statieNode, BAD_CAST "status_plecare", BAD_CAST statie.status_plecare.c_str());
        xmlNewProp(statieNode, BAD_CAST "intarziere", BAD_CAST statie.intarziere.c_str());
        xmlNewProp(statieNode, BAD_CAST "estimare_plecare", BAD_CAST statie.estimare_plecare.c_str());
    }
    return true;
}

//adaugare tren
bool comanda_1(string &comanda, trenuri &tren, int client_fd) {
    verifica_si_creeaza_fisier("trenuri.xml");

    xmlDocPtr doc = xmlParseFile(nume_xml.c_str());

    if (doc == NULL) {
        std::cerr << "Eroare: Nu am putut deschide fisierul XML." << nume_xml << std::endl;
        string aux = "[server] Operatia \"" + comanda + "\" a esuat!\n";
        send(client_fd, aux.c_str(), aux.size() + 1, 0);
        return false;
    }


    xmlNodePtr root = xmlDocGetRootElement(doc);

    if (root == NULL) {
        std::cerr << "Eroare: Fisierul XML nu are radacina.\n";
        xmlFreeDoc(doc);
        string aux = "[server] Operatia \"" + comanda + "\" a esuat!\n";

        send(client_fd, aux.c_str(), aux.size() + 1, 0);
        return false;
    }

    if (adauga_tren(doc, root, tren) == false) {
        string aux = "[server] Operatia \"" + comanda + "\" a esuat, trenul exista in baza de date!\n";

        send(client_fd, aux.c_str(), aux.size() + 1, 0);
        return false;
    }


    if (xmlSaveFormatFileEnc(nume_xml.c_str(), doc, "UTF-8", 1) == -1) {
        std::cerr << "Eroare: Nu am putut salva fisierul XML.\n";
        xmlFreeDoc(doc);
        string aux = "[server] Operatia \"" + comanda + "\" a esuat!\n";
        send(client_fd, aux.c_str(), aux.size() + 1, 0);
        return false;
    }


    xmlFreeDoc(doc);
    xmlCleanupParser();

    string aux = "[server] Operatia \"" + comanda + "\" s-a afectuat cu succes!\n";
    send(client_fd, aux.c_str(), aux.size() + 1, 0);

    return true;
}

//adaugare statie
bool comanda_2(string &comanda, statii &statie, int client_fd) {
    verifica_si_creeaza_fisier("trenuri.xml");

    xmlDocPtr doc = xmlParseFile("trenuri.xml");

    if (doc == NULL) {
        std::cerr << "Eroare: Nu am putut deschide fisierul XML.\n";
        string aux = "[server] Operatia \"" + comanda + "\" a esuat!\n";
        send(client_fd, aux.c_str(), aux.size() + 1, 0);
        return false;
    }


    xmlNodePtr root = xmlDocGetRootElement(doc);

    if (root == NULL) {
        std::cerr << "Eroare: Fisierul XML nu are radacina.\n";
        xmlFreeDoc(doc);
        string aux = "[server] Operatia \"" + comanda + "\" a esuat!\n";
        send(client_fd, aux.c_str(), aux.size() + 1, 0);
        return false;
    }


    xmlNodePtr trenNode = root->xmlChildrenNode;

    int x1 = 0, y1 = 0, x2 = 0, y2 = 0, x3 = 0, y3 = 0;
    string_to_ora(statie.status_plecare, x1, y1);
    string_to_ora(statie.intarziere, x2, y2);

    x3 = x1 + x2;
    y3 = y1 + y2;
    if (y3 >= 60) {
        x3 += 1;
        y3 = y3 % 60;
    }

    string str_1 = to_string(x3);
    string str_2 = to_string(y3);
    statie.estimare_plecare = str_1 + ":" + str_2;

    while (trenNode) {
        if (trenNode->type == XML_ELEMENT_NODE && xmlStrEqual(trenNode->name, BAD_CAST "tren")) {
            xmlChar *id_attr = xmlGetProp(trenNode, BAD_CAST "id");
            if (id_attr && strcmp((const char *) id_attr, statie.id_tren.c_str()) == 0) {
                if (statie_existent(trenNode, statie.nume)) {
                    std::cerr << "Eroare: Statia " << statie.nume << " exista deja în trenul cu ID-ul " << statie.
                            id_tren << ".\n";
                    string aux = "[server] Operatia \"" + comanda + "\" a esuat! Statia deja exista.\n";
                    send(client_fd, aux.c_str(), aux.size() + 1, 0);
                    xmlFree(id_attr);
                    xmlFreeDoc(doc);

                    return false;
                }


                xmlNodePtr statieNode = xmlNewNode(NULL, BAD_CAST "statie");

                xmlNodePtr numeNode = xmlNewChild(statieNode, NULL, BAD_CAST "nume", BAD_CAST statie.nume.c_str());
                xmlNewProp(statieNode, BAD_CAST "status_sosire", BAD_CAST statie.status_sosire.c_str());
                xmlNewProp(statieNode, BAD_CAST "status_plecare", BAD_CAST statie.status_plecare.c_str());
                xmlNewProp(statieNode, BAD_CAST "intarziere", BAD_CAST statie.intarziere.c_str());
                xmlNewProp(statieNode, BAD_CAST "estimare_plecare", BAD_CAST statie.estimare_plecare.c_str());

                // adaugam statia le tren
                xmlAddChild(trenNode, statieNode);


                if (xmlSaveFormatFileEnc("trenuri.xml", doc, "UTF-8", 1) == -1) {
                    std::cerr << "Eroare: Nu am putut salva fisierul XML.\n";
                    xmlFree(id_attr);
                    xmlFreeDoc(doc);
                    return false;
                }


                string aux = "[server] Operatia \"" + comanda + "\" s-a efectuat cu succes!\n";
                send(client_fd, aux.c_str(), aux.size() + 1, 0);

                xmlFree(id_attr);
                xmlFreeDoc(doc);
                return true;
            }
            xmlFree(id_attr);
        }
        trenNode = trenNode->next;
    }


    std::cerr << "Eroare: Trenul cu ID-ul " << statie.id_tren << " nu a fost gasit.\n";
    xmlFreeDoc(doc);
    string aux = "[server] Operatia \"" + comanda +
                 "\" a esuat, trenul nu se afla in baza de date pentru a adauga statia!\n";
    send(client_fd, aux.c_str(), aux.size() + 1, 0);
    return false;
}

//modificare statie
bool comanda_3(string &comanda, statii &statie, int client_fd) {
    verifica_si_creeaza_fisier("trenuri.xml");

    xmlDocPtr doc = xmlParseFile("trenuri.xml");
    if (doc == NULL) {
        std::cerr << "Eroare: Nu am putut deschide fisierul XML.\n";
        return false;
    }


    xmlNodePtr root = xmlDocGetRootElement(doc);

    if (root == NULL) {
        std::cerr << "Eroare: Fisierul XML nu are radacina.\n";
        xmlFreeDoc(doc);
        return false;
    }

    int x1 = 0, y1 = 0, x2 = 0, y2 = 0, x3 = 0, y3 = 0;
    string_to_ora(statie.status_plecare, x1, y1);
    string_to_ora(statie.intarziere, x2, y2);

    x3 = x1 + x2;
    y3 = y1 + y2;
    if (y3 >= 60) {
        x3 += 1;
        y3 = y3 % 60;
    }

    string str_1 = to_string(x3);
    string str_2 = to_string(y3);
    statie.estimare_plecare = str_1 + ":" + str_2;


    xmlNodePtr trenNode = root->xmlChildrenNode;
    while (trenNode) {
        if (trenNode->type == XML_ELEMENT_NODE && xmlStrEqual(trenNode->name, BAD_CAST "tren")) {
            xmlChar *id_attr = xmlGetProp(trenNode, BAD_CAST "id");

            if (id_attr && strcmp((const char *) id_attr, statie.id_tren.c_str()) == 0) {
                xmlNodePtr statieNode = trenNode->xmlChildrenNode;

                while (statieNode) {
                    if (statieNode->type == XML_ELEMENT_NODE && xmlStrEqual(statieNode->name, BAD_CAST "statie")) {
                        xmlNodePtr numeNode = statieNode->xmlChildrenNode;
                        while (numeNode) {
                            if (numeNode->type == XML_ELEMENT_NODE && xmlStrEqual(numeNode->name, BAD_CAST "nume")) {
                                std::string nume_statie_existent = (const char *) xmlNodeGetContent(numeNode);

                                if (trim_whitespace(nume_statie_existent) == trim_whitespace(statie.nume)) {
                                    // modificare statie
                                    xmlSetProp(statieNode, BAD_CAST "status_sosire",
                                               BAD_CAST statie.status_sosire.c_str());
                                    xmlSetProp(statieNode, BAD_CAST "status_plecare",
                                               BAD_CAST statie.status_plecare.c_str());
                                    xmlSetProp(statieNode, BAD_CAST "intarziere", BAD_CAST statie.intarziere.c_str());
                                    xmlSetProp(statieNode, BAD_CAST "estimare_plecare",
                                               BAD_CAST statie.estimare_plecare.c_str());


                                    if (xmlSaveFormatFileEnc("trenuri.xml", doc, "UTF-8", 1) == -1) {
                                        std::cerr << "Eroare: Nu am putut salva fisierul XML.\n";
                                        xmlFree(id_attr);
                                        xmlFreeDoc(doc);
                                        return false;
                                    }


                                    string aux = "[server] Operatia \"" + comanda + "\" s-a efectuat cu succes!\n";
                                    send(client_fd, aux.c_str(), aux.size() + 1, 0);

                                    xmlFree(id_attr);
                                    xmlFreeDoc(doc);
                                    return true;
                                }
                            }
                            numeNode = numeNode->next;
                        }
                    }
                    statieNode = statieNode->next;
                }

                std::cerr << "Eroare: Statia cu numele " << statie.nume << " nu a fost gasita în trenul cu ID-ul " <<
                        statie.id_tren << ".\n";
                string aux = "[server] Operatia \"" + comanda +
                             "\" a esuat, statia nu a fost gasita pe ruta trenului!\n";
                send(client_fd, aux.c_str(), aux.size() + 1, 0);
                xmlFree(id_attr);
                xmlFreeDoc(doc);
                return false;
            }
            xmlFree(id_attr);
        }
        trenNode = trenNode->next;
    }

    std::cerr << "Eroare: Trenul cu ID-ul " << statie.id_tren << " nu a fost gasit.\n";

    string aux = "[server] Operatia \"" + comanda + "\" a esuat, trenul nu a fost gasit!\n";
    send(client_fd, aux.c_str(), aux.size() + 1, 0);

    xmlFreeDoc(doc);
    return false;
}

//status ruta tren
bool comanda_4(string &comanda, trenuri &tren, int client_fd) {
    verifica_si_creeaza_fisier("trenuri.xml");

    xmlDocPtr doc = xmlParseFile("trenuri.xml");
    if (doc == NULL) {
        send(client_fd, "Eroare la deschiderea fisierului XML.\n", 39, 0);
        return false;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);


    if (!id_existent(root, tren.id_tren)) {
        send(client_fd, "[server] Trenul cu ID-ul specificat nu a fost gasit.\n", 44, 0);
        xmlFreeDoc(doc);
        return false;
    }


    xmlNodePtr trenNode = root->xmlChildrenNode;

    while (trenNode) {
        if (trenNode->type == XML_ELEMENT_NODE && xmlStrEqual(trenNode->name, BAD_CAST "tren")) {
            xmlChar *id_attr = xmlGetProp(trenNode, BAD_CAST "id");

            if (id_attr && strcmp((const char *) id_attr, tren.id_tren.c_str()) == 0) {
                string result = "Tren ID: " + string((const char *) id_attr) + "\n";
                result += "Ruta: " + string((const char *) xmlGetProp(trenNode, BAD_CAST "ruta")) + "\n";
                result += "Status Plecare: " + string((const char *) xmlGetProp(trenNode, BAD_CAST "status_plecare")) +
                        "\n";
                result += "Status Sosire: " + string((const char *) xmlGetProp(trenNode, BAD_CAST "status_sosire")) +
                        "\n";
                result += "Intarziere: " + string((const char *) xmlGetProp(trenNode, BAD_CAST "intarziere")) + "\n";

                // afisare statii
                xmlNodePtr statieNode = trenNode->xmlChildrenNode;
                result += "Statii:\n";
                while (statieNode) {
                    if (statieNode->type == XML_ELEMENT_NODE && xmlStrEqual(statieNode->name, BAD_CAST "statie")) {
                        xmlNodePtr numeNode = statieNode->xmlChildrenNode;
                        while (numeNode) {
                            if (numeNode->type == XML_ELEMENT_NODE && xmlStrEqual(numeNode->name, BAD_CAST "nume")) {
                                result += " - " + string((const char *) xmlNodeGetContent(numeNode)) + "\n";
                            }
                            numeNode = numeNode->next;
                        }
                    }
                    statieNode = statieNode->next;
                }

                send(client_fd, result.c_str(), result.size() + 1, 0);
                xmlFree(id_attr);
                xmlFreeDoc(doc);
                return true;
            }
            xmlFree(id_attr);
        }
        trenNode = trenNode->next;
    }

    send(client_fd, "[server] Trenul cu ID-ul specificat nu a fost gasit.\n", 44, 0);
    xmlFreeDoc(doc);
    return false;
}

//status statie tren
bool comanda_5(string &comanda, statii &statie, int client_fd) {
    verifica_si_creeaza_fisier("trenuri.xml");

    xmlDocPtr doc = xmlParseFile("trenuri.xml");

    if (doc == NULL) {
        send(client_fd, "Eroare la deschiderea fisierului XML.\n", 39, 0);
        return false;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    xmlNodePtr trenNode = root->xmlChildrenNode;

    while (trenNode) {
        if (trenNode->type == XML_ELEMENT_NODE && xmlStrEqual(trenNode->name, BAD_CAST "tren")) {
            xmlChar *id_attr = xmlGetProp(trenNode, BAD_CAST "id");
            if (id_attr && strcmp((const char *) id_attr, statie.id_tren.c_str()) == 0) {
                string result = "[server] Tren ID: " + string((const char *) id_attr) + "\n";


                xmlNodePtr statieNode = trenNode->xmlChildrenNode;
                while (statieNode) {
                    if (statieNode->type == XML_ELEMENT_NODE && xmlStrEqual(statieNode->name, BAD_CAST "statie")) {
                        xmlNodePtr numeNode = statieNode->xmlChildrenNode;

                        while (numeNode) {
                            if (numeNode->type == XML_ELEMENT_NODE && xmlStrEqual(numeNode->name, BAD_CAST "nume")) {
                                std::string nume_statie_existent = (const char *) xmlNodeGetContent(numeNode);
                                if (nume_statie_existent == statie.nume) {
                                    result += "Statie: " + nume_statie_existent + "\n";
                                    result += "Status Sosire: " + string(
                                        (const char *) xmlGetProp(statieNode, BAD_CAST "status_sosire")) + "\n";
                                    result += "Status Plecare: " + string(
                                        (const char *) xmlGetProp(statieNode, BAD_CAST "status_plecare")) + "\n";
                                    result += "Intarziere: " + string(
                                        (const char *) xmlGetProp(statieNode, BAD_CAST "intarziere")) + "\n";
                                    result += "Estimare Plecare: " + string(
                                        (const char *) xmlGetProp(statieNode, BAD_CAST "estimare_plecare")) + "\n";

                                    send(client_fd, result.c_str(), result.size() + 1, 0);
                                    xmlFree(id_attr);
                                    xmlFreeDoc(doc);
                                    return true;
                                }
                            }
                            numeNode = numeNode->next;
                        }
                    }
                    statieNode = statieNode->next;
                }
            }
            xmlFree(id_attr);
        }
        trenNode = trenNode->next;
    }

    send(client_fd, "[server] Statia nu a fost gasita în trenul cu ID-ul specificat.\n", 56, 0);
    xmlFreeDoc(doc);
    return false;
}

//actualizare tren
bool comanda_6(string &comanda, trenuri &tren, int client_fd) {
    verifica_si_creeaza_fisier("trenuri.xml");


    xmlDocPtr doc = xmlParseFile("trenuri.xml");
    if (doc == NULL) {
        send(client_fd, "Eroare la deschiderea fisierului XML.\n", 33, 0);
        return false;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    xmlNodePtr trenNode = root->xmlChildrenNode;

    while (trenNode) {
        if (trenNode->type == XML_ELEMENT_NODE && xmlStrEqual(trenNode->name, BAD_CAST "tren")) {
            xmlChar *id_attr = xmlGetProp(trenNode, BAD_CAST "id");

            if (id_attr && strcmp((const char *) id_attr, tren.id_tren.c_str()) == 0) {
                int x1 = 0, y1 = 0, x2 = 0, y2 = 0, x3 = 0, y3 = 0;
                string_to_ora(tren.status_sosire, x1, y1);
                string_to_ora(tren.intarziere, x2, y2);

                x3 = x1 + x2;
                y3 = y1 + y2;
                if (y3 >= 60) {
                    x3 += 1;
                    y3 = y3 % 60;
                }

                string str_1 = to_string(x3);
                string str_2 = to_string(y3);
                tren.estimare_sosire = str_1 + ":" + str_2;

                xmlSetProp(trenNode, BAD_CAST "ruta", BAD_CAST tren.ruta.c_str());
                xmlSetProp(trenNode, BAD_CAST "status_plecare", BAD_CAST tren.status_plecare.c_str());
                xmlSetProp(trenNode, BAD_CAST "status_sosire", BAD_CAST tren.status_sosire.c_str());
                xmlSetProp(trenNode, BAD_CAST "intarziere", BAD_CAST tren.intarziere.c_str());
                xmlSetProp(trenNode, BAD_CAST "estimare_sosire", BAD_CAST tren.estimare_sosire.c_str());

                if (xmlSaveFormatFileEnc("trenuri.xml", doc, "UTF-8", 1) == -1) {
                    send(client_fd, "[server] Eroare la salvarea fisierului XML.\n", 34, 0);
                    xmlFree(id_attr);
                    xmlFreeDoc(doc);
                    return false;
                }

                send(client_fd, "[server] Trenul a fost actualizat cu succes.\n", 35, 0);
                xmlFree(id_attr);
                xmlFreeDoc(doc);
                return true;
            }
            xmlFree(id_attr);
        }
        trenNode = trenNode->next;
    }

    send(client_fd, "[server] Trenul cu ID-ul specificat nu a fost gasit.\n", 44, 0);
    xmlFreeDoc(doc);
    return false;
}

//actualizare status plecare
bool comanda_7(string &comanda, trenuri &tren, int client_fd) {
    verifica_si_creeaza_fisier("trenuri.xml");


    xmlDocPtr doc = xmlParseFile("trenuri.xml");

    if (doc == NULL) {
        send(client_fd, "[server] Eroare la deschiderea fisierului XML.\n", 33, 0);
        return false;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    xmlNodePtr trenNode = root->xmlChildrenNode;

    while (trenNode) {
        if (trenNode->type == XML_ELEMENT_NODE && xmlStrEqual(trenNode->name, BAD_CAST "tren")) {
            xmlChar *id_attr = xmlGetProp(trenNode, BAD_CAST "id");
            if (id_attr && strcmp((const char *) id_attr, tren.id_tren.c_str()) == 0) {
                xmlSetProp(trenNode, BAD_CAST "status_plecare", BAD_CAST tren.status_plecare.c_str());


                if (xmlSaveFormatFileEnc("trenuri.xml", doc, "UTF-8", 1) == -1) {
                    send(client_fd, "[server] Eroare la salvarea fisierului XML.\n", 34, 0);
                    xmlFree(id_attr);
                    xmlFreeDoc(doc);
                    return false;
                }

                send(client_fd, "[server] Status plecare actualizat cu succes.\n", 37, 0);
                xmlFree(id_attr);
                xmlFreeDoc(doc);
                return true;
            }
            xmlFree(id_attr);
        }
        trenNode = trenNode->next;
    }

    send(client_fd, "[server] Trenul cu ID-ul specificat nu a fost gasit.\n", 44, 0);
    xmlFreeDoc(doc);
    return false;
}

//actualizare status sosire
bool comanda_8(string &comanda, trenuri &tren, int client_fd) {
    verifica_si_creeaza_fisier("trenuri.xml");


    xmlDocPtr doc = xmlParseFile("trenuri.xml");

    if (doc == NULL) {
        send(client_fd, "[server] Eroare la deschiderea fisierului XML.\n", 33, 0);
        return false;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    xmlNodePtr trenNode = root->xmlChildrenNode;

    while (trenNode) {
        if (trenNode->type == XML_ELEMENT_NODE && xmlStrEqual(trenNode->name, BAD_CAST "tren")) {
            xmlChar *id_attr = xmlGetProp(trenNode, BAD_CAST "id");
            if (id_attr && strcmp((const char *) id_attr, tren.id_tren.c_str()) == 0) {
                int x1 = 0, y1 = 0, x2 = 0, y2 = 0, x3 = 0, y3 = 0;
                string_to_ora(tren.status_sosire, x1, y1);
                xmlChar *intarziere_attr = xmlGetProp(trenNode, BAD_CAST "intarziere");
                string_to_ora((const char *) (intarziere_attr), x2, y2);

                x3 = x1 + x2;
                y3 = y1 + y2;
                if (y3 >= 60) {
                    x3 += 1;
                    y3 = y3 % 60;
                }

                string str_1 = to_string(x3);
                string str_2 = to_string(y3);
                tren.estimare_sosire = str_1 + ":" + str_2;

                xmlSetProp(trenNode, BAD_CAST "status_sosire", BAD_CAST tren.status_sosire.c_str());
                xmlSetProp(trenNode, BAD_CAST "estimare_sosire", BAD_CAST tren.estimare_sosire.c_str());


                if (xmlSaveFormatFileEnc("trenuri.xml", doc, "UTF-8", 1) == -1) {
                    send(client_fd, "[server] Eroare la salvarea fisierului XML.\n", 34, 0);
                    xmlFree(id_attr);
                    xmlFreeDoc(doc);
                    return false;
                }

                send(client_fd, "[server] Status sosire actualizat cu succes.\n", 36, 0);

                xmlFree(id_attr);
                xmlFreeDoc(doc);
                return true;
            }
            xmlFree(id_attr);
        }
        trenNode = trenNode->next;
    }

    send(client_fd, "[server] Trenul cu ID-ul specificat nu a fost gasit.\n", 44, 0);
    xmlFreeDoc(doc);
    return false;
}

//actualizare intarziere tren
bool comanda_9(string &comanda, trenuri &tren, int client_fd) {
    verifica_si_creeaza_fisier("trenuri.xml");


    xmlDocPtr doc = xmlParseFile("trenuri.xml");

    if (doc == NULL) {
        send(client_fd, "[server] Eroare la deschiderea fisierului XML.\n", 33, 0);
        return false;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    xmlNodePtr trenNode = root->xmlChildrenNode;

    while (trenNode) {
        if (trenNode->type == XML_ELEMENT_NODE && xmlStrEqual(trenNode->name, BAD_CAST "tren")) {
            xmlChar *id_attr = xmlGetProp(trenNode, BAD_CAST "id");
            if (id_attr && strcmp((const char *) id_attr, tren.id_tren.c_str()) == 0) {
                int x1 = 0, y1 = 0, x2 = 0, y2 = 0, x3 = 0, y3 = 0;
                xmlChar *plecare = xmlGetProp(trenNode, BAD_CAST "status_sosire");

                string_to_ora((const char *) plecare, x1, y1);
                string_to_ora((tren.intarziere), x2, y2);

                x3 = x1 + x2;
                y3 = y1 + y2;
                if (y3 >= 60) {
                    x3 += 1;
                    y3 = y3 % 60;
                }

                string str_1 = to_string(x3);
                string str_2 = to_string(y3);
                tren.estimare_sosire = str_1 + ":" + str_2;

                xmlSetProp(trenNode, BAD_CAST "intarziere", BAD_CAST tren.intarziere.c_str());
                xmlSetProp(trenNode, BAD_CAST "estimare_sosire", BAD_CAST tren.estimare_sosire.c_str());


                if (xmlSaveFormatFileEnc("trenuri.xml", doc, "UTF-8", 1) == -1) {
                    send(client_fd, "[server] Eroare la salvarea fisierului XML.\n", 34, 0);
                    xmlFree(id_attr);
                    xmlFreeDoc(doc);
                    return false;
                }

                send(client_fd, "[server] Intarziere actualizata cu succes.\n", 35, 0);
                xmlFree(id_attr);
                xmlFreeDoc(doc);
                return true;
            }
            xmlFree(id_attr);
        }
        trenNode = trenNode->next;
    }

    send(client_fd, "[server] Trenul cu ID-ul specificat nu a fost gasit.\n", 44, 0);
    xmlFreeDoc(doc);
    return false;
}

//plecari urm ora
bool comanda_10(string &comanda, trenuri &tren, int client_fd) {
    verifica_si_creeaza_fisier("trenuri.xml");


    xmlDocPtr doc = xmlParseFile("trenuri.xml");

    if (doc == NULL) {
        send(client_fd, "[server] Eroare la deschiderea fisierului XML.\n", 33, 0);
        return false;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    xmlNodePtr trenNode = root->xmlChildrenNode;

    string result = "[server] Plecari urmatoarea ora:\n";

    while (trenNode) {
        if (trenNode->type == XML_ELEMENT_NODE && xmlStrEqual(trenNode->name, BAD_CAST "tren")) {
            xmlChar *status_plecare_attr = xmlGetProp(trenNode, BAD_CAST "status_plecare");
            if (status_plecare_attr) {
                if (compara_ore((const char *) status_plecare_attr, tren.plecari_urmatoare) > 0) {
                    result += "ID Tren: " + std::string((const char *) xmlGetProp(trenNode, BAD_CAST "id")) + "\n";
                    result += "Ruta: " + std::string((const char *) xmlGetProp(trenNode, BAD_CAST "ruta")) + "\n";
                    result += "Status Plecare: " + std::string((const char *) status_plecare_attr) + "\n";
                    result += "\n";
                }
            }
            xmlFree(status_plecare_attr);
        }
        trenNode = trenNode->next;
    }

    if (result.size() == 0) {
        result = "[server] Nu sunt trenuri programate pentru plecare în urmatoarea ora.\n";
    }

    send(client_fd, result.c_str(), result.size() + 1, 0);
    xmlFreeDoc(doc);
    return true;
}

//sosiri urm ora
bool comanda_11(string &comanda, trenuri &tren, int client_fd) {
    verifica_si_creeaza_fisier("trenuri.xml");


    xmlDocPtr doc = xmlParseFile("trenuri.xml");

    if (doc == NULL) {
        send(client_fd, "[server] Eroare la deschiderea fisierului XML.\n", 33, 0);
        return false;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    xmlNodePtr trenNode = root->xmlChildrenNode;

    string result = "[server] Sosiri urmatoarea ora:\n";

    while (trenNode) {
        if (trenNode->type == XML_ELEMENT_NODE && xmlStrEqual(trenNode->name, BAD_CAST "tren")) {
            xmlChar *estimare_sosire_attr = xmlGetProp(trenNode, BAD_CAST "estimare_sosire");
            if (estimare_sosire_attr) {
                if (compara_ore((const char *) estimare_sosire_attr, tren.sosiri_urmatoare) > 0) {
                    result += "ID Tren: " + std::string((const char *) xmlGetProp(trenNode, BAD_CAST "id")) + "\n";
                    result += "Ruta: " + std::string((const char *) xmlGetProp(trenNode, BAD_CAST "ruta")) + "\n";
                    result += "Estimare Sosire: " + std::string((const char *) estimare_sosire_attr) + "\n";
                    result += "\n";
                }
            }
            xmlFree(estimare_sosire_attr);
        }
        trenNode = trenNode->next;
    }

    if (result.size() == 0) {
        result = "[server] Nu sunt trenuri programate sa soseasca în urmatoarea ora.\n";
    }

    send(client_fd, result.c_str(), result.size() + 1, 0);
    xmlFreeDoc(doc);
    return true;
}

//intarzierea trenurilor
bool comanda_12(string &comanda, trenuri &tren, int client_fd) {
    verifica_si_creeaza_fisier("trenuri.xml");

    xmlDocPtr doc = xmlParseFile("trenuri.xml");
    if (doc == NULL) {
        send(client_fd, "[server] Eroare la deschiderea fisierului XML.\n", 33, 0);
        return false;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    xmlNodePtr trenNode = root->xmlChildrenNode;

    string result = "[server] Intarzierile trenurilor:\n";

    while (trenNode) {
        if (trenNode->type == XML_ELEMENT_NODE && xmlStrEqual(trenNode->name, BAD_CAST "tren")) {
            xmlChar *intarziere_attr = xmlGetProp(trenNode, BAD_CAST "intarziere");

            if (intarziere_attr) {
                result += "ID Tren: " + std::string((const char *) xmlGetProp(trenNode, BAD_CAST "id")) + "\n";
                result += "Ruta: " + std::string((const char *) xmlGetProp(trenNode, BAD_CAST "ruta")) + "\n";
                result += "Intarziere: " + std::string((const char *) intarziere_attr) + "\n";
                result += "\n";
            }
            xmlFree(intarziere_attr);
        }
        trenNode = trenNode->next;
    }

    if (result.size() == 0) {
        result = "[server] Nu exista trenuri cu întarziere.\n";
    }

    send(client_fd, result.c_str(), result.size() + 1, 0);
    xmlFreeDoc(doc);
    return true;
}

//intarziere tren cu id
bool comanda_13(string &comanda, trenuri &tren, int client_fd) {
    verifica_si_creeaza_fisier("trenuri.xml");

    xmlDocPtr doc = xmlParseFile("trenuri.xml");

    if (doc == NULL) {
        send(client_fd, "[server] Eroare la deschiderea fisierului XML.\n", 33, 0);
        return false;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    xmlNodePtr trenNode = root->xmlChildrenNode;

    while (trenNode) {
        if (trenNode->type == XML_ELEMENT_NODE && xmlStrEqual(trenNode->name, BAD_CAST "tren")) {
            xmlChar *id_attr = xmlGetProp(trenNode, BAD_CAST "id");
            if (id_attr && strcmp((const char *) id_attr, tren.id_tren.c_str()) == 0) {
                xmlChar *intarziere_attr = xmlGetProp(trenNode, BAD_CAST "intarziere");
                std::string result = "[server] Intarzierea pentru trenul cu ID-ul " + tren.id_tren + " este: " +
                                     std::string(
                                         (const char *) intarziere_attr) + "\n";
                send(client_fd, result.c_str(), result.size() + 1, 0);
                xmlFree(id_attr);
                xmlFree(intarziere_attr);
                xmlFreeDoc(doc);
                return true;
            }
            xmlFree(id_attr);
        }
        trenNode = trenNode->next;
    }

    send(client_fd, "[server] Trenul cu ID-ul specificat nu a fost gasit.\n", 44, 0);
    xmlFreeDoc(doc);
    return false;
}

//mersul trenurilor
bool comanda_14(string &comanda, trenuri &tren, int client_fd) {
    verifica_si_creeaza_fisier("trenuri.xml");

    xmlDocPtr doc = xmlParseFile("trenuri.xml");
    if (doc == NULL) {
        send(client_fd, "[server] Eroare la deschiderea fisierului XML.\n", 33, 0);
        return false;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    xmlNodePtr trenNode = root->xmlChildrenNode;
    string result = "[server] Mersul trenurilor:\n";

    while (trenNode) {
        if (trenNode->type == XML_ELEMENT_NODE && xmlStrEqual(trenNode->name, BAD_CAST "tren")) {
            xmlChar *id_attr = xmlGetProp(trenNode, BAD_CAST "id");
            xmlChar *ruta_attr = xmlGetProp(trenNode, BAD_CAST "ruta");
            xmlChar *plecare_attr = xmlGetProp(trenNode, BAD_CAST "status_plecare");
            xmlChar *sosire_attr = xmlGetProp(trenNode, BAD_CAST "status_sosire");
            xmlChar *intarziere_attr = xmlGetProp(trenNode, BAD_CAST "intarziere");
            xmlChar *estimare_sosire_attr = xmlGetProp(trenNode, BAD_CAST "estimare_sosire");

            result += "ID Tren: " + std::string((const char *) id_attr) + "\n";
            result += "Ruta: " + std::string((const char *) ruta_attr) + "\n";
            result += "Status Plecare: " + std::string((const char *) plecare_attr) + "\n";
            result += "Status Sosire: " + std::string((const char *) sosire_attr) + "\n";
            result += "Intarziere: " + std::string((const char *) intarziere_attr) + "\n";
            result += "Estimare Sosire: " + std::string((const char *) estimare_sosire_attr) + "\n";

            result += "\n";

            xmlFree(id_attr);
            xmlFree(ruta_attr);
            xmlFree(plecare_attr);
            xmlFree(sosire_attr);
            xmlFree(intarziere_attr);
            xmlFree(estimare_sosire_attr);
        }
        trenNode = trenNode->next;
    }

    send(client_fd, result.c_str(), result.size() + 1, 0);
    xmlFreeDoc(doc);
    return true;
}

//ruta trenurilor
bool comanda_15(string &comanda, trenuri &tren, int client_fd) {
    verifica_si_creeaza_fisier("trenuri.xml");

    xmlDocPtr doc = xmlParseFile("trenuri.xml");
    if (doc == NULL) {
        send(client_fd, "[server] Eroare la deschiderea fisierului XML.\n", 33, 0);
        return false;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    xmlNodePtr trenNode = root->xmlChildrenNode;

    string result = "[server] Ruta trenurilor:\n";
    while (trenNode) {
        if (trenNode->type == XML_ELEMENT_NODE && xmlStrEqual(trenNode->name, BAD_CAST "tren")) {
            xmlChar *id_attr = xmlGetProp(trenNode, BAD_CAST "id");
            xmlChar *ruta_attr = xmlGetProp(trenNode, BAD_CAST "ruta");

            result += "ID Tren: " + std::string((const char *) id_attr) + "\n";
            result += "Ruta: " + std::string((const char *) ruta_attr) + "\n";
            result += "\n";

            xmlFree(id_attr);
            xmlFree(ruta_attr);
        }
        trenNode = trenNode->next;
    }

    if (result.size() == 0) {
        result = "[server] Nu exista trenuri în baza de date.\n";
    }

    send(client_fd, result.c_str(), result.size() + 1, 0);
    xmlFreeDoc(doc);
    return true;
}

//help
bool comanda_16(string &comanda, trenuri &tren, int client_fd) {
    send(client_fd, comenzi_valide.c_str(), comenzi_valide.size() + 1, 0);

    return true;
}

//quit
bool comanda_17(string &comanda, trenuri &tren, int client_fd) {
    string aux = "quit";
    send(client_fd, aux.c_str(), aux.size() + 1, 0);

    return true;
}

// statii tren cu id
bool comanda_18(string &comanda, trenuri &tren, int client_fd) {
    verifica_si_creeaza_fisier("trenuri.xml");

    xmlDocPtr doc = xmlParseFile("trenuri.xml");
    if (doc == NULL) {
        send(client_fd, "[server] Eroare la deschiderea fisierului XML.\n", 33, 0);
        return false;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    xmlNodePtr trenNode = root->xmlChildrenNode;

    string result = "[server] Statiile trenului cu ID-ul " + tren.id_tren + ":\n";
    bool tren_gasit = false;


    while (trenNode) {
        if (trenNode->type == XML_ELEMENT_NODE && xmlStrEqual(trenNode->name, BAD_CAST "tren")) {
            xmlChar *id_attr = xmlGetProp(trenNode, BAD_CAST "id");

            if (id_attr && strcmp((const char *) id_attr, tren.id_tren.c_str()) == 0) {
                tren_gasit = true;
                xmlNodePtr statieNode = trenNode->xmlChildrenNode;


                while (statieNode) {
                    if (statieNode->type == XML_ELEMENT_NODE && xmlStrEqual(statieNode->name, BAD_CAST "statie")) {
                        xmlNodePtr numeNode = statieNode->xmlChildrenNode;
                        while (numeNode) {
                            if (numeNode->type == XML_ELEMENT_NODE && xmlStrEqual(numeNode->name, BAD_CAST "nume")) {
                                std::string nume_statie = (const char *) xmlNodeGetContent(numeNode);
                                result += nume_statie + "\n";
                            }
                            numeNode = numeNode->next;
                        }
                    }
                    statieNode = statieNode->next;
                }
            }
            xmlFree(id_attr);
        }
        trenNode = trenNode->next;
    }

    if (!tren_gasit) {
        result = "[server] Trenul cu ID-ul " + tren.id_tren + " nu a fost gasit.\n";
    }

    send(client_fd, result.c_str(), result.size() + 1, 0);
    xmlFreeDoc(doc);
    return true;
}


// lansarea comenzilor si blocarea mutex-ului
bool lansare_comenzi(string &comanda, trenuri &tren, statii &statie, int id_comanda, int client_fd) {
    string aux = "[server] Comanda \"" + comanda + "\" a fost prelucrata.\n";
    send(client_fd, aux.c_str(), aux.size() + 1, 0);

    cout << aux << endl;


    bool returnare;
    if (id_comanda == 1) {
        pthread_mutex_lock(&mutexx);

        returnare = comanda_1(comanda, tren, client_fd);

        pthread_mutex_unlock(&mutexx);
        return returnare;
    }
    if (id_comanda == 2) {
        pthread_mutex_lock(&mutexx);

        returnare = comanda_2(comanda, statie, client_fd);

        pthread_mutex_unlock(&mutexx);
        return returnare;
    }
    if (id_comanda == 3) {
        pthread_mutex_lock(&mutexx);

        returnare = comanda_3(comanda, statie, client_fd);

        pthread_mutex_unlock(&mutexx);
        return returnare;
    }
    if (id_comanda == 4) {
        pthread_mutex_lock(&mutexx);

        returnare = comanda_4(comanda, tren, client_fd);

        pthread_mutex_unlock(&mutexx);
        return returnare;
    }
    if (id_comanda == 5) {
        pthread_mutex_lock(&mutexx);

        returnare = comanda_5(comanda, statie, client_fd);

        pthread_mutex_unlock(&mutexx);
        return returnare;
    }
    if (id_comanda == 6) {
        pthread_mutex_lock(&mutexx);

        returnare = comanda_6(comanda, tren, client_fd);

        pthread_mutex_unlock(&mutexx);
        return returnare;
    }
    if (id_comanda == 7) {
        pthread_mutex_lock(&mutexx);

        returnare = comanda_7(comanda, tren, client_fd);

        pthread_mutex_unlock(&mutexx);
        return returnare;
    }
    if (id_comanda == 8) {
        pthread_mutex_lock(&mutexx);

        returnare = comanda_8(comanda, tren, client_fd);

        pthread_mutex_unlock(&mutexx);
        return returnare;
    }
    if (id_comanda == 9) {
        pthread_mutex_lock(&mutexx);

        returnare = comanda_9(comanda, tren, client_fd);

        pthread_mutex_unlock(&mutexx);
        return returnare;
    }
    if (id_comanda == 10) {
        pthread_mutex_lock(&mutexx);

        returnare = comanda_10(comanda, tren, client_fd);

        pthread_mutex_unlock(&mutexx);
        return returnare;
    }
    if (id_comanda == 11) {
        pthread_mutex_lock(&mutexx);

        returnare = comanda_11(comanda, tren, client_fd);

        pthread_mutex_unlock(&mutexx);
        return returnare;
    }
    if (id_comanda == 12) {
        pthread_mutex_lock(&mutexx);

        returnare = comanda_12(comanda, tren, client_fd);

        pthread_mutex_unlock(&mutexx);
        return returnare;
    }
    if (id_comanda == 13) {
        pthread_mutex_lock(&mutexx);

        returnare = comanda_13(comanda, tren, client_fd);

        pthread_mutex_unlock(&mutexx);
        return returnare;
    }
    if (id_comanda == 14) {
        pthread_mutex_lock(&mutexx);

        returnare = comanda_14(comanda, tren, client_fd);

        pthread_mutex_unlock(&mutexx);
        return returnare;
    }
    if (id_comanda == 15) {
        pthread_mutex_lock(&mutexx);

        returnare = comanda_15(comanda, tren, client_fd);

        pthread_mutex_unlock(&mutexx);
        return returnare;
    }
    if (id_comanda == 16) {
        pthread_mutex_lock(&mutexx);

        returnare = comanda_16(comanda, tren, client_fd);

        pthread_mutex_unlock(&mutexx);
        return returnare;
    }
    if (id_comanda == 17) {
        pthread_mutex_lock(&mutexx);

        returnare = comanda_17(comanda, tren, client_fd);

        pthread_mutex_unlock(&mutexx);
        return returnare;
    }
    if (id_comanda == 18) {
        pthread_mutex_lock(&mutexx);

        returnare = comanda_18(comanda, tren, client_fd);

        pthread_mutex_unlock(&mutexx);
        return returnare;
    }


    return false;
}

struct ThreadData {
    int client_fd;
    std::string client_ip;
};

void *thread_client(void *arg) {
    auto *data = (ThreadData *) arg;

    int client_fd = data->client_fd;

    std::string client_ip = data->client_ip;

    int id_comanda = 0;

    trenuri tren;

    statii statie;

    std::vector<std::string> comenzi;

    while (true) {
        id_comanda = 0;

        std::string comanda_full;

        bool gata_1 = false;

        //citire comanda
        if (!mesaj_to_char(client_fd, comanda_full)) {
           close(client_fd);
            std::cout << "[server] Client:" << client_ip << " deconectat." << std::endl;
           return NULL;
        }

        if (comanda_full == "Gata") {
            gata_1 = true;
        } else {
            comenzi.push_back(comanda_full);

            std::cout << "[server] Comanda primita: " << comanda_full << "    " << "\n";
        }


        //procesare comenzi
        if (gata_1 == true) {
            for (auto &comanda: comenzi) {
                std::string raspuns;
                usleep(100);
                if (validare_comanda(comanda, raspuns, tren, statie, id_comanda)) {
                    if (lansare_comenzi(comanda, tren, statie, id_comanda, client_fd)) {
                        std::string aux = "[server] COMANDA: \"" + comanda + "\" TERMINATA, CU SUCCES\n";

                        send(client_fd, aux.c_str(), aux.size() + 1, 0);
                    } else {
                        std::string aux = "[server] COMANDA: \"" + comanda + " \" TERMINATA, FARA SUCCES\n";
                        send(client_fd, aux.c_str(), aux.size() + 1, 0);
                    }
                } else {
                    std::string aux = "[server] COMANDA: \"" + comanda + "\" INVALIDA\n";
                    send(client_fd, aux.c_str(), aux.size() + 1, 0);
                }
            }
            string auxx = "[server] Toate comenzile au fost procesate.\n";
            send(client_fd, auxx.c_str(), auxx.size() + 1, 0);
            usleep(10);
            std::cout << auxx;
            std::string final_msg = "Gata";

            send(client_fd, final_msg.c_str(), final_msg.size() + 1, 0);

            comenzi.clear();
        }
    }
}


int main() {
    sockaddr_in server, from;
    int sd;

    // creare socket

    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1) {
        perror("Eroare la socket()");
        return 1;
    }

    int optval = 1;
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("Eroare la setsockopt()");
        return 1;
    }

    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(sd, (struct sockaddr *) &server, sizeof(server)) == -1) {
        perror("Eroare la bind()");
        return 1;
    }


    sockaddr_in actual_addr{};
    socklen_t len = sizeof(actual_addr);

    if (getsockname(sd, (sockaddr *) &actual_addr, &len) == -1) {
        perror("Eroare la getsockname()");
        return errno;
    }


    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &actual_addr.sin_addr, ip, INET_ADDRSTRLEN);


    std::cout << "[server] Server pornit si asculta pe IP-ul \"" << ip << "\" si portul \"" << ntohs(
        actual_addr.sin_port) << "\" \n";


    if (listen(sd, 10) == -1) {
        perror("Eroare la listen()");
        return 1;
    }

    std::cout << "[server] Serverul este gata sa accepte clienti..." << std::endl;

    while (true) {
        socklen_t client_len = sizeof(from);
        int client = accept(sd, (struct sockaddr *) &from, &client_len);

        if (client < 0) {
            perror("Eroare la accept()");
            continue;
        }

        std::cout << "[server] Client conectat: " << inet_ntoa(from.sin_addr) << std::endl;

        ThreadData *data = new ThreadData();

        data->client_fd = client;

        char client_ip[INET_ADDRSTRLEN];

        inet_ntop(AF_INET, &from.sin_addr, client_ip, INET_ADDRSTRLEN);

        data->client_ip = std::string(client_ip);

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, thread_client, (void *) data) != 0) {
            perror("Eroare la crearea thread-ului");
            continue;
        }


        pthread_detach(thread_id);
    }
}

