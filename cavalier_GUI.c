
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


/* Variables globales */
int damier[8][8];    // tableau associe au damier
int couleur;        // 0 : pour noir, 1 : pour blanc, 3 : pour pion, 4 : pion avec cavalier noir, 5 : pion avec cavalier blanc

int port;        // numero port passé lors de l'appel

char *addr_j2, *port_j2;    // Info sur adversaire


pthread_t thr_id;    // Id du thread fils gerant connexion socket

int sockfd, newsockfd, sockfd_swp = -1; // descripteurs de socket
int addr_size;     // taille adresse
struct sockaddr *their_addr;    // structure pour stocker adresse adversaire

fd_set master, read_fds, write_fds;    // ensembles de socket pour toutes les sockets actives avec select
int fdmax;            // utilise pour select


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


/* Fonction transforme coordonnees du damier graphique en indexes pour matrice du damier */
void coord_to_indexes(const gchar *coord, int *col, int *lig) {
    char *c;
    c = malloc(3 * sizeof(char));
    c = strncpy(c, coord, 1);
    c[1] = '\0';

    if (strcmp(c, "A") == 0) {
        *col = 0;
    }
    if (strcmp(c, "B") == 0) {
        *col = 1;
    }
    if (strcmp(c, "C") == 0) {
        *col = 2;
    }
    if (strcmp(c, "D") == 0) {
        *col = 3;
    }
    if (strcmp(c, "E") == 0) {
        *col = 4;
    }
    if (strcmp(c, "F") == 0) {
        *col = 5;
    }
    if (strcmp(c, "G") == 0) {
        *col = 6;
    }
    if (strcmp(c, "H") == 0) {
        *col = 7;
    }
    *lig = atoi(coord + 1) - 1;
}

/* Fonction transforme coordonnees du damier graphique en indexes pour matrice du damier */
void indexes_to_coord(int col, int lig, char *coord) {
    char c;

    if (col == 0) {
        c = 'A';
    }
    if (col == 1) {
        c = 'B';
    }
    if (col == 2) {
        c = 'C';
    }
    if (col == 3) {
        c = 'D';
    }
    if (col == 4) {
        c = 'E';
    }
    if (col == 5) {
        c = 'F';
    }
    if (col == 6) {
        c = 'G';
    }
    if (col == 7) {
        c = 'H';
    }
    sprintf(coord, "%c%d\0", c, lig + 1);
}

/* Fonction permettant afficher image pion dans case du damier (indiqué par sa colonne et sa ligne) */
void affiche_pion(int col, int lig) {
    char *coord;
    coord = malloc(3 * sizeof(char));
    indexes_to_coord(col, lig, coord);
    gtk_image_set_from_file(GTK_IMAGE(gtk_builder_get_object(p_builder, coord)), "UI_Glade/case_pion.png");
}

/* Fonction permettant afficher image cavalier noir dans case du damier (indiqué par sa colonne et sa ligne) */
void affiche_cav_noir(int col, int lig) {
    char *coord;
    coord = malloc(3 * sizeof(char));
    indexes_to_coord(col, lig, coord);
    gtk_image_set_from_file(GTK_IMAGE(gtk_builder_get_object(p_builder, coord)), "UI_Glade/case_cav_noir.png");
}

/* Fonction permettant afficher image cavalier blanc dans case du damier (indiqué par sa colonne et sa ligne) */
void affiche_cav_blanc(int col, int lig) {
    char *coord;
    coord = malloc(3 * sizeof(char));
    indexes_to_coord(col, lig, coord);
    gtk_image_set_from_file(GTK_IMAGE(gtk_builder_get_object(p_builder, coord)), "UI_Glade/case_cav_blanc.png");
}


/* Fonction appelee lors du clique sur une case du damier */
static void coup_joueur(GtkWidget *p_case) {
    int col, lig, type_msg, nb_piece, score;
    char buf[MAXDATASIZE];

    // Traduction coordonnees damier en indexes matrice damier
    coord_to_indexes(gtk_buildable_get_name(GTK_BUILDABLE(gtk_bin_get_child(GTK_BIN(p_case)))), &col, &lig);


    /***** TO DO *****/

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

/* Fonction exécutée par le thread gérant les communications à travers la socket */
/* Fonction exécutée par le thread gérant les communications à travers la socket */
static void * f_com_socket(void *p_arg){
    int i, nbytes, col, lig;

    char buf[MAXDATASIZE], *tmp, *p_parse;
    int len, bytes_sent, t_msg_recu;

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
                    close(fd_signal);
                    FD_CLR(fd_signal, &master);

                    close(sockfd);
                    FD_CLR(sockfd, &master);

                    memset(&hints, 0, sizeof(hints));
                    hints.ai_family = AF_UNSPEC;
                    hints.ai_socktype = SOCK_STREAM;

                    printf("Info serveur : %s:%s\n", addr_j2, port_j2);
                    //rv = getaddrinfo(serveur, p_arg, &hints, &servinfo);
                    rv = getaddrinfo("127.0.0.1", port_j2, &hints, &servinfo);

                    if(rv != 0) {
                        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
                        exit(1);
                    }

                    for(p = servinfo; p != NULL; p = p->ai_next) {
                        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                            perror("client: socket");
                            continue;
                        }
                        printf("Client connexion au serveur joueur adverse\n");
                        if((rv=connect(sockfd, p->ai_addr, p->ai_addrlen)) == -1) {
                            close(sockfd);
                            perror("client: connect");
                            continue;
                        }
                        break;
                    }

                    if(p == NULL)
                    {
                        printf("ERREUR sortie boucle p==NULL\n");
                    }else{
                        FD_SET(sockfd, &master);
                        if(sockfd > fdmax){
                            fdmax = sockfd;
                        }
                        init_interface_jeu();
                        // Cavalier Noir
                        damier[7][7] = 0;
                        couleur = 0;
                    }
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
                    init_interface_jeu();
                    // Cavalier Blanc
                    couleur = 1;
                    damier[0][0] = 1;
                    gele_damier(); // Joueur jouant en deuxième
                }else{
                    /* Reception et traitement des messages du joueur adverse */

                    /***** TO DO *****/
                    printf("\nReception des messages du joueur adverses\n");

                    // stockage de la socket client ou serveur dans une même variable globale pour le traitement des messages
                    if(newsockfd == -1)
                        sockfd_swp = newsockfd;
                    else
                        sockfd_swp = sockfd;

                    printf("sockfd_echange : %d\n", sockfd_swp);

                    /*
                     0 : pour noir,
                     1 : pour blanc,
                     3 : pour pion,
                     4 : pion avec cavalier noir,
                     5 : pion avec cavalier blanc
                    */

                    // Clique sur damier
                    // La case est une case jouable
                    // La case est une case injouable
                    // La case est une case pion
                    /* Gèle du damier adverse */
                    /* Dégèle du damier du joueur */

                    printf("---- TRAITEMENT MESSAGE ADVERSE ----\n");
                    printf("col : %d - lig : %d\n", col, lig);
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
