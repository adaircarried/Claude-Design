"""
pendant.py - Teach pendant del brazo RR (programa de la PC de la U2)
=====================================================================

Ejecutar en Windows:      python pendant.py

Es el programa de la U2 que habla con el ESP32 por el protocolo binario, y a
la vez la herramienta con la que se enseñan los puntos de la U3:

  * Jog articular (J1, J2) y cartesiano (X, Y en linea recta), por pasos
  * Estado en vivo: posicion, estado, fallos, cola
  * Puntos enseñados: guardar la pose actual con un nombre, ir a ella
    (se guardan en puntos.json, junto a este archivo)
  * Diagnostico del enlace y panel de INYECCION DE FALLAS para la demo:
    CRC corrupto, trama cortada, fuera de rango, rafagas, desborde, caida
    de la PC

Hilos: la interfaz nunca espera al robot. Cada orden corre en un hilo corto
y el estado se consulta 5 veces por segundo en otro, que ademas hace de
latido para el watchdog del firmware. Los hilos no tocan los widgets: dejan
lo que hay que pintar en una cola que la interfaz vacia cada 100 ms.

Tecla ESC = STOP, desde cualquier parte de la ventana.
"""

from __future__ import annotations

import json
import os
import queue
import random
import threading
import time
import tkinter as tk
from tkinter import messagebox, simpledialog, ttk

import protocolo as P
from robot import ErrorRobot, Robot, SinRespuesta

# Pose de referencia de SETZERO. DEBE COINCIDIR con ZERO_POSE_Q1_DEG y
# ZERO_POSE_Q2_DEG de src/config.h: brazo horizontal, antebrazo colgando.
POSE_CERO = (0.0, -90.0)

ARCHIVO_PUNTOS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "puntos.json")
PERIODO_ESTADO_S = 0.2
PERIODO_DIAG_S = 1.0


class Pendant:
    def __init__(self, raiz: tk.Tk, fabrica_transporte=None):
        """fabrica_transporte(puerto) -> transporte. Por defecto, el serial
        real; los tests lo sustituyen por el firmware simulado."""
        self.raiz = raiz
        self.fabrica_transporte = fabrica_transporte
        self.robot: Robot | None = None
        self.est: P.Status | None = None
        self.diag: P.Diag | None = None
        self.latido = tk.BooleanVar(value=True)
        self.cola_gui: "queue.Queue" = queue.Queue()
        self.puntos = self._cargar_puntos()
        self._vivo = True

        raiz.title("Teach pendant - Brazo RR 2 GDL (U2)")
        raiz.minsize(1040, 640)
        raiz.bind("<Escape>", lambda e: self.parar())
        raiz.protocol("WM_DELETE_WINDOW", self.salir)
        self._construir()
        raiz.after(100, self._bombear)
        threading.Thread(target=self._sondeo, daemon=True).start()

    # =========================================================================
    # Interfaz
    # =========================================================================
    def _construir(self):
        r = self.raiz
        r.columnconfigure(0, weight=1)
        r.rowconfigure(2, weight=1)

        # --- conexion ---------------------------------------------------------
        fc = ttk.Frame(r, padding=6)
        fc.grid(row=0, column=0, sticky="ew")
        ttk.Label(fc, text="Puerto:").pack(side="left")
        self.cb_puerto = ttk.Combobox(fc, width=12, values=self._puertos())
        self.cb_puerto.pack(side="left", padx=4)
        if self.cb_puerto["values"]:
            self.cb_puerto.current(0)
        ttk.Button(fc, text="↻", width=3,
                   command=lambda: self.cb_puerto.configure(values=self._puertos())).pack(side="left")
        self.bt_conectar = ttk.Button(fc, text="Conectar", command=self.conectar)
        self.bt_conectar.pack(side="left", padx=6)
        self.lb_bus = ttk.Label(fc, text="sin conexión", foreground="gray")
        self.lb_bus.pack(side="left", padx=10)
        self.lb_rtt = ttk.Label(fc, text="")
        self.lb_rtt.pack(side="right")

        # --- cuerpo: tres columnas ----------------------------------------------
        cuerpo = ttk.Frame(r, padding=(6, 0))
        cuerpo.grid(row=1, column=0, sticky="nsew")
        for c in range(3):
            cuerpo.columnconfigure(c, weight=1, uniform="col")

        self._col_estado(ttk.Frame(cuerpo)).grid(row=0, column=0, sticky="nsew", padx=4)
        self._col_jog(ttk.Frame(cuerpo)).grid(row=0, column=1, sticky="nsew", padx=4)
        self._col_puntos(ttk.Frame(cuerpo)).grid(row=0, column=2, sticky="nsew", padx=4)

        # --- registro --------------------------------------------------------------
        fr = ttk.LabelFrame(r, text="Registro", padding=4)
        fr.grid(row=2, column=0, sticky="nsew", padx=10, pady=6)
        fr.columnconfigure(0, weight=1)
        fr.rowconfigure(0, weight=1)
        self.tx_log = tk.Text(fr, height=9, font=("Consolas", 9), state="disabled", wrap="none")
        self.tx_log.grid(row=0, column=0, sticky="nsew")
        sb = ttk.Scrollbar(fr, command=self.tx_log.yview)
        sb.grid(row=0, column=1, sticky="ns")
        self.tx_log.configure(yscrollcommand=sb.set)
        self.tx_log.tag_configure("err", foreground="#b00020")
        self.tx_log.tag_configure("ok", foreground="#1b6e20")

    def _col_estado(self, f):
        fe = ttk.LabelFrame(f, text="Estado del robot", padding=6)
        fe.pack(fill="x")
        self.v_estado = {}
        for i, (clave, texto) in enumerate([
                ("estado", "Estado"), ("fallo", "Fallo"), ("cero", "Cero fijado"),
                ("drivers", "Drivers"), ("q1", "q1 (hombro)"), ("q2", "q2 (codo)"),
                ("xy", "Efector X, Y"), ("cola", "En cola")]):
            ttk.Label(fe, text=texto + ":").grid(row=i, column=0, sticky="w")
            v = tk.StringVar(value="—")
            ttk.Label(fe, textvariable=v, font=("Consolas", 10, "bold")).grid(row=i, column=1, sticky="w", padx=6)
            self.v_estado[clave] = v

        fo = ttk.LabelFrame(f, text="Operación", padding=6)
        fo.pack(fill="x", pady=6)
        for i, (t, fn) in enumerate([("ENABLE", self.habilitar), ("DISABLE", self.liberar),
                                     ("SETZERO", self.fijar_cero), ("CLEAR", self.limpiar)]):
            ttk.Button(fo, text=t, command=fn).grid(row=i // 2, column=i % 2, sticky="ew", padx=2, pady=2)
        fo.columnconfigure((0, 1), weight=1)
        tk.Button(fo, text="STOP  (Esc)", bg="#c62828", fg="white", activebackground="#8e0000",
                  font=("Segoe UI", 14, "bold"), height=2,
                  command=self.parar).grid(row=2, column=0, columnspan=2, sticky="ew", pady=(8, 0))
        return f

    def _col_jog(self, f):
        fj = ttk.LabelFrame(f, text="Jog articular (por pasos)", padding=6)
        fj.pack(fill="x")
        self.paso_grados = tk.DoubleVar(value=1.0)
        fp = ttk.Frame(fj)
        fp.grid(row=0, column=0, columnspan=3, sticky="w")
        ttk.Label(fp, text="Paso:").pack(side="left")
        for v in (0.1, 1.0, 5.0, 10.0):
            ttk.Radiobutton(fp, text=f"{v:g}°", value=v, variable=self.paso_grados).pack(side="left")
        for fila, eje, nombre in ((1, 1, "J1 hombro"), (2, 2, "J2 codo")):
            ttk.Label(fj, text=nombre, width=10).grid(row=fila, column=0, sticky="w")
            ttk.Button(fj, text="−", width=6, command=lambda e=eje: self.jog(e, -1)).grid(row=fila, column=1, pady=2)
            ttk.Button(fj, text="+", width=6, command=lambda e=eje: self.jog(e, +1)).grid(row=fila, column=2, pady=2)

        fl = ttk.LabelFrame(f, text="Jog cartesiano (MOVL en línea recta)", padding=6)
        fl.pack(fill="x", pady=6)
        self.paso_mm = tk.DoubleVar(value=5.0)
        fp = ttk.Frame(fl)
        fp.grid(row=0, column=0, columnspan=3, sticky="w")
        ttk.Label(fp, text="Paso:").pack(side="left")
        for v in (0.5, 1.0, 5.0, 10.0):
            ttk.Radiobutton(fp, text=f"{v:g} mm", value=v, variable=self.paso_mm).pack(side="left")
        for fila, eje in ((1, "X"), (2, "Y")):
            ttk.Label(fl, text=f"{eje} ({'horizontal' if eje == 'X' else 'vertical'})", width=14).grid(row=fila, column=0, sticky="w")
            ttk.Button(fl, text="−", width=6, command=lambda e=eje: self.jog_cart(e, -1)).grid(row=fila, column=1, pady=2)
            ttk.Button(fl, text="+", width=6, command=lambda e=eje: self.jog_cart(e, +1)).grid(row=fila, column=2, pady=2)

        fv = ttk.LabelFrame(f, text="Velocidad", padding=6)
        fv.pack(fill="x")
        self.vel_movj = tk.IntVar(value=40)
        self.vel_movl = tk.IntVar(value=15)
        for fila, (t, var) in enumerate((("MOVJ (viajes)", self.vel_movj), ("MOVL (tramos finos)", self.vel_movl))):
            ttk.Label(fv, text=t).grid(row=fila, column=0, sticky="w")
            ttk.Scale(fv, from_=1, to=100, variable=var, orient="horizontal",
                      command=lambda _v, x=var: x.set(round(float(_v)))).grid(row=fila, column=1, sticky="ew", padx=4)
            ttk.Label(fv, textvariable=var, width=4).grid(row=fila, column=2)
        fv.columnconfigure(1, weight=1)
        return f

    def _col_puntos(self, f):
        fp = ttk.LabelFrame(f, text="Puntos enseñados", padding=6)
        fp.pack(fill="both", expand=True)
        self.lst_puntos = tk.Listbox(fp, height=7, font=("Consolas", 9), exportselection=False)
        self.lst_puntos.pack(fill="both", expand=True)
        fb = ttk.Frame(fp)
        fb.pack(fill="x", pady=(4, 0))
        for i, (t, fn) in enumerate([("Guardar pose actual…", self.guardar_punto), ("Borrar", self.borrar_punto),
                                     ("Ir (MOVJ)", lambda: self.ir_a_punto("MOVJ")),
                                     ("Ir (MOVL)", lambda: self.ir_a_punto("MOVL"))]):
            ttk.Button(fb, text=t, command=fn).grid(row=i // 2, column=i % 2, sticky="ew", padx=2, pady=2)
        fb.columnconfigure((0, 1), weight=1)
        self._refrescar_puntos()

        fd = ttk.LabelFrame(f, text="Diagnóstico del enlace (U2)", padding=6)
        fd.pack(fill="x", pady=6)
        self.v_diag = tk.StringVar(value="—")
        ttk.Label(fd, textvariable=self.v_diag, font=("Consolas", 9), justify="left").pack(anchor="w")
        fi = ttk.LabelFrame(f, text="Inyección de fallas", padding=6)
        fi.pack(fill="x")
        pruebas = [("CRC corrupto", self.f_crc), ("Trama cortada", self.f_cortada),
                   ("Sin EOF", self.f_sin_eof), ("Fuera de rango", self.f_rango),
                   ("Motor inválido", self.f_motor), ("Cmd desconocido", self.f_cmd),
                   ("Ráfaga 2 KB", self.f_rafaga), ("Desborde", self.f_desborde)]
        for i, (t, fn) in enumerate(pruebas):
            ttk.Button(fi, text=t, command=fn).grid(row=i // 2, column=i % 2, sticky="ew", padx=2, pady=1)
        fi.columnconfigure((0, 1), weight=1)
        ttk.Checkbutton(fi, text="Latido activo (desmarca para simular PC caída)",
                        variable=self.latido).grid(row=4, column=0, columnspan=2, sticky="w", pady=(4, 0))
        return f

    # =========================================================================
    # Infraestructura: hilos, cola de la interfaz, registro
    # =========================================================================
    def _puertos(self):
        try:
            from robot import puertos_disponibles
            return puertos_disponibles()
        except Exception:
            return []

    def log(self, texto: str, tag: str = ""):
        """Seguro desde cualquier hilo."""
        self.cola_gui.put(("log", texto, tag))

    def _bombear(self):
        try:
            while True:
                item = self.cola_gui.get_nowait()
                if item[0] == "log":
                    _, texto, tag = item
                    self.tx_log.configure(state="normal")
                    self.tx_log.insert("end", texto + "\n", tag)
                    self.tx_log.see("end")
                    self.tx_log.configure(state="disabled")
                elif item[0] == "fn":
                    item[1]()
        except queue.Empty:
            pass
        self._pintar_estado()
        if self._vivo:
            self.raiz.after(100, self._bombear)

    def _en_hilo(self, nombre: str, fn, *, requiere=True):
        """Ejecuta una orden sin congelar la ventana y registra el resultado."""
        if requiere and not self.robot:
            self.log(f"✗ {nombre}: primero conecta el robot", "err")
            return
        def tarea():
            try:
                res = fn()
                self.log(f"✓ {nombre}" + (f": {res}" if res else ""), "ok")
            except ErrorRobot as e:
                self.log(f"✗ {nombre}: rechazado por el robot, {P.nombre_error(e.codigo)}", "err")
            except SinRespuesta as e:
                self.log(f"✗ {nombre}: {e}", "err")
            except Exception as e:
                self.log(f"✗ {nombre}: {type(e).__name__}: {e}", "err")
        threading.Thread(target=tarea, daemon=True).start()

    def _sondeo(self):
        """Pide STATUS 5 veces por segundo y DIAG una vez por segundo.
        Es tambien el LATIDO del watchdog: si se desmarca 'Latido activo', el
        firmware deja de recibir tramas y para el robot al segundo."""
        ultimo_diag = 0.0
        while self._vivo:
            time.sleep(PERIODO_ESTADO_S)
            r = self.robot
            if not r or not self.latido.get():
                continue
            try:
                self.est = r.estado()
                if time.monotonic() - ultimo_diag > PERIODO_DIAG_S:
                    self.diag = r.diag()
                    ultimo_diag = time.monotonic()
            except (SinRespuesta, ErrorRobot):
                self.est = None

    def _pintar_estado(self):
        r, st, d = self.robot, self.est, self.diag
        if not r or not st:
            return
        v = self.v_estado
        v["estado"].set(st.nombre_estado)
        v["fallo"].set(st.nombre_fallo)
        v["cero"].set("sí" if st.con_cero else "NO (haz SETZERO)")
        v["drivers"].set("energizados" if st.drivers else "LIBRES")
        v["q1"].set(f"{r.pasos_a_grados(st.pasos1):+8.2f}°  ({st.pasos1} pasos)")
        v["q2"].set(f"{r.pasos_a_grados(st.pasos2):+8.2f}°  ({st.pasos2} pasos)")
        v["xy"].set(f"{st.x_mm:+7.1f}, {st.y_mm:+7.1f} mm")
        v["cola"].set(f"{st.en_cola}" + ("  (moviendo)" if st.en_movimiento else ""))
        self.lb_rtt.configure(text=f"ida y vuelta {r.rtt_ms:.1f} ms")
        if d:
            self.v_diag.set(
                f"tramas OK   {d.ok:6d}    rechazadas {d.rechazadas:5d}\n"
                f"err CRC     {d.crc:6d}    err trama  {d.trama:5d}\n"
                f"err timeout {d.timeout:6d}    desbordes  {d.desborde:5d}\n"
                f"err paridad {d.paridad:6d}\n"
                f"latencia en el ESP32: última {d.lat_ultima_us} µs, máx {d.lat_max_us} µs")

    # =========================================================================
    # Conexion
    # =========================================================================
    def conectar(self):
        if self.robot:
            self.robot.cerrar()
            self.robot, self.est, self.diag = None, None, None
            self.bt_conectar.configure(text="Conectar")
            self.lb_bus.configure(text="sin conexión", foreground="gray")
            self.log("Desconectado")
            return
        puerto = self.cb_puerto.get().strip()
        if not puerto and not self.fabrica_transporte:
            messagebox.showwarning("Puerto", "Elige el puerto COM del ESP32")
            return
        def tarea():
            try:
                if self.fabrica_transporte:
                    r = Robot(self.fabrica_transporte(puerto), al_evento=self.log)
                else:
                    r = Robot.conectar(puerto, al_evento=self.log)
                pong = r.ping()
            except ImportError:
                self.log("✗ Falta pyserial: ejecuta  pip install -r requirements.txt", "err")
                return
            except Exception as e:
                self.log(f"✗ No se pudo conectar en {puerto}: {e}", "err")
                return
            self.robot = r
            sinc = pong.coincide_con_pc()
            def ok():
                self.bt_conectar.configure(text="Desconectar")
                self.lb_bus.configure(
                    text=f"Bus {pong.bus()}  " + ("✓ sincronizado" if sinc else "✗ NO COINCIDE con la PC"),
                    foreground="#1b6e20" if sinc else "#b00020")
            self.cola_gui.put(("fn", ok))
            self.log(f"✓ Conectado en {puerto}: firmware v{pong.version}, bus {pong.bus()}, "
                     f"{pong.pasos_por_vuelta} pasos por vuelta", "ok")
        threading.Thread(target=tarea, daemon=True).start()

    # =========================================================================
    # Operacion
    # =========================================================================
    def habilitar(self): self._en_hilo("ENABLE", lambda: self.robot.habilitar())
    def limpiar(self):   self._en_hilo("CLEAR", lambda: self.robot.limpiar())

    def parar(self):
        self._en_hilo("STOP", lambda: self.robot.parar() or "frena con rampa y mantiene el par; CLEAR para seguir")

    def liberar(self):
        if messagebox.askyesno("DISABLE", "Se liberan los motores: el brazo CAERÁ por gravedad "
                               "y se perderá el cero.\n\n¿Continuar?"):
            self._en_hilo("DISABLE", lambda: self.robot.liberar())

    def fijar_cero(self):
        q1, q2 = POSE_CERO
        if messagebox.askyesno("SETZERO", f"¿El brazo está en la pose de referencia?\n\n"
                               f"q1 = {q1:g}°: brazo horizontal\nq2 = {q2:g}°: antebrazo colgando\n\n"
                               f"Los motores deben estar energizados (ENABLE) para sostenerla."):
            self._en_hilo("SETZERO", lambda: self.robot.fijar_cero(q1, q2))

    def jog(self, eje: int, signo: int):
        g = signo * self.paso_grados.get()
        self._en_hilo(f"JOG J{eje} {g:+g}°", lambda: self.robot.jog(eje, g) and None)

    def jog_cart(self, eje: str, signo: int):
        st = self.est
        if not st or not st.con_cero:
            self.log("✗ El jog cartesiano necesita el cero fijado (SETZERO)", "err")
            return
        d = signo * self.paso_mm.get()
        x, y = (st.x_mm + d, st.y_mm) if eje == "X" else (st.x_mm, st.y_mm + d)
        self._en_hilo(f"MOVL {eje}{d:+g} mm -> ({x:.1f}, {y:.1f})",
                      lambda: self.robot.movl(x, y, vel=self.vel_movl.get()) and None)

    # =========================================================================
    # Puntos enseñados
    # =========================================================================
    def _cargar_puntos(self) -> dict:
        try:
            with open(ARCHIVO_PUNTOS, encoding="utf-8") as fh:
                return json.load(fh)
        except (FileNotFoundError, json.JSONDecodeError):
            return {}

    def _guardar_archivo(self):
        if os.path.exists(ARCHIVO_PUNTOS):
            os.replace(ARCHIVO_PUNTOS, ARCHIVO_PUNTOS + ".bak")     # copia de seguridad
        with open(ARCHIVO_PUNTOS, "w", encoding="utf-8") as fh:
            json.dump(self.puntos, fh, indent=2, ensure_ascii=False)

    def _refrescar_puntos(self):
        self.lst_puntos.delete(0, "end")
        for nombre, p in self.puntos.items():
            self.lst_puntos.insert("end", f"{nombre[:14]:<14} q({p['q1']:+.1f},{p['q2']:+.1f}) "
                                          f"xy({p['x']:.0f},{p['y']:.0f})")

    def _seleccionado(self) -> str | None:
        sel = self.lst_puntos.curselection()
        if not sel:
            self.log("✗ Selecciona un punto de la lista", "err")
            return None
        return list(self.puntos)[sel[0]]

    def guardar_punto(self):
        if not self.robot:
            self.log("✗ Primero conecta el robot", "err")
            return
        nombre = simpledialog.askstring("Guardar punto", "Nombre del punto (ej. sobre_dispensador):",
                                        parent=self.raiz)
        if not nombre:
            return
        nombre = nombre.strip().replace(" ", "_")
        def tarea():
            st = self.robot.estado()                  # lectura fresca, no la del sondeo
            if not st.con_cero:
                raise ErrorRobot(P.Err.SIN_CERO, P.Cmd.GET_STATUS, "un punto sin cero no significa nada")
            r = self.robot
            self.puntos[nombre] = {"q1": round(r.pasos_a_grados(st.pasos1), 3),
                                   "q2": round(r.pasos_a_grados(st.pasos2), 3),
                                   "pasos1": st.pasos1, "pasos2": st.pasos2,
                                   "x": st.x_mm, "y": st.y_mm}
            self._guardar_archivo()
            self.cola_gui.put(("fn", self._refrescar_puntos))
            return f"'{nombre}' en {os.path.basename(ARCHIVO_PUNTOS)}"
        self._en_hilo("Guardar punto", tarea)

    def borrar_punto(self):
        n = self._seleccionado()
        if n and messagebox.askyesno("Borrar", f"¿Borrar el punto '{n}'?"):
            del self.puntos[n]
            self._guardar_archivo()
            self._refrescar_puntos()
            self.log(f"✓ Punto '{n}' borrado", "ok")

    def ir_a_punto(self, modo: str):
        n = self._seleccionado()
        if not n:
            return
        p = self.puntos[n]
        def tarea():
            r = self.robot
            if modo == "MOVJ":
                seq = r.movj(p["q1"], p["q2"], vel=self.vel_movj.get(), avisar=True)
            else:
                seq = r.movl(p["x"], p["y"], vel=self.vel_movl.get(), avisar=True)
            r.esperar_fin(seq, timeout=60)
            return f"llegó a '{n}'"
        self._en_hilo(f"{modo} a '{n}'", tarea)

    # =========================================================================
    # Inyeccion de fallas (demo de los criterios de la U2)
    # =========================================================================
    def _crudo(self, nombre: str, construir):
        """construir(seq) -> (bytes, cmd). Espera la respuesta a ese SEQ."""
        def tarea():
            seq = self.robot.reservar_seq()
            datos, cmd = construir(seq)
            self.log(f"-> {nombre}: {datos.hex(' ')}")
            try:
                self.robot.pedir_crudo(datos, seq, cmd)
                return "el robot la ACEPTÓ (no debería)"
            except ErrorRobot as e:
                return f"el robot la rechazó con {P.nombre_error(e.codigo)} y sigue operando"
        self._en_hilo(nombre, tarea)

    def f_crc(self):
        def c(seq):
            t = bytearray(P.construir(seq, P.Cmd.GET_STATUS)); t[4] ^= 0x10
            return bytes(t), P.Cmd.GET_STATUS
        self._crudo("CRC corrupto (1 bit cambiado)", c)

    def f_cortada(self):
        self._crudo("Trama cortada a la mitad",
                    lambda s: (P.construir(s, P.Cmd.JOG, 1, 0, P.pl_jog(100))[:7], P.Cmd.JOG))

    def f_sin_eof(self):
        def c(seq):
            t = bytearray(P.construir(seq, P.Cmd.GET_STATUS)); t[-1] = 0
            return bytes(t), P.Cmd.GET_STATUS
        self._crudo("Trama sin EOF", c)

    def f_rango(self):
        self._crudo("MOVJ a q1 = 999°",
                    lambda s: (P.construir(s, P.Cmd.MOVJ, 0, 0, P.pl_movj(44400, 0, 50)), P.Cmd.MOVJ))

    def f_motor(self):
        self._crudo("JOG al motor 3 (no existe)",
                    lambda s: (P.construir(s, P.Cmd.JOG, 3, 0, P.pl_jog(10)), P.Cmd.JOG))

    def f_cmd(self):
        self._crudo("Comando 0x7E (no existe)", lambda s: (P.construir(s, 0x7E), 0x7E))

    def f_rafaga(self):
        def tarea():
            basura = bytes(random.randrange(256) for _ in range(2048))
            self.robot.escribir_crudo(basura)
            time.sleep(0.4)                           # 2 KB tardan ~180 ms a 115200
            self.robot.ping()
            return "2 KB de basura enviados; el robot responde al PING"
        self._en_hilo("Ráfaga de basura", tarea)

    def f_desborde(self):
        def tarea():
            antes = self.robot.diag().desborde
            self.robot.pausa(800)                     # TaskComms deja de leer 800 ms...
            self.robot.escribir_crudo(bytes(3000))    # ...mientras se inunda el puerto
            time.sleep(1.2)
            despues = self.robot.diag().desborde
            return (f"contador de desbordes {antes} -> {despues}; el firmware descartó, "
                    f"se resincronizó y sigue vivo")
        self._en_hilo("Desborde de buffer", tarea)

    # =========================================================================
    def salir(self):
        self._vivo = False
        if self.robot:
            self.robot.cerrar()
        self.raiz.destroy()


def main():
    raiz = tk.Tk()
    try:
        ttk.Style().theme_use("vista")      # en Windows
    except tk.TclError:
        pass
    Pendant(raiz)
    raiz.mainloop()


if __name__ == "__main__":
    main()
