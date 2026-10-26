"""
Tests del protocolo del lado de la PC. Sin hardware:

    python -m unittest test_protocolo -v     (desde la carpeta pc/)

Los vectores son los mismos que usan los tests en C++ del firmware
(test/test_frame): si ambos pasan, la PC y el ESP32 hablan el mismo idioma.
"""
import random
import unittest

import protocolo as P


class RelojFalso:
    def __init__(self):
        self.t = 0.0
    def __call__(self):
        return self.t


class TestProtocolo(unittest.TestCase):

    def test_crc_vector_estandar(self):
        self.assertEqual(P.crc16(b"123456789"), 0x29B1)

    def test_ping_identico_al_del_firmware(self):
        # El test test_vector_ping_para_python del firmware imprime:
        #   AA 00 01 01 00 00 88 50 55
        self.assertEqual(P.construir(1, P.Cmd.PING).hex(" ").upper(),
                         "AA 00 01 01 00 00 88 50 55")

    def test_ida_y_vuelta(self):
        t = P.construir(7, P.Cmd.MOVJ, 0, P.FLAG_NOTIFY, P.pl_movj(10000, -10000, 50))
        r = P.Parser().alimentar(t)
        self.assertEqual(len(r), 1)
        tipo, tr = r[0]
        self.assertEqual(tipo, "trama")
        self.assertEqual((tr.seq, tr.cmd, tr.flags), (7, P.Cmd.MOVJ, P.FLAG_NOTIFY))
        self.assertEqual(tr.payload, P.pl_movj(10000, -10000, 50))

    def test_crc_alterado(self):
        t = bytearray(P.construir(3, P.Cmd.JOG, 1, 0, P.pl_jog(400)))
        t[7] ^= 0x01
        self.assertEqual(P.Parser().alimentar(bytes(t)), [("error", "CRC")])

    def test_sin_eof(self):
        t = bytearray(P.construir(3, P.Cmd.PING))
        t[-1] = 0
        self.assertEqual(P.Parser().alimentar(bytes(t)), [("error", "TRAMA")])

    def test_len_excesivo(self):
        self.assertEqual(P.Parser().alimentar(bytes([P.SOF, 200])), [("error", "TRAMA")])

    def test_sof_en_el_payload(self):
        pl = bytes([P.SOF, P.EOF, P.SOF, 0, P.SOF, P.EOF])
        r = P.Parser().alimentar(P.construir(0xAA, P.Cmd.MOVL, 0, 0, pl))
        self.assertEqual(r[0][1].payload, pl)

    def test_banner_de_texto_y_luego_tramas(self):
        flujo = b"\r\n=== Brazo RR 2 GDL ===\r\nOK\r\n" + P.construir(1, P.Rsp.ACK, 0, 0, b"\x01") \
                + P.construir(2, P.Rsp.DONE)
        r = P.Parser().alimentar(flujo)
        self.assertEqual([x[1].cmd for x in r], [P.Rsp.ACK, P.Rsp.DONE])

    def test_timeout(self):
        reloj = RelojFalso()
        p = P.Parser(reloj)
        t = P.construir(5, P.Cmd.JOG, 1, 0, P.pl_jog(1))
        p.alimentar(t[:6])
        reloj.t = 0.010
        self.assertEqual(p.revisar_timeout(), [])
        reloj.t = 0.050
        self.assertEqual(p.revisar_timeout(), [("error", "TIMEOUT")])
        self.assertEqual(p.alimentar(t)[0][0], "trama")

    def test_ruido(self):
        random.seed(1234)
        reloj = RelojFalso()
        p = P.Parser(reloj)
        falsos = 0
        for _ in range(200_000):
            reloj.t += 0.0001
            falsos += sum(1 for tipo, _ in p.alimentar(bytes([random.randrange(256)])) if tipo == "trama")
        self.assertEqual(falsos, 0)
        reloj.t += 0.1
        self.assertEqual(p.alimentar(P.construir(77, P.Cmd.PING))[-1][1].seq, 77)

    def test_payloads_de_respuesta(self):
        st = P.Status.de(bytes([3, 0, 0x03, 2, 9]) + (4000).to_bytes(4, "little", signed=True)
                         + (-4000).to_bytes(4, "little", signed=True)
                         + (2000).to_bytes(2, "little", signed=True) + (-1500).to_bytes(2, "little", signed=True))
        self.assertEqual(st.nombre_estado, "LISTO")
        self.assertTrue(st.con_cero and st.drivers and not st.en_movimiento)
        self.assertEqual((st.pasos1, st.pasos2, st.x_mm, st.y_mm), (4000, -4000, 200.0, -150.0))

        pong = P.Pong.de(bytes([1]) + (115200).to_bytes(4, "little") + bytes([8, 1, 1]) + (16000).to_bytes(2, "little"))
        self.assertTrue(pong.coincide_con_pc())
        self.assertEqual(pong.bus(), "115200 8E1")

    def test_payload_excesivo(self):
        with self.assertRaises(ValueError):
            P.construir(0, P.Cmd.PING, payload=bytes(33))


if __name__ == "__main__":
    unittest.main()
