/*
 * publisher_tcp.c — Publicador (periodista) del sistema de noticias deportivas, versión TCP.
 *
 * Se conecta al broker TCP y envía N mensajes sobre un partido (tema).
 * Protocolo de aplicación (texto, una línea por mensaje, terminada en '\n'):
 *     PUB|<tema>|<seq>|<texto>
 * El número de secuencia <seq> permite verificar en el suscriptor (y en Wireshark)
 * si los mensajes llegan completos y en orden.
 *
 * Compilar:  gcc -Wall -o publisher_tcp publisher_tcp.c
 * Uso:       ./publisher_tcp <ip_broker> <tema> [num_mensajes] [ms_entre_mensajes]
 * Ejemplo:   ./publisher_tcp 127.0.0.1 ColombiaVsBrasil 10 500
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>   /* inet_pton, htons */
#include <sys/socket.h>  /* socket, connect, send */

#define PUERTO 6000      /* mismo puerto que broker_tcp.c */

/* Eventos de ejemplo que "reporta" el periodista; se recorren en ciclo. */
static const char *eventos[] = {
    "Inicia el partido",
    "Tarjeta amarilla al numero 10 de Equipo B",
    "Gol de Equipo A al minuto 32",
    "Cambio: jugador 10 entra por jugador 20",
    "Tiro de esquina para Equipo B",
    "Gol de Equipo B al minuto 45",
    "Fin del primer tiempo",
    "Inicia el segundo tiempo",
    "Tarjeta roja al numero 5 de Equipo A",
    "Gol de Equipo A al minuto 78",
    "Fin del partido"
};
#define NUM_EVENTOS (int)(sizeof(eventos) / sizeof(eventos[0]))

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <ip_broker> <tema> [num_mensajes] [ms_entre_mensajes]\n", argv[0]);
        return 1;
    }
    const char *ip   = argv[1];
    const char *tema = argv[2];
    int total = (argc > 3) ? atoi(argv[3]) : 10;   /* el enunciado pide al menos 10 */
    int pausa = (argc > 4) ? atoi(argv[4]) : 500;  /* milisegundos entre mensajes */

    /* socket(): crea un socket IPv4 (AF_INET) de flujo (SOCK_STREAM) => TCP.
       Devuelve un descriptor de archivo o -1 si falla. */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    /* Dirección del broker: familia, puerto (en orden de red con htons) e IP
       (convertida de texto a binario con inet_pton). */
    struct sockaddr_in broker;
    memset(&broker, 0, sizeof(broker));
    broker.sin_family = AF_INET;
    broker.sin_port   = htons(PUERTO);
    if (inet_pton(AF_INET, ip, &broker.sin_addr) != 1) {
        fprintf(stderr, "IP invalida: %s\n", ip);
        return 1;
    }

    /* connect(): inicia el three-way handshake (SYN, SYN-ACK, ACK) con el broker.
       Solo retorna con éxito cuando la conexión quedó establecida. */
    if (connect(sock, (struct sockaddr *)&broker, sizeof(broker)) < 0) {
        perror("connect");
        return 1;
    }
    printf("Publicador conectado al broker %s:%d, tema '%s'\n", ip, PUERTO, tema);

    char mensaje[512];
    for (int i = 1; i <= total; i++) {
        snprintf(mensaje, sizeof(mensaje), "PUB|%s|%d|%s\n",
                 tema, i, eventos[(i - 1) % NUM_EVENTOS]);

        /* send(): entrega los bytes al buffer de envío de TCP. TCP se encarga de
           segmentarlos, numerarlos, retransmitir si se pierden y entregarlos en orden. */
        if (send(sock, mensaje, strlen(mensaje), 0) < 0) {
            perror("send");
            break;
        }
        printf("Enviado: %s", mensaje);
        usleep(pausa * 1000);
    }

    /* close(): cierra la conexión (envía FIN; se observa el cierre de 4 vías en Wireshark). */
    close(sock);
    printf("Publicador terminado.\n");
    return 0;
}
