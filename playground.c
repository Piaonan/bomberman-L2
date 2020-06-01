#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include "playground.h"
#include "player.h"
#include "tool.h"
#define MAXI 512

extern int sockM, sockG;

/* Cette fonction cherche une place libre pour les joueurs */
coord_t place_libre(coord_t hl, case_t mur[hl.h][hl.l], bool b){
	coord_t pos={0,0};
	int op1 = b==true? 1:-1,
	op2 = b==true? 0:1, h, l;
	
	for(int i=1; i<hl.h-1; i++){
		h=i*op1+(hl.h-1)*op2;
		for(int j=1; j<hl.l-1; j++){
			l=j*op1+(hl.l-1)*op2;
			if(mur[h][l].vie==0
			&&mur[h][l].type==NORMAL){
				pos.h=h; pos.l=l;
				return pos;
			}
		}
	}
	return pos;
}

/* Cette fonction permet de parser le niveau */
void nouveau_mur(int fd, coord_t hl, case_t mur[hl.h][hl.l]){
	char buffer[MAXI]={0};
	int modulo=0;
	
	for(int i=0; i<hl.h*2; i++){
		if(read(fd, buffer, hl.l)<1){
			fatal_error("lecture impossible\r\n");
		}
		
		for(int j=0; j<hl.l; j++){
			if(i<hl.h){
				if(buffer[j]=='0'){
					mur[i][j].vie=-1;
				} else if (buffer[j]==' '){
					mur[i][j].vie=0;
				} else {
					mur[i][j].vie=buffer[j]-'0';
				}
			} else { 
				modulo = i % hl.h;
				if(buffer[j]=='@'){
					mur[modulo][j].type=NOMBRE;
				} else if (buffer[j]=='+'){
					mur[modulo][j].type=VITESSE;
				} else if (buffer[j]=='*'){
					mur[modulo][j].type=RAYON;
				} else {
					mur[modulo][j].type=NORMAL;
				}
			}
		}
		saut(fd);
	}
}

/* Cette fonction est le constructeur d'un terrain_t */
terrain_t nouveau_terrain(int fd,int h,int l, case_t mur[h][l]){
	terrain_t t;
	t.hl.h = h; t.hl.l = l;
	nouveau_mur(fd,t.hl,mur);
	
	t.j1 = nouveau_joueur(32,place_libre(t.hl, mur, false));
	t.j2 = nouveau_joueur(35,place_libre(t.hl, mur, true));
	
	t.discordance = malloc(sizeof(coord_t));
	t.distorsion = malloc(sizeof(coord_t));
	if(t.discordance==NULL
	||t.distorsion==NULL) 
		fatal_error("memory allocation failure in playground.c on nouveau_terrain\r\n");
	
	t.discordance->h = 0, t.discordance->l = 0;
	if(sockG!=0){
		int h_terminal, l_terminal;
		mesurer2(&h_terminal,&l_terminal);
		t.distorsion->h=((h_terminal-t.hl.h)/2)>0? (h_terminal-t.hl.h)/2:0;
		t.distorsion->l=((l_terminal-t.hl.l)/2)>0? (l_terminal-t.hl.l)/2:0;
	}
	
	close(fd);
	return t;
}

/* Cette fonction calcule l'emplacement du rayon lors de l'explosion d'une bombe */
void rayonner(int op1, int op2, joueur_t *j, terrain_t t, case_t mur[t.hl.h][t.hl.l]){
	char buffer[MAXI]={0},
	*bufferOpti = calloc(1,sizeof(char)), *optiBuffer = NULL;
	if(sockG!=0) optiBuffer = calloc(1,sizeof(char));
	if(bufferOpti==NULL 
	|| (sockG!=0 && optiBuffer == NULL))
		fatal_error("memory allocation failure in playground.c on afficher_rayon\r\n");
	
	int rayon = j->rayonBombe,
	hauteur = j->listeBombe->position.h,
	largeur = j->listeBombe->position.l;	
	
	for(int i=1; i<=rayon;i++){
		if(hauteur+i*op1 < 1
		|| hauteur+i*op1 > (t.hl.h-2)
		|| largeur+i*op2 < 1 
		|| largeur+i*op2 > (t.hl.l-2)
		|| mur[hauteur+i*op1][largeur+i*op2].vie==-1) {
			afficher_rayon(bufferOpti,optiBuffer);
			return;
		}
		if(mur[hauteur+i*op1][largeur+i*op2].vie>0)mur[hauteur+i*op1][largeur+i*op2].vie--; 
		else if(t.j1->position.h == hauteur+i*op1 
		&& t.j1->position.l == largeur+i*op2)t.j1->vie--;
		else if(t.j2->position.h == hauteur+i*op1 
		&& t.j2->position.l == largeur+i*op2)t.j2->vie--;
		sprintf(buffer,"\x1b[%d;%dH\x1b[91mO\x1b[37m",
			t.discordance->h+hauteur+1+i*op1,t.discordance->l+largeur+1+i*op2);
		bufferOpti = optimisation(bufferOpti,buffer);
		if(sockG!=0){
			sprintf(buffer,"\x1b[%d;%dH\x1b[91mO\x1b[37m",
				t.distorsion->h+hauteur+1+i*op1,t.distorsion->l+largeur+1+i*op2);
			optiBuffer = optimisation(optiBuffer,buffer);
		}
	}
	
	afficher_rayon(bufferOpti,optiBuffer);
}

/* Cette fonction renvoie l'issue du jeu */
int champion(terrain_t t){
	if(t.j1->vie<=0 && t.j2->vie<=0) return 3;
	else if(t.j1->vie<=0) return 2;
	else if(t.j2->vie<=0) return 1;
  
	return 0;
}

/* Cette fonction vérifie si des bombes vont exploser et le cas échéant affiche
 * leurs explosions puis renvoie l'issue du jeu */
int explosion(joueur_t* j, terrain_t t, case_t mur[t.hl.h][t.hl.l]){
	static struct timespec nano = {0, 150000000};
	bool explose = false;
	int victoire = 0;
	bombe_t *save = NULL;
	
	if(j->listeBombe!=NULL){
		while(j->listeBombe!=NULL && !bombe_latente(j->listeBombe)){
			explose = true;
			rayonner(1, 0, j, t, mur); // Hauteur + 
			rayonner(-1, 0, j, t, mur); // Hauteur -
			rayonner(0, 1, j, t, mur); // Largeur +
			rayonner(0, -1, j, t, mur); // Largeur -
			
			if(j->listeBombe->position.h == j->position.h 
			&& j->listeBombe->position.l == j->position.l)j->vie--;
			victoire = champion(t);
			
			save = j->listeBombe;
			j->listeBombe = save->suivante;
			j->nombreBombe++;
			free(save);
		}
		if(explose) {
			nanosleep(&nano,NULL);
			victoire = victoire==0? afficher_mur(t,mur):victoire;
		}
	}
	return victoire;
}

/* Cette fonction affiche le terrain et renvoie l'issue du jeu */
int afficher_terrain(terrain_t t, case_t mur[t.hl.h][t.hl.l]){
	if(t.j1->listeBombe!=NULL) 
		afficher_bombe(t.j1->listeBombe,*(t.discordance),*(t.distorsion));
	if(t.j2->listeBombe!=NULL) 
		afficher_bombe(t.j2->listeBombe,*(t.discordance),*(t.distorsion));
	
	afficher_joueurs(t.j1, t.j2, *(t.discordance), *(t.distorsion));
	
	int victoire = explosion(t.j1,t,mur);
	return victoire==0? explosion(t.j2,t,mur):victoire;
}

/* Cette fonction vérifie si le terminal est assez grand pour afficher la carte,
 * renvoie -1 si ce n'est pas le cas, renvoie 0 si c'est de la bonne taille,
 * renvoie 1 si c'est de la bonne taille et si la carte n'a jamais été affichée auparavant */
int tailler(terrain_t t){
	char buffer[MAXI]={0};
	static int hauteur = 0, largeur = 0;
	int code = 0, h_terminal = 0, l_terminal = 0;
	initialiser();
	
	mesurer1(&h_terminal,&l_terminal);
	
	if(hauteur!=h_terminal
	|| largeur!=l_terminal
	|| t.discordance->h==0
	|| t.discordance->l==0) {
		nettoyer();
		code = 1; 
	}
	
	if (h_terminal-2 < t.hl.h 
	|| l_terminal-2 < t.hl.l) {
		sprintf(buffer,"Le terminal est trop petit, \r\nredimensionnez-le ou appuyez sur e.");
		write(1,buffer,strlen(buffer));
		if(sockG!=0) envoyer_des_tas("Le master a des soucis de taille...");
		return -1;
	}
	
	t.discordance->h=(h_terminal-t.hl.h)/2;
	t.discordance->l=(l_terminal-t.hl.l)/2;
	
	hauteur = h_terminal, largeur = l_terminal;
	return code;
}

/* Cette fonction permet d'afficher tous les murs du terrain */
int afficher_mur(terrain_t t, case_t mur[t.hl.h][t.hl.l]){
	char buffer[MAXI]={0},
	*bufferOpti = calloc(1,sizeof(char)), *optiBuffer = NULL;
	if(sockG!=0) optiBuffer = calloc(1,sizeof(char));
	if(bufferOpti==NULL 
	|| (sockG!=0 && optiBuffer == NULL)) 
		fatal_error("memory allocation failure in playground.c on afficher_mur\r\n");
	
	for(int i=0; i<t.hl.h; i++){
		sprintf(buffer,"\x1b[%d;%dH",t.discordance->h+i+1,t.discordance->l+1);
		bufferOpti = optimisation(bufferOpti,buffer);
		if(sockG!=0){
			sprintf(buffer,"\x1b[%d;%dH",t.distorsion->h+i+1,t.distorsion->l+1);
			optiBuffer = optimisation(optiBuffer,buffer);
		}
		for(int j=0; j<t.hl.l; j++){
			if(mur[i][j].vie==-1){
				bufferOpti = optimisation(bufferOpti,"0");
				if(sockG!=0) optiBuffer = optimisation(optiBuffer,"0");;
			} else if(mur[i][j].vie>0){
				sprintf(buffer,"%c",mur[i][j].vie+'0');
				bufferOpti = optimisation(bufferOpti,buffer);
				if(sockG!=0) optiBuffer = optimisation(optiBuffer,buffer);
			} else if(mur[i][j].type==NORMAL){
				bufferOpti = optimisation(bufferOpti," ");
				if(sockG!=0) optiBuffer = optimisation(optiBuffer," ");
			} else if(mur[i][j].type==VITESSE){
				bufferOpti = optimisation(bufferOpti,"+");
				if(sockG!=0) optiBuffer = optimisation(optiBuffer,"+");
			} else if(mur[i][j].type==RAYON){
				bufferOpti = optimisation(bufferOpti,"*");
				if(sockG!=0) optiBuffer = optimisation(optiBuffer,"*");
			} else {
				bufferOpti = optimisation(bufferOpti,"@");
				if(sockG!=0) optiBuffer = optimisation(optiBuffer,"@");
			}
		}
	}
	write(1,bufferOpti,strlen(bufferOpti));
	free(bufferOpti);
	if(sockG!=0){
		envoyer_des_tas(optiBuffer);
		free(optiBuffer);
	}
	return afficher_terrain(t,mur);
}

/* Cette fonction exécute le déplacement du joueur si c'est possible */
void essayer_bouger(terrain_t t, case_t mur[t.hl.h][t.hl.l], joueur_t *j1, joueur_t *j2, int zs, int qd){
	if(joueur_latent(j1)) return;
	if(mur[j1->position.h+zs][j1->position.l+qd].vie==0 
	&& !collision_joueur(j1->position,*j2,zs,qd) 
	&& !collision_bombe(j1->listeBombe,j2->listeBombe,j1->position,zs,qd)){
		afficher_joueur_unique(j1, *(t.discordance),*(t.distorsion), zs, qd);
		if(mur[j1->position.h][j1->position.l].type==NORMAL){
			return;
		} else if(mur[j1->position.h][j1->position.l].type==VITESSE){
			j1->vitesse++;
		} else if(mur[j1->position.h][j1->position.l].type==RAYON){
			j1->rayonBombe++;
		} else {
			j1->nombreBombe++;
		}
		mur[j1->position.h][j1->position.l].type=NORMAL;
	}
}

/* Cette fonction pose une bombe de (joueur_t*) j1 si c'est possible */
void essayer_poser(joueur_t *j){
	coord_t posBombe = j->position;
	if(collision_bombe(j->listeBombe,NULL,posBombe,0,0)
	|| j->nombreBombe == 0) return;
	
	ajouter_bombe(j,posBombe);
}

/* Cette fonction vérifie s'il y a besoin ou non d'une actualisation de (terrain_t) t */
bool actualisation(terrain_t t){
	if(t.j1->listeBombe==NULL && t.j2->listeBombe==NULL) return false;
	return !bombe_latente(t.j1->listeBombe) || !bombe_latente(t.j2->listeBombe);
}

void terrain_libre(terrain_t t){
	joueur_libre(t.j1,t.j2);
	free(t.discordance);
	free(t.distorsion);
}
