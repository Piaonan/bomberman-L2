#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include "player.h"
#include "tool.h"
#define MAXI 512

extern int sockM, sockG;

/* Cette fonction permet de créer un nouveau joueur de code (int) couleur 
 * qui sera positionné aux coordonnées pos puis le renvoie */
joueur_t *nouveau_joueur(int couleur, coord_t position){
	joueur_t *j = malloc(sizeof(joueur_t));
	if(j==NULL)
		fatal_error("memory allocation failure in player.c on player\r\n");
	
	j->vie=3;
	j->couleur=couleur;
	j->direction='N';
	j->position = position;
	j->rayonBombe=3;
	j->vitesse=5;
	j->nombreBombe=1;
	j->mouvement=0;
	j->listeBombe=NULL;
	return j;
}

/* Cette fonction vérifie s'il y a une collision entre le joueur j
 * et les coordonnées pos avec un déplacement zs (vertical) et qd (horizontal) */
bool collision_joueur(coord_t position, joueur_t j, int zs, int qd){
	if(position.h==(j.position.h-zs) && position.l==(j.position.l-qd))
		return true;
	return false;
}

/* Cette fonction vérifie si le joueur j peut bouger selon sa vitesse */
bool joueur_latent(joueur_t* j){
	if (j->mouvement==j->vitesse){
		j->mouvement=0; 
		return true;
	}
	j->mouvement++;
	return false;
}

/* Cette fonction change l'apparence de (joueur_t*) j selon son futur déplacement zs (vertical) et qd (horizontal) */
void redirection(joueur_t *j, int zs, int qd){
	if(zs) j->direction='N';
	else if(qd) j->direction='Z';
}

/* Cette fonction permet l'affichage des joueurs (joueur_t* j1, joueur_t* j2) en appliquant 
 * un décalage de (coord_t) pos (afin de centrer) */
void afficher_joueurs(joueur_t* j1, joueur_t* j2, coord_t pos1, coord_t pos2){
	afficher_joueur_unique(j1,pos1,pos2,0,0);
	afficher_joueur_unique(j2,pos1,pos2,0,0);
}

/* Cette fonction affiche (joueur_t*) j ainsi que ses bombes lors d'un déplacement 
 * zs (vertical) qs (horizontal) et toujours avec un décalage de (coord_t) pos après 
 * avoir effacé la position actuelle du joueur sur le terminal */
void afficher_joueur_unique(joueur_t* j, coord_t pos1, coord_t pos2, int zs, int qd){
	char buffer[MAXI]={0};
	redirection(j, zs, qd);
	
	sprintf(buffer, "\x1b[%d;%dH ", 
		j->position.h+1+pos1.h, j->position.l+1+pos1.l);
	
	j->position.h+=zs;
	j->position.l+=qd;
	sprintf(buffer, "%s\x1b[%d;%dH\x1b[%dm%c\x1b[37m", 
		buffer, j->position.h+1+pos1.h, j->position.l+1+pos1.l, j->couleur, j->direction);
	write(1, buffer, strlen(buffer));
	
	if(sockG!=0){
		sprintf(buffer, "\x1b[%d;%dH ", 
		j->position.h+1+pos2.h-zs, j->position.l+1+pos2.l-qd);
		
		sprintf(buffer, "%s\x1b[%d;%dH\x1b[%dm%c\x1b[37m", 
			buffer, j->position.h+1+pos2.h, j->position.l+1+pos2.l, j->couleur, j->direction);
		envoyer_des_tas(buffer);
	}
	if(j->listeBombe!=NULL
	&& (zs!=0 || qd!=0)) afficher_bombe(j->listeBombe,pos1,pos2);
}

/* FONCTIONS POUR LES BOMBES */

/* Cette fonction affiche toutes les bombes de la (bombe_t*) listeBombe 
 * avec un décalage de (coord_t) pos */
void afficher_bombe(bombe_t* listeBombe, coord_t pos1, coord_t pos2){
	char buffer[MAXI]={0}, 
	*bufferOpti = calloc(1,sizeof(char)), *optiBuffer = NULL;
	if(sockG!=0) optiBuffer = calloc(1,sizeof(char));
	if(bufferOpti==NULL 
	|| (sockG!=0 && optiBuffer == NULL)) 
		fatal_error("memory allocation failure in player.c on afficher_bombe");
	
	while(listeBombe != NULL){
		sprintf(buffer, "\x1b[%d;%dH\x1b[91;5m@\x1b[37;0m", 
			listeBombe->position.h+pos1.h+1, listeBombe->position.l+pos1.l+1);
		bufferOpti = optimisation(bufferOpti,buffer);
		if(sockG!=0){
			sprintf(buffer, "\x1b[%d;%dH\x1b[91;5m@\x1b[37;0m", 
				listeBombe->position.h+pos2.h+1, listeBombe->position.l+pos2.l+1);
			optiBuffer = optimisation(optiBuffer,buffer);
		}
		listeBombe = listeBombe->suivante;
	}
	write(1, bufferOpti, strlen(bufferOpti));
	free(bufferOpti);
	if(sockG!=0){
		envoyer_des_tas(optiBuffer);
		free(optiBuffer);
	}
}

/* Cette fonction construit une (bombe_t*) bombe à (joueur_t*) j ayant 
 * la position '(coord_t) position' puis la renvoie */
bombe_t* nouvelle_bombe(joueur_t* j,coord_t position){
	bombe_t* bombe = malloc(sizeof(bombe_t));
	if(bombe==NULL)
		fatal_error("memory allocation failure in player.c on bomb\r\n");
	
	bombe->timer = time(NULL);
	bombe->position = position;
	bombe->suivante = NULL;
	bombe->j = j;
	return bombe;
}

/* Cette fonction ajoute dans la liste de bombe de (joueur_t*) j,
 * une nouvelle bombe ayant la position '(coord_t) position' */
void ajouter_bombe(joueur_t* j,coord_t position){
	bombe_t* bombe = nouvelle_bombe(j,position);
	if(j->listeBombe == NULL){
		j->listeBombe = bombe;
	}
	else{
		bombe_t* prochaine = j->listeBombe;
		while(prochaine->suivante != NULL){
			prochaine = prochaine->suivante;
		}
		prochaine->suivante = bombe;
	}
	j->nombreBombe--;
}

/* Cette fonction vérifie s'il y a ou non une collision entre une bombe en regardant les listes de bombes 
 * (bombe_t*) liste_bombe_j1 et (bombe_t*) liste_bombe_j2 par rapport à 
 * une position actuelle (coord_t) position qui se déplacera
 * prochainement de (int) zs (vertical) et (int) qd (horizontal) */
bool collision_bombe(bombe_t* liste_bombe_j1,bombe_t* liste_bombe_j2,coord_t position,int zs,int qd){
	while(liste_bombe_j1 != NULL){
		if((liste_bombe_j1->position.h-zs) == position.h 
		&& (liste_bombe_j1->position.l-qd) == position.l) return true;
		liste_bombe_j1 = liste_bombe_j1->suivante;
	}
	while(liste_bombe_j2 != NULL){
		if((liste_bombe_j2->position.h-zs) == position.h 
		&& (liste_bombe_j2->position.l-qd) == position.l) return true;
		liste_bombe_j2 = liste_bombe_j2->suivante;
	}
	return false;
}

/* Cette fonction est le destructeur des bombes, 
 * il détruit les listes (bombe_t*) liste_bombe_j1 et (bombe_t*) liste_bombe_j2  */
void liberez_les_bombes(bombe_t* liste_bombe_j1,bombe_t* liste_bombe_j2){
	bombe_t* save = NULL;
	while(liste_bombe_j1 != NULL){
		save = liste_bombe_j1->suivante;
		free(liste_bombe_j1);
		liste_bombe_j1 = save;
	}
	while(liste_bombe_j2 != NULL){
		save = liste_bombe_j2->suivante;
		free(liste_bombe_j2);
		liste_bombe_j2 = save;
	}
}

/* Cette fonction détruit (joueur_t*) j1 (joueur_t*) j2 et leurs bombes */
void joueur_libre(joueur_t *j1, joueur_t *j2){
	liberez_les_bombes(j1->listeBombe,j2->listeBombe);
	free(j1);
	free(j2);
}

/* Cette fonction vérifie si la bombe b a été posée il y a plus de 5 secondes */
bool bombe_latente(bombe_t* b){
	if(b==NULL) return true;
	time_t duration = time(NULL) - b->timer;
	if (3 > duration) 
		return true;
	return false;
}
