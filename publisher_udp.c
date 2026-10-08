/*
 * publisher_udp.c — Publicador (periodista) del sistema de noticias deportivas, versión UDP.
 *
 * Envía N datagramas al broker UDP, uno por evento del partido (tema):
 *     PUB|<tema>|<seq>|<texto>
 * No hay conexión: cada sendto() lanza un datagrama independiente y no hay confirmación
 * de que haya llegado. El número de secuencia <seq> permite al suscriptor detectar
 * pérdidas, duplicados o desorden.
 *
 * Compilar:  gcc -Wall -o publisher_udp publisher_udp.c
 * Uso:       ./publisher_udp <ip_broker> <tema> [num_mensajes] [ms_entre_mensajes]
 * Ejemplo:   ./publisher_udp 127.0.0.1 ColombiaVsBrasil 10 500
 *            ./publisher_udp 127.0.0.1 ColombiaVsBrasil 5000 0   (ráfaga para provocar pérdida)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>   /* inet_pton, htons */
#include <sys/socket.h>  /* socket, sendto */

#define PUERTO 5001      /* mismo puerto que broker_udp.c */

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
    int total = (argc > 3) ? atoi(argv[3]) : 10;
    int pausa = (argc > 4) ? atoi(argv[4]) : 500;
    int silencioso = total > 50;   /* en ráfagas grandes no imprime cada mensaje */

    /* socket(): SOCK_DGRAM => UDP. No hay connect() ni handshake. */
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
    printf("Publicador UDP -> broker %s:%d, tema '%s'\n", ip, PUERTO, tema);

    char mensaje[512];
    for (int i = 1; i <= total; i++) {
        snprintf(mensaje, sizeof(mensaje), "PUB|%s|%d|%s",
                 tema, i, eventos[(i - 1) % NUM_EVENTOS]);

        /* sendto(): envía UN datagrama a la dirección indicada. Como no hay conexión,
           cada llamada lleva la dirección de destino. Que sendto() retorne bien solo
           significa que el datagrama salió del host, no que haya llegado. */
        if (sendto(sock, mensaje, strlen(mensaje), 0,
                   (struct sockaddr *)&broker, sizeof(broker)) < 0) {
            perror("sendto");
        } else if (!silencioso) {
            printf("Enviado: %s\n", mensaje);
        }
        if (pausa > 0) usleep(pausa * 1000);
    }

    /* close(): libera el socket. No se envía nada a la red (no hay FIN en UDP). */
    close(sock);
    printf("Publicador terminado (%d mensajes enviados).\n", total);
    return 0;
}
