#ifndef MENU_H_DU_PROJET
#define MENU_H_DU_PROJET

typedef struct action_s{
	char* nom_action;
	int(*fonction)(const char*);
}action_t;

typedef struct menu_s {
	int nbAction;
	char* nom_menu;
	action_t** tab_actions;
}menu_t;

menu_t* lister (int(*connecter)(const char*),int(*jouer)(const char*),int(*quitter)(const char*));
void liberez_le_menu(menu_t*);
void lancer_menu(menu_t*);


#endif
