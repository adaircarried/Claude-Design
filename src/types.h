/* =============================================================================
 * types.h  -  Tipos compartidos entre modulos
 * =============================================================================
 *
 * POR QUE EXISTE ESTE ARCHIVO
 * ---------------------------
 * Setpoint_t lo necesitan TaskMotion (consumidor) y TaskComms (productor).
 * Si el tipo viviera en motion.h, comms.cpp tendria que hacer #include
 * "motion.h" y quedaria acoplado al modulo de movimiento: exactamente el
 * acoplamiento que la cola de setpoints existe para evitar.
 *
 * Con types.h, en el Parcial 2 el parser binario reemplaza a comms.cpp sin
 * que motion.cpp se recompile siquiera por un cambio de cabecera, y en el
 * Parcial 3 la logica de juego encola sin conocer nada del control de ejes.
 * ---------------------------------------------------------------------------*/

#pragma once
#include <stdint.h>

/* ---------------------------------------------------------------------------
 * Tipo de movimiento. El orden de los primeros cinco se conserva tal cual
 * para no romper compatibilidad con el protocolo binario del Parcial 2, donde
 * estos valores viajaran como un byte dentro de la trama.
 * -------------------------------------------------------------------------*/
typedef enum {
    MOTION_JOG     = 0,   /* un eje a la vez, velocidad reducida           */
    MOTION_MOVJ    = 1,   /* interpolacion articular, ambos ejes a la vez  */
    MOTION_MOVL    = 2,   /* interpolacion lineal en cartesiano            */
    MOTION_HOME    = 3,   /* rutina de referenciado                        */
    MOTION_STOP    = 4,   /* parada con desaceleracion (ver nota abajo)    */
    MOTION_GRIPPER = 5,   /* RESERVADO PARCIAL 3: abrir/cerrar la pinza.
                             Se declara ya para que el enum no cambie de
                             numeracion cuando llegue el momento.           */
    MOTION_TEST    = 6,   /* prueba de perdida de pasos (ver motion.cpp)   */
    MOTION_SETZERO = 7    /* cero enseñado: "la pose actual vale target_a,
                             target_b grados". Va por la cola, y no como
                             llamada directa, para que nunca se redefina la
                             posicion con un movimiento a medias.          */
} MotionType_t;

/* ---------------------------------------------------------------------------
 * NOTA IMPORTANTE SOBRE MOTION_STOP
 * ---------------------------------------------------------------------------
 * MOTION_STOP existe en el enum, pero un STOP NO se atiende encolandolo.
 * Si TaskMotion esta ejecutando un MOVJ de 3 segundos y solo lee la cola
 * entre movimientos, un STOP encolado no haria nada hasta que el movimiento
 * terminara. Un paro que llega tarde no es un paro.
 *
 * El paro viaja FUERA DE BANDA: motion_request_abort() levanta una bandera
 * que el lazo interno de TaskMotion consulta cada MOTION_TICK_MS y ademas
 * vacia la cola. Ver supervisor.h.
 * -------------------------------------------------------------------------*/

/* ---------------------------------------------------------------------------
 * Un setpoint. Es la UNICA forma de pedirle movimiento al robot.
 * Quien llena la cola cambia en cada parcial; TaskMotion no cambia nunca:
 *      P1 -> consola de texto        (comms.cpp)
 *      P2 -> parser binario con CRC  (comms.cpp reescrito)
 *      P3 -> logica de juego         (nuevo modulo, mismo contrato)
 * -------------------------------------------------------------------------*/
typedef struct {
    MotionType_t type;
    float        target_a;    /* q1 en grados, o X en mm si es MOVL */
    float        target_b;    /* q2 en grados, o Y en mm si es MOVL */
    uint16_t     speed_pct;   /* 1 a 100 */
    uint8_t      axis_id;     /* 1 o 2; solo se usa en JOG           */
    int32_t      aux;         /* uso libre por tipo:
                                 TEST -> numero de ciclos
                                 GRIPPER (P3) -> angulo del servo    */
    uint8_t      seq;         /* SEQ de la trama que lo pidio (U2); 0 en
                                 la consola de texto. Viaja con el
                                 setpoint para que el aviso DONE/FAILED
                                 diga a que orden corresponde.       */
    uint8_t      flags;       /* SP_FLAG_*                           */
} Setpoint_t;

#define SP_FLAG_DIR     0x01  /* JOG en sentido negativo                    */
#define SP_FLAG_NOTIFY  0x02  /* avisar con DONE cuando termine             */

/* ---------------------------------------------------------------------------
 * CODIGOS DE ERROR
 * ---------------------------------------------------------------------------
 * Son los del protocolo binario (docs/PROTOCOLO_U2.md, seccion 6) y TAMBIEN
 * los que usa TaskMotion para decir por que no pudo ejecutar un setpoint.
 * Viven aqui, y no en el modulo del protocolo, para que motion.cpp no tenga
 * que conocer nada del protocolo: solo publica un codigo.
 * -------------------------------------------------------------------------*/
typedef enum {
    ERR_NONE             = 0x00,
    ERR_CRC              = 0x01,
    ERR_TRAMA            = 0x02,
    ERR_TIMEOUT          = 0x03,
    ERR_CMD_DESCONOCIDO  = 0x04,
    ERR_MOTOR_INVALIDO   = 0x05,
    ERR_LONGITUD         = 0x06,
    ERR_FUERA_DE_RANGO   = 0x07,
    ERR_COLA_LLENA       = 0x08,
    ERR_SIN_CERO         = 0x09,
    ERR_DRIVERS_APAGADOS = 0x0A,
    ERR_FALLO_ACTIVO     = 0x0B,
    ERR_DESBORDE_RX      = 0x0C,
    ERR_NO_IMPLEMENTADO  = 0x0D,
    ERR_RAMA_CODO        = 0x0E,
    ERR_TRAYECTORIA      = 0x0F,
    ERR_ABORTADO         = 0x10   /* un STOP o un fallo lo interrumpio */
} ErrCode_t;

/* ---------------------------------------------------------------------------
 * Aviso de TaskMotion sobre un setpoint que termino o que no pudo ejecutarse.
 * Viaja por una cola propia (motionEventQueue) hasta TaskComms, que es quien
 * lo convierte en una trama DONE o FAILED. Asi TaskMotion nunca escribe en el
 * puerto serial (regla 3 del proyecto).
 * -------------------------------------------------------------------------*/
typedef enum { MEV_DONE = 0, MEV_FAILED = 1 } MotionEventKind_t;

typedef struct {
    uint8_t seq;
    uint8_t kind;   /* MotionEventKind_t */
    uint8_t code;   /* ErrCode_t; ERR_NONE si kind = MEV_DONE */
} MotionEvent_t;

/* ---------------------------------------------------------------------------
 * Estado global del robot. Lo publica TaskSupervisor; lo leen telemetria y
 * comunicaciones. Un solo valor, escrito desde una sola tarea.
 * -------------------------------------------------------------------------*/
typedef enum {
    STATE_BOOT     = 0,
    STATE_UNHOMED  = 1,   /* energizado pero sin referencia -> no acepta MOVJ/MOVL */
    STATE_HOMING   = 2,
    STATE_IDLE     = 3,   /* referenciado y quieto, con par de retencion */
    STATE_MOVING   = 4,
    STATE_STOPPED  = 5,   /* tras un STOP: quieto, con par, cola vacia */
    STATE_DISABLED = 6,   /* ENA liberado. EL BRAZO CAE POR GRAVEDAD.  */
    STATE_FAULT    = 7
} RobotState_t;

typedef enum {
    FAULT_NONE            = 0,
    FAULT_LIMIT_HIT       = 1,  /* final de carrera pisado fuera del homing */
    FAULT_HOMING_TIMEOUT  = 2,  /* recorrio HOMING_MAX_TRAVEL_DEG sin tocar */
    FAULT_SOFT_LIMIT      = 3,  /* posicion fuera de los limites articulares */
    FAULT_WORKSPACE       = 4,  /* efector fuera del anillo util             */
    FAULT_ESTOP           = 5,  /* paro solicitado por el usuario            */
    FAULT_QUEUE_FULL      = 6,
    FAULT_RX_OVERFLOW     = 7,  /* preparado para el Parcial 2               */
    FAULT_LINK_LOST       = 8   /* el watchdog del enlace paro el robot      */
} FaultCode_t;

/* Configuracion del codo. Debe mantenerse CONSTANTE a lo largo de una
 * trayectoria MOVL: cambiar de rama a mitad de camino hace que el codo se
 * voltee violentamente aunque el efector siga sobre la recta. */
typedef enum {
    ELBOW_DOWN = 0,   /* q2 > 0: el codo queda por debajo de la recta
                         hombro-efector                                    */
    ELBOW_UP   = 1    /* q2 < 0: el codo queda por encima. Es el valor por
                         defecto: en un brazo vertical con la mesa debajo,
                         mantiene el codo lejos de la mesa.                 */
} ElbowConfig_t;
