/******************************************************************************
 * Laboratoire 3
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2026
 * Marc-André Gardner
 * 
 * Fichier de déclaration des structures et fonctions de communication 
 * entre les programmes.
 * Ne modifiez pas les structs et prototypes de fonction écrits ici.
 ******************************************************************************/

#ifndef COMM_MEM_H
#define COMM_MEM_H

// Permet de protéger le header lorsqu'il est inclus par un fichier C++
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sched.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>


// États de synchronisation
#define ETAT_NON_INITIALISE    0
#define ETAT_PRET_SANS_DONNEES 1
#define ETAT_PRET_AVEC_DONNEES 2


// Délai entre deux tentatives d'initialisation du lecteur
#define DELAI_INIT_READER_USEC 1000  

// Le reste de ce fichier constitue une suggestion de structures et fonctions
// à créer pour lire et écrire l'espace mémoire partagé.

// Structure contenant les informations sur la vidéo
struct videoInfos{
    uint32_t largeur;       // en pixels
    uint32_t hauteur;
    uint32_t canaux;        // Nombre de canaux (1 = niveaux de gris, 3 = BGR)
    uint32_t fps;
};

// Cette structure permet d'accéder facilement aux diverses informations stockées
// au début de l'espace partagé
struct memPartageHeader{
    pthread_mutex_t mutex;          // Mutex pour protéger les conditions
    pthread_cond_t condEcrivain;    // Condition sur laquelle l'ecrivain attend
    pthread_cond_t condLecteur;     // Condition sur laquelle le lecteur attend
    volatile uint32_t etat;         // État de synchronisation (voir constantes ETAT_*)
    struct videoInfos infos;        // Informations sur la vidéo
};

// Cette structure permet de mémoriser l'information sur une zone mémoire partagée.
// C'est un pointeur vers une instance de cette structure qui sera passée aux
// différentes fonctions
struct memPartage{
    int fd;                         // Descripteur de fichier retourné par shm_open
    struct memPartageHeader *header;// Pointeur vers le header dans la mémoire partagée
    size_t tailleDonnees;           // Taille de la zone de données (après le header)
    unsigned char* data;            // Pointeur vers la zone de données (après le header)
};

// Appelée au début du programme pour l'initialisation de la zone mémoire (cas du lecteur).
// Reçoit un pointeur vers une structure memPartage _vide_.
// Cette fonction doit _remplir_ cette structure avec les informations nécessaires
// une fois la mémoire partagée initialisée.
int initMemoirePartageeLecteur(const char* identifiant,
                                struct memPartage *zone);

// Appelée au début du programme pour l'initialisation de la zone mémoire (cas de l'écrivain).
// Reçoit un pointeur vers une structure memPartage _vide_.
// Cette fonction doit _remplir_ cette structure avec les informations nécessaires
// une fois la mémoire partagée initialisée.
// Reçoit également un pointeur vers une struct videoInfos initialisée avec les valeurs
// correctes pour cette exécution (la bonne largeur et hauteur de l'image, etc.).
// Cette fonction doit initialiser la zone mémoire avec les valeurs correspondantes
// (par exemple, la taille des données correspond au nombre d'octets requis pour contenir
// UNE image, qui peut être déduit à partir des valeurs contenues dans la struct videoInfos).
int initMemoirePartageeEcrivain(const char* identifiant,
                                struct memPartage *zone,
                                struct videoInfos *infos);

// Appelée par le lecteur pour se mettre en attente de données sur la zone mémoire partagée
// Lorsque cette fonction retourne, le mutex devrait être verrouillé par le processus en cours!
int attenteLecteur(struct memPartage *zone);

// Fonction spéciale similaire à attenteLecteur, mais asynchrone : cette fonction ne bloque jamais.
// Cela est utile pour le compositeur, qui ne doit pas bloquer l'entièreté des flux si un seul est plus lent.
// Utilisez la valeur de retour pour permettre à l'appelant de déterminer si la zone mémoire partagée
// est prête ou non à être lue.
// Lorsque cette fonction retourne une valeur indiquant que la lecture est possible, le mutex devrait être 
// verrouillé par le processus en cours!
int attenteLecteurAsync(struct memPartage *zone);

// Appelée par l'écrivain pour se mettre en attente de la lecture du résultat précédent par un lecteur
// Lorsque cette fonction retourne, le mutex devrait être verrouillé par le processus en cours!
int attenteEcrivain(struct memPartage *zone);

// Appelée par le lecteur pour signaler qu'il a fini de lire (réveille l'écrivain correspondant)
void signalLecteur(struct memPartage *zone);

// Appelée par l'écrivain pour signaler qu'il a fini d'écrire (réveille le lecteur correspondant)
void signalEcrivain(struct memPartage *zone);

// N'oubliez pas d'implémenter les fonctions décrites ici dans commMemoirePartagee.c!

#ifdef __cplusplus
}
#endif

#endif
