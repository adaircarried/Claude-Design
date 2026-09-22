# Auditoría: Manual CWI de COMIMSA contra el nivel I de la matriz ILUO

**Documento auditado:** Programa de Preparación para la Certificación de
Inspectores en Soldadura CWI de la AWS. Subgerencia de Capacitación y
Certificación, COMIMSA, 2017. 440 páginas, español, 10 capítulos.

**Método:** extracción del texto de los cinco bloques PDF, reconstrucción de la
estructura por el pie de página de cada lámina, y verificación visual de las
secciones sin capa de texto (82 de 440 páginas son sólo imagen, y ahí es donde
una búsqueda por palabras se equivoca).

---

## Estructura del manual

| Cap. | Páginas | Tema | ¿Aplica al nivel I? |
|---|---|---|---|
| 1 | 1 – 17 | Inspección y certificación de soldadura | No |
| 2 | 18 – 45 | Seguridad (ANSI Z49.1) | **Sí** |
| 3 | 46 – 130 | Procesos de soldadura, 13 procesos más corte | **Parcial** (GMAW: p61 – 67) |
| 4 | 131 – 218 | Geometría, terminología y símbolos | **Sí** |
| 5 | 219 – 263 | Planos, códigos, normas, calificación | **Sí** |
| 6 | 264 – 307 | Ensayos destructivos | No |
| 7 | 308 – 319 | Práctica métrica para inspección | No |
| 8 | 320 – 359 | Metalurgia de la soldadura | No |
| 9 | 360 – 398 | Discontinuidades | **Sí** |
| 10 | 399 – 440 | Ensayos no destructivos | No |

Unas 189 de las 440 páginas aplican directamente al nivel I, el 43 %. El 57 %
restante es contenido de inspector que no aparece en la matriz.

---

## Cobertura, tema por tema

### 1. Seguridad de soldadura — cubierto

Capítulo 2 completo, 28 páginas, basado en ANSI Z49.1.

| Subtema | Páginas |
|---|---|
| Peligros potenciales, marco ANSI Z49.1 | 19 |
| Responsabilidad de la administración, OSHA | 20 |
| Ambiente de trabajo, vigía de incendios, mamparas | 21 – 22 |
| Protección de ojos y cara | 23 – 25 |
| Ropa de protección | 26 – 27 |
| Protección de oídos | 28 |
| Humos y gases, ventilación | 29 – 30 |
| Espacios confinados | 31 – 33 |
| Manejo de gases comprimidos, reguladores | 34 – 42 |
| Descarga eléctrica | 43 – 45 |

**Hueco:** seguridad de celda robótica. No hay nada sobre paros de emergencia,
interlocks, vallas, modo teach contra automático, velocidad reducida ni LOTO.
Es un manual de inspector, no de operador de celda.

### 2. Equipo GMAW — parcial

Páginas 61 a 67, siete páginas.

Tiene: definición del proceso, gases de protección (argón, helio, CO2, O2 y
mezclas), figura 3.15 de nomenclatura de la pistola en p64 (gas nozzle, contact
tube, contact tube setback, electrode extension, stickout, standoff distance),
sistema de identificación del electrodo (p65), ventajas y desventajas (p66),
discontinuidades típicas del proceso (p67).

**Huecos:** fuente de voltaje constante y curva V-A; alimentador y rodillos;
liner y difusor; flujómetro y regulador aplicados a GMAW; polaridad DCEP para
GMAW (el manual sólo trata DCEP y DCEN en SMAW, p56, y GTAW, p77);
clasificación AWS del alambre (ER70S-x no aparece en ninguna página);
periféricos de celda robótica (TCP, reamer, corte de alambre, detección de
colisión).

### 3. Mecanismos de transferencia de metal — parcial, y con un error

Página 63, una sola tabla, más la figura 3.14 de los modos.

Nombra los cuatro modos y los asocia a un gas de protección.

**Error a corregir.** La tabla de p63 dice que la transferencia *spray* opera a
corriente y voltaje **bajos**. Es al revés: spray exige corriente por encima de
la corriente de transición, es el modo de corriente alta; el de corriente baja
es el cortocircuito. Verificado sobre la imagen de la página, no sobre el texto
extraído. Si el examen se arma con esta tabla, la respuesta correcta y la
respuesta del manual no coinciden.

**Huecos:** corriente de transición, relación con el diámetro del alambre,
criterio de selección por espesor y posición.

### 4. Variables del proceso — ausente

El hueco más grande. Búsquedas sin ningún resultado en las 440 páginas:
*aporte de calor*, *heat input*, *amperaje*, *extensión del electrodo*, *ángulo
de trabajo*, *ángulo de desplazamiento*, *flujo de gas*, *voltaje de arco*,
*inductancia*.

Sólo aparecen: *stickout* una vez (p71, referido a FCAW autoprotegido) y
*velocidad de avance* o *de desplazamiento* en p343, p386 y p389, siempre como
causa de una discontinuidad, nunca como variable que se ajusta.

No hay capítulo de variables porque un inspector juzga el resultado, no ajusta
el parámetro.

### 5. Discontinuidades — cubierto

Capítulo 9, páginas 360 a 398, más refuerzo por proceso en el capítulo 3
(p60, p67, p74) y la inspección visual del capítulo 10.

| Subtema | Páginas |
|---|---|
| Discontinuidad contra defecto, criterio de aceptación | 362 |
| Los 16 tipos catalogados | 363 |
| Grietas: severidad, propagación | 364 – 365 |
| Grietas: longitudinal, transversal, cráter, garganta, pie, raíz, ZAT | 366 |
| Grietas en caliente | 367 |
| Grietas en frío | 368 |
| Resto del catálogo | 369 – 398 |

### 6. Geometría de las juntas soldadas — cubierto

Capítulo 4, páginas 133 a 174.

| Subtema | Páginas |
|---|---|
| Uniones soldadas, tipos de uniones | 133 – 139, 143 – 145 |
| Partes de la junta | 140 – 142 |
| Tipos de soldadura | 146 – 152, 170 – 173 |
| Soldaduras terminadas | 153 |
| Terminología de fusión y penetración | 154 – 155, 174 |
| Terminología de tamaño de soldadura | 156 – 160 |
| Terminología de aplicación | 161 – 168 |

### 7. Posiciones de soldadura — cubierto, pero escondido

No hay capítulo propio. Están dentro del capítulo 5, en **páginas 255 a 257**,
como figuras de AWS D1.1:

- Figura 5.20: posiciones 1G, 2G, 3G y 4G de ranura en placa
- Figura 5.21: posiciones 1F, 2F, 3F y 4F de filete
- Figura 5.22: posiciones 1G rotada, 2G, 5G, 6G y 6GR en tubo

Son páginas de pura imagen. Una búsqueda de texto por "posiciones de soldadura"
devuelve cero, y por eso es fácil darlas por ausentes.

Para el producto de Tenneco, que es lámina, las posiciones de tubería (5G, 6G,
6GR) no aplican.

### 8. Simbología de soldadura — sobre-cubierto

Páginas 175 a 218, **44 páginas**, basadas en AWS A2.4. Es la sección más
extensa del manual aplicable al nivel I.

| Subtema | Páginas |
|---|---|
| Introducción, referencia a AWS A2.4 | 175 |
| Weld symbol contra welding symbol | 176 |
| Símbolos de ranura y de filete, tabla completa | 177 |
| Los 8 elementos del símbolo | 178, 180, 190 |
| Figura 4.39, localización estándar de los elementos | 179 |
| Localización del weld symbol, lado de flecha y otro lado | 181 – 182 |
| Combinación de weld symbols | 183 |
| Múltiples líneas de referencia | 184 |
| Símbolos suplementarios | 185 – 189 |
| Dimensiones, longitud, paso, soldadura intermitente | 191 – 209 |
| Dimensionamiento del símbolo | 210 – 218 |

### 9. Documentos de control de soldadura — parcial

Capítulo 5, páginas 219 a 263.

| Subtema | Páginas |
|---|---|
| Planos | 221 – 225 |
| Códigos, normas, especificaciones | 226 – 231 |
| Control e identificación de materiales | 232 – 239 |
| Calificación de procedimientos y soldadores | 240 – 243 |
| AWS D1.1, ASME Sección IX, API 1104 | 240 – 244 |
| WPS y PQR, relación entre ambos | 245 |
| Variables esenciales | 244, 252 – 253 |
| Pruebas de calificación | 245 |
| Tabla de límites por tipo y posición, figura 5.19 | 254 |

**Huecos inevitables:** hoja de configuración de producto y hoja de parámetros.
Son documentos internos de Tenneco y ningún manual AWS los va a contener.

---

## Resultado

| Tema del nivel I | Estado | Páginas dedicadas |
|---|---|---|
| 1. Seguridad de soldadura | Cubierto, falta celda robótica | 28 |
| 2. Equipo GMAW | Parcial | 7 |
| 3. Mecanismos de transferencia | Parcial, con un error | 2 |
| 4. Variables del proceso | Ausente | 0 |
| 5. Discontinuidades | Cubierto | 39 |
| 6. Geometría de las juntas | Cubierto | 42 |
| 7. Posiciones | Cubierto | 3 |
| 8. Simbología | Sobre-cubierto | 44 |
| 9. Documentos de control | Parcial | 24 |

Cinco de nueve temas cubiertos, tres parciales, uno ausente.

**La causa de fondo:** es un manual de **inspector**, no de **operador**. Un CWI
juzga si la soldadura cumple; un ajustador de celda la produce. Por eso el
manual es muy fuerte en lo que se ve y se mide (símbolos, geometría,
discontinuidades, posiciones) y flojo en lo que se ajusta (variables, equipo,
transferencia).
