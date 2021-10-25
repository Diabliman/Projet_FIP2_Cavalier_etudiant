
Commande de compilation :
gcc -Wall -o cavalier_GUI cavalier_GUI.c $(pkg-config --cflags --libs gtk+-3.0)

Lancement projet avec <num_port> le port TCP d'écoute :
./cavalier_GUI <num_port>

