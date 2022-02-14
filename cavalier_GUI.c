
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <inttypes.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <gtk/gtk.h>


#define MAXDATASIZE 256


struct point2D{
    int lig;
    int col;
};


/* Variables globales */

struct point2D deplacements[8] ={
        {.lig=1,.col=2},
        {.lig=2,.col=1},
        {.lig=-2,.col=1},
        {.lig=-1,.col=2},
        {.lig=-2,.col=-1},
        {.lig=-1,.col=-2},
        {.lig=2,.col=-1},
        {.lig=1,.col=-2},
};

struct point2D bl_pos = {.col=-1,.lig=-1};
struct point2D wh_pos = {.col=-1,.lig=-1};

enum couleur { BL = 0, WH = 1, PION = 3, BL_PION = 4, WH_PION = 5 };

int damier[8][8];    // tableau associe au damier
int couleur;        // 0 : pour noir, 1 : pour blanc, 3 : pour pion, 4 : pion avec cavalier noir, 5 : pion avec cavalier blanc

int port;        // numero port passé lors de l'appel

char *addr_j2, *port_j2;    // Info sur adversaire


pthread_t thr_id;    // Id du thread fils gerant connexion socket

int sockfd, newsockfd =-1, sockfd_swp = -1; // descripteurs de socket
int addr_size;     // taille adresse
struct sockaddr *their_addr;    // structure pour stocker adresse adversaire

fd_set master, read_fds, write_fds;    // ensembles de socket pour toutes les sockets actives avec select
int fdmax;            // utilise pour select

uint16_t taille_msg, nb;

/* Variables globales associées à l'interface graphique */
GtkBuilder *p_builder = NULL;
GError *p_err = NULL;



// Entetes des fonctions  

/* Fonction permettant afficher image pion dans case du damier (indiqué par sa colonne et sa ligne) */
void affiche_pion(int col, int lig);

/* Fonction permettant afficher image cavalier noir dans case du damier (indiqué par sa colonne et sa ligne) */
void affiche_cav_noir(int col, int lig);

/* Fonction permettant afficher image cavalier blanc dans case du damier (indiqué par sa colonne et sa ligne) */
void affiche_cav_blanc(int col, int lig);

/* Fonction transformant coordonnees du damier graphique en indexes pour matrice du damier */
void coord_to_indexes(const gchar *coord, int *col, int *lig);

/* Fonction appelee lors du clique sur une case du damier */
static void coup_joueur(GtkWidget *p_case);

/* Fonction retournant texte du champs adresse du serveur de l'interface graphique */
char *lecture_addr_serveur(void);

/* Fonction retournant texte du champs port du serveur de l'interface graphique */
char *lecture_port_serveur(void);

/* Fonction retournant texte du champs login de l'interface graphique */
char *lecture_login(void);

/* Fonction retournant texte du champs adresse du cadre Joueurs de l'interface graphique */
char *lecture_addr_adversaire(void);

/* Fonction retournant texte du champs port du cadre Joueurs de l'interface graphique */
char *lecture_port_adversaire(void);

/* Fonction affichant boite de dialogue si partie gagnee */
void affiche_fenetre_gagne(void);

/* Fonction affichant boite de dialogue si partie perdue */
void affiche_fenetre_perdu(void);

/* Fonction appelee lors du clique du bouton Se connecter */
static void clique_connect_serveur(GtkWidget *b);

/* Fonction desactivant bouton demarrer partie */
void disable_button_start(void);

/* Fonction appelee lors du clique du bouton Demarrer partie */
static void clique_connect_adversaire(GtkWidget *b);

/* Fonction desactivant les cases du damier */
void gele_damier(void);

/* Fonction activant les cases du damier */
void degele_damier(void);

/* Fonction permettant d'initialiser le plateau de jeu */
void init_interface_jeu(void);

/* Fonction reinitialisant la liste des joueurs sur l'interface graphique */
void reset_liste_joueurs(void);

/* Fonction permettant d'ajouter un joueur dans la liste des joueurs sur l'interface graphique */
void affich_joueur(char *login, char *adresse, char *port);

void sensitive_loop(int sensitiveState);

int can_move(int col, int lig);
int check_wh_win();
int wh_can_reach_bl();
int check_bl_win();
int available_path();
char* get_image_from_code();

void send_message(int type_msg, struct point2D coords);

int receive_message(void);

/* Fonction transforme coordonnees du damier graphique en indexes pour matrice du damier */
void coord_to_indexes(const gchar *coord, int *col, int *lig) {
    char *c;
    c = malloc(3 * sizeof(char));
    c = strncpy(c, coord, 1);
    c[1] = '\0';

    *col = strcmp(c, "A");

    *lig = atoi(coord + 1) - 1;
}

/* Fonction transforme coordonnees du damier graphique en indexes pour matrice du damier */
void indexes_to_coord(int col, int lig, char *coord) {
    char c;
    c = 'A' + col;
    sprintf(coord, "%c%d\0", c, lig + 1);
}

/* Fonction permettant afficher image pion dans case du damier (indiqué par sa colonne et sa ligne) */
void affiche_pion(int col, int lig) {
    char *coord;
    coord = malloc(3 * sizeof(char));
    indexes_to_coord(col, lig, coord);
    damier[col][lig]=3;
    gtk_image_set_from_file(GTK_IMAGE(gtk_builder_get_object(p_builder, coord)), "UI_Glade/case_pion.png");
}

/* Fonction permettant afficher image cavalier noir dans case du damier (indiqué par sa colonne et sa ligne) */
void affiche_cav_noir(int col, int lig) {
    char *coord;
    coord = malloc(3 * sizeof(char));
    indexes_to_coord(col, lig, coord);
    bl_pos.col=col;
    bl_pos.lig=lig;
    damier[col][lig]=BL;
    gtk_image_set_from_file(GTK_IMAGE(gtk_builder_get_object(p_builder, coord)), "UI_Glade/case_cav_noir.png");
}

void print_damier(){
    for(int col=0;col<8;col++){
        for(int lig=0;lig<8;lig++){
            printf("%d ",damier[col][lig]);
        }
        printf("\n");
    }
    printf("\n");
}

/* Fonction permettant afficher image cavalier blanc dans case du damier (indiqué par sa colonne et sa ligne) */
void affiche_cav_blanc(int col, int lig) {
    char *coord;
    coord = malloc(3 * sizeof(char));
    indexes_to_coord(col, lig, coord);
    wh_pos.col=col;
    wh_pos.lig=lig;
    damier[col][lig]=WH;
    gtk_image_set_from_file(GTK_IMAGE(gtk_builder_get_object(p_builder, coord)), "UI_Glade/case_cav_blanc.png");
}


void refresh_map(){
    char *coord;
    coord = malloc(3 * sizeof(char));
    for(int i=0;i<8;i++){
        for(int j=0;j<8;j++){
            indexes_to_coord(i, j, coord);
            int code=damier[i][j];
            if(code!=-1) {
                char *imageUrl = get_image_from_code(code);
                gtk_image_set_from_file(GTK_IMAGE(gtk_builder_get_object(p_builder, coord)), imageUrl);
                if(code==-2){
                    damier[i][j]=-1;
                }
            }
        }
    }
}

char* get_image_from_code(int code){
    switch(code){
        case WH:
            return "UI_Glade/case_cav_blanc.png";
        case BL:
            return "UI_Glade/case_cav_noir.png";
        case PION:
            return "UI_Glade/case_pion.png";
        case -2:
            return "UI_Glade/case_def.png";
        default:
            printf("Error code %d invalide",code);
            exit(-1);
    }
}


void affiche_deplacement(){
    char *coord;
    coord = malloc(3 * sizeof(char));
    int col, lig;
    if(couleur==BL){
        col=bl_pos.col;
        lig=bl_pos.lig;
    }else if(couleur==WH){
        col=wh_pos.col;
        lig=wh_pos.lig;
    }
    for(int i=0;i<8;i++){
        int availableCol=col+deplacements[i].col;
        int availableLig=lig+deplacements[i].lig;
        if ((availableCol >= 0 && availableLig >= 0 && availableCol < 7 && availableLig < 7) && damier[availableCol][availableLig]==-1){
            damier[availableCol][availableLig]=-2;
            indexes_to_coord(availableCol, availableLig, coord);
            gtk_image_set_from_file(GTK_IMAGE(gtk_builder_get_object(p_builder, coord)), "UI_Glade/case_dispo.png");
        }
    }
}

int is_valid_pos(int lig,int col){
    int playerCol,playerLig;
    if(couleur==BL){
        playerCol=bl_pos.col;
        playerLig=bl_pos.lig;
    }else if(couleur==WH){
        playerCol=wh_pos.col;
        playerLig=wh_pos.lig;
    }
    for(int i=0;i<8;i++){
        int availableCol=playerCol+deplacements[i].col;
        int availableLig=playerLig+deplacements[i].lig;
        if (availableCol == col && availableLig == lig){
            return 1;
        }
    }
    return 0;
}

void check_win(){
    int win;
    if(couleur==WH){
        win = check_wh_win();
        printf("win : %d",win);
        if(win==1){
            send_message(1,wh_pos);
            affiche_fenetre_gagne();
        }
    }else if(couleur==BL){
        win = check_bl_win();
        printf("win : %d",win);
        if(win == 1){
            send_message(1,bl_pos);
            affiche_fenetre_gagne();
        }
    }
}


int check_wh_win(){
    if(can_move(bl_pos.col,bl_pos.lig) == 0)
        return 1;
    if(wh_can_reach_bl()){
        return 1;
    }
    return 0;
}

int check_bl_win(){
    if(can_move(wh_pos.col,wh_pos.lig)==0)
        return 1;
    if(available_path() == 0){
        return 1;
    }
    return 0;
}

int available_path(){
    //TODO pathfinding pour vérifier que wh a un chemin vers bl
    return 1;
}

int wh_can_reach_bl(){
    for(int i=0;i<8;i++){
        if(wh_pos.col+deplacements[i].col == bl_pos.col && wh_pos.lig+deplacements[i].lig==bl_pos.lig)
            return 1;
    }
    return 0;
}

int can_move(int col, int lig){
    for(int i=0;i<8;i++){
        int possibleCol=col+deplacements[i].col;
        int possibleLig=lig+deplacements[i].lig;
        if((possibleCol >=0 && possibleCol < 8 ) && (possibleLig >=0 && possibleLig < 8) && damier[col][lig] != PION)
            return 1;
    }
    return 0;
}

void send_message(int type_msg, struct point2D coords){
    char buf[MAXDATASIZE];
    char head[2], buffer_type[2];
    snprintf(buf,MAXDATASIZE,"%u,%u,", htons((uint16_t) coords.lig), htons((uint16_t) coords.col));

    //implémenter la logique de jeu win/loose
    nb = htons((uint16_t) type_msg);
    memcpy(buffer_type, &nb, 2);

    if(send(newsockfd,buffer_type,2, 0)==-1){
        perror("send type_msg");
    }
    //taille du message
    taille_msg = htons((uint16_t) strlen(buf));
    memcpy(head, &taille_msg, 2);
    send(newsockfd, head, 2, 0);

    //coords
    if(send(newsockfd, buf, strlen(buf), 0) == -1){
        perror("send");
    }

}

/* Fonction appelee lors du clique sur une case du damier */
static void coup_joueur(GtkWidget *p_case) {
    //type_msg = 0 :
    //type_msg = 1 : win
    //type_msg = 2 : lose
    int col, lig, type_msg, nb_piece, score;


    // Traduction coordonnees damier en indexes matrice damier
    coord_to_indexes(gtk_buildable_get_name(GTK_BUILDABLE(gtk_bin_get_child(GTK_BIN(p_case)))), &col, &lig);

    int pos_result = is_valid_pos(lig,col);

    if(pos_result==1){
        if(couleur==WH){
            affiche_pion(wh_pos.col,wh_pos.lig);
            affiche_cav_blanc(col,lig);
            send_message(0, wh_pos);
        }
        else if(couleur==BL){
            affiche_pion(bl_pos.col,bl_pos.lig);
            affiche_cav_noir(col,lig);
            send_message(0, bl_pos);
        }
        gele_damier();
    }
    else{
        printf("Error:  col: %d lig: %d is not available\n",col,lig);
    }


/*
    char msg[50];

    uint16_t taille_msg, type_ms;
    char head[2];

    printf("Client : alright time to send deez nuts\n");

    snprintf(msg, 50, "%u,%u,", htons((uint16_t) lig), htons((uint16_t) col));

    taille_msg=htons((uint16_t) strlen(msg));
    memcpy(head, &taille_msg, 2);

    if(newsockfd=-1){ // on est du côté client sinon, on est du côté serveur
        type_ms = htons((uint16_t) '1');
        send(sockfd, type_ms, 2, 0);
        send(sockfd, head, 2, 0);

        if(send(sockfd, msg, strlen(msg), 0) == -1) {
            perror("send client");
        }
        printf("Client : sent deez nuts\n");
    } else {
        send(newsockfd, head, 2, 0);

        if(send(newsockfd, msg, strlen(msg), 0) == -1) {
            perror("send serveur");
        }
    }*/


}

/* Fonction retournant texte du champs adresse du serveur de l'interface graphique */
char *lecture_addr_serveur(void) {
    GtkWidget *entry_addr_srv;
    entry_addr_srv = (GtkWidget *) gtk_builder_get_object(p_builder, "entry_adr");
    return (char *) gtk_entry_get_text(GTK_ENTRY(entry_addr_srv));
}

/* Fonction retournant texte du champs port du serveur de l'interface graphique */
char *lecture_port_serveur(void) {
    GtkWidget *entry_port_srv;
    entry_port_srv = (GtkWidget *) gtk_builder_get_object(p_builder, "entry_port");
    return (char *) gtk_entry_get_text(GTK_ENTRY(entry_port_srv));
}

/* Fonction retournant texte du champs login de l'interface graphique */
char *lecture_login(void) {
    GtkWidget *entry_login;
    entry_login = (GtkWidget *) gtk_builder_get_object(p_builder, "entry_login");
    return (char *) gtk_entry_get_text(GTK_ENTRY(entry_login));
}

/* Fonction retournant texte du champs adresse du cadre Joueurs de l'interface graphique */
char *lecture_addr_adversaire(void) {
    GtkWidget *entry_addr_j2;
    entry_addr_j2 = (GtkWidget *) gtk_builder_get_object(p_builder, "entry_addr_j2");
    return (char *) gtk_entry_get_text(GTK_ENTRY(entry_addr_j2));
}

/* Fonction retournant texte du champs port du cadre Joueurs de l'interface graphique */
char *lecture_port_adversaire(void) {
    GtkWidget *entry_port_j2;
    entry_port_j2 = (GtkWidget *) gtk_builder_get_object(p_builder, "entry_port_j2");
    return (char *) gtk_entry_get_text(GTK_ENTRY(entry_port_j2));
}

/* Fonction affichant boite de dialogue si partie gagnee */
void affiche_fenetre_gagne(void) {
    GtkWidget *dialog;

    GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;

    dialog = gtk_message_dialog_new(GTK_WINDOW(gtk_builder_get_object(p_builder, "window1")), flags, GTK_MESSAGE_INFO,
                                    GTK_BUTTONS_CLOSE, "Fin de la partie.\n\n Vous avez gagné!!!");
    gtk_dialog_run(GTK_DIALOG (dialog));

    gtk_widget_destroy(dialog);
}

/* Fonction affichant boite de dialogue si partie perdue */
void affiche_fenetre_perdu(void) {
    GtkWidget *dialog;

    GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;

    dialog = gtk_message_dialog_new(GTK_WINDOW(gtk_builder_get_object(p_builder, "window1")), flags, GTK_MESSAGE_INFO,
                                    GTK_BUTTONS_CLOSE, "Fin de la partie.\n\n Vous avez perdu!");
    gtk_dialog_run(GTK_DIALOG (dialog));

    gtk_widget_destroy(dialog);
}

/* Fonction appelee lors du clique du bouton Se connecter */
static void clique_connect_serveur(GtkWidget *b) {
    /***** TO DO *****/
    int sockfd, rv;
    struct addrinfo hints, *servinfo, *p;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    rv = getaddrinfo(lecture_addr_serveur(), lecture_port_serveur(), &hints, &servinfo);

    if(rv != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    for(p = servinfo; p != NULL; p = p->ai_next)
    {
        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("Erreur socket Client");
            continue;
        }
        if((connect(sockfd, p->ai_addr, p->ai_addrlen)) == -1) {
            close(sockfd);
            perror("Erreur de connexion qu serveur");
            continue;
        }
        break;
    }


}

/* Fonction desactivant bouton demarrer partie */
void disable_button_start(void) {
    gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "button_start"), FALSE);
}

/* Fonction traitement signal bouton Demarrer partie */
static void clique_connect_adversaire(GtkWidget *b) {
    if (newsockfd == -1) {
        // Deactivation bouton demarrer partie
        gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "button_start"), FALSE);

        // Recuperation  adresse et port adversaire au format chaines caracteres
        addr_j2 = lecture_addr_adversaire();
        port_j2 = lecture_port_adversaire();

        printf("[Port joueur : %d] Adresse j2 lue : %s\n", port, addr_j2);
        printf("[Port joueur : %d] Port j2 lu : %s\n", port, port_j2);


        pthread_kill(thr_id, SIGUSR1);
    }
}

void sensitive_loop(int sensitiveState) {
    char evt[10] = "eventbox";
    for (char i = '1'; i <= '8'; i++) {
        for (char c = 'A'; c <= 'H'; c++) {
            evt[8] = c;
            evt[9] = i;
            gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, evt), sensitiveState);
        }
    }
}

/* Fonction desactivant les cases du damier */
void gele_damier(void) {
    sensitive_loop(FALSE);
}

/* Fonction activant les cases du damier */
void degele_damier(void) {
    sensitive_loop(TRUE);
}

/* Fonction permettant d'initialiser le plateau de jeu */
void init_interface_jeu(void) {
    // Initilisation du damier (A1=cavalier_noir, H8=cavalier_blanc)
    affiche_cav_blanc(7, 7);
    affiche_cav_noir(0, 0);

    /***** TO DO *****/

}

/* Fonction reinitialisant la liste des joueurs sur l'interface graphique */
void reset_liste_joueurs(void) {
    GtkTextIter start, end;

    gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(gtk_text_view_get_buffer(
            GTK_TEXT_VIEW(gtk_builder_get_object(p_builder, "textview_joueurs")))), &start);
    gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(gtk_text_view_get_buffer(
            GTK_TEXT_VIEW(gtk_builder_get_object(p_builder, "textview_joueurs")))), &end);

    gtk_text_buffer_delete(GTK_TEXT_BUFFER(gtk_text_view_get_buffer(
            GTK_TEXT_VIEW(gtk_builder_get_object(p_builder, "textview_joueurs")))), &start, &end);
}

/* Fonction permettant d'ajouter un joueur dans la liste des joueurs sur l'interface graphique */
void affich_joueur(char *login, char *adresse, char *port) {
    const gchar *joueur;

    joueur = g_strconcat(login, " - ", adresse, " : ", port, "\n", NULL);

    gtk_text_buffer_insert_at_cursor(GTK_TEXT_BUFFER(gtk_text_view_get_buffer(
            GTK_TEXT_VIEW(gtk_builder_get_object(p_builder, "textview_joueurs")))), joueur, strlen(joueur));
}

int receive_message(void){
    char buf[MAXDATASIZE], head[5], buffer_type[5], *tmp, *p_parse;
    int len, bytes_sent, t_msg_recu;
    int col, lig;

    recv(newsockfd, buffer_type, 2, 0);
    memcpy(&nb, buffer_type, 2);
    int msg_type = (int)ntohs(nb);
    printf("type du msg : %d\n", msg_type);

    if(msg_type == 0){
        //réseau
        recv(newsockfd, head, 2, 0);
        memcpy(&taille_msg, head, 2);
        t_msg_recu = (int) ntohs(taille_msg);
        printf("taille du message : %d\n", t_msg_recu);

        //coordonnees
        recv(newsockfd, buf, t_msg_recu*sizeof(char), 0);
        tmp = strtok_r(buf,",",&p_parse);
        sscanf(tmp, "%u", &nb);
        lig = (int) ntohs(nb);

        tmp = strtok_r(NULL,",",&p_parse);
        sscanf(tmp, "%u", &nb);
        col = (int) ntohs(nb);
        if(couleur==WH){
            affiche_pion(bl_pos.col,bl_pos.lig);
            affiche_cav_noir(col,lig);

        }else if(couleur==BL){
            affiche_pion(wh_pos.col,wh_pos.lig);
            affiche_cav_blanc(col,lig);
        }

        printf("---- TRAITEMENT MESSAGE ADVERSE ----\n");
        printf("col : %d - lig : %d\n", col, lig);
    }
    if(msg_type==1){
        affiche_fenetre_perdu();
    }
    return msg_type;
}

/* Fonction exécutée par le thread gérant les communications à travers la socket */
static void * f_com_socket(void *p_arg){
    int i;
    sigset_t signal_mask;
    int fd_signal, rv;
    char* serveur = "127.0.0.1";
    struct addrinfo hints, *servinfo, *p;

    uint16_t type_msg, col_j2;
    uint16_t ucol, ulig;

    /* Association descripteur au signal SIGUSR1 */
    sigemptyset(&signal_mask);
    sigaddset(&signal_mask, SIGUSR1);

    if(sigprocmask(SIG_BLOCK, &signal_mask, NULL) == -1){
        printf("[Port joueur : %d] Erreur sigprocmask\n", port);
        return 0;
    }

    fd_signal = signalfd(-1, &signal_mask, 0);

    if(fd_signal == -1){
        printf("[Port joueur : %d] Erreur signalfd\n", port);
        return 0;
    }

    /* Ajout descripteur du signal dans ensemble de descripteur utilisé avec fonction select */
    FD_SET(fd_signal, &master);

    if(fd_signal>fdmax){
        fdmax=fd_signal;
    }


    while(1){
        read_fds = master;  // copie des ensembles

        if(select(fdmax+1, &read_fds, &write_fds, NULL, NULL)==-1){
            perror("Problème avec select");
            exit(4);
        }
        //printf("[Port joueur : %d] Entree dans boucle for\n", port);
        for(i = 0; i <= fdmax; i++){
            //printf("[Port joueur : %d] newsockfd=%d, iteration %d boucle for\n", port, newsockfd, i);
            if(FD_ISSET(i, &read_fds)){
                if(i == fd_signal){
                    /* Cas de l'envoie du signal par l'interface graphique pour connexion au joueur adverse */

                    // partie client

                    memset(&hints, 0, sizeof(hints));
                    hints.ai_family = AF_INET;
                    hints.ai_flags = AI_PASSIVE;
                    hints.ai_socktype = SOCK_STREAM;

                    printf("Info serveur : %s:%s\n", addr_j2, port_j2);
                    //rv = getaddrinfo(serveur, p_arg, &hints, &servinfo);
                    rv = getaddrinfo("127.0.0.1", port_j2, &hints, &servinfo);

                    if(rv != 0) {
                        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
                        exit(1);
                    }

                    for(p = servinfo; p != NULL; p = p->ai_next) {
                        if((newsockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                            perror("client: socket");
                            continue;
                        }
                        printf("Client connexion au serveur joueur adverse\n");
                        if((rv=connect(newsockfd, p->ai_addr, p->ai_addrlen)) == -1) {
                            close(newsockfd);
                            perror("client: connect");
                            continue;
                        }
                        break;
                    }

                    if(p == NULL)
                    {
                        printf("ERREUR sortie boucle p==NULL\n");
                    }

                    close(fd_signal);
                    FD_CLR(fd_signal, &master);

                    close(sockfd);
                    FD_CLR(sockfd, &master);


                    FD_SET(newsockfd, &master);
                    if(newsockfd > fdmax){
                        fdmax = newsockfd;
                    }

                    // Cavalier Noir
                    init_interface_jeu();
                    couleur = BL;
                }
                if(i == sockfd){
                    /* Acceptation connexion adversaire */

                    // partie server

                    printf("Serveur port :  %s \n", (char *)p_arg);

                    addr_size = sizeof(their_addr);

                    newsockfd = accept(sockfd, (struct sockaddr *) &their_addr, &addr_size);

                    if (newsockfd == -1) {
                        perror("accept");
                    }

                    FD_SET(newsockfd, &master);
                    if(newsockfd > fdmax){
                        fdmax = newsockfd;
                    }

                    printf("New client receive\n");

                    // Cavalier Blanc
                    init_interface_jeu();
                    couleur = WH;

                    gele_damier(); // Joueur jouant en deuxième
                } else {
                    /* Reception et traitement des messages du joueur adverse */
                    printf("BL col : %d, BL lig : %d\n",bl_pos.col,bl_pos.lig);
                    printf("WH col : %d, BL lig : %d\n",wh_pos.col,wh_pos.lig);
                    refresh_map();
                    printf("\nReception des messages du joueur adverses\n");
                    check_win();
                    affiche_deplacement();
                    if(receive_message()==0){
                        degele_damier();
                    }
                    //print_damier();
                }
            }
        }
    }
    return NULL;
}

int main(int argc, char **argv) {
    int i, j, ret;

    if (argc != 2) {
        printf("\nPrototype : ./othello num_port\n\n");

        exit(1);
    }
    /* Initialisation de GTK+ */
    gtk_init(&argc, &argv);

    /* Creation d'un nouveau GtkBuilder */
    p_builder = gtk_builder_new();

    if (p_builder != NULL) {
        /* Chargement du XML dans p_builder */
        gtk_builder_add_from_file(p_builder, "UI_Glade/Cavalier.glade", &p_err);

        if (p_err == NULL) {
            /* Recuparation d'un pointeur sur la fenetre. */
            GtkWidget *p_win = (GtkWidget *) gtk_builder_get_object(p_builder, "window1");

            /* Gestion evenement clic pour chacune des cases du damier */
            char evt[10] = "eventbox";
            for (char i = '1'; i <= '8'; i++) {
                for (char c = 'A'; c <= 'H'; c++) {
                    evt[8] = c;
                    evt[9] = i;
                    g_signal_connect(gtk_builder_get_object(p_builder, evt), "button_press_event",
                                     G_CALLBACK(coup_joueur), NULL);
                }
            }

            /* Gestion clic boutons interface */
            g_signal_connect(gtk_builder_get_object(p_builder, "button_connect"), "clicked",
                             G_CALLBACK(clique_connect_serveur), NULL);
            g_signal_connect(gtk_builder_get_object(p_builder, "button_start"), "clicked",
                             G_CALLBACK(clique_connect_adversaire), NULL);

            /* Gestion clic bouton fermeture fenetre */
            g_signal_connect_swapped(G_OBJECT(p_win), "destroy", G_CALLBACK(gtk_main_quit), NULL);


            /* Recuperation numero port donne en parametre */
            port = atoi(argv[1]);

            /* Initialisation du damier de jeu */
            for (i = 0; i < 8; i++) {
                for (j = 0; j < 8; j++) {
                    damier[i][j] = -1;
                }
            }

            /***** TO DO *****/
            pthread_create(&thr_id, NULL, f_com_socket, argv[1]);

            // Initialisation socket et autres objets, et création thread pour communications avec joueur adverse
            struct addrinfo s_init, *servinfo, *p;

            memset(&s_init, 0, sizeof(s_init));
            s_init.ai_family = AF_INET;
            s_init.ai_socktype = SOCK_STREAM;
            s_init.ai_flags = AI_PASSIVE;

            if (getaddrinfo(NULL, argv[1], &s_init, &servinfo) != 0) {
                fprintf(stderr, "Erreur getaddrinfo\n");
                exit(1);
            }

            for(p = servinfo; p != NULL; p = p->ai_next) {
                if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                    perror("Serveur: socket");
                    continue;
                }
                int option=1;
                setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

                if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
                    close(sockfd);
                    perror("Serveur: erreur bind");
                    continue;
                }
                break;
            }
            if (p == NULL) {
                fprintf(stderr, "Serveur: echec bind\n");
                exit(2);
            }
            freeaddrinfo(servinfo);
            if (listen(sockfd, 5) == -1) {
                perror("listen");
                exit(1);
            }

            FD_ZERO(&master);
            FD_ZERO(&read_fds);
            FD_SET(sockfd,&master);

            gtk_widget_show_all(p_win);
            gtk_main();
        } else {
            /* Affichage du message d'erreur de GTK+ */
            g_error ("%s", p_err->message);
            g_error_free(p_err);
        }

    }
    return EXIT_SUCCESS;
}
