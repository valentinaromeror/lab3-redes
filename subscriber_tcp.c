/*
 * subscriber_tcp.c — Suscriptor (hincha) del sistema de noticias deportivas, versión TCP.
 *
 * Se conecta al broker TCP, se suscribe a uno o varios partidos (temas) enviando
 *     SUB|<tema>\n
 * y luego imprime cada actualización que el broker le reenvía:
 *     PUB|<tema>|<seq>|<texto>
 * Revisa el número de secuencia por tema para detectar mensajes perdidos o desordenados
 * (en TCP no debería ocurrir; sirve para comparar con la versión UDP).
 *
 * Compilar:  gcc -Wall -o subscriber_tcp subscriber_tcp.c
 * Uso:       ./subscriber_tcp <ip_broker> <tema1> [tema2 ...]
 * Ejemplo:   ./subscriber_tcp 127.0.0.1 ColombiaVsBrasil ArgentinaVsChile
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>   /* inet_pton, htons */
#include <sys/socket.h>  /* socket, connect, send, recv */

#define PUERTO 6000      /* mismo puerto que broker_tcp.c */
#define MAX_TEMAS 10     /* mismo límite que el broker */

char temas[MAX_TEMAS][50];
int ultimo_seq[MAX_TEMAS];  /* último número de secuencia recibido por tema */
int num_temas = 0;

/* Procesa una línea completa recibida del broker y verifica la secuencia. */
void procesar_linea(char *linea) {
    char tema[50], texto[512];
    int seq;
    if (sscanf(linea, "PUB|%49[^|]|%d|%511[^\n]", tema, &seq, texto) != 3) {
        printf("[?] %s\n", linea);
        return;
    }
    for (int i = 0; i < num_temas; i++) {
        if (strcmp(temas[i], tema) == 0) {
            const char *estado = "OK";
            if (seq <= ultimo_seq[i])          estado = "DESORDEN/DUPLICADO";
            else if (seq > ultimo_seq[i] + 1)  estado = "SE PERDIERON MENSAJES";
            printf("[%s] #%d %s   (%s)\n", tema, seq, texto, estado);
            if (seq > ultimo_seq[i]) ultimo_seq[i] = seq;
            return;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <ip_broker> <tema1> [tema2 ...]\n", argv[0]);
        return 1;
    }
    const char *ip = argv[1];

    /* socket(): socket IPv4 de flujo => TCP. */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in broker;
    memset(&broker, 0, sizeof(broker));
    broker.sin_family = AF_INET;
    broker.sin_port   = htons(PUERTO);
    if (inet_pton(AF_INET, ip, &broker.sin_addr) != 1) {
        fprintf(stderr, "IP invalida: %s\n", ip);
        return 1;
    }

    /* connect(): three-way handshake con el broker. */
    if (connect(sock, (struct sockaddr *)&broker, sizeof(broker)) < 0) {
        perror("connect");
        return 1;
    }
    printf("Suscriptor conectado al broker %s:%d\n", ip, PUERTO);

    /* Envía una línea SUB| por cada partido que se quiere seguir. */
    char mensaje[128];
    for (int i = 2; i < argc && num_temas < MAX_TEMAS; i++) {
        snprintf(temas[num_temas], 50, "%s", argv[i]);
        ultimo_seq[num_temas] = 0;
        num_temas++;
        snprintf(mensaje, sizeof(mensaje), "SUB|%s\n", argv[i]);
        send(sock, mensaje, strlen(mensaje), 0);
        printf("Suscrito a: %s\n", argv[i]);
    }

    /* TCP es un flujo de bytes, no de mensajes: un recv() puede traer media línea
       o varias líneas juntas. Por eso se acumula en 'pendiente' y se procesa
       línea por línea, separando por '\n'. */
    char pendiente[4096];
    size_t usados = 0;
    while (1) {
        /* recv(): bloquea hasta que llegan bytes. Retorna 0 si el broker cerró
           la conexión (FIN) y -1 si hubo error (p. ej. RST). */
        ssize_t n = recv(sock, pendiente + usados, sizeof(pendiente) - usados - 1, 0);
        if (n <= 0) {
            printf("El broker cerro la conexion.\n");
            break;
        }
        usados += n;
        pendiente[usados] = '\0';

        char *inicio = pendiente, *fin;
        while ((fin = strchr(inicio, '\n')) != NULL) {
            *fin = '\0';
            procesar_linea(inicio);
            inicio = fin + 1;
        }
        /* Mueve al inicio del buffer lo que quedó sin '\n' (línea incompleta). */
        usados = strlen(inicio);
        memmove(pendiente, inicio, usados);
    }

    close(sock);
    return 0;
}
