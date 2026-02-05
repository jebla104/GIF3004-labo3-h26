#!/bin/bash

# Ce script assume :
#   - Qu'il est exécuté dans le même répertoire que les fichiers exécutables
#   - Que les vidéos sont situés dans des dossiers *p (par exemple 160p) dans le même répertoire

sudo rm /dev/shm/mem*           # On retire les identifiants des zones mémoire partagées des précédentes exécutions
echo "[Script] 11_deadlineTroisVideos"
echo "[Script] Lancement decodeurs"
sudo ./decodeur -s DEADLINE -d 16,33,33 240p/02_Sintel.ulv /mem1 &
sudo ./decodeur -s DEADLINE -d 3,33,33 240p/01_ToS.ulv /mem2 &
sudo ./decodeur -s NORT 240p/04_Caminandes.ulv /mem3 &
echo "[Script] En attente de creation de /mem1, /mem2 et /mem3"
while [ ! -f /dev/shm/mem1 ] || [ ! -f /dev/shm/mem2 ] || [ ! -f /dev/shm/mem3 ]
do
    sleep 0.05
done
echo "[Script] /mem1, /mem2 et /mem3 crees, lancement compositeur"
sudo ./compositeur -s DEADLINE -d 9,33,33 /mem1 /mem2 /mem3 &
wait;

