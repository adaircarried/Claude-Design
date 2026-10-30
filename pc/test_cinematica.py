"""Tests de la cinematica de la PC:  python -m unittest test_cinematica -v"""
import unittest

import cinematica as K


class TestCinematica(unittest.TestCase):

    def test_pose_de_referencia(self):
        # SETZERO: brazo horizontal, antebrazo colgando -> (200, -150) mm
        x, y = K.directa(0, -90)
        self.assertAlmostEqual(x, 200, places=6)
        self.assertAlmostEqual(y, -150, places=6)
        q1, q2 = K.inversa(200, -150)
        self.assertAlmostEqual(q1, 0, places=6)
        self.assertAlmostEqual(q2, -90, places=6)

    def test_ida_y_vuelta(self):
        for x in range(150, 331, 30):
            for y in range(-200, 201, 40):
                try:
                    q1, q2 = K.inversa(x, y)
                except ValueError:
                    continue
                xx, yy = K.directa(q1, q2)
                self.assertAlmostEqual(xx, x, places=6)
                self.assertAlmostEqual(yy, y, places=6)

    def test_codo_arriba_es_q2_negativo(self):
        # Misma convencion que el firmware (corregida en oct 2026)
        self.assertLess(K.inversa(250, 100, codo_arriba=True)[1], 0)
        self.assertGreater(K.inversa(250, 100, codo_arriba=False)[1], 0)

    def test_fuera_de_alcance(self):
        with self.assertRaises(ValueError):
            K.inversa(400, 0)
        with self.assertRaises(ValueError):
            K.inversa(10, 0)


if __name__ == "__main__":
    unittest.main()
