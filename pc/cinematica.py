"""
cinematica.py - Cinematica del brazo RR del lado de la PC
==========================================================

La misma matematica que src/kinematics.cpp del firmware, para que el pendant
pueda convertir una coordenada (X, Y) en angulos y mandar un MOVJ.

Convencion (igual que el firmware):
  * q1: angulo del brazo, antihorario desde +X (horizontal hacia el tablero)
  * q2: angulo del antebrazo RELATIVO al brazo
  * codo arriba = q2 < 0 (el valor por defecto del robot)

El firmware vuelve a validar todo (limites y espacio de trabajo) y responde
NACK FUERA_DE_RANGO si el punto no se puede alcanzar: aqui solo se calcula.
"""

from __future__ import annotations

import math

# Deben coincidir con LINK_L1_MM, LINK_L2_MM y WS_* de src/config.h
L1_MM = 200.0
L2_MM = 150.0
R_MIN_EFECTIVO_MM = 141.7     # impuesto por el limite del codo (+-135 grados)
R_MAX_MM = 340.0
Y_MIN_MM = -250.0


def directa(q1_deg: float, q2_deg: float) -> tuple[float, float]:
    q1, q2 = math.radians(q1_deg), math.radians(q2_deg)
    return (L1_MM * math.cos(q1) + L2_MM * math.cos(q1 + q2),
            L1_MM * math.sin(q1) + L2_MM * math.sin(q1 + q2))


def inversa(x: float, y: float, codo_arriba: bool = True) -> tuple[float, float]:
    """(X, Y) en mm -> (q1, q2) en grados. ValueError si no se alcanza."""
    r = math.hypot(x, y)
    if r > L1_MM + L2_MM or r < abs(L1_MM - L2_MM):
        raise ValueError(f"({x:.1f}, {y:.1f}) esta a {r:.1f} mm del hombro: "
                         f"fuera del alcance del brazo")
    c = (x * x + y * y - L1_MM ** 2 - L2_MM ** 2) / (2 * L1_MM * L2_MM)
    c = max(-1.0, min(1.0, c))                 # ruido numerico en el borde
    q2 = math.acos(c)
    if codo_arriba:
        q2 = -q2
    q1 = math.atan2(y, x) - math.atan2(L2_MM * math.sin(q2), L1_MM + L2_MM * math.cos(q2))
    return math.degrees(q1), math.degrees(q2)
