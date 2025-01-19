
# Mersul Trenurilor

Acest proiect implementează o aplicație server-client pentru furnizarea centralizată a informațiilor despre mersul trenurilor. Aplicația oferă informații despre rute, ore de plecare și sosire, întârzieri și alte detalii relevante.

## Cum să rulezi proiectul

1. **Lansarea serverului**  
   Deschide un terminal și rulează comanda:  
   ```bash
   ./compile_server.sh
   ```  
   După rulare, în consolă va apărea mesajul:  
   ```
   Server pornit și ascultă pe IP-ul "ip" și portul "port"
   ```

2. **Lansarea clientului**  
   Deschide o altă consolă și rulează comanda:  
   ```bash
   ./compile_client.sh [ip] [port]
   ```  
   Înlocuiește `[ip]` și `[port]` cu valorile specificate în mesajul serverului.

3. **Introducerea comenzilor**  
   - În fișierul `example_comenzi.txt` se găsesc câteva exemple de comenzi.  
   - Fiecare comandă introdusă în client trebuie urmată de comanda `Trimite`, **pe un rând nou**, și apăsarea tastei `Enter` pentru a fi transmisă către server.

## Documentație tehnică

Detalii tehnice despre aplicație sunt disponibile în raportul tehnic anexat proiectului.

---

Acest proiect a fost creat pentru a demonstra funcționalitatea unui sistem distribuit de tip server-client pentru informații despre mersul trenurilor.
