#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include "playground.h"
#include "player.h"
#include "menu.h"
#include "tool.h"
#define PORT 4000
#define MAXI 512

int sockM = 0, sockG = 0;
bool continuer = true;

/* Cette fonction permet de démarrer le jeu après avoir choisi un niveau,
 * le terrain se recharge à chaque seconde, affiche le joueur gagnant s'il y en a un,
 * un appui sur la touche e quitte directement le jeu */
int jouer (const char * fdname){
	int nbFD = 0, issue = 0, statut = 0,
	fd = dimensionner(fdname, &nbFD, &issue);
	
	case_t mur [nbFD][issue];
	terrain_t t = nouveau_terrain(fd,nbFD,issue,mur);
	issue = 0;
	
	nbFD = sockG==0? 1:2;
	struct pollfd fds[nbFD];
	fds[0].fd = 0;
	fds[0].events = POLLIN;
	if(nbFD==2){
		fds[1].fd = sockG;
		fds[1].events = POLLIN;
	}
	char c = '!';
	
	while(c!='e' && poll(fds,nbFD,1000)>=0 && !issue){
		
		if(nbFD==2 && fds[1].revents!=0){
			if((recv(sockG, &c, 1, 0) < 1) || c=='e'){
				socket_libre();
			}
			else if ((statut=tailler(t))!=-1){
				if(statut==1 && !issue) issue = afficher_mur(t,mur);
				if (c=='A') essayer_bouger(t,mur,t.j2,t.j1,-1,0);
				else if (c=='D') essayer_bouger(t,mur,t.j2,t.j1,0,-1);
				else if (c=='B') essayer_bouger(t,mur,t.j2,t.j1,1,0);
				else if (c=='C') essayer_bouger(t,mur,t.j2,t.j1,0,1);
				else if (c=='\r') essayer_poser(t.j2);
			}
		}
		
		if(fds[0].revents!=0 && read(fds[0].fd,&c,1) && (statut=tailler(t))!=-1){
			if(statut==1 && !issue) issue = afficher_mur(t,mur);
			if (c=='A') essayer_bouger(t,mur,t.j1,t.j2,-1,0);
			else if (c=='D') essayer_bouger(t,mur,t.j1,t.j2,0,-1);
			else if (sockM==0 && c=='z') essayer_bouger(t,mur,t.j2,t.j1,-1,0);
			else if (sockM==0 && c=='q') essayer_bouger(t,mur,t.j2,t.j1,0,-1);
			else if (c=='B') essayer_bouger(t,mur,t.j1,t.j2,1,0);
			else if (sockM==0 && c=='s') essayer_bouger(t,mur,t.j2,t.j1,1,0);
			else if (sockM==0 && c=='d') essayer_bouger(t,mur,t.j2,t.j1,0,1);
			else if (c=='C') essayer_bouger(t,mur,t.j1,t.j2,0,1);
			else if (c=='\r') essayer_poser(t.j1);
			else if (sockM==0 && c==' ') essayer_poser(t.j2);
		}
		
		if(!issue && (tailler(t)==1 || actualisation(t)))
			issue = afficher_mur(t,mur);
		
	}
	issue_du_niveau(issue,t.discordance->l*2+t.hl.l,t.distorsion->l*2+t.hl.l);
	terrain_libre(t);
	
	return issue;
}

/* Cette fonction permet de se connecter avec quelqu'un grâce aux sockets */
int connecter(const char *message){
	char buffer[MAXI]={0};
	nettoyer();
	if(sockM!=0){
		sprintf(buffer,"Aucune action requise...\r\n");
		write(1,buffer,strlen(buffer));
		sleep(2);
		return -1;
	}
	struct sockaddr_in sinM, sinG;
	socklen_t lenG = sizeof(sinG);
	int oui = 1;
	
	if ( (sockM = socket(AF_INET, SOCK_STREAM, 0))==-1 ){
		perror("socket ");
		sockM=0;
		sleep(2);
		return -1;
	}
	
	sprintf(buffer,"La socket %d est maintenant ouverte en mode TCP/IP\r\n%s", sockM, message);
	write(1,buffer,strlen(buffer));
		
	/* Configuration */
	sinM.sin_addr.s_addr = htonl(INADDR_ANY);   /* Adresse IP automatique */
	sinM.sin_family = AF_INET;                 /* Protocole familial (IP) */
	sinM.sin_port = htons(PORT);              /* Listage du port */
		
	if (setsockopt(sockM,SOL_SOCKET,SO_REUSEADDR,&oui,sizeof(int))==-1
	|| bind(sockM, (struct sockaddr*)&sinM, sizeof(sinM))==-1
	|| listen(sockM, 1)==-1){
		perror("setsocketopt | bind | listen ");
		sleep(2);
		fatal_error("exécutez avec la commande sudo pourrait fonctionner\r\n");
	}	
	
	sprintf(buffer,"Patientez pendant qu'un challenger se connecte sur le port %d...\r\n", PORT);
	write(1,buffer,strlen(buffer));        
	
	sockG = accept(sockM, (struct sockaddr*)&sinG, &lenG);
	
	sprintf(buffer,
		"Un challenger s'est connecté avec le socket %d de %s:%d\r\n", 
			sockG, inet_ntoa(sinG.sin_addr), htons(sinG.sin_port));
	write(1,buffer,strlen(buffer)); 
	sleep(2);
	return 0;
}

/* Cette fonction permet de quitter la boucle principale du programme */
int quitter(const char *message){
	write(1, message, strlen(message));
	if(sockM!=0){
		shutdown(sockG, 2);
		close(sockM);
	}
	continuer = false;
	return 0;
}

int main() {
	raw_mode();
	
	menu_t* m = lister(connecter,jouer,quitter);
	
	while(continuer) lancer_menu(m);
	
	liberez_le_menu(m);
	
	cooked_mode();
	
	return EXIT_SUCCESS;
}
