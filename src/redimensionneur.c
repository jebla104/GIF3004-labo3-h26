/******************************************************************************
 * Laboratoire 3
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2026
 * Marc-André Gardner
 *
 * Fichier implémentant le programme de redimensionnement d'images
 ******************************************************************************/

#include <sys/resource.h>

#include "allocateurMemoire.h"
#include "commMemoirePartagee.h"
#include "utils.h"

int main(int argc, char *argv[]) {
  setbuf(stdout, NULL);

  char signatureProfilage[128] = {0};
  char *nomProgramme = (argv[0][0] == '.') ? argv[0] + 2 : argv[0];
  snprintf(signatureProfilage, 128, "profilage-%s-%u.txt", nomProgramme,
           (unsigned int)getpid());
  InfosProfilage profInfos;
  initProfilage(&profInfos, signatureProfilage);
  evenementProfilage(&profInfos, ETAT_INITIALISATION);

  struct SchedParams params = {
      .modeOrdonnanceur = ORDONNANCEMENT_NORT,
      .runtime = 0,
      .deadline = 0,
      .period = 0,
  };

  char *entree = NULL, *sortie = NULL;
  unsigned int outWidth = 427, outHeight = 240;
  int methode = 0;

  if (argc >= 2 && strcmp(argv[1], "--debug") == 0) {
    printf("Mode debug sélectionné pour le redimensionneur\n");
    entree = (char *)"/mem1";
    sortie = (char *)"/mem2";
    outWidth = 427;
    outHeight = 240;
    methode = 1;
  } else {
    int c;
    opterr = 0;
    while ((c = getopt(argc, argv, "s:d:w:h:r:")) != -1) {
      switch (c) {
      case 's':
        parseSchedOption(optarg, &params);
        break;
      case 'd':
        parseDeadlineParams(optarg, &params);
        break;
      case 'w':
        outWidth = (unsigned int)atoi(optarg);
        break;
      case 'h':
        outHeight = (unsigned int)atoi(optarg);
        break;
      case 'r':
        methode = atoi(optarg);
        break;
      default:
        break;
      }
    }
    if (argc - optind < 2) {
      fprintf(stderr,
              "Usage: %s [options] -w W -h H [-r methode] entree sortie\n",
              argv[0]);
      return -1;
    }
    entree = argv[optind];
    sortie = argv[optind + 1];
  }

  printf("[redimensionneur] entree=%s, sortie=%s, %ux%u, methode=%d, "
         "ordonnancement=%d\n",
         entree, sortie, outWidth, outHeight, methode, params.modeOrdonnanceur);

  struct memPartage zoneEntree;
  if (initMemoirePartageeLecteur(entree, &zoneEntree) != 0) {
    fprintf(stderr, "[redimensionneur] Échec initMemoirePartageeLecteur(%s)\n",
            entree);
    return -1;
  }
  uint32_t larg = zoneEntree.header->infos.largeur;
  uint32_t haut = zoneEntree.header->infos.hauteur;
  uint32_t canaux = zoneEntree.header->infos.canaux;
  uint32_t fps = zoneEntree.header->infos.fps;

  struct videoInfos infosOut;
  infosOut.largeur = outWidth;
  infosOut.hauteur = outHeight;
  infosOut.canaux = canaux;
  infosOut.fps = fps;
  struct memPartage zoneSortie;
  if (initMemoirePartageeEcrivain(sortie, &zoneSortie, &infosOut) != 0) {
    fprintf(stderr, "[redimensionneur] Échec initMemoirePartageeEcrivain(%s)\n",
            sortie);
    return -1;
  }

  size_t tailleEntree = (size_t)larg * haut * canaux;
  size_t tailleSortie = (size_t)outWidth * outHeight * canaux;

  prepareMemoire(tailleEntree, tailleSortie * 4);
  struct rlimit rl = {RLIM_INFINITY, RLIM_INFINITY};
  setrlimit(RLIMIT_MEMLOCK, &rl);
  mlockall(MCL_CURRENT | MCL_FUTURE);
  appliquerOrdonnancement(&params, "redimensionneur");

  ResizeGrid rg;
  if (methode == 0)
    rg = resizeNearestNeighborInit(outHeight, outWidth, haut, larg);
  else
    rg = resizeBilinearInit(outHeight, outWidth, haut, larg);

  unsigned char *bufEntree =
      (unsigned char *)tempsreel_malloc(tailleEntree);
  unsigned char *bufSortie =
      (unsigned char *)tempsreel_malloc(tailleSortie);

  if (bufEntree == NULL || bufSortie == NULL) {
    fprintf(stderr,
            "[redimensionneur] Erreur d'allocation tempsreel_malloc pour les "
            "buffers internes\n");
    return -1;
  }

  while (1) {
    evenementProfilage(&profInfos, ETAT_ATTENTE_MUTEXLECTURE);
    attenteLecteur(&zoneEntree);

    evenementProfilage(&profInfos, ETAT_TRAITEMENT);
    memcpy(bufEntree, zoneEntree.data, tailleEntree);
    signalLecteur(&zoneEntree);

    if (methode == 0)
      resizeNearestNeighbor(bufEntree, haut, larg, bufSortie, outHeight,
                            outWidth, rg, canaux);
    else
      resizeBilinear(bufEntree, haut, larg, bufSortie, outHeight, outWidth, rg,
                     canaux);

    evenementProfilage(&profInfos, ETAT_ATTENTE_MUTEXECRITURE);
    attenteEcrivain(&zoneSortie);

    evenementProfilage(&profInfos, ETAT_TRAITEMENT);
    memcpy(zoneSortie.data, bufSortie, tailleSortie);
    signalEcrivain(&zoneSortie);

    if (params.modeOrdonnanceur == ORDONNANCEMENT_DEADLINE) {
      sched_yield();
    }
  }

  resizeDestroy(rg);
  tempsreel_free(bufEntree);
  tempsreel_free(bufSortie);
  return 0;
}
