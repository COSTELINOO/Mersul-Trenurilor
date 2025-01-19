#!/bin/bash


if [ ! -f server.cpp ]; then
    echo "Eroare: Fisierul server.cpp nu exista!"
    exit 1
fi


g++ server.cpp -o server $(pkg-config --cflags --libs libxml-2.0) -lpthread


if [ $? -eq 0 ]; then
    echo "Compilare reusita! Serverul a fost generat: ./server"
    
    # Rulam serverul
    echo "Pornim serverul..."
    ./server
else
    echo "Eroare la compilare!"
    exit 1
fi

