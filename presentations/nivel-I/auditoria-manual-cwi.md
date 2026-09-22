# Auditoría: Manual CWI de COMIMSA contra el nivel I de la matriz ILUO

**Documento auditado:** Programa de Preparación para la Certificación de
Inspectores en Soldadura CWI de la AWS. Subgerencia de Capacitación y
Certificación, COMIMSA, 2017. 440 páginas, español, 10 capítulos.

**Método:** extracción del texto de los cinco bloques PDF, reconstrucción de la
estructura por el pie de página de cada lámina, y verificación visual de las
páginas sin capa de texto. En los cinco capítulos que tocan el nivel I hay
**97 páginas cuyo contenido es sólo imagen**, y ahí es donde una búsqueda por
palabras se equivoca.

**Segunda pasada.** La primera versión de esta auditoría daba por ausente el
tema 4 y subestimaba el 2. Dos causas: el manual usa vocabulario de inspector
(dice *entrada de calor*, no *aporte de calor*, que es lo que yo buscaba) y sus
figuras más importantes están en páginas sin texto. Corregido abajo; las
correcciones van marcadas.

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

Unas 205 de las 440 páginas aplican directamente al nivel I, el 47 %. El resto
es contenido de inspector que no aparece en la matriz.

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

### 2. Equipo GMAW — parcial, con más contenido del que parecía

Páginas 61 a 67. Tres de esas siete páginas son figuras sin texto, y son las
que más valen.

| Contenido | Página |
|---|---|
| Definición del proceso, gases (argón, helio, CO2, O2 y mezclas) | 61 |
| **Figura 10.8, diagrama completo del equipo GMAW** | **62** |
| Tabla de modos de transferencia y figura 3.14 | 63 |
| Figura 3.15, nomenclatura de la pistola | 64 |
| **Figura 3.11, sistema de clasificación del electrodo** | **65** |
| Ventajas y desventajas | 66 |
| Discontinuidades típicas del proceso | 67 |

**Corrección.** La figura 10.8 de la p62 numera diez componentes del equipo:
work lead, water to gun, water from gun, gun switch circuit, shielding gas to
gun, cable assembly, shielding gas from cylinder, welding contactor control,
power cable y primary input power, más power source, electrode feed unit,
welding gun, shielding gas supply y shielding gas regulator. El alimentador y
el regulador sí están; lo que no está es el detalle de rodillos, liner y
difusor.

**Corrección.** La figura 3.11 de la p65 enseña la clasificación **ER XXS-X**:
ER para electrode rod, XX para resistencia, S para solid wire y -X para
composición química. O sea que ER70S-6 sí se puede decodificar con el manual,
aunque la cadena "ER70S" no aparezca escrita en ninguna página.

La p64 da la nomenclatura de la pistola: gas nozzle, contact tube, contact tube
setback, electrode extension, stickout y standoff distance.

**Huecos que siguen:** curva V-A de la fuente de voltaje constante; rodillos,
liner y difusor; polaridad DCEP aplicada a GMAW (el manual sólo trata DCEP y
DCEN en SMAW, p56, y GTAW, p77); periféricos de celda robótica (TCP, reamer,
corte de alambre, detección de colisión).

### 3. Mecanismos de transferencia de metal — parcial, y con un error

Página 63, una sola tabla, más la figura 3.14 de los modos.

Nombra los cuatro modos y los asocia a un gas de protección.

**Error a corregir.** La tabla de p63 dice que la transferencia *spray* opera a
corriente y voltaje **bajos**. Es al revés: spray exige corriente por encima de
la corriente de transición, es el modo de corriente alta; el de corriente baja
es el cortocircuito. Verificado sobre la imagen de la página, no sobre el texto
extraído. Si el examen se arma con esta tabla, la respuesta correcta y la
respuesta del manual no coinciden.

**Añadido en la segunda pasada.** La p243 trae la tabla D1.1 de junta
precalificada, y su Nota A dice: *"Not prequalified for gas metal arc welding
using short circuiting transfer nor GTAW."* Es decir, la transferencia por
cortocircuito **no está precalificada** en el código estructural. Es la única
consecuencia de código que el manual liga a un modo de transferencia, y es un
ejemplo excelente para el examen.

**Huecos:** corriente de transición, relación con el diámetro del alambre,
criterio de selección por espesor y posición.

### 4. Variables del proceso — parcial

**Corrección respecto a la primera versión, que lo daba por ausente.** El
contenido existe, repartido en tres sitios distintos y bajo vocabulario de
inspector.

**a) La fórmula de entrada de calor, p343.** El manual la escribe así:

> Entrada de calor, Joules/in = (Corriente de soldadura × Voltaje de soldadura
> × 60) / velocidad de desplazamiento, in/min

Y añade la relación: mientras la entrada de calor aumenta, la velocidad de
enfriamiento disminuye. Es decir, las tres variables operativas del nivel L
(corriente, voltaje y velocidad de desplazamiento) sí están, dentro del
capítulo de metalurgia. La misma idea reaparece en la p94 aplicada a PAW.
Velocidad de enfriamiento también en p335, p338, p340 y p349. Precalentado en
p343, figura 8.15.

**b) Efecto de las variables sobre el defecto, capítulo 9.** El manual no
enseña a ajustar, pero sí enseña qué pasa cuando el ajuste está mal:

| Efecto | Causa que da el manual | Página |
|---|---|---|
| Socavación | Calor excesivo, corriente excesiva, velocidad de desplazamiento excesiva | 386 |
| Convexidad en filete | Velocidad de avance demasiado lenta | 389 |
| Fusión incompleta | Insuficiente aplicación de calor | 373 |
| Discontinuidades en LBW y EBW | Velocidades de desplazamiento altas | 398 |

**c) Variables esenciales en el sentido del código.** p139, p244, p245, p251,
p252 y p253. La p251 es la más útil: los nueve pasos de la calificación de un
procedimiento, que abren con *"Seleccionar las variables de soldadura"* y siguen
con *"monitorear... registrando todas las variables importantes y
observaciones"*. La p252 enumera las variables esenciales de la calificación
del soldador: posición, configuración de la junta, tipo y tamaño de electrodo,
proceso, tipo y espesor del metal base y técnica específica.

**d) Variables de la junta, p243.** La tabla D1.1 de junta precalificada fija,
para GMAW y FCAW, la abertura de raíz (R), el ángulo de ranura (α), las
posiciones permitidas y, sobre todo, las **tolerancias "as detailed" contra "as
fit-up"** (R = +1/16, −0 contra +1/4, −1/16; α = +10°, −0 contra +10°, −5°).
Esto es lo más parecido que hay en el manual a lo que tú haces al validar una
hoja de parámetros en la celda.

**e) Terminología de técnica, p161 a p168.** Pase, capa y cordón (p161),
oscilación transversal contra sin movimiento apreciable (p162), secuencias de
retroceso, bloque y cascada para controlar distorsión (p163 a p166), filete
intermitente en cadena y escalonado (p167) y cajeado (p168).

**Huecos que siguen:** ángulo de trabajo y ángulo de desplazamiento por su
nombre; empuje contra arrastre; el stickout tratado como variable ajustable en
GMAW (sólo aparece como cota en la figura de la p64 y como variable de FCAW
autoprotegido en la p71); inductancia; flujo de gas con unidades.

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
| **Tabla D1.1 de junta precalificada: abertura de raíz, ángulo de ranura y tolerancias** | **243** |

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

**Añadido en la segunda pasada.** La p243 liga posición con proceso y junta: su
columna *Permitted Welding Positions* dice qué posiciones admite cada
designación de junta precalificada. Para GMAW y FCAW en junta B-U2a-GF son F, V
y OH.

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
| **Precalificación de WPS, tabla D1.1 de junta precalificada** | **243** |
| **Los nueve pasos de la calificación de un procedimiento** | **251** |
| Probetas de calificación: ASME QW-463, QW-451.1, API 1104 | 246 – 250 |

**Huecos inevitables:** hoja de configuración de producto y hoja de parámetros.
Son documentos internos de Tenneco y ningún manual AWS los va a contener.

---

## Resultado

| Tema del nivel I | Estado | Páginas |
|---|---|---|
| 1. Seguridad de soldadura | Cubierto, falta celda robótica | 28 |
| 2. Equipo GMAW | Parcial | 7 |
| 3. Mecanismos de transferencia | Parcial, con un error | 3 |
| 4. Variables del proceso | Parcial, repartido en tres capítulos | ~22 |
| 5. Discontinuidades | Cubierto | 39 |
| 6. Geometría de las juntas | Cubierto | 43 |
| 7. Posiciones | Cubierto | 4 |
| 8. Simbología | Sobre-cubierto | 44 |
| 9. Documentos de control | Parcial | 27 |

**Cinco temas cubiertos, cuatro parciales, ninguno ausente.**

**La causa de fondo:** es un manual de **inspector**, no de **operador**. Un CWI
juzga si la soldadura cumple; un ajustador de celda la produce. Por eso el
manual es muy fuerte en lo que se ve y se mide (símbolos, geometría,
discontinuidades, posiciones) y trata las variables por su consecuencia, no por
su ajuste: aparecen como entrada de calor en metalurgia, como causa de defecto
en discontinuidades y como variable esencial en calificación, nunca como una
perilla que se mueve.

## Lo que sigue faltando, y que hay que cubrir por fuera

1. Seguridad de celda robótica: paros de emergencia, interlocks, vallas, modo
   teach contra automático, velocidad reducida, LOTO.
2. Curva V-A de la fuente de voltaje constante; rodillos, liner y difusor;
   polaridad DCEP en GMAW.
3. Corriente de transición y criterio de selección del modo de transferencia,
   más la corrección de la tabla de la p63.
4. Ángulos de trabajo y desplazamiento, empuje contra arrastre, stickout como
   variable ajustable, flujo de gas.
5. Hoja de configuración de producto y hoja de parámetros de Tenneco.
