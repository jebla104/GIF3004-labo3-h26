/******************************************************************************
 * Laboratoire 3
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2026
 * Marc-André Gardner
 *
 * Fichier implémentant le programme de décodage des fichiers ULV
 ******************************************************************************/

// Gestion des ressources et permissions
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <time.h>

#include "allocateurMemoire.h"
#include "commMemoirePartagee.h"
#include "utils.h"

#include "jpgd.h"

// Définition de diverses structures pouvant vous être utiles pour la lecture
// d'un fichier ULV
#define HEADER_SIZE 4
const char header[] = "SETR";

/******************************************************************************
 * FORMAT DU FICHIER VIDEO
 * Offset     Taille     Type      Description
 * 0          4          char      Header (toujours "SETR" en ASCII)
 * 4          4          uint32    Largeur des images du vidéo
 * 8          4          uint32    Hauteur des images du vidéo
 * 12         4          uint32    Nombre de canaux dans les images
 * 16         4          uint32    Nombre d'images par seconde (FPS)
 * 20         4          uint32    Taille (en octets) de la première image -> N
 * 24         N          char      Contenu de la première image (row-first)
 * 24+N       4          uint32    Taille (en octets) de la seconde image -> N2
 * 24+N+4     N2         char      Contenu de la seconde image
 * 24+N+N2    4          uint32    Taille (en octets) de la troisième image ->
 *N2
 * ...                             Toutes les images composant la vidéo, à la
 *suite 4          uint32    0 (indique la fin du fichier)
 ******************************************************************************/

int main(int argc, char *argv[]) {
  setbuf(stdout, NULL);

  char signatureProfilage[128] = {0};
  char *nomProgramme = (argv[0][0] == '.') ? argv[0] + 2 : argv[0];
  snprintf(signatureProfilage, 128, "profilage-%s-%u.txt", nomProgramme,
           (unsigned int)getpid());
  InfosProfilage profInfos;
  initProfilage(&profInfos, signatureProfilage);

  evenementProfilage(&profInfos, ETAT_INITIALISATION);

  struct SchedParams params;
  params.modeOrdonnanceur = ORDONNANCEMENT_NORT;
  params.runtime = 0;
  params.deadline = 0;
  params.period = 0;

  char *files[3] = {
      (char *)"240p/02_Sintel.ulv",
      (char *)"/test_decodeur",
      NULL,
  };
  size_t fileNbr = parseArgs(argc, argv, &params, files);

  if (fileNbr < 2 || files[0] == NULL || files[1] == NULL ||
      files[0][0] == '\0' || files[1][0] == '\0') {
    fprintf(stderr, "Usage: %s [options] <fichier.ulv> <identifiant_shm>\n",
            argv[0]);
    return -1;
  }

  printf("[decodeur] Fichier ULV : %s\n", files[0]);
  printf("[decodeur] Zone mémoire partagée : %s\n", files[1]);

  int fd = open(files[0], O_RDONLY);
  if (fd < 0) {
    perror("[decodeur] open ULV");
    return -1;
  }

  struct stat stULV;
  if (fstat(fd, &stULV) != 0) {
    perror("[decodeur] fstat ULV");
    close(fd);
    return -1;
  }
  size_t fileSize = (size_t)stULV.st_size;

  const unsigned char *fileData = (const unsigned char *)mmap(
      NULL, fileSize, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);
  if (fileData == MAP_FAILED) {
    perror("[decodeur] mmap ULV");
    close(fd);
    return -1;
  }
  close(fd); // fd peut être fermé après mmap

  if (fileSize < 20) {
    fprintf(stderr, "[decodeur] Fichier ULV trop court\n");
    return -1;
  }
  if (memcmp(fileData, "SETR", 4) != 0) {
    fprintf(stderr, "[decodeur] Fichier ULV invalide (mauvais magic)\n");
    return -1;
  }

  const unsigned char *p = fileData + 4;
  uint32_t largeur, hauteur, canaux, fps;
  memcpy(&largeur, p, sizeof(uint32_t));
  memcpy(&hauteur, p + 4, sizeof(uint32_t));
  memcpy(&canaux, p + 8, sizeof(uint32_t));
  memcpy(&fps, p + 12, sizeof(uint32_t));

  printf("[decodeur] Vidéo : %ux%u, %u canaux, %u fps\n", largeur, hauteur,
         canaux, fps);

  struct videoInfos infos;
  infos.largeur = largeur;
  infos.hauteur = hauteur;
  infos.canaux = canaux;
  infos.fps = fps;

  struct memPartage zoneSortie;
  if (initMemoirePartageeEcrivain(files[1], &zoneSortie, &infos) != 0) {
    fprintf(stderr, "[decodeur] Échec initMemoirePartageeEcrivain\n");
    return -1;
  }

  size_t tailleImage = (size_t)largeur * hauteur * canaux;
  prepareMemoire(tailleImage, tailleImage);
  struct rlimit rl = {RLIM_INFINITY, RLIM_INFINITY};
  setrlimit(RLIMIT_MEMLOCK, &rl);
  mlockall(MCL_CURRENT | MCL_FUTURE);

  appliquerOrdonnancement(&params, "decodeur");

  const unsigned char *frameStart = fileData + 20; // début des trames
  long cible_us = (fps > 0) ? (1000000L / (long)fps) : 33333L;

  while (1) {
    const unsigned char *cur = frameStart;

    while (1) {
      uint32_t frameSize;
      memcpy(&frameSize, cur, sizeof(uint32_t));
      cur += 4;

      if (frameSize == 0)
        break;

      evenementProfilage(&profInfos, ETAT_TRAITEMENT);

      struct timespec t_avant;
      clock_gettime(CLOCK_MONOTONIC, &t_avant);

      int w = 0, h = 0, comps = 0;
      unsigned char *decoded = jpgd::decompress_jpeg_image_from_memory(
          cur, (int)frameSize, &w, &h, &comps, (int)canaux);
      cur += frameSize;

      if (decoded == NULL) {
        fprintf(stderr, "[decodeur] Erreur décompression JPEG\n");
        continue;
      }

      struct timespec t_apres;
      clock_gettime(CLOCK_MONOTONIC, &t_apres);

      evenementProfilage(&profInfos, ETAT_ATTENTE_MUTEXECRITURE);
      attenteEcrivain(&zoneSortie);

      evenementProfilage(&profInfos, ETAT_TRAITEMENT);
      memcpy(zoneSortie.data, decoded, tailleImage);

      tempsreel_free(decoded);

      signalEcrivain(&zoneSortie);

      if (params.modeOrdonnanceur == ORDONNANCEMENT_DEADLINE) {
        sched_yield();
      }

      long elapsed_us = (t_apres.tv_sec - t_avant.tv_sec) * 1000000L +
                        (t_apres.tv_nsec - t_avant.tv_nsec) / 1000L;
      if (elapsed_us < cible_us) {
        evenementProfilage(&profInfos, ETAT_ENPAUSE);
        usleep((useconds_t)(cible_us - elapsed_us));
      }
    }
  }

  return 0;
}
