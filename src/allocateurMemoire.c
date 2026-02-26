/******************************************************************************
 * Laboratoire 3
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2026
 * Marc-André Gardner
 *
 * Fichier implémentant les fonctions de l'allocateur mémoire temps réel
 ******************************************************************************/

#include "allocateurMemoire.h"

typedef struct
{
    unsigned char *pool;
    unsigned char *libre;
    size_t nBlocs;
    size_t tailleBloc;
} AllocPool;

static AllocPool poolGros = {NULL, NULL, 0, 0};
static AllocPool poolPetit = {NULL, NULL, 0, 0};
static size_t _taille_gros_bloc = 0;

int prepareMemoire(size_t tailleImageEntree, size_t tailleImageSortie)
{
    size_t tailleGrosBloc = (tailleImageEntree > tailleImageSortie) ? tailleImageEntree : tailleImageSortie;

    if (tailleGrosBloc == 0)
        return -1;

    if (poolGros.pool != NULL)
    {
        free(poolGros.pool);
        free(poolGros.libre);
        poolGros.pool = NULL;
        poolGros.libre = NULL;
        poolGros.nBlocs = 0;
        poolGros.tailleBloc = 0;
    }
    if (poolPetit.pool != NULL)
    {
        free(poolPetit.pool);
        free(poolPetit.libre);
        poolPetit.pool = NULL;
        poolPetit.libre = NULL;
        poolPetit.nBlocs = 0;
        poolPetit.tailleBloc = 0;
    }

    poolGros.nBlocs = ALLOC_N_GROS_BLOCS;
    poolGros.tailleBloc = tailleGrosBloc;
    poolGros.pool = (unsigned char *)malloc(poolGros.nBlocs * poolGros.tailleBloc);
    poolGros.libre = (unsigned char *)malloc(poolGros.nBlocs);

    if (poolGros.pool == NULL || poolGros.libre == NULL)
        return -1;

    for (size_t i = 0; i < poolGros.nBlocs; ++i)
        poolGros.libre[i] = 1;

    poolPetit.nBlocs = ALLOC_N_PETITS_BLOCS;
    poolPetit.tailleBloc = ALLOC_TAILLE_PETIT;
    poolPetit.pool = (unsigned char *)malloc(poolPetit.nBlocs * poolPetit.tailleBloc);
    poolPetit.libre = (unsigned char *)malloc(poolPetit.nBlocs);

    if (poolPetit.pool == NULL || poolPetit.libre == NULL)
        return -1;

    for (size_t i = 0; i < poolPetit.nBlocs; ++i)
        poolPetit.libre[i] = 1;

    _taille_gros_bloc = tailleGrosBloc;
    return 0;
}

void *tempsreel_malloc(size_t taille)
{
    if (taille == 0)
        return NULL;

    if (_taille_gros_bloc > 0 && taille > _taille_gros_bloc)
    {
        fprintf(stderr,
                "[tempsreel_malloc] ATTENTION : demande de %zu octets > taille gros bloc (%zu octets). "
                "Vérifiez l'appel à prepareMemoire().\n",
                taille, _taille_gros_bloc);
        return malloc(taille);
    }

    AllocPool *pool = NULL;

    if (taille <= ALLOC_TAILLE_PETIT && poolPetit.pool != NULL)
        pool = &poolPetit;
    else if (poolGros.pool != NULL && taille <= poolGros.tailleBloc)
        pool = &poolGros;

    if (pool != NULL)
    {
        for (size_t i = 0; i < pool->nBlocs; ++i)
        {
            if (pool->libre[i])
            {
                pool->libre[i] = 0;
                return pool->pool + i * pool->tailleBloc;
            }
        }
        fprintf(stderr, "tempsreel_malloc: pool épuisé!\n");
    }

    return malloc(taille);
}

void tempsreel_free(void *ptr)
{
    if (ptr == NULL)
        return;

    if (poolGros.pool != NULL)
    {
        unsigned char *p = (unsigned char *)ptr;
        unsigned char *debut = poolGros.pool;
        unsigned char *fin = poolGros.pool + poolGros.nBlocs * poolGros.tailleBloc;
        if (p >= debut && p < fin)
        {
            size_t index = (size_t)(p - debut) / poolGros.tailleBloc;
            if (index < poolGros.nBlocs)
                poolGros.libre[index] = 1;
            return;
        }
    }

    if (poolPetit.pool != NULL)
    {
        unsigned char *p = (unsigned char *)ptr;
        unsigned char *debut = poolPetit.pool;
        unsigned char *fin = poolPetit.pool + poolPetit.nBlocs * poolPetit.tailleBloc;
        if (p >= debut && p < fin)
        {
            size_t index = (size_t)(p - debut) / poolPetit.tailleBloc;
            if (index < poolPetit.nBlocs)
                poolPetit.libre[index] = 1;
            return;
        }
    }

    free(ptr);
}