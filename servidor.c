#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include "chat.h"

struct sala salas[MAX_SALAS];
int num_salas = 0;

// Crear nueva sala
int crear_sala(const char *nombre) {
    if (num_salas >= MAX_SALAS) return -1;
    strcpy(salas[num_salas].nombre, nombre);
    salas[num_salas].num_usuarios = 0;
    num_salas++;
    return num_salas - 1;
}

// Buscar sala por nombre
int buscar_sala(const char *nombre) {
    for (int i = 0; i < num_salas; i++) {
        if (strcmp(salas[i].nombre, nombre) == 0) return i;
    }
    return -1;
}

// Agregar usuario
int agregar_usuario_a_sala(int indice_sala, const char *nombre_usuario, int cola_usuario) {
    if (indice_sala < 0 || indice_sala >= num_salas) return -1;

    struct sala *s = &salas[indice_sala];
    if (s->num_usuarios >= MAX_USUARIOS_POR_SALA) return -1;

    // Verificar duplicados
    for (int i = 0; i < s->num_usuarios; i++) {
        if (strcmp(s->usuarios[i].nombre, nombre_usuario) == 0) {
            return -1;
        }
    }

    strcpy(s->usuarios[s->num_usuarios].nombre, nombre_usuario);
    s->usuarios[s->num_usuarios].cola_id = cola_usuario;
    s->num_usuarios++;
    return 0;
}

int quitar_usuario_de_sala(int indice_sala, const char *nombre_usuario) {
    if (indice_sala < 0 || indice_sala >= num_salas) return -1;
    struct sala *s = &salas[indice_sala];

    for (int i = 0; i < s->num_usuarios; i++) {
        if (strcmp(s->usuarios[i].nombre, nombre_usuario) == 0) {
            for (int j = i; j < s->num_usuarios - 1; j++) {
                s->usuarios[j] = s->usuarios[j + 1];
            }
            s->num_usuarios--;
            return 0;
        }
    }
    return -1;
}

// Enviar mensaje a todos en la sala
void enviar_a_todos_en_sala(int indice_sala, struct mensaje *msg) {
    if (indice_sala < 0 || indice_sala >= num_salas) return;
    struct sala *s = &salas[indice_sala];
    for (int i = 0; i < s->num_usuarios; i++) {
        if (msgsnd(s->usuarios[i].cola_id, msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
            perror("Error al enviar mensaje");
        }
    }
}

int main() {
    key_t key_global = ftok("/tmp", 'A');
    int cola_global = msgget(key_global, IPC_CREAT | 0666);
    if (cola_global == -1) {
        perror("Error al crear cola global");
        exit(1);
    }

    printf("Servidor de chat iniciado.\n");
    struct mensaje msg;

    while (1) {
        if (msgrcv(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0, 0) == -1) {
            perror("Error al recibir mensaje");
            continue;
        }

        if (msg.mtype == MSG_JOIN) {
            int indice_sala = buscar_sala(msg.sala);
            if (indice_sala == -1) indice_sala = crear_sala(msg.sala);

            if (agregar_usuario_a_sala(indice_sala, msg.remitente, msg.cola_id) == 0) {
                printf("Usuario %s entró a %s\n", msg.remitente, msg.sala);

                struct mensaje respuesta;
                respuesta.mtype = MSG_CONFIRM;
                strcpy(respuesta.remitente, "Servidor");
                strcpy(respuesta.sala, msg.sala);
                sprintf(respuesta.texto, "Te has unido a la sala: %s", msg.sala);

                msgsnd(msg.cola_id, &respuesta, sizeof(struct mensaje) - sizeof(long), 0);
            }

        } else if (msg.mtype == MSG_MSG) {
            printf("[%s] %s: %s\n", msg.sala, msg.remitente, msg.texto);

            int indice_sala = buscar_sala(msg.sala);
            if (indice_sala != -1) {
                enviar_a_todos_en_sala(indice_sala, &msg);

                // Guardar en historial
                char filename[100];
                sprintf(filename, "%s_historial.txt", msg.sala);
                FILE *f = fopen(filename, "a");
                if (f) {
                    fprintf(f, "%s: %s\n", msg.remitente, msg.texto);
                    fclose(f);
                }
            }

        } else if (msg.mtype == MSG_LEAVE) {
            int indice_sala = buscar_sala(msg.sala);
            if (indice_sala != -1) {
                quitar_usuario_de_sala(indice_sala, msg.remitente);
                printf("Usuario %s salió de %s\n", msg.remitente, msg.sala);
            }

        } else if (msg.mtype == MSG_LIST) {
            struct mensaje respuesta;
            respuesta.mtype = MSG_LIST;
            strcpy(respuesta.remitente, "Servidor");
            respuesta.texto[0] = '\0';

            for (int i = 0; i < num_salas; i++) {
                strcat(respuesta.texto, salas[i].nombre);
                strcat(respuesta.texto, " ");
            }
            msgsnd(msg.cola_id, &respuesta, sizeof(struct mensaje) - sizeof(long), 0);

        } else if (msg.mtype == MSG_USERS) {
            int indice_sala = buscar_sala(msg.sala);
            if (indice_sala != -1) {
                struct mensaje respuesta;
                respuesta.mtype = MSG_USERS;
                strcpy(respuesta.remitente, "Servidor");
                respuesta.texto[0] = '\0';

                struct sala *s = &salas[indice_sala];
                for (int i = 0; i < s->num_usuarios; i++) {
                    strcat(respuesta.texto, s->usuarios[i].nombre);
                    strcat(respuesta.texto, " ");
                }
                msgsnd(msg.cola_id, &respuesta, sizeof(struct mensaje) - sizeof(long), 0);
            }
        }
    }
    return 0;
}
