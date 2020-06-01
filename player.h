#ifndef PLAYER_H_DU_PROJET
#define PLAYER_H_DU_PROJET

typedef struct coord_s {
	int h; //hauteur
	int l; //largeur
}coord_t;

typedef struct joueur_s{
	int vie;
	char direction;
	coord_t position;
	struct bombe_s* listeBombe;
	int couleur;
	int nombreBombe;
	int vitesse;
	int mouvement;
	int rayonBombe; 
}joueur_t;

typedef struct bombe_s {
	time_t timer;
	coord_t position;
	joueur_t* j;
	struct bombe_s* suivante;
}bombe_t;

//FONCTIONS POUR LES JOUEURS
joueur_t *nouveau_joueur(int couleur,coord_t position);
bool collision_joueur(coord_t position, joueur_t j, int zs, int qd);
bool joueur_latent(joueur_t *j);
void joueur_libre(joueur_t *j1, joueur_t *j2);
void afficher_joueurs(joueur_t* j1, joueur_t* j2, coord_t pos1, coord_t pos2);
void afficher_joueur_unique(joueur_t* j, coord_t pos1, coord_t pos2, int zs, int qd);

//FONCTIONS POUR LES BOMBES
bool collision_bombe(bombe_t*,bombe_t*,coord_t,int,int);
void ajouter_bombe(joueur_t*,coord_t);
void afficher_bombe(bombe_t*,coord_t,coord_t);
bool bombe_latente(bombe_t*);
#endif


