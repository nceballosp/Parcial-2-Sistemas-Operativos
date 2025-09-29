#ifndef CHAT_H
#define CHAT_H

#define MAX_SALAS 10
#define MAX_USUARIOS_POR_SALA 20
#define MAX_TEXTO 256
#define MAX_NOMBRE 50

// Tipos de mensajes
#define MSG_JOIN     1
#define MSG_CONFIRM  2
#define MSG_MSG      3
#define MSG_LEAVE    4
#define MSG_ERROR    5
#define MSG_LIST     6
#define MSG_USERS    7

// Estructura para mensajes
struct mensaje {
    long mtype;                 // Tipo de mensaje
    char remitente[MAX_NOMBRE]; // Nombre usuario
    char texto[MAX_TEXTO];      // Texto del mensaje
    char sala[MAX_NOMBRE];      // Nombre de sala
    int cola_id;                // ID de la cola privada del remitente
};

// Estructura para usuario
struct usuario {
    char nombre[MAX_NOMBRE];
    int cola_id; 
};

// Estructura para sala
struct sala {
    char nombre[MAX_NOMBRE];
    int num_usuarios;
    struct usuario usuarios[MAX_USUARIOS_POR_SALA];
};

#endif
