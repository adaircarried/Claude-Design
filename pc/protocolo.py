"""
protocolo.py - Trama del protocolo binario de la U2 (lado de la PC)
====================================================================

Implementacion en Python del mismo formato que src/frame.cpp del firmware:

    SOF | LEN | SEQ | CMD | MOTOR | FLAGS | PAYLOAD | CRC_L CRC_H | EOF
   0xAA                                                            0x55

Especificacion completa en docs/PROTOCOLO_U2.md.

Este modulo es PURO: no abre puertos ni usa hilos. Por eso se prueba sin
hardware (test_protocolo.py) contra los mismos vectores que los tests en C++
del firmware. Si los dos lados producen y aceptan exactamente los mismos
bytes, se entienden.

Lo reutilizara la U3: el programa maestro con vision manda sus subrutinas
con estas mismas funciones.
"""

from __future__ import annotations

import struct
import time
from dataclasses import dataclass
from enum import IntEnum

# --- Trama (deben coincidir con config.h del firmware) -----------------------
SOF = 0xAA
EOF = 0x55
MAX_PAYLOAD = 32
OVERHEAD = 9
TIMEOUT_ENTRE_BYTES_S = 0.020

# --- Bus ---------------------------------------------------------------------
BAUDIOS = 115200
BITS = 8
PARIDAD = "E"          # par
BITS_PARO = 1


class Cmd(IntEnum):
    PING = 0x01
    GET_STATUS = 0x02
    GET_DIAG = 0x03
    MODE_TEXT = 0x04
    DEBUG_PAUSE = 0x05
    JOG = 0x10
    MOVJ = 0x11
    MOVL = 0x12
    GRIPPER = 0x13
    STOP = 0x20
    ENABLE = 0x21
    DISABLE = 0x22
    SETZERO = 0x23
    CLEAR = 0x24


class Rsp(IntEnum):
    PONG = 0x81
    STATUS = 0x82
    DIAG = 0x83
    ACK = 0x90
    NACK = 0x91
    DONE = 0x92
    FAILED = 0x93


class Err(IntEnum):
    NONE = 0x00
    CRC = 0x01
    TRAMA = 0x02
    TIMEOUT = 0x03
    CMD_DESCONOCIDO = 0x04
    MOTOR_INVALIDO = 0x05
    LONGITUD = 0x06
    FUERA_DE_RANGO = 0x07
    COLA_LLENA = 0x08
    SIN_CERO = 0x09
    DRIVERS_APAGADOS = 0x0A
    FALLO_ACTIVO = 0x0B
    DESBORDE_RX = 0x0C
    NO_IMPLEMENTADO = 0x0D
    RAMA_CODO = 0x0E
    TRAYECTORIA = 0x0F
    ABORTADO = 0x10


FLAG_DIR = 0x01       # JOG en sentido negativo
FLAG_NOTIFY = 0x02    # avisar con DONE al terminar

# Nombres legibles de RobotState_t y FaultCode_t (types.h)
ESTADOS = ["ARRANQUE", "SIN_REFERENCIA", "REFERENCIANDO", "LISTO",
           "EN_MOVIMIENTO", "DETENIDO", "DRIVERS_LIBRES", "FALLO"]
FALLOS = ["ninguno", "FINAL_DE_CARRERA", "HOMING_SIN_EXITO", "LIMITE_ARTICULAR",
          "FUERA_DE_ESPACIO", "PARO_USUARIO", "COLA_LLENA", "DESBORDE_RX",
          "ENLACE_PERDIDO"]


def nombre_error(codigo: int) -> str:
    try:
        return Err(codigo).name
    except ValueError:
        return f"0x{codigo:02X}"


def nombre_cmd(codigo: int) -> str:
    for enum in (Cmd, Rsp):
        try:
            return enum(codigo).name
        except ValueError:
            pass
    return f"0x{codigo:02X}"


# --- CRC-16/CCITT-FALSE --------------------------------------------------------
def crc16(data: bytes, crc: int = 0xFFFF) -> int:
    """Polinomio 0x1021, inicial 0xFFFF. crc16(b"123456789") == 0x29B1."""
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


# --- Construccion ------------------------------------------------------------------
@dataclass
class Trama:
    seq: int
    cmd: int
    motor: int = 0
    flags: int = 0
    payload: bytes = b""

    def __str__(self) -> str:
        return (f"{nombre_cmd(self.cmd)} seq={self.seq} motor={self.motor} "
                f"flags=0x{self.flags:02X} payload={self.payload.hex(' ') or '-'}")


def construir(seq: int, cmd: int, motor: int = 0, flags: int = 0,
              payload: bytes = b"") -> bytes:
    if len(payload) > MAX_PAYLOAD:
        raise ValueError(f"payload de {len(payload)} bytes; el maximo es {MAX_PAYLOAD}")
    cuerpo = bytes([len(payload), seq & 0xFF, cmd & 0xFF, motor & 0xFF, flags & 0xFF]) + payload
    crc = crc16(cuerpo)
    return bytes([SOF]) + cuerpo + bytes([crc & 0xFF, crc >> 8, EOF])


# --- Payloads de los comandos (little-endian, como el ESP32) -----------------------
def pl_jog(pasos: int) -> bytes:
    return struct.pack("<I", pasos)

def pl_movj(pasos1: int, pasos2: int, vel: int) -> bytes:
    return struct.pack("<iiB", pasos1, pasos2, vel)

def pl_movl(x_mm: float, y_mm: float, vel: int) -> bytes:
    return struct.pack("<hhB", round(x_mm * 10), round(y_mm * 10), vel)

def pl_setzero(pasos1: int, pasos2: int) -> bytes:
    return struct.pack("<ii", pasos1, pasos2)

def pl_pausa(ms: int) -> bytes:
    return struct.pack("<H", ms)


# --- Respuestas decodificadas ---------------------------------------------------
@dataclass
class Pong:
    version: int
    baudios: int
    bits: int
    paridad: int        # 0 N, 1 E, 2 O
    paro: int
    pasos_por_vuelta: int

    @staticmethod
    def de(p: bytes) -> "Pong":
        return Pong(*struct.unpack("<BIBBBH", p))

    def coincide_con_pc(self) -> bool:
        return (self.baudios == BAUDIOS and self.bits == BITS and
                self.paridad == {"N": 0, "E": 1, "O": 2}[PARIDAD] and self.paro == BITS_PARO)

    def bus(self) -> str:
        return f"{self.baudios} {self.bits}{'NEO'[self.paridad]}{self.paro}"


@dataclass
class Status:
    estado: int
    fallo: int
    banderas: int
    en_cola: int
    ultimo_done: int
    pasos1: int
    pasos2: int
    x_mm: float
    y_mm: float

    @staticmethod
    def de(p: bytes) -> "Status":
        e, f, b, c, u, s1, s2, x, y = struct.unpack("<BBBBBiihh", p)
        return Status(e, f, b, c, u, s1, s2, x / 10.0, y / 10.0)

    @property
    def con_cero(self) -> bool:      return bool(self.banderas & 0x01)
    @property
    def drivers(self) -> bool:       return bool(self.banderas & 0x02)
    @property
    def en_movimiento(self) -> bool: return bool(self.banderas & 0x04)
    @property
    def nombre_estado(self) -> str:
        return ESTADOS[self.estado] if self.estado < len(ESTADOS) else str(self.estado)
    @property
    def nombre_fallo(self) -> str:
        return FALLOS[self.fallo] if self.fallo < len(FALLOS) else str(self.fallo)


@dataclass
class Diag:
    ok: int
    crc: int
    trama: int
    timeout: int
    desborde: int
    rechazadas: int
    paridad: int
    lat_max_us: int
    lat_ultima_us: int

    @staticmethod
    def de(p: bytes) -> "Diag":
        return Diag(*struct.unpack("<9H", p))


# --- Decodificacion: la misma maquina de estados que frame.cpp ----------------------
class Parser:
    """Se alimenta byte a byte (o en bloques) y devuelve lo que encuentra.

    alimentar() devuelve una lista de tuplas (tipo, dato):
        ("trama", Trama)       trama valida
        ("error", "CRC")       trama rechazada: "CRC", "TRAMA" o "TIMEOUT"
    El texto que imprime el firmware antes de pasar a modo binario (el banner
    de arranque) se ignora: no empieza con SOF.
    """

    def __init__(self, reloj=time.monotonic):
        self._reloj = reloj
        self._reset()
        self._ultimo = 0.0

    def _reset(self):
        self._estado = "SOF"
        self._buf = bytearray()
        self._len = 0

    @property
    def ocupado(self) -> bool:
        return self._estado != "SOF"

    def revisar_timeout(self) -> list:
        if self.ocupado and self._reloj() - self._ultimo > TIMEOUT_ENTRE_BYTES_S:
            self._reset()
            return [("error", "TIMEOUT")]
        return []

    def alimentar(self, datos: bytes) -> list:
        salida = []
        for b in datos:
            salida += self.revisar_timeout()
            self._ultimo = self._reloj()
            salida += self._byte(b)
        return salida

    def _byte(self, b: int) -> list:
        if self._estado == "SOF":
            if b == SOF:
                self._estado, self._buf = "LEN", bytearray()
            return []
        if self._estado == "LEN":
            if b > MAX_PAYLOAD:
                self._reset()
                if b == SOF:
                    self._estado = "LEN"
                return [("error", "TRAMA")]
            self._len = b
            self._buf.append(b)
            self._estado = "CUERPO"
            return []
        if self._estado == "CUERPO":                   # SEQ CMD MOTOR FLAGS PAYLOAD
            self._buf.append(b)
            if len(self._buf) == 5 + self._len:
                self._estado = "CRC"
            return []
        if self._estado == "CRC":
            self._buf.append(b)
            if len(self._buf) == 7 + self._len:
                self._estado = "EOF"
            return []
        # EOF
        cuerpo, crc_rx = bytes(self._buf[:5 + self._len]), self._buf[5 + self._len] | (self._buf[6 + self._len] << 8)
        self._reset()
        if b != EOF:
            if b == SOF:
                self._estado = "LEN"
            return [("error", "TRAMA")]
        if crc16(cuerpo) != crc_rx:
            return [("error", "CRC")]
        return [("trama", Trama(seq=cuerpo[1], cmd=cuerpo[2], motor=cuerpo[3],
                                flags=cuerpo[4], payload=cuerpo[5:]))]
