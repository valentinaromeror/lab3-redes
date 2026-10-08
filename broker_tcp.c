#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define PUERTO 6000
#define MAX_CLIENTES 100
#define MAX_TEMAS 10

int clientes[MAX_CLIENTES];
char temas[MAX_CLIENTES][MAX_TEMAS][50];
int num_temas[MAX_CLIENTES];

int main() {
    int servidor, i, j, k;
    struct sockaddr_in dir;
    char buffer[1024];

    signal(SIGPIPE, SIG_IGN);

    for (i = 0; i < MAX_CLIENTES; i++) {
        clientes[i] = -1;
    }

    servidor = socket(AF_INET, SOCK_STREAM, 0);
    if (servidor < 0) {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(servidor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&dir, 0, sizeof(dir));
    dir.sin_family = AF_INET;
    dir.sin_addr.s_addr = INADDR_ANY;
    dir.sin_port = htons(PUERTO);

    if (bind(servidor, (struct sockaddr *)&dir, sizeof(dir)) < 0) {
        perror("bind");
        exit(1);
    }

    listen(servidor, 10);
    printf("Broker TCP escuchando en el puerto %d\n", PUERTO);

    while (1) {
        fd_set lectura;
        FD_ZERO(&lectura);
        FD_SET(servidor, &lectura);
        int max = servidor;

        for (i = 0; i < MAX_CLIENTES; i++) {
            if (clientes[i] != -1) {
                FD_SET(clientes[i], &lectura);
                if (clientes[i] > max) {
                    max = clientes[i];
                }
            }
        }

        select(max + 1, &lectura, NULL, NULL, NULL);

        if (FD_ISSET(servidor, &lectura)) {
            int nuevo = accept(servidor, NULL, NULL);
            for (i = 0; i < MAX_CLIENTES; i++) {
                if (clientes[i] == -1) {
                    clientes[i] = nuevo;
                    num_temas[i] = 0;
                    break;
                }
            }
            if (i == MAX_CLIENTES) {
                close(nuevo);
            } else {
                printf("Nuevo cliente conectado (socket %d)\n", nuevo);
            }
        }

        for (i = 0; i < MAX_CLIENTES; i++) {
            if (clientes[i] == -1 || !FD_ISSET(clientes[i], &lectura)) {
                continue;
            }

            int n = recv(clientes[i], buffer, sizeof(buffer) - 1, 0);
            if (n <= 0) {
                printf("Cliente desconectado (socket %d)\n", clientes[i]);
                close(clientes[i]);
                clientes[i] = -1;
                continue;
            }
            buffer[n] = '\0';

            char *linea = strtok(buffer, "\n");
            while (linea != NULL) {
                linea[strcspn(linea, "\r")] = '\0';

                if (strncmp(linea, "SUB|", 4) == 0) {
                    if (num_temas[i] < MAX_TEMAS) {
                        snprintf(temas[i][num_temas[i]], 50, "%s", linea + 4);
                        num_temas[i]++;
                        printf("Cliente %d suscrito a %s\n", clientes[i], linea + 4);
                    }
                } else if (strncmp(linea, "PUB|", 4) == 0) {
                    char tema[50];
                    sscanf(linea + 4, "%49[^|]", tema);
                    printf("Recibido: %s\n", linea);

                    char mensaje[1100];
                    sprintf(mensaje, "%s\n", linea);
                    int enviados = 0;

                    for (j = 0; j < MAX_CLIENTES; j++) {
                        if (clientes[j] == -1 || j == i) {
                            continue;
                        }
                        for (k = 0; k < num_temas[j]; k++) {
                            if (strcmp(temas[j][k], tema) == 0) {
                                send(clientes[j], mensaje, strlen(mensaje), 0);
                                enviados++;
                                break;
                            }
                        }
                    }
                    printf("Reenviado a %d suscriptores de %s\n", enviados, tema);
                }

                linea = strtok(NULL, "\n");
            }
        }
    }

    close(servidor);
    return 0;
}