#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <pthread.h>
#include "chat.h"

int cola_global;
int cola_privada;
char nombre_usuario[MAX_NOMBRE];
char sala_actual[MAX_NOMBRE] = "";

// Hilo receptor
void *recibir_mensajes(void *arg) {
    struct mensaje msg;
    while (1) {
        if (cola_privada != -1) {
            // Recibir mensajes de la cola de la sala
            if (msgrcv(cola_privada, &msg, sizeof(struct mensaje) - sizeof(long), 0, 0) == -1) {
                perror("Error al recibir mensaje de la sala");
                continue;
            }

            // Mostrar el mensaje si no es del propio usuario
            if (strcmp(msg.remitente, nombre_usuario) != 0) {
                printf("%s: %s\n", msg.remitente, msg.texto);
            }
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <nombre_usuario>\n", argv[0]);
        exit(1);
    }
    strcpy(nombre_usuario, argv[1]);

    key_t key_global = ftok("/tmp", 'A');
    cola_global = msgget(key_global, 0666);
    if (cola_global == -1) {
        perror("Error al conectar a la cola global");
        exit(1);
    }

    cola_privada = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
    if (cola_privada == -1) {
        perror("Error al crear cola privada");
        exit(1);
    }

    printf("Bienvenido, %s.\n", nombre_usuario);

    pthread_t hilo_receptor;
    pthread_create(&hilo_receptor, NULL, recibir_mensajes, NULL);

    struct mensaje msg;
    char comando[MAX_TEXTO];

    while (1) {
        printf("> ");
        fgets(comando, MAX_TEXTO, stdin);
        comando[strcspn(comando, "\n")] = '\0';

        if (strncmp(comando, "join ", 5) == 0) {
            char sala[MAX_NOMBRE];
            sscanf(comando, "join %s", sala);

            msg.mtype = MSG_JOIN;
            strcpy(msg.remitente, nombre_usuario);
            strcpy(msg.sala, sala);
            msg.cola_id = cola_privada;
            msg.texto[0] = '\0';

            msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0);
            strcpy(sala_actual, sala);

        } else if (strcmp(comando, "leave") == 0) {
            if (strlen(sala_actual) == 0) {
                printf("No estás en ninguna sala.\n");
                continue;
            }
            msg.mtype = MSG_LEAVE;
            strcpy(msg.remitente, nombre_usuario);
            strcpy(msg.sala, sala_actual);
            msg.cola_id = cola_privada;
            msg.texto[0] = '\0';

            msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0);
            sala_actual[0] = '\0';

        } else if (strcmp(comando, "/list") == 0) {
            msg.mtype = MSG_LIST;
            strcpy(msg.remitente, nombre_usuario);
            msg.cola_id = cola_privada;
            msg.texto[0] = '\0';
            msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0);

        } else if (strcmp(comando, "/users") == 0) {
            if (strlen(sala_actual) == 0) {
                printf("No estás en ninguna sala.\n");
                continue;
            }
            msg.mtype = MSG_USERS;
            strcpy(msg.remitente, nombre_usuario);
            strcpy(msg.sala, sala_actual);
            msg.cola_id = cola_privada;
            msg.texto[0] = '\0';
            msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0);

        } else if (strlen(comando) > 0) {
            if (strlen(sala_actual) == 0) {
                printf("No estás en ninguna sala. Usa 'join <sala>'\n");
                continue;
            }
            msg.mtype = MSG_MSG;
            strcpy(msg.remitente, nombre_usuario);
            strcpy(msg.sala, sala_actual);
            msg.cola_id = cola_privada;
            strcpy(msg.texto, comando);

            msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0);
        }
    }

    return 0;
}