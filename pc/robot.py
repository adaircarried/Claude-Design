"""
robot.py - Enlace con el brazo por el protocolo binario de la U2
=================================================================

Capa entre el pendant (o el programa maestro de la U3) y el puerto serie:

    pendant.py / maestro (U3)
            |   robot.movj(30, -60, vel=50)
            v
    robot.py        <- esta capa: SEQ, espera de ACK/NACK, avisos DONE/FAILED
            |
    protocolo.py    <- formato de trama (puro, probado sin hardware)
            |
    puerto COM  115200 8E1

Uso rapido de diagnostico, sin interfaz grafica:

    python robot.py COM7

Reglas de diseño:
  * Las ordenes de MOVIMIENTO no se reintentan nunca solas. Si se perdiera
    el ACK pero la orden si se hubiera ejecutado, un reintento moveria el
    robot dos veces. Solo se reintentan las consultas (PING, STATUS, DIAG).
  * Un NACK se convierte en una excepcion ErrorRobot con su codigo: ningun
    rechazo pasa desapercibido.
  * Al abrir el puerto se dejan DTR y RTS en bajo para que el ESP32 no se
    reinicie (perderia el cero).
"""

from __future__ import annotations

import queue
import sys
import threading
import time
from collections import deque

import protocolo as P


class ErrorRobot(Exception):
    """El robot rechazo la orden (NACK) o no pudo ejecutarla (FAILED)."""
    def __init__(self, codigo: int, cmd: int = 0, contexto: str = ""):
        self.codigo = codigo
        self.cmd = cmd
        super().__init__(f"{P.nombre_cmd(cmd)} rechazado: {P.nombre_error(codigo)}"
                         + (f" ({contexto})" if contexto else ""))


class SinRespuesta(Exception):
    """El robot no contesto a tiempo."""


# =============================================================================
# Transportes: el serial real, o cualquier objeto con write/read/close
# =============================================================================
class TransporteSerial:
    def __init__(self, puerto: str):
        import serial                                   # pyserial
        s = serial.Serial()
        s.port = puerto
        s.baudrate = P.BAUDIOS
        s.bytesize = serial.EIGHTBITS
        s.parity = serial.PARITY_EVEN
        s.stopbits = serial.STOPBITS_ONE
        s.timeout = 0.01
        # Abrir el COM mueve DTR/RTS y el circuito de arranque de la placa
        # reiniciaria el ESP32. Se fijan en bajo ANTES de abrir.
        s.dtr = False
        s.rts = False
        s.open()
        self._s = s

    def write(self, datos: bytes):  self._s.write(datos)
    def read(self, n: int) -> bytes: return self._s.read(n)
    def close(self):                 self._s.close()


def puertos_disponibles() -> list[str]:
    from serial.tools import list_ports
    return [p.device for p in list_ports.comports()]


# =============================================================================
# Robot
# =============================================================================
class Robot:
    TIMEOUT_S = 0.5          # DISABLE tarda ~300 ms; el resto, < 1 ms

    def __init__(self, transporte, al_evento=None):
        self._t = transporte
        self._parser = P.Parser()
        self._lock = threading.Lock()
        self._seq = 0
        self._pendientes: dict[int, list] = {}          # seq -> [Event, Trama | None]
        self._eventos: "queue.Queue[P.Trama]" = queue.Queue()
        self.al_evento = al_evento                       # callback(texto) para el registro
        self.registro: deque[str] = deque(maxlen=500)
        self.pasos_por_grado = 16000 / 360.0             # se confirma con PING
        self.rtt_ms = 0.0
        self._vivo = True
        self._hilo = threading.Thread(target=self._lector, daemon=True)
        self._hilo.start()

    @classmethod
    def conectar(cls, puerto: str, al_evento=None) -> "Robot":
        return cls(TransporteSerial(puerto), al_evento)

    def cerrar(self):
        self._vivo = False
        try:
            self._t.close()
        except Exception:
            pass

    # --- registro -------------------------------------------------------------
    def _log(self, texto: str):
        linea = f"{time.strftime('%H:%M:%S')}  {texto}"
        self.registro.append(linea)
        if self.al_evento:
            self.al_evento(linea)

    # --- hilo lector ------------------------------------------------------------
    def _lector(self):
        while self._vivo:
            try:
                datos = self._t.read(256)
            except Exception as e:                     # cable desconectado
                self._log(f"ERROR de lectura: {e}")
                time.sleep(0.2)
                continue
            for tipo, x in self._parser.alimentar(datos) + self._parser.revisar_timeout():
                if tipo == "error":
                    self._log(f"<- trama rechazada por la PC: {x}")
                    continue
                self._recibida(x)

    def _recibida(self, tr: P.Trama):
        if tr.cmd in (P.Rsp.DONE, P.Rsp.FAILED):
            if tr.cmd == P.Rsp.DONE:
                self._log(f"<- DONE seq={tr.seq}: movimiento terminado")
            else:
                self._log(f"<- FAILED seq={tr.seq}: {P.nombre_error(tr.payload[0])}")
            self._eventos.put(tr)
            return
        with self._lock:
            pend = self._pendientes.get(tr.seq)
        if pend is None:
            # Respuesta sin orden pendiente: p. ej. el NACK de un desborde
            if tr.cmd == P.Rsp.NACK:
                self._log(f"<- NACK sin orden pendiente: {P.nombre_error(tr.payload[1])}")
            return
        pend[1] = tr
        pend[0].set()

    # --- peticion / respuesta ---------------------------------------------------
    def _siguiente_seq(self) -> int:
        self._seq = self._seq % 255 + 1              # 1..255; 0 es de la consola
        while self._seq in self._pendientes:
            self._seq = self._seq % 255 + 1
        return self._seq

    def _pedir_bytes(self, seq: int, datos: bytes, cmd: int, timeout: float) -> P.Trama:
        ev = threading.Event()
        with self._lock:
            self._pendientes[seq] = [ev, None]
        t0 = time.perf_counter()
        self._t.write(datos)
        ok = ev.wait(timeout)
        with self._lock:
            _, resp = self._pendientes.pop(seq, [None, None])
        if not ok or resp is None:
            raise SinRespuesta(f"{P.nombre_cmd(cmd)} seq={seq}: sin respuesta en {timeout * 1000:.0f} ms")
        self.rtt_ms = (time.perf_counter() - t0) * 1000
        if resp.cmd == P.Rsp.NACK:
            raise ErrorRobot(resp.payload[1], cmd)
        return resp

    def pedir(self, cmd: int, motor: int = 0, flags: int = 0, payload: bytes = b"",
              timeout: float = TIMEOUT_S, reintentos: int = 0) -> P.Trama:
        for intento in range(reintentos + 1):
            with self._lock:
                seq = self._siguiente_seq()
            try:
                return self._pedir_bytes(seq, P.construir(seq, cmd, motor, flags, payload), cmd, timeout)
            except SinRespuesta:
                if intento == reintentos:
                    raise

    # --- comandos -----------------------------------------------------------------
    def grados_a_pasos(self, grados: float) -> int:
        return round(grados * self.pasos_por_grado)

    def pasos_a_grados(self, pasos: int) -> float:
        return pasos / self.pasos_por_grado

    def ping(self) -> P.Pong:
        pong = P.Pong.de(self.pedir(P.Cmd.PING, reintentos=2).payload)
        self.pasos_por_grado = pong.pasos_por_vuelta / 360.0
        return pong

    def estado(self) -> P.Status:
        return P.Status.de(self.pedir(P.Cmd.GET_STATUS, reintentos=1).payload)

    def diag(self) -> P.Diag:
        return P.Diag.de(self.pedir(P.Cmd.GET_DIAG, reintentos=1).payload)

    def habilitar(self):  self.pedir(P.Cmd.ENABLE)
    def liberar(self):    self.pedir(P.Cmd.DISABLE, timeout=1.0)
    def limpiar(self):    self.pedir(P.Cmd.CLEAR)
    def parar(self):      self.pedir(P.Cmd.STOP)
    def modo_texto(self): self.pedir(P.Cmd.MODE_TEXT)
    def pausa(self, ms: int): self.pedir(P.Cmd.DEBUG_PAUSE, payload=P.pl_pausa(ms))

    def fijar_cero(self, q1: float, q2: float):
        self.pedir(P.Cmd.SETZERO, payload=P.pl_setzero(self.grados_a_pasos(q1), self.grados_a_pasos(q2)))

    def jog(self, eje: int, grados: float, avisar: bool = False) -> int:
        pasos = self.grados_a_pasos(grados)
        flags = (P.FLAG_DIR if pasos < 0 else 0) | (P.FLAG_NOTIFY if avisar else 0)
        return self.pedir(P.Cmd.JOG, motor=eje, flags=flags, payload=P.pl_jog(abs(pasos))).seq

    def movj(self, q1: float, q2: float, vel: int = 50, avisar: bool = False) -> int:
        """Devuelve el SEQ, para esperar su DONE con esperar_fin()."""
        return self.pedir(P.Cmd.MOVJ, flags=P.FLAG_NOTIFY if avisar else 0,
                          payload=P.pl_movj(self.grados_a_pasos(q1), self.grados_a_pasos(q2), vel)).seq

    def movl(self, x_mm: float, y_mm: float, vel: int = 20, avisar: bool = False) -> int:
        return self.pedir(P.Cmd.MOVL, flags=P.FLAG_NOTIFY if avisar else 0,
                          payload=P.pl_movl(x_mm, y_mm, vel)).seq

    def esperar_fin(self, seq: int, timeout: float = 30.0) -> None:
        """Espera el DONE de una orden enviada con avisar=True.

        Mientras espera sigue pidiendo STATUS: eso hace de latido para el
        watchdog del firmware, que pararia el robot tras 1 s de silencio.
        Lanza ErrorRobot si llega FAILED (por ejemplo, ABORTADO por un STOP)."""
        limite = time.monotonic() + timeout
        while time.monotonic() < limite:
            try:
                ev = self._eventos.get(timeout=0.2)
            except queue.Empty:
                self.estado()                           # latido
                continue
            if ev.seq != seq:
                continue
            if ev.cmd == P.Rsp.FAILED:
                raise ErrorRobot(ev.payload[0], P.Cmd.MOVJ, f"seq={seq}")
            return
        raise SinRespuesta(f"seq={seq}: sin DONE en {timeout:.0f} s")

    # --- inyeccion de fallas (demo de la U2) --------------------------------------
    def pedir_crudo(self, datos: bytes, seq: int, cmd: int, timeout: float = TIMEOUT_S) -> P.Trama:
        """Envia bytes arbitrarios esperando la respuesta para `seq`."""
        return self._pedir_bytes(seq, datos, cmd, timeout)

    def escribir_crudo(self, datos: bytes):
        self._t.write(datos)

    def reservar_seq(self) -> int:
        with self._lock:
            return self._siguiente_seq()


# =============================================================================
# Diagnostico rapido por linea de comandos
# =============================================================================
def _main():
    if len(sys.argv) < 2:
        print("Uso: python robot.py COMx")
        print("Puertos:", ", ".join(puertos_disponibles()) or "ninguno")
        return
    r = Robot.conectar(sys.argv[1], al_evento=print)
    try:
        pong = r.ping()
        print(f"PONG v{pong.version}  bus {pong.bus()}  "
              f"{'SINCRONIZADO' if pong.coincide_con_pc() else 'NO COINCIDE CON LA PC'}  "
              f"RTT {r.rtt_ms:.1f} ms")
        st = r.estado()
        print(f"Estado {st.nombre_estado}, fallo {st.nombre_fallo}, cero={st.con_cero}, "
              f"drivers={st.drivers}, q1={r.pasos_a_grados(st.pasos1):.2f}, "
              f"q2={r.pasos_a_grados(st.pasos2):.2f}, XY=({st.x_mm}, {st.y_mm}) mm")
        print("Diagnostico:", r.diag())
    except (SinRespuesta, ErrorRobot) as e:
        print("ERROR:", e)
    finally:
        r.cerrar()


if __name__ == "__main__":
    _main()
