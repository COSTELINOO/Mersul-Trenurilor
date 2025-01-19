#!/bin/bash


if [ $# -ne 2 ]; then
    echo "Utilizare: $0 <IP> <PORT>"
    exit 1
fi

IP=$1
PORT=$2


if [ ! -f client.cpp ]; then
    echo "Eroare: Fisierul client.cpp nu exista!"
    exit 1
fi


g++ client.cpp -o client


if [ $? -eq 0 ]; then
 

    echo "Pornim clientul ce se conecteaza la serverul cu IP-ul=$IP si PORT-ul=$PORT..."
    ./client "$IP" "$PORT"
else
    echo "Eroare la compilare!"
    exit 1
fi

