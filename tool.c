#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "tool.h"
#define MAXI 512

struct termios origin;
extern int sockM, sockG;

/* Cette fonction cache le curseur */
void dissimuler(){
	char buffer[MAXI]={0};
	sprintf(buffer, "\x1b[?25l");
	write(1, buffer, strlen(buffer));
}

/* Cette fonction permet de montrer de nouveau le curseur */
void montrer(){
	char buffer[MAXI]={0};
	sprintf(buffer, "\x1b[?25h");
	write(1, buffer, strlen(buffer));
}

/* Cette fonction permet de passer en mode non-canonique
 * et renvoie le termios origin afin de le restaurer lors de l'arrêt du programme */
void raw_mode (){
	struct termios raw;
	if(tcgetattr(0, &origin)==-1){
		perror("tcgetattr ");
		exit(EXIT_FAILURE);
	}
	raw = origin;
	
	cfmakeraw(&raw);
	if(tcsetattr(0, TCSAFLUSH, &raw)==-1){
		perror("tcsetattr ");
		exit(EXIT_FAILURE);
	}
	dissimuler();
}

/* Cette fonction permet de rétablir le terminal */
void cooked_mode(){
	tcsetattr(0, TCSAFLUSH, &origin);
	montrer();
}

/* Cette fonction permet d'afficher une erreur rencontrée et exit proprement */
void fatal_error(const char *message){
	nettoyer();
	write(2,message,strlen(message));
	cooked_mode();
	exit(EXIT_FAILURE);
}

/* Cette fonction avance le curseur du descripteur de fichier fd jusqu'au prochain \n */
void saut(int fd){
	char c;
	while( read(fd,&c,1)==1 && c!='\n' );
}

/* Cette fonction initialise la position du curseur actuelle à 0,0 */
void initialiser(){
	char buffer[MAXI]={0};
	sprintf(buffer,"\x1b[u");
	write(1,buffer,strlen(buffer));
	if(sockG!=0) envoyer_des_tas(buffer);
}

/* Cette fonction efface tout dans le terminal */
void nettoyer(){
	char buffer[MAXI]={0};
	sprintf(buffer,"\x1b[H\x1b[2J");
	write(1,buffer,strlen(buffer));
	if(sockG!=0) envoyer_des_tas(buffer);
}

/* Cette fonction envoie la taille du terminal (1) dans des pointeurs */
void mesurer1(int *h, int *l){
	struct winsize window;
	if(ioctl(0, TIOCGWINSZ, &window)==-1){
		fatal_error("récupération de la taille du terminal impossible\r\n");
	}
	(*h) = window.ws_row;
	(*l) = window.ws_col;
}

/* Cette fonction envoie la taille du terminal (2) dans des pointeurs */
void mesurer2(int *h, int *l){
	envoyer_des_tas("?");
	unsigned int dataRecv1, dataRecv2;
	if (recv(sockG, (char*)&dataRecv1, 4, 0) < 1
	|| recv(sockG, (char*)&dataRecv2, 4, 0) < 1){
		socket_libre();
	}
	(*h) = ntohl(dataRecv1);
	(*l) = ntohl(dataRecv2);
}

/* Cette fonction permet d'afficher le message de victoire selon l'issue 'valeur' du jeu */
void message_de_victoire(int valeur, int l_terminal, bool sockets){
	char buffer[MAXI]={0};
	
	int centrer_l;
	if(valeur == 3){
		centrer_l = (l_terminal-11)/2;
		sprintf(buffer, "\x1b[1;%dHEx aequo...",centrer_l+1);
	} else {
		centrer_l = valeur==1? (l_terminal-33)/2:(l_terminal-35)/2;
		sprintf(buffer,
			"\x1b[1;%dHLe joueur %s remporte le niveau.",
				centrer_l+1,
					valeur==1? "vert":"violet");
	}
	
	if(!sockets) write(1, buffer, strlen(buffer));
	else envoyer_des_tas(buffer);
}

/* Cette fonction mesure la taille d'un terminal avant d'appeler la fonction
 * message_de_victoire pour que le message soit centré */
void issue_du_niveau(int issue, int l1, int l2){
	if (issue == 0) return;
	message_de_victoire(issue,l1,false);
	if(sockG!=0){
		message_de_victoire(issue,l2,true);
	}
	sleep(1);
}

/* Cette fonction affiche un message lors de la fin d'un mod */
void fin_de_message(int j1, int j2, int h_terminal, int l_terminal, bool sockets){
	char buffer[MAXI]={0};
	
	int	centrer_h = (h_terminal-3)/2>0? (h_terminal-3)/2:0,
	centrer_l = (l_terminal-18)/2>0? (l_terminal-18)/2:0;
	
	sprintf(buffer,
		"\x1b[H\x1b[2J\x1b[%d;%dHFIN DE LA PARTIE !",centrer_h+1,centrer_l+1);
	centrer_l = (l_terminal-31)/2>1? (l_terminal-31)/2:0;
	sprintf(buffer,
		"%s\x1b[%d;%dHLE JOUEUR VERT A EU %d POINTS...",buffer,centrer_h+2,centrer_l+1,j1);
	centrer_l = (l_terminal-33)/2>1? (l_terminal-33)/2:0;
	sprintf(buffer,
		"%s\x1b[%d;%dHLE JOUEUR VIOLET A EU %d POINTS...",buffer,centrer_h+3,centrer_l+1,j2);
	if(!sockets) write(1,buffer,strlen(buffer));
	else envoyer_des_tas(buffer);
}

/* Cette fonction mesure la taille d'un terminal avant d'appeler la fonction
 * fin_de_message pour que le message soit centré */
void fin_de_la_partie(int j1, int j2){
	int h_terminal = 0, l_terminal = 0;
	mesurer1(&h_terminal,&l_terminal);
	fin_de_message(j1,j2,h_terminal,l_terminal,false);
	
	if(sockG!=0){
		mesurer2(&h_terminal,&l_terminal);
		fin_de_message(j1,j2,h_terminal,l_terminal,true);
	}
	sleep(2);
}

/* Cette fonction affiche simplement le rayon d'explosion */
void afficher_rayon(char *bufferOpti, char *optiBuffer){
	if(strlen(bufferOpti)!=0) write(1,bufferOpti,strlen(bufferOpti));
	free(bufferOpti);
	if(sockG!=0){
		if(strlen(optiBuffer)!=0)envoyer_des_tas(optiBuffer);
		free(optiBuffer);
	}
}

/* Cette fonction concatène opti et src et renvoie le nouveau string opti */
char* optimisation(char* opti, const char* src){
	opti = realloc(opti,strlen(opti)+strlen(src)+1);
	if(opti==NULL) fatal_error("memory allocation failure in tool.c on optimisation\r\n");
	strcat(opti,src);
	return opti;
}

/* Cette fonction détermine la taille de la map et la met dans des pointeurs */
int dimensionner(const char * fd_nom, int *h, int *l){
	char c, buffer[MAXI]={0};
	int fd = open (fd_nom,O_RDONLY), i=0;
	if (fd==-1){
		fatal_error("ouverture impossible\r\n");
	}
	while(read(fd,&c,1)==1 && c!='\n'){
		buffer[i++]=c;
	}
	buffer[i] = '\0';
	(*h)=atoi(buffer);
	(*l)=atoi(strpbrk(buffer," ")+1);
	return fd;
}

/* Cette fonction permet d'envoyer un tas de char correctement par socket */
void envoyer_des_tas(const char *tas){
	// On convertit la taille du tas en entier big-endian
	unsigned int dataSend = htonl(strlen(tas));
	
	// On envoie l'entier converti avant d'envoyer le tas
	if(send(sockG, (char*)&dataSend, 4, 0) == -1
	|| send(sockG, tas, strlen(tas), 0) == -1){
		socket_libre();
	}
}

void socket_libre(){
	if(sockM!=0){
		shutdown(sockG, 2);
		close(sockM);
		sockM=0, sockG=0;
		fatal_error("Fin de la connexion.\r\n");
	}
}
