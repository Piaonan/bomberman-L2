#include "menu.h"
#include "tool.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <poll.h>
#include <sys/socket.h>
#define MAXI 1024

extern int sockM, sockG;


/* Cette fonction prend en paramètre un const char* renvoie le menu_t* 
 * construit nommé texte  */
menu_t* construire_menu(const char* texte){
	menu_t* newMenu = malloc(sizeof(menu_t));
	if(newMenu==NULL)
		fatal_error("memory allocation failure in menu.c on construire_menu\r\n");
	
	newMenu->nom_menu = calloc(strlen(texte)+1,sizeof(char));
	if(newMenu->nom_menu==NULL)
		fatal_error("memory allocation failure in menu.c on construire_menu\r\n");
	strcpy(newMenu->nom_menu,texte);
	newMenu->nbAction = 0;
	newMenu->tab_actions = NULL;
	return newMenu;
}

/* Cette fonction crée une action nommée texte (const char*) dans le menu_t* m et 
 * associée à la fonction f (int(*)(const char*)) */
void ajouter_action_menu(menu_t* m, const char* texte, int(*f)(const char*)){
	m->tab_actions = realloc(m->tab_actions,sizeof(action_t*)*(m->nbAction+1));
	if(m->tab_actions==NULL)
		fatal_error("memory allocation failure in menu.c on ajouter_action_menu\r\n");
	
	m->tab_actions[m->nbAction] = malloc(sizeof(action_t));
	if(m->tab_actions[m->nbAction]==NULL)
		fatal_error("memory allocation failure in menu.c on ajouter_action_menu\r\n");
	
	m->tab_actions[m->nbAction]->fonction = f;
	
	m->tab_actions[m->nbAction]->nom_action = calloc(strlen(texte)+1,sizeof(char));
	if(m->tab_actions[m->nbAction]->nom_action==NULL)
		fatal_error("memory allocation failure in menu.c on ajouter_action_menu\r\n");
	strcpy(m->tab_actions[m->nbAction]->nom_action,texte);
	
	m->nbAction++;
}

/* Cette fonction crée un menu et l'envoie à la fin après avoir 
 * parcouru tous les mods du jeu */
menu_t* lister (int(*connecter)(const char*),int(*jouer)(const char*),int(*quitter)(const char*)){
	menu_t *m = construire_menu("Choisissez une option !");
	ajouter_action_menu(m,"Se connecter au serveur",connecter);
	
	DIR * dossier = opendir(".");
	struct dirent *mod = NULL;
	while((mod=readdir(dossier))!=NULL){
		if(mod->d_type==DT_DIR&&mod->d_name[0]!='.'){
			ajouter_action_menu(m,mod->d_name,jouer);
		}
	}
	closedir(dossier);
	
	ajouter_action_menu(m,"Quitter le jeu",quitter);
	return m;
}

/* Cette fonction est le destructeur de (menu_t*) m */
void liberez_le_menu(menu_t* m){
	free(m->nom_menu);
	for(int i=0;i<m->nbAction;i++){
		free(m->tab_actions[i]->nom_action);
		free(m->tab_actions[i]);
	}
	free(m->tab_actions);
	free(m);
}

/* Cette fonction affiche toutes les action_t de (menu_t*) m et utilise (int) choix
 * afin de surligner la position choix actuel */
void afficher_menu(menu_t* m, int choix){
	char buffer[MAXI]={0};
	
	sprintf(buffer, "\x1b[H\x1b[2J->%s<-\r\n", m->nom_menu);
	
	for(int i = 0;i<m->nbAction;i++){
		if(choix == i)
		sprintf(buffer, "%s\x1b[33;7;5mOption %d - %s\x1b[37;0m\r\n",
		buffer,i+1,m->tab_actions[i]->nom_action);
		else sprintf(buffer, "%sOption %d - %s\r\n",buffer,i+1,m->tab_actions[i]->nom_action);
	}
	write(1, buffer, strlen(buffer));
	if(sockG!=0) envoyer_des_tas(buffer);
}

/* Cette fonction permet d'augmenter les points de chaque joueur (int* j1) et (int* j2) 
 * reçus en paramètre en fonction de (int) score */
void pointer(int score, int *j1, int *j2){
	if(score==1) (*j1)++;
	else if(score==2) (*j2)++;
}

/* Cette fonction permet de lire le deroulement.txt du mod choisi qui se trouve dans le tab_actions
 * de (menu_t*) m et correspondant au (int) choix afin de savoir dans quel ordre effectuer
 * les niveaux ainsi que de compter les points */
void commencement(menu_t* m, int choix){
	char buffer[MAXI]={0}, c = 'x';
	sprintf(buffer,"%s/deroulement.txt",m->tab_actions[choix]->nom_action);
	int fd = open (buffer,O_RDONLY), j1 = 0, j2 = 0, score = 0;
	if (fd==-1) {
		fatal_error("ouverture impossible\r\n");
	}
	
	saut(fd);
	buffer[0]='\0';
	char buffer_bis[MAXI]={0};
	while(read(fd,&c,1)==1){
		if(c=='\r') {
			sprintf(buffer_bis,
			"%s/niveaux/%s",m->tab_actions[choix]->nom_action,buffer);
			score = m->tab_actions[choix]->fonction(buffer_bis);
			buffer[0]='\0';
			pointer(score, &j1, &j2);
		}
		else if(c!='\n')sprintf(buffer,"%s%c",buffer,c);
	}
	if(strlen(buffer)!=0){
		sprintf(buffer_bis,
			"%s/niveaux/%s",m->tab_actions[choix]->nom_action,buffer);
		score = m->tab_actions[choix]->fonction(buffer_bis);
		pointer(score, &j1, &j2);
	}
	close(fd);
	fin_de_la_partie(j1,j2);
}

/* Cette fonction permet d'afficher le (menu_t*) m et de lancer commencement sur l'action choisie
 * S'il s'agit de la dernière action sélectionnée on ne lance pas commencement (car il s'agit de quitter) */
void lancer_menu(menu_t* m){
	char c = 'x';
	static int choix = 0;
	
	int nbFD = sockG==0? 1:2;
	struct pollfd fds[nbFD];
	fds[0].fd = 0;
	fds[0].events = POLLIN;
	fds[0].revents = 0;
	if(nbFD==2){
		fds[1].fd = sockG;
		fds[1].events = POLLIN;
		fds[1].revents = 0;
	}
	afficher_menu(m,choix);
	
	do{
		if(fds[0].revents!=0 && read(fds[0].fd,&c,1)){
			if (c=='A') choix = choix==0? m->nbAction-1:(choix-1)%m->nbAction;
			else if (c=='B') choix = (choix+1)%m->nbAction;
			if (c=='A' || c=='B') afficher_menu(m,choix);
		} else if (nbFD==2 && fds[1].revents!=0){
			if(recv(sockG, &c, 1, 0) < 1 || c=='e'){
				socket_libre();
			}
			else if (c=='\r') c='x';
		}
	}while(c!='\r' && poll(fds,nbFD,-1)!=-1);
	
	if(choix==0) m->tab_actions[choix]->fonction("Connexion au serveur...\r\n");
	else if(choix==(m->nbAction-1)) m->tab_actions[choix]->fonction("Merci !\r\n");
	else commencement(m, choix);
}
