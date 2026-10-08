/*
 * subscriber_udp.c — Suscriptor (hincha) del sistema de noticias deportivas, versión UDP.
 *
 * Envía al broker un datagrama SUB|<tema> por cada partido; el broker guarda la dirección
 * (IP:puerto) de origen de ese datagrama y desde entonces le reenvía los PUB de ese partido.
 * Revisa el número de secuencia de cada mensaje para detectar pérdida, duplicados o desorden.
 * Al presionar Ctrl+C imprime un resumen por partido (recibidos, perdidos, fuera de orden).
 *
 * Compilar:  gcc -Wall -o subscriber_udp subscriber_udp.c
 * Uso:       ./subscriber_udp <ip_broker> <tema1> [tema2 ...]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>   /* inet_pton, htons */
#include <sys/socket.h>  /* socket, sendto, recvfrom */

#define PUERTO 5001      /* mismo puerto que broker_udp.c */
#define MAX_TEMAS 10

char temas[MAX_TEMAS][50];
int num_temas = 0;
int ultimo_seq[MAX_TEMAS];     /* mayor seq recibido por tema */
int recibidos[MAX_TEMAS];      /* datagramas recibidos por tema */
int desorden[MAX_TEMAS];       /* llegaron con seq menor o igual al último */
volatile sig_atomic_t terminar = 0;

void al_ctrl_c(int s) { (void)s; terminar = 1; }

void resumen(void) {
    printf("\n===== Resumen =====\n");
    for (int i = 0; i < num_temas; i++) {
        int perdidos = ultimo_seq[i] - (recibidos[i] - desorden[i]);
        printf("%-20s recibidos=%d  ultimo_seq=%d  perdidos=%d  fuera_de_orden/duplicados=%d\n",
               temas[i], recibidos[i], ultimo_seq[i], perdidos < 0 ? 0 : perdidos, desorden[i]);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <ip_broker> <tema1> [tema2 ...]\n", argv[0]);
        return 1;
    }
    const char *ip = argv[1];

    /* Ctrl+C no mata el programa de golpe: marca 'terminar' para imprimir el resumen.
       sigaction sin SA_RESTART hace que recvfrom() se interrumpa al llegar la señal. */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = al_ctrl_c;
    sigaction(SIGINT, &sa, NULL);

    /* socket(): SOCK_DGRAM => UDP. El sistema le asigna un puerto efímero en el
       primer sendto(); por ese mismo puerto recibirá los mensajes del broker. */
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in broker;
    memset(&broker, 0, sizeof(broker));
    broker.sin_family = AF_INET;
    broker.sin_port   = htons(PUERTO);
    if (inet_pton(AF_INET, ip, &broker.sin_addr) != 1) {
        fprintf(stderr, "IP invalida: %s\n", ip);
        return 1;
    }

    char buffer[1024];
    for (int i = 2; i < argc && num_temas < MAX_TEMAS; i++) {
        snprintf(temas[num_temas], 50, "%s", argv[i]);
        num_temas++;
        snprintf(buffer, sizeof(buffer), "SUB|%s", argv[i]);
        /* sendto(): datagrama de suscripción. Si se pierde, el broker nunca sabrá
           que existimos: UDP no avisa. */
        sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr *)&broker, sizeof(broker));
        printf("Suscripcion enviada: %s\n", argv[i]);
    }
    printf("Esperando mensajes (Ctrl+C para terminar y ver resumen)...\n");

    while (!terminar) {
        struct sockaddr_in origen;
        socklen_t tam = sizeof(origen);
        /* recvfrom(): bloquea hasta que llega UN datagrama completo y dice de quién vino.
           A diferencia de TCP, cada llamada entrega exactamente un mensaje. Si el broker
           se cae, recvfrom() simplemente sigue esperando: no hay FIN que lo avise. */
        ssize_t n = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&origen, &tam);
        if (n < 0) continue;   /* interrumpido por Ctrl+C */
        buffer[n] = '\0';

        char tema[50], texto[512];
        int seq;
        if (sscanf(buffer, "PUB|%49[^|]|%d|%511[^\n]", tema, &seq, texto) != 3) continue;
        for (int i = 0; i < num_temas; i++) {
            if (strcmp(temas[i], tema) != 0) continue;
            const char *estado = "OK";
            recibidos[i]++;
            if (seq <= ultimo_seq[i])          { estado = "DESORDEN/DUPLICADO"; desorden[i]++; }
            else if (seq > ultimo_seq[i] + 1)    estado = "SE PERDIERON MENSAJES";
            if (seq > ultimo_seq[i]) ultimo_seq[i] = seq;
            if (recibidos[i] <= 50 || strcmp(estado, "OK") != 0)   /* no inunda la pantalla en ráfagas */
                printf("[%s] #%d %s   (%s)\n", tema, seq, texto, estado);
            break;
        }
    }

    resumen();
    close(sock);
    return 0;
}
