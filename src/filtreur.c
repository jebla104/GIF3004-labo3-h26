/******************************************************************************
 * Laboratoire 3
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2026
 * Marc-André Gardner
 *
 * Fichier implémentant le programme de filtrage des images
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
  int typeFiltre = 0;

  if (argc >= 2 && strcmp(argv[1], "--debug") == 0) {
    printf("Mode debug sélectionné pour le filtreur\n");
    entree = (char *)"/mem1";
    sortie = (char *)"/mem2";
    typeFiltre = 0;
  } else {
    int c;
    opterr = 0;
    while ((c = getopt(argc, argv, "s:d:f:")) != -1) {
      switch (c) {
      case 's':
        parseSchedOption(optarg, &params);
        break;
      case 'd':
        parseDeadlineParams(optarg, &params);
        break;
      case 'f':
        typeFiltre = atoi(optarg);
        break;
      default:
        break;
      }
    }
    if (argc - optind < 2) {
      fprintf(stderr, "Usage: %s [options] -f type(0|1) entree sortie\n",
              argv[0]);
      return -1;
    }
    entree = argv[optind];
    sortie = argv[optind + 1];
  }

  printf("[filtreur] entree=%s, sortie=%s, filtre=%d, ordonnancement=%d\n",
         entree, sortie, typeFiltre, params.modeOrdonnanceur);

  struct memPartage zoneEntree;
  if (initMemoirePartageeLecteur(entree, &zoneEntree) != 0) {
    fprintf(stderr, "[filtreur] Échec initMemoirePartageeLecteur(%s)\n",
            entree);
    return -1;
  }
  uint32_t larg = zoneEntree.header->infos.largeur;
  uint32_t haut = zoneEntree.header->infos.hauteur;
  uint32_t canaux = zoneEntree.header->infos.canaux;
  uint32_t fps = zoneEntree.header->infos.fps;

  struct videoInfos infosOut;
  infosOut.largeur = larg;
  infosOut.hauteur = haut;
  infosOut.canaux = canaux;
  infosOut.fps = fps;
  struct memPartage zoneSortie;
  if (initMemoirePartageeEcrivain(sortie, &zoneSortie, &infosOut) != 0) {
    fprintf(stderr, "[filtreur] Échec initMemoirePartageeEcrivain(%s)\n",
            sortie);
    return -1;
  }

  size_t tailleImage = (size_t)larg * haut * canaux;

  prepareMemoire(tailleImage * 4, tailleImage);
  struct rlimit rl = {RLIM_INFINITY, RLIM_INFINITY};
  setrlimit(RLIMIT_MEMLOCK, &rl);
  mlockall(MCL_CURRENT | MCL_FUTURE);
  appliquerOrdonnancement(&params, "filtreur");

  unsigned char *bufEntree =
      (unsigned char *)tempsreel_malloc(tailleImage);
  unsigned char *bufSortie =
      (unsigned char *)tempsreel_malloc(tailleImage);

  if (bufEntree == NULL || bufSortie == NULL) {
    fprintf(stderr,
            "[filtreur] Erreur d'allocation tempsreel_malloc pour les buffers "
            "internes\n");
    return -1;
  }

  while (1) {
    evenementProfilage(&profInfos, ETAT_ATTENTE_MUTEXLECTURE);
    attenteLecteur(&zoneEntree);

    evenementProfilage(&profInfos, ETAT_TRAITEMENT);
    memcpy(bufEntree, zoneEntree.data, tailleImage);
    signalLecteur(&zoneEntree);

    if (typeFiltre == 0)
      lowpassFilter(haut, larg, bufEntree, bufSortie, 3, 5.0f, canaux);
    else
      highpassFilter(haut, larg, bufEntree, bufSortie, 3, 5.0f, canaux);

    evenementProfilage(&profInfos, ETAT_ATTENTE_MUTEXECRITURE);
    attenteEcrivain(&zoneSortie);

    evenementProfilage(&profInfos, ETAT_TRAITEMENT);
    memcpy(zoneSortie.data, bufSortie, tailleImage);
    signalEcrivain(&zoneSortie);

    if (params.modeOrdonnanceur == ORDONNANCEMENT_DEADLINE) {
      sched_yield();
    }
  }

  tempsreel_free(bufEntree);
  tempsreel_free(bufSortie);
  return 0;
}
