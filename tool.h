#ifndef TOOL_H_DU_PROJET
#define TOOL_H_DU_PROJET

void fatal_error(const char *message);
void raw_mode ();
void cooked_mode ();
void saut(int fd);
void initialiser();
void nettoyer();
void mesurer1(int *h, int *l);
void mesurer2(int *h, int *l);
void issue_du_niveau(int issue, int l1, int l2);
void fin_de_la_partie(int j1, int j2);
void afficher_rayon(char *bufferOpti, char *optiBuffer);
char* optimisation(char* opti, const char* src);
int dimensionner(const char * fd_nom, int *h, int *l);
void envoyer_des_tas(const char *tas);
void socket_libre();

#endif
