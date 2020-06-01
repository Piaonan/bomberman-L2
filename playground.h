#ifndef PLAYGROUND_H_DU_PROJET
#define PLAYGROUND_H_DU_PROJET
#include "player.h"

typedef struct case_s{
    int vie; /* -1 = indestructible, 0 = rien */
    
    enum {
		NORMAL, //sans boost
		VITESSE, //boost vitesse
		RAYON, //boost rayon des bombes
		NOMBRE //boost nombre maximum des bombes
    } type;
}case_t;

typedef struct terrain_s {
	coord_t hl;
	joueur_t *j1;
	joueur_t *j2;
	coord_t *discordance;
	coord_t *distorsion;
}terrain_t;

terrain_t nouveau_terrain(int fd, int h, int l, case_t mur[h][l]);
int tailler(terrain_t t);
int afficher_mur(terrain_t t, case_t mur[t.hl.h][t.hl.l]);
void essayer_bouger(terrain_t t, case_t mur[t.hl.h][t.hl.l], joueur_t *j1, joueur_t *j2, int zs, int qd);
void essayer_poser(joueur_t *j);
bool actualisation(terrain_t t);
void terrain_libre(terrain_t t);
#endif
