/******************************************************************************
 * Laboratoire 3
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2026
 * Marc-André Gardner
 *
 * Fichier implémentant les fonctions de communication inter-processus
 ******************************************************************************/

#include "commMemoirePartagee.h"

/* -------------------------------------------------------------------------- *
 *  initMemoirePartageeEcrivain
 *  Crée et mappe la zone mémoire partagée du côté écrivain.
 * -------------------------------------------------------------------------- */
int initMemoirePartageeEcrivain(const char *identifiant,
                                struct memPartage *zone,
                                struct videoInfos *infos) {
  zone->fd = shm_open(identifiant, O_CREAT | O_RDWR | O_TRUNC, 0666);
  if (zone->fd == -1) {
    perror("initMemoirePartageeEcrivain: shm_open");
    return -1;
  }

  size_t tailleDonnees =
      (size_t)infos->largeur * infos->hauteur * infos->canaux;
  size_t tailleTotal = sizeof(struct memPartageHeader) + tailleDonnees;

  if (ftruncate(zone->fd, (off_t)tailleTotal) == -1) {
    perror("initMemoirePartageeEcrivain: ftruncate");
    return -1;
  }

  void *ptr =
      mmap(NULL, tailleTotal, PROT_READ | PROT_WRITE, MAP_SHARED, zone->fd, 0);
  if (ptr == MAP_FAILED) {
    perror("initMemoirePartageeEcrivain: mmap");
    return -1;
  }

  zone->header = (struct memPartageHeader *)ptr;
  zone->data = (unsigned char *)(zone->header + 1);
  zone->tailleDonnees = tailleDonnees;

  zone->header->infos = *infos;
  zone->header->etat = ETAT_NON_INITIALISE;

  pthread_mutexattr_t mattr;
  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
  pthread_mutexattr_setprotocol(&mattr, PTHREAD_PRIO_INHERIT);
  pthread_mutexattr_setrobust(&mattr, PTHREAD_MUTEX_ROBUST);
  pthread_mutex_init(&zone->header->mutex, &mattr);
  pthread_mutexattr_destroy(&mattr);

  pthread_condattr_t cattr;
  pthread_condattr_init(&cattr);
  pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);
  pthread_cond_init(&zone->header->condEcrivain, &cattr);
  pthread_cond_init(&zone->header->condLecteur, &cattr);
  pthread_condattr_destroy(&cattr);

  zone->header->etat = ETAT_PRET_SANS_DONNEES;

  return 0;
}

/* -------------------------------------------------------------------------- *
 *  initMemoirePartageeLecteur
 *  Ouvre et mappe la zone mémoire partagée du côté lecteur.
 * -------------------------------------------------------------------------- */
int initMemoirePartageeLecteur(const char *identifiant,
                               struct memPartage *zone) {
  while ((zone->fd = shm_open(identifiant, O_RDWR, 0)) == -1) {
    if (errno == ENOENT) {
      usleep(DELAI_INIT_READER_USEC);
      continue;
    }
    perror("initMemoirePartageeLecteur: shm_open");
    return -1;
  }

  struct stat st;
  do {
    if (fstat(zone->fd, &st) == -1) {
      perror("initMemoirePartageeLecteur: fstat");
      return -1;
    }
    if (st.st_size < (off_t)sizeof(struct memPartageHeader))
      usleep(DELAI_INIT_READER_USEC);
  } while (st.st_size < (off_t)sizeof(struct memPartageHeader));

  struct memPartageHeader *hdr =
      mmap(NULL, sizeof(struct memPartageHeader), PROT_READ | PROT_WRITE,
           MAP_SHARED, zone->fd, 0);
  if (hdr == MAP_FAILED) {
    perror("initMemoirePartageeLecteur: mmap header");
    return -1;
  }

  while (hdr->etat == ETAT_NON_INITIALISE)
    usleep(DELAI_INIT_READER_USEC);

  size_t tailleDonnees =
      (size_t)hdr->infos.largeur * hdr->infos.hauteur * hdr->infos.canaux;
  size_t tailleTotal = sizeof(struct memPartageHeader) + tailleDonnees;

  munmap(hdr, sizeof(struct memPartageHeader));

  void *ptr =
      mmap(NULL, tailleTotal, PROT_READ | PROT_WRITE, MAP_SHARED, zone->fd, 0);
  if (ptr == MAP_FAILED) {
    perror("initMemoirePartageeLecteur: mmap complet");
    return -1;
  }

  zone->header = (struct memPartageHeader *)ptr;
  zone->data = (unsigned char *)(zone->header + 1);
  zone->tailleDonnees = tailleDonnees;

  return 0;
}

/* -------------------------------------------------------------------------- *
 *  attenteLecteur
 *  Bloque jusqu'à ce que l'état soit ETAT_PRET_AVEC_DONNEES.
 *  Le mutex est verrouillé au retour.
 * -------------------------------------------------------------------------- */
int attenteLecteur(struct memPartage *zone) {
  pthread_mutex_lock(&zone->header->mutex);
  while (zone->header->etat != ETAT_PRET_AVEC_DONNEES)
    pthread_cond_wait(&zone->header->condLecteur, &zone->header->mutex);
  return 0;
}

/* -------------------------------------------------------------------------- *
 *  attenteLecteurAsync
 *  Version non-bloquante de attenteLecteur.
 *  Retourne 1 (mutex verrouillé, données dispo) ou 0 (rien de dispo).
 * -------------------------------------------------------------------------- */
int attenteLecteurAsync(struct memPartage *zone) {
  if (pthread_mutex_trylock(&zone->header->mutex) != 0)
    return 0;
  if (zone->header->etat == ETAT_PRET_AVEC_DONNEES)
    return 1;
  pthread_mutex_unlock(&zone->header->mutex);
  return 0;
}

/* -------------------------------------------------------------------------- *
 *  attenteEcrivain
 *  Bloque jusqu'à ce que l'état soit ETAT_PRET_SANS_DONNEES.
 *  Le mutex est verrouillé au retour.
 * -------------------------------------------------------------------------- */
int attenteEcrivain(struct memPartage *zone) {
  pthread_mutex_lock(&zone->header->mutex);
  while (zone->header->etat != ETAT_PRET_SANS_DONNEES)
    pthread_cond_wait(&zone->header->condEcrivain, &zone->header->mutex);
  return 0;
}

/* -------------------------------------------------------------------------- *
 *  signalLecteur
 *  Appelée par le lecteur après avoir lu : libère l'écrivain.
 * -------------------------------------------------------------------------- */
void signalLecteur(struct memPartage *zone) {
  zone->header->etat = ETAT_PRET_SANS_DONNEES;
  pthread_cond_signal(&zone->header->condEcrivain);
  pthread_mutex_unlock(&zone->header->mutex);
}

/* -------------------------------------------------------------------------- *
 *  signalEcrivain
 *  Appelée par l'écrivain après avoir écrit : réveille le lecteur.
 * -------------------------------------------------------------------------- */
void signalEcrivain(struct memPartage *zone) {
  zone->header->etat = ETAT_PRET_AVEC_DONNEES;
  pthread_cond_signal(&zone->header->condLecteur);
  pthread_mutex_unlock(&zone->header->mutex);
}
