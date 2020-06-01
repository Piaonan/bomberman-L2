#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <termios.h>
#include <time.h>
#define PORT 4000
#define MAXI 512

struct termios origin;

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
	char buffer[MAXI]={0};
	sprintf(buffer, "\x1b[?25l");
	write(1, buffer, strlen(buffer));
}

/* Cette fonction permet de rétablir le terminal */
void cooked_mode(){
	tcsetattr(0, TCSAFLUSH, &origin);
	char buffer[MAXI]={0};
	sprintf(buffer, "\x1b[H\x1b[2J\x1b[?25h");
	write(1, buffer, strlen(buffer));
}

/* Cette fonction permet d'afficher une erreur rencontrée et exit proprement */
void fatal_error(const char *message){
	char buffer[MAXI]={0};
	cooked_mode();
	sprintf(buffer,"\x1b[H\x1b[2J%s",message);
	write(2,buffer,strlen(buffer));
	exit(EXIT_FAILURE);
}

/* Cette fonction permet de lire un tas de char correctement par socket pour les afficher */
void lire_des_tas(int sock){
	unsigned int dataRecv;
	// On récupère l'entier en big-endian
	if (recv(sock, (char*)&dataRecv, 4, 0) < 1){
		fatal_error("Fin de la connexion.\n");
	}
	
	/* On convertit l'entier récupéré en little-endian si l'ordinateur 
	* stock les entiers en mémoire en little-endian, sinon s'il les 
	* stock en big-endian l'entier est convertit en big-endian */
	unsigned int taille = ntohl(dataRecv);
	char *bufferOpti = calloc(taille+1,sizeof(char));
	if(bufferOpti == NULL) 
		fatal_error("memory allocation failure in guest.c on lire_des_tas\n");
	if(recv(sock, bufferOpti, taille, 0) < 1){
		fatal_error("Fin de la connexion.\n");
	}
	if(bufferOpti[0]=='?'){
		struct winsize window;
		if(ioctl(0, TIOCGWINSZ, &window)==-1)
			fatal_error("récupération de la taille du terminal impossible\n");
		unsigned int dataSend1 = htonl(window.ws_row);
		unsigned int dataSend2 = htonl(window.ws_col);
		if(send(sock, (char*)&dataSend1, 4, 0) == -1
		|| send(sock, (char*)&dataSend2, 4, 0) == -1)
			fatal_error("envoie impossible");
	} else write(1,bufferOpti,taille);
	free(bufferOpti);
}

int main(){
	raw_mode();
	
	int sock;
	struct sockaddr_in sin;
	char buffer[MAXI]={0}, c='?';
	
	/* Création de la socket */
	if( (sock = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		perror("socket ");
		sleep(1);
		fatal_error("obtention de socket impossible\n");
	}
	
	/* Configuration de la connexion */
	sin.sin_addr.s_addr = inet_addr("127.0.0.1");
	sin.sin_family = AF_INET;
	sin.sin_port = htons(PORT);
	
	if(connect(sock, (struct sockaddr*)&sin, sizeof(sin)) == -1){
		perror("connect ");
		sleep(1);
		fatal_error("connexion impossible\n");
	}
	
	/* Si l'on a réussi à se connecter */
	sprintf(buffer,
		"\x1b[H\x1b[2JConnexion à %s sur le port %d\r\nAppuyer sur e pour quitter\r\nPatientez maintenant SVP", 
			inet_ntoa(sin.sin_addr), htons(sin.sin_port));
	write(1,buffer,strlen(buffer));
	
	struct pollfd fds[2];
	fds[0].fd = 0;
	fds[1].fd = sock;
	fds[0].events = POLLIN;
	fds[1].events = POLLIN;
	
	while(c!='e'&&poll(fds,2,-1)!=-1){
		if(fds[0].revents!=0 
		&& read(fds[0].fd,&c,1)
		&& send(sock, &c, 1, 0)==-1){
			perror("send ");
			sleep(1);
			c='e';
		} else if(fds[1].revents!=0){
			lire_des_tas(sock);
		}
	}
	
	close(sock);
	cooked_mode();
	return EXIT_SUCCESS;
}
