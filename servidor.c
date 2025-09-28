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

// Función para crear una nueva sala
int crear_sala(const char *nombre) {
    if (num_salas >= MAX_SALAS) {
        return -1; // No se pueden crear más salas
    }

    // Crear una cola de mensajes para la sala
    key_t key = ftok("/tmp", num_salas + 1); // Generar una clave única
    int cola_id = msgget(key, IPC_CREAT | 0666);
    if (cola_id == -1) {
        perror("Error al crear la cola de la sala");
        return -1;
    }

    // Inicializar la sala
    strcpy(salas[num_salas].nombre, nombre);
    salas[num_salas].cola_id = cola_id;
    salas[num_salas].num_usuarios = 0;

    num_salas++;
    return num_salas - 1; // Retornar el índice de la sala creada
}

// Función para buscar una sala por nombre
int buscar_sala(const char *nombre) {
    for (int i = 0; i < num_salas; i++) {
        if (strcmp(salas[i].nombre, nombre) == 0) {
            return i;
        }
    }
    return -1; // No encontrada
}

// Función para agregar un usuario a una sala
int agregar_usuario_a_sala(int indice_sala, const char *nombre_usuario) {
    if (indice_sala < 0 || indice_sala >= num_salas) {
        return -1;
    }

    struct sala *s = &salas[indice_sala];
    if (s->num_usuarios >= MAX_USUARIOS_POR_SALA) {
        return -1; // Sala llena
    }

    // Verificar si el usuario ya está en la sala
    for (int i = 0; i < s->num_usuarios; i++) {
        if (strcmp(s->usuarios[i], nombre_usuario) == 0) {
            return -1; // Usuario ya está en la sala
        }
    }

    // Agregar el usuario
    strcpy(s->usuarios[s->num_usuarios], nombre_usuario);
    s->num_usuarios++;
    return 0;
}
int quitar_usuario_de_sala(int indice_sala, const char *nombre_usuario) {
    if (indice_sala < 0 || indice_sala >= num_salas) return -1;
    struct sala *s = &salas[indice_sala];

    for (int i = 0; i < s->num_usuarios; i++) {
        if (strcmp(s->usuarios[i], nombre_usuario) == 0) {
            // Desplazar los usuarios hacia atrás
            for (int j = i; j < s->num_usuarios - 1; j++) {
                strcpy(s->usuarios[j], s->usuarios[j + 1]);
            }
            s->num_usuarios--;
            return 0;
        }
    }
    return -1; // No estaba en la sala
}


// Función para enviar un mensaje a todos los usuarios de una sala
void enviar_a_todos_en_sala(int indice_sala, struct mensaje *msg) {
    if (indice_sala < 0 || indice_sala >= num_salas) {
        return;
    }

    struct sala *s = &salas[indice_sala];
    for (int i = 0; i < s->num_usuarios; i++) {
        // Enviar el mensaje a la cola de la sala
        if (msgsnd(s->cola_id, msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
            perror("Error al enviar mensaje a la sala");
        }
    }
}

int main() {
    // Crear la cola global para solicitudes de clientes
    key_t key_global = ftok("/tmp", 'A');
    int cola_global = msgget(key_global, IPC_CREAT | 0666);
    if (cola_global == -1) {
        perror("Error al crear la cola global");
        exit(1);
    }

    printf("Servidor de chat iniciado. Esperando clientes...\n");

    struct mensaje msg;

    while (1) {
        // Recibir mensajes de la cola global
        if (msgrcv(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0, 0) == -1) {
            perror("Error al recibir mensaje");
            continue;
        }

        // Procesar el mensaje según su tipo
        if (msg.mtype == MSG_JOIN) { // JOIN
            printf("Solicitud de unirse a la sala: %s por %s\n", msg.sala, msg.remitente);

            // Buscar o crear la sala
            int indice_sala = buscar_sala(msg.sala);
            if (indice_sala == -1) {
                indice_sala = crear_sala(msg.sala);
                if (indice_sala == -1) {
                    printf("No se pudo crear la sala %s\n", msg.sala);
                    continue;
                }
                printf("Nueva sala creada: %s\n", msg.sala);
            }

            // Agregar el usuario a la sala
            if (agregar_usuario_a_sala(indice_sala, msg.remitente) == 0) {
                printf("Usuario %s agregado a la sala %s\n", msg.remitente, msg.sala);

                // Enviar confirmación al cliente (usando el mismo tipo de mensaje)
                msg.mtype = MSG_CONFIRM; // Tipo de respuesta
                sprintf(msg.texto, "Te has unido a la sala: %s", msg.sala);
                msg.cola_id = salas[buscar_sala(msg.sala)].cola_id;

                if (msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
                    perror("Error al enviar confirmación");
                }
            } else {
                printf("No se pudo agregar al usuario %s a la sala %s\n", msg.remitente, msg.sala);
            }
        } else if (msg.mtype == MSG_MSG) { // MSG
            printf("Mensaje en la sala %s de %s: %s\n", msg.sala, msg.remitente, msg.texto);

            // Buscar la sala
            int indice_sala; 
            indice_sala = buscar_sala(msg.sala);
            if (indice_sala != -1) {
                // Reenviar el mensaje a todos en la sala
                enviar_a_todos_en_sala(indice_sala, &msg);
            }
            char filename[100];
            sprintf(filename, "%s_historial.txt", msg.sala);
            FILE *f = fopen(filename, "a");
            if (f) {
                fprintf(f, "%s: %s\n", msg.remitente, msg.texto);
                fclose(f);
            }

            indice_sala = buscar_sala(msg.sala);
            if (indice_sala != -1) {
                enviar_a_todos_en_sala(indice_sala, &msg);
            }
        } else if (msg.mtype == MSG_LEAVE) { // Exit
            int indice_sala = buscar_sala(msg.sala);
            if (indice_sala != -1) {
                if (quitar_usuario_de_sala(indice_sala, msg.remitente) == 0) {
                    printf("Usuario %s salió de la sala %s\n", msg.remitente, msg.sala);
                }
            }
        } else if (msg.mtype == MSG_LIST){
            printf("Solicitud de listar salas por %s\n", msg.remitente);
            struct mensaje respuesta;
            respuesta.mtype = MSG_LIST;
            strcpy(respuesta.remitente, "Servidor");
            strcpy(respuesta.sala, "");
            respuesta.cola_id = 0;

            char buffer[MAX_TEXTO] = "";
            for (int i = 0; i < num_salas; i++) {
                strcat(buffer, salas[i].nombre);
                strcat(buffer, " ");
            }
            strcpy(respuesta.texto, buffer);

            if (msgsnd(cola_global, &respuesta, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
                perror("Error al enviar lista de salas");
            }
        } else if (msg.mtype == MSG_USERS) {
            int indice_sala = buscar_sala(msg.sala);
            if (indice_sala != -1) {
                struct mensaje respuesta;
                respuesta.mtype = MSG_USERS;
                strcpy(respuesta.remitente, "Servidor");
                strcpy(respuesta.sala, msg.sala);
                respuesta.cola_id = 0;

                char buffer[MAX_TEXTO] = "";
                for (int i = 0; i < salas[indice_sala].num_usuarios; i++) {
                    strcat(buffer, salas[indice_sala].usuarios[i]);
                    strcat(buffer, " ");
                }
                strcpy(respuesta.texto, buffer);

                if (msgsnd(cola_global, &respuesta, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
                    perror("Error al enviar lista de usuarios");
                }
            }
        }
    }

    return 0;
}