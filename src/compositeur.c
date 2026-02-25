/******************************************************************************
 * Laboratoire 3
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2026
 * Marc-André Gardner
 *
 * Programme compositeur
 *
 * Récupère plusieurs flux vidéos à partir d'espaces mémoire partagés et les
 * affiche directement dans le framebuffer de la carte graphique.
 *
 * IMPORTANT : CE CODE ASSUME QUE TOUS LES FLUX QU'IL REÇOIT SONT EN 427x240
 * (427 pixels en largeur, 240 en hauteur). TOUTE AUTRE TAILLE ENTRAINERA UN
 * COMPORTEMENT INDÉFINI. Les flux peuvent comporter 1 ou 3 canaux. Dans ce
 * dernier cas, ils doivent être dans l'ordre BGR et NON RGB.
 *
 * Le code permettant l'affichage est inspiré de celui présenté sur le blog
 * Raspberry Compote
 *(http://raspberrycompote.blogspot.ie/2014/03/low-level-graphics-on-raspberry-pi-part_14.html),
 * par J-P Rosti, publié sous la licence CC-BY 3.0.
 *
 * Merci à Yannick Hold-Geoffroy pour l'aide apportée pour la gestion
 * du framebuffer.
 ******************************************************************************/

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>

#include <sys/resource.h>
#include <sys/time.h>

#include <sys/types.h>

// Allocation mémoire, mmap et mlock
#include <sys/mman.h>

// Gestion des ressources et permissions
#include <sys/resource.h>

// Mesure du temps
#include <time.h>

// Obtenir la taille des fichiers
#include <sys/stat.h>

// Contrôle de la console
#include <linux/fb.h>
#include <linux/kd.h>

// Gestion des erreurs
#include <err.h>
#include <errno.h>

#include "allocateurMemoire.h"
#include "commMemoirePartagee.h"
#include "utils.h"

static unsigned char *g_imageGlobale = NULL;
static int g_currentPage = 0;

static void flushDisplay(int fbfd, unsigned char *fb, size_t hauteurFB,
                         struct fb_var_screeninfo *vinfoPtr, int fbLineLength) {
  g_currentPage = (g_currentPage + 1) % 2;
  unsigned char *dest = fb + g_currentPage * fbLineLength * hauteurFB;
  memcpy(dest, g_imageGlobale, fbLineLength * hauteurFB);
  vinfoPtr->yoffset = g_currentPage * vinfoPtr->yres;
  vinfoPtr->activate = FB_ACTIVATE_VBL;
  ioctl(fbfd, FBIOPAN_DISPLAY, vinfoPtr);
}

// Fonction permettant de récupérer le temps courant sous forme double
double get_time() {
  struct timeval t;
  struct timezone tzp;
  gettimeofday(&t, &tzp);
  return (double)t.tv_sec + (double)(t.tv_usec) * 1e-6;
}

// Cette fonction écrit l'image dans le framebuffer, à la position demandée.
// Elle est déjà codée pour vous, mais vous devez l'utiliser correctement. En
// particulier, n'oubliez pas que cette fonction assume que TOUTES LES IMAGES
// QU'ELLE REÇOIT SONT EN 427x240 (1 ou 3 canaux). Cette fonction peut gérer
// l'affichage de 1, 2, 3 ou 4 images sur le même écran, en utilisant la
// séparation préconisée dans l'énoncé. La position (premier argument) doit être
// un entier inférieur au nombre total d'images à afficher (second argument). Le
// troisième argument est le descripteur de fichier du framebuffer (nommé fbfb
// dans la fonction main()). Le quatrième argument est un pointeur sur le memory
// map de ce framebuffer (nommé fbd dans la fonction main()). Les cinquième et
// sixième arguments sont la largeur et la hauteur de ce framebuffer. Le
// septième est une structure contenant l'information sur le framebuffer (nommé
// vinfo dans la fonction main()). Le huitième est la longueur effective d'une
// ligne du framebuffer (en octets), contenue dans finfo.line_length dans la
// fonction main(). Le neuvième argument est le buffer contenant l'image à
// afficher, et les trois derniers arguments ses dimensions.
void ecrireImage(const int position, const int total, int fbfd,
                 unsigned char *fb, size_t largeurFB, size_t hauteurFB,
                 struct fb_var_screeninfo *vinfoPtr, int fbLineLength,
                 const unsigned char *data, size_t hauteurSource,
                 size_t largeurSource, size_t canauxSource) {
  if (g_imageGlobale == NULL)
    g_imageGlobale = (unsigned char *)calloc(fbLineLength * hauteurFB, 1);
  unsigned char *imageGlobale = g_imageGlobale;

  if (position >= total) {
    return;
  }

  const unsigned char *dataTraite = data;
  unsigned char *d = NULL;
  if (canauxSource == 1) {
    d = (unsigned char *)tempsreel_malloc(largeurSource * hauteurSource * 3);
    unsigned int pos = 0;
    for (unsigned int i = 0; i < hauteurSource; ++i) {
      for (unsigned int j = 0; j < largeurSource; ++j) {
        d[pos++] = data[i * largeurSource + j];
        d[pos++] = data[i * largeurSource + j];
        d[pos++] = data[i * largeurSource + j];
      }
    }
    dataTraite = d;
  }

  if (total == 1) {
    g_currentPage = (g_currentPage + 1) % 2;
    unsigned char *currentFramebuffer =
        fb + g_currentPage * fbLineLength * hauteurFB;
    for (unsigned int ligne = 0; ligne < hauteurSource; ligne++) {
      memcpy(currentFramebuffer + ligne * fbLineLength,
             dataTraite + ligne * largeurSource * 3, largeurFB * 3);
    }
  } else if (total == 2) {
    if (position == 0) {
      for (unsigned int ligne = 0; ligne < hauteurSource; ligne++) {
        memcpy(imageGlobale + ligne * fbLineLength,
               dataTraite + ligne * largeurSource * 3, largeurFB * 3);
      }
    } else {
      for (unsigned int ligne = hauteurSource; ligne < hauteurSource * 2;
           ligne++) {
        memcpy(imageGlobale + ligne * fbLineLength,
               dataTraite + (ligne - hauteurSource) * largeurSource * 3,
               largeurFB * 3);
      }
    }
  } else if (total == 3 || total == 4) {
    off_t offsetLigne = 0;
    off_t offsetColonne = 0;
    switch (position) {
    case 0:
      break;
    case 1:
      offsetColonne = largeurSource;
      break;
    case 2:
      offsetLigne = hauteurSource;
      break;
    case 3:
      offsetLigne = hauteurSource;
      offsetColonne = largeurSource;
      break;
    }
    offsetLigne *= fbLineLength;
    offsetColonne *= 3;
    for (unsigned int ligne = 0; ligne < hauteurSource; ligne++) {
      memcpy(imageGlobale + offsetLigne + offsetColonne,
             dataTraite + ligne * largeurSource * 3, largeurSource * 3);
      offsetLigne += fbLineLength;
    }
  }

  if (canauxSource == 1)
    tempsreel_free(d);

  if (total == 1) {
    vinfoPtr->yoffset = g_currentPage * vinfoPtr->yres;
    vinfoPtr->activate = FB_ACTIVATE_VBL;
    ioctl(fbfd, FBIOPAN_DISPLAY, vinfoPtr);
  }
}

#define MAX_FLUX 4

int main(int argc, char *argv[]) {
  int nbrActifs = 0; // Sera initialisé par parseArgs ci-dessous

  // On desactive le buffering pour les printf(), pour qu'il soit possible de
  // les voir depuis votre ordinateur
  setbuf(stdout, NULL);

  // Initialise le profilage
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

  char *files[5] = {
      "", "", "", "", NULL,
  };
  nbrActifs = (int)parseArgs(argc, argv, &params, files);
  if (nbrActifs < 1 || nbrActifs > MAX_FLUX) {
    fprintf(stderr,
            "[compositeur] Nombre de flux invalide: %d (attendu: 1-%d)\n",
            nbrActifs, MAX_FLUX);
    return -1;
  }

  struct memPartage zones[MAX_FLUX];
  struct timespec nextWakeup[MAX_FLUX];
  long period_ns[MAX_FLUX];
  int initialise[MAX_FLUX];
  long minPeriod_ns = 1000000000L;
  memset(initialise, 0, sizeof(initialise));

  for (int i = 0; i < nbrActifs; i++) {
    if (initMemoirePartageeLecteur(files[i], &zones[i]) != 0) {
      fprintf(stderr, "[compositeur] Échec initMemoirePartageeLecteur(%s)\n",
              files[i]);
      return -1;
    }
    uint32_t fps = zones[i].header->infos.fps;
    period_ns[i] = (fps > 0) ? (1000000000L / (long)fps) : 33333333L;
    if (period_ns[i] < minPeriod_ns)
      minPeriod_ns = period_ns[i];
    nextWakeup[i].tv_sec = 0;
    nextWakeup[i].tv_nsec = 0;
  }

  prepareMemoire(427 * 240 * 3, 427 * 240 * 3);
  struct rlimit rl = {RLIM_INFINITY, RLIM_INFINITY};
  setrlimit(RLIMIT_MEMLOCK, &rl);
  mlockall(MCL_CURRENT | MCL_FUTURE);
  appliquerOrdonnancement(&params, "compositeur");

  FILE *fstats = fopen("stats.txt", "w");
  if (fstats)
    setbuf(fstats, NULL);

  int stat_count[MAX_FLUX];
  double stat_max_delai_ms[MAX_FLUX];
  struct timespec stat_last_display[MAX_FLUX];
  memset(stat_count, 0, sizeof(stat_count));
  memset(stat_max_delai_ms, 0, sizeof(stat_max_delai_ms));

  struct timespec temps_debut;
  clock_gettime(CLOCK_MONOTONIC, &temps_debut);
  struct timespec last_dump = temps_debut;
  for (int i = 0; i < nbrActifs; i++)
    stat_last_display[i] = temps_debut;

  long int screensize = 0;
  int fbfd = open("/dev/fb0", O_RDWR);
  if (fbfd == -1) {
    perror("Erreur lors de l'ouverture du framebuffer ");
    return -1;
  }

  struct fb_var_screeninfo vinfo;
  struct fb_var_screeninfo orig_vinfo;
  struct fb_fix_screeninfo finfo;
  if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) {
    perror("Erreur lors de la requete d'informations sur le framebuffer ");
  }

  memcpy(&orig_vinfo, &vinfo, sizeof(struct fb_var_screeninfo));

  vinfo.bits_per_pixel = 24;
  switch (nbrActifs) {
  case 1:
    vinfo.xres = 427;
    vinfo.yres = 240;
    break;
  case 2:
    vinfo.xres = 427;
    vinfo.yres = 480;
    break;
  case 3:
  case 4:
    vinfo.xres = 854;
    vinfo.yres = 480;
    break;
  default:
    printf("Nombre de sources invalide!\n");
    return -1;
    break;
  }

  vinfo.xres_virtual = vinfo.xres;
  vinfo.yres_virtual = vinfo.yres * 2;
  if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &vinfo)) {
    perror("Erreur lors de l'appel a ioctl ");
  }

  if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) {
    perror("Erreur lors de l'appel a ioctl (2) ");
  }

  screensize = finfo.line_length * vinfo.yres * 2;
  if (finfo.smem_len > 0)
    screensize = finfo.smem_len;
  unsigned char *fbp = (unsigned char *)mmap(
      0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
  if (fbp == MAP_FAILED) {
    perror("Erreur lors du mmap de l'affichage ");
    return -1;
  }

  while (1) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    // for group flush
    int displayed_any = 0;

    // if some frames are late then we need a smaller sleep time to ensure we
    // display it as fast as possible without being blocking
    int any_overdue = 0;

    struct timespec nearest;
    int nearest_set = 0;

    for (int i = 0; i < nbrActifs; i++) {
      // strange issues happened with delay stacking and other float related
      // issues. forums said rpi0 could have issues with float precision. it
      // represents the time between next time for a frame and now, meaning if
      // its negative, the frame is "late" and we need to query for it!!
      long long diff_ns =
          (long long)(nextWakeup[i].tv_sec - now.tv_sec) * 1000000000LL +
          (nextWakeup[i].tv_nsec - now.tv_nsec);

      if (!(diff_ns > 0 && initialise[i])) {
        if (attenteLecteurAsync(&zones[i])) {
          evenementProfilage(&profInfos, ETAT_TRAITEMENT);
          ecrireImage(
              i, nbrActifs, fbfd, fbp, vinfo.xres, vinfo.yres, &vinfo,
              finfo.line_length, zones[i].data, zones[i].header->infos.hauteur,
              zones[i].header->infos.largeur, zones[i].header->infos.canaux);
          signalLecteur(&zones[i]);
          displayed_any = 1;

          struct timespec t_now;
          clock_gettime(CLOCK_MONOTONIC, &t_now);
          double delai_ms =
              (t_now.tv_sec - stat_last_display[i].tv_sec) * 1000.0 +
              (t_now.tv_nsec - stat_last_display[i].tv_nsec) * 1e-6;
          if (delai_ms > stat_max_delai_ms[i])
            stat_max_delai_ms[i] = delai_ms;
          stat_last_display[i] = t_now;
          stat_count[i]++;

          if (!initialise[i])
            initialise[i] = 1;

          clock_gettime(CLOCK_MONOTONIC, &nextWakeup[i]);
          nextWakeup[i].tv_nsec += period_ns[i];
          while (nextWakeup[i].tv_nsec >= 1000000000L) {
            nextWakeup[i].tv_nsec -= 1000000000L;
            nextWakeup[i].tv_sec++;
          }
        } else if (initialise[i]) {
          any_overdue = 1;
        }
      }

      if (initialise[i]) {
        if (!nearest_set || nextWakeup[i].tv_sec < nearest.tv_sec ||
            (nextWakeup[i].tv_sec == nearest.tv_sec &&
             nextWakeup[i].tv_nsec < nearest.tv_nsec)) {
          nearest = nextWakeup[i];
          nearest_set = 1;
        }
      }
    }

    // stats dump
    if (fstats) {
      double elapsed_total = (now.tv_sec - temps_debut.tv_sec) +
                             (now.tv_nsec - temps_debut.tv_nsec) * 1e-9;
      double elapsed_dump = (now.tv_sec - last_dump.tv_sec) +
                            (now.tv_nsec - last_dump.tv_nsec) * 1e-9;
      if (elapsed_dump >= 5.0) {
        fprintf(fstats, "[%.1f] ", elapsed_total);
        for (int i = 0; i < nbrActifs; i++) {
          double fps_moy =
              (elapsed_dump > 0.0) ? (stat_count[i] / elapsed_dump) : 0.0;
          fprintf(fstats, "Entree %d: moy=%.1f fps, max=%.1f ms | ", i + 1,
                  fps_moy, stat_max_delai_ms[i]);
          stat_count[i] = 0;
          stat_max_delai_ms[i] = 0.0;
        }
        fprintf(fstats, "\n");
        last_dump = now;
      }
    }

    if (displayed_any && nbrActifs > 1)
      flushDisplay(fbfd, fbp, vinfo.yres, &vinfo, finfo.line_length);

    long long sleep_ns = 0;
    if (nearest_set) {
      struct timespec now_sleep;
      clock_gettime(CLOCK_MONOTONIC, &now_sleep);
      long long diff_ns =
          (long long)(nearest.tv_sec - now_sleep.tv_sec) * 1000000000LL +
          (nearest.tv_nsec - now_sleep.tv_nsec);
      if (diff_ns > 0)
        sleep_ns = diff_ns;
    }

    if (any_overdue) {
      long long small = minPeriod_ns / 10;
      if (sleep_ns == 0 || small < sleep_ns)
        sleep_ns = small;
    }

    if (sleep_ns > 0) {
      if (sleep_ns > minPeriod_ns)
        sleep_ns = minPeriod_ns;
      struct timespec sleep_ts;
      sleep_ts.tv_sec = 0;
      sleep_ts.tv_nsec = sleep_ns;
      evenementProfilage(&profInfos, ETAT_ENPAUSE);
      nanosleep(&sleep_ts, NULL);
    } else if (displayed_any &&
               params.modeOrdonnanceur == ORDONNANCEMENT_DEADLINE) {
      sched_yield();
    }
  }

  munmap(fbp, screensize);

  if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &orig_vinfo)) {
    printf("Error re-setting variable information.\n");
  }
  close(fbfd);

  return 0;
}
