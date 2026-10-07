#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PUERTO 5001
#define MAX_SUBS 100

struct sockaddr_in subs[MAX_SUBS];
char temas[MAX_SUBS][50];
int num_subs = 0;

int main() {
    int sock, i;
    struct sockaddr_in dir, origen;
    socklen_t tam;
    char buffer[1024];

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(1);
    }

    memset(&dir, 0, sizeof(dir));
    dir.sin_family = AF_INET;
    dir.sin_addr.s_addr = INADDR_ANY;
    dir.sin_port = htons(PUERTO);

    if (bind(sock, (struct sockaddr *)&dir, sizeof(dir)) < 0) {
        perror("bind");
        exit(1);
    }

    printf("Broker UDP escuchando en el puerto %d\n", PUERTO);

    while (1) {
        tam = sizeof(origen);
        int n = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&origen, &tam);
        if (n < 0) {
            continue;
        }
        buffer[n] = '\0';
        buffer[strcspn(buffer, "\r\n")] = '\0';

        if (strncmp(buffer, "SUB|", 4) == 0) {
            if (num_subs < MAX_SUBS) {
                subs[num_subs] = origen;
                snprintf(temas[num_subs], 50, "%s", buffer + 4);
                num_subs++;
                printf("%s:%d suscrito a %s\n", inet_ntoa(origen.sin_addr), ntohs(origen.sin_port), buffer + 4);
            }
        } else if (strncmp(buffer, "PUB|", 4) == 0) {
            char tema[50];
            sscanf(buffer + 4, "%49[^|]", tema);
            printf("Recibido: %s\n", buffer);

            int enviados = 0;
            for (i = 0; i < num_subs; i++) {
                if (strcmp(temas[i], tema) == 0) {
                    sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr *)&subs[i], sizeof(subs[i]));
                    enviados++;
                }
            }
            printf("Reenviado a %d suscriptores de %s\n", enviados, tema);
        }
    }

    close(sock);
    return 0;
}