# Bordes panorámicos — validación de 0.10.13 beta

## Resultado y alcance

**PASS** para el remate decorativo de los extremos exteriores comprobados.
Se probaron un exterior y ambos lados del castillo, con anchos lógicos 240,
347 y 356; también una restauración capturada fotograma a fotograma, Fuzzy
y el ciclo de transformación en coche. El APK ARM64 está compilado y firmado
con la misma identidad que 0.10.12.

El cambio **no convierte el relleno en terreno transitable**. Solo se activa
fuera de X `[0,4096)`, con Y dentro de `[0,2048)`, en escenas donde la
panorámica ya está autorizada. Las paredes interiores, los huecos jugables
y los límites de cámara no se interpretan como fronteras del mapa. No se
afirma cobertura de todos los niveles, instalación física ni 120 FPS sostenidos.

Informes reproducibles y fuentes:

- [Matriz de bordes y prueba temporal](panorama-edges-0.10.13-results.json).
- [Regresiones, helper y coste de CPU](panorama-edges-0.10.13-regressions.json).
- [Compilación y contenido del APK](android-0.10.13-build.json).
- [Helper](../src/widescreen_edges.h), [pruebas unitarias](widescreen_edges_test.cpp)
  y [validador de recorridos](validate_panorama_edges_gameplay.py).

## Diseño y restricciones

La reconstrucción anterior devolvía el tile transparente para coordenadas
fuera del mapa. Ahora BG0/BG1 pueden continuar una banda de 32 píxeles del
borde, reflejada de forma alternada. Se conservan la fase de píxel, los
cuadrantes, los bancos de paleta, la transparencia y los volteos originales.
Los fondos BG2/BG3 conservan sus mapas y desplazamientos; no se refleja la
imagen completa, ningún sprite ni el HUD de sprites.

Se inspeccionaron variantes de 16, 32 y 64 píxeles con el PPU real. La de
16 repetía demasiado las formas; la de 64 podía alcanzar una tubería y lava
más interiores. Se eligieron 32 y después se revisaron las imágenes del juego
en ejecución: suelo floreado continuo en el exterior y mampostería coherente
en ambos extremos del castillo. La decoración puede reutilizar los adornos
que formen parte de las capas de terreno; no es un generador de nuevos niveles.

El adaptador exige `ready`, de modo que las comprobaciones existentes de
escena, fondos, OAM y ventanas siguen siendo obligatorias. El proveedor del
PPU solo lo consulta fuera de los 240 píxeles nativos. Se mantienen las bandas
de Fuzzy, las escenas no compatibles y las ventanas no resueltas. No cambia
ningún archivo del motor respecto a los 18 parches de 0.10.12.

La auditoría del código del juego distingue límites de cámara y terreno:
`LoadObjectsAndScreenExits` inicializa la tabla de pantallas a `0x80`, mientras
`ExtObjFE_Init` puede añadir ese bit a pantallas con terreno real. La rutina
de colisión en `080433C4–080433C8` usa los seis bits bajos de la entrada.
Por tanto, una entrada vacía o el bit de prevención de desplazamiento no
acreditan una pared física. El relleno no usa esas suposiciones.

## Caché, errores de entrada y rendimiento

La caché fija ocupa **8.704 bytes**: 8.192 de entradas de tiles y 512 de
estados de fila. Se invalida en cada fotograma y al retirar o reconstruir
las observaciones de una escena. Solo la primera consulta de una fila de
ocho píxeles resuelve sus ocho entradas, cuatro por capa. Las siguientes
consultas no vuelven a leer el mapa; también se memorizan los fallos para
no reintentarlos por píxel. No se reserva memoria en el bucle de dibujo.

La prueba del helper pasa **225.181 casos** con ASan y UBSan, `-Wall`,
`-Wextra`, `-Wpedantic` y `-Werror`. Cubre ambos extremos, fases, cuadrantes,
HFlip/VFlip, paletas, propiedad de capa, `INT_MIN`/`INT_MAX`, punteros no
admitidos, filas fallidas y renovación después de cambiar el mapa. Las
consultas no admitidas no leen memoria. LeakSanitizer se desactivó por las
restricciones de `/proc` del entorno; ASan y UBSan estuvieron activos.

Un microbenchmark instrumental con memoria real congelada midió una banda
de 58 × 160, en ambas capas, durante siete muestras de 800 iteraciones:

| Escena | Helper anterior, mediana | Helper decorado, mediana | Incremento |
| --- | ---: | ---: | ---: |
| Exterior izquierdo | 0,076 ms | 0,098 ms | 0,021 ms |
| Castillo izquierdo | 0,086 ms | 0,111 ms | 0,025 ms |
| Castillo derecho | 0,087 ms | 0,117 ms | 0,030 ms |

Hubo **800–840 lecturas por fotograma**, no una nueva cadena de lecturas por
cada píxel. Los incrementos se calculan antes de redondear. Son tiempos de
CPU del helper en este host Linux, con instrumentación; excluyen el resto
del PPU, la GPU, Android y la cadencia de pantalla. No demuestran un coste
total equivalente en un teléfono ni una frecuencia de 120 FPS.

## Comparaciones del juego

Base del código: `1579bb1e26796c5a21a63ebf68b2cf0558055266` (0.10.12).
Motor fijado: `e7728148c6829ba526f682876430a0c9022dc6c0`, sin cambios.

| Ejecutable | SHA-256 |
| --- | --- |
| Referencia 0.10.12 | `84adabcc0c585219314a304165a1d19b4e924b87e4deee8b23dde623b1aa1ba1` |
| Candidato 0.10.13 | `6e02d7ddd005f5b0271d9ef99190c5be37724926a0b84b71f224b31c211ddd61` |

Cada par usa exactamente los mismos activos verificados, estado inicial y
acciones. El validador compara la trayectoria por fotograma, los puntos
finales, la programación de capturas, CPU, bus, IO, audio, guardado y META.
El PPU serializado conserva tanto su temporización como el fotograma nativo
de 240 × 160. La única exclusión de estado son los ocho bytes del reloj de
pared RTC inactivo establecidos por el comparador anterior; se exige que
el RTC esté realmente inactivo.

La entrada al exterior se preparó con el fixture sintético documentado,
seguido de 360 fotogramas normales. Después Yoshi llega al límite X = 8
mediante los controles del juego, sin cambiar su posición por memoria. Los
estados del castillo proceden del recorrido histórico y sus entradas naturales.

### Matriz de bordes

| Escena | Ancho | Fotogramas del par | Capturas | Píxeles laterales cambiados, suma |
| --- | ---: | ---: | ---: | ---: |
| Exterior izquierdo | 240 | 130 | 4 | 0 |
| Exterior izquierdo | 347 | 130 | 4 | 9.970 |
| Exterior izquierdo | 356 | 130 | 4 | 11.046 |
| Castillo izquierdo | 240 | 130 | 4 | 0 |
| Castillo izquierdo | 347 | 130 | 4 | 22.923 |
| Castillo izquierdo | 356 | 130 | 4 | 25.171 |
| Castillo derecho y tubería | 240 | 63 | 5 | 0 |
| Castillo derecho y tubería | 347 | 63 | 5 | 16.516 |
| Castillo derecho y tubería | 356 | 63 | 5 | 17.772 |

**Nueve pares PASS:** 969 fotogramas por versión, 978 filas de traza iguales,
39 centros originales exactos y 48 estados comparados. Los cambios suman
103.398 píxeles laterales entre las capturas y aparecen exclusivamente en el
lado previsto. Las 19 capturas protegidas permanecen enteras e idénticas.
En 240 no cambia ningún píxel.

Los nueve recorridos candidatos son nuevos. Se ejecutaron seis referencias
nuevas para la matriz y se reutilizaron las tres referencias de 356 ya
ejecutadas durante la preparación de esta tarea, verificando su identidad.
La reutilización no se cuenta como otra ejecución.

El primer cambio de estado de una tubería puede conservar una imagen ya
compuesta bajo el estado anterior; por eso no se interpreta todo cambio de
estado observado como una nueva imagen. Las capturas protegidas explícitas
60, 62 y 63 del recorrido derecho son idénticas. El validador pasa además
28 casos positivos y mutaciones, tanto normal como con `python -O`.

### Continuidad fotograma a fotograma

Una prueba adicional captura **cada uno de 60 fotogramas consecutivos** de
ambos ejecutores con Yoshi quieto junto al extremo exterior izquierdo:

- Los fotogramas 1–2 conservan exactamente las bandas de restauración.
- Del 3 al 60 hay **58 imágenes consecutivas con relleno visible**, sin
  desapariciones intermedias, con 2.759–2.763 píxeles decorados por imagen.
- La anchura permanece en 356, la cámara en X = 0 y Yoshi en X = 8.
- Todos los centros, estados del juego y PPU serializados coinciden.

Se conserva la animación auténtica del fondo; exigir un hash lateral fijo
habría rechazado esa animación. Esta prueba está separada de los 969
fotogramas de la matriz y no acredita todos los posibles recorridos.

### Fuzzy y coche

Se probaron ambos recorridos a **356 × 160**:

| Recorrido | Fotogramas | Filas de traza iguales | Capturas exactas | Estados equivalentes |
| --- | ---: | ---: | ---: | ---: |
| Fuzzy | 1.900 | 1.901 | 40 | 41 |
| Coche | 1.526 | 1.527 | 33 | 34 |

Las 73 capturas completas coinciden, incluida la panorámica. La temporización
PPU, los informes de logros y el estado de «Yo shi pude» son idénticos.
Fuzzy mantiene su efecto real durante 782 fotogramas, recupera el estado
normal en el 1.800 y termina con 101 fotogramas normales, sin reinicio ni
muerte. Sus 15 capturas activas conservan ambos márgenes negros; las de
1.800, 1.860 y 1.900 recuperan los laterales. El coche completa el ciclo con
25 capturas de vehículo estable.

La referencia Fuzzy a 356 es nueva. La del coche se reutilizó después de
verificar todas las entradas. Dos intentos bloqueados por el entorno antes
de crear un socket local no ejecutaron el juego ni se contabilizan como
recorridos. Las ejecuciones posteriores con TCP local terminaron correctamente.

## APK y reproducción

Las **1.825 entradas de compilación Android** permanecieron estables. Las
1.779 entradas compartidas coinciden con el ejecutor congelado usado para
las pruebas. El APK excluye los diagnósticos de escritorio; `libmain.so` se
recompiló, mientras SDL2 y libc++ conservan sus bytes. El archivo principal
del juego se reutilizó tras comprobar sus 102 entradas originales.

La primera compilación llegó a D8 y falló al observar un directorio temporal
de sincronización entre las clases generadas. Se retiró únicamente la salida
DEX fallida, conservando fuentes y clases. El segundo intento pasó las 39
tareas y la verificación de firma v2, CRC, alineación, versiones, bibliotecas,
21 enlaces JNI y los 26 logros. Los dos intentos figuran en el informe.

El aplicador de parches tenía un test con una lista histórica de 16 etapas.
Se actualizó a las 18 ya existentes: sus tres pruebas pasan para los 19
estados reconocidos, la idempotencia y el rechazo de cambios ajenos.

Prueba independiente sin activos del juego:

```bash
g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -O1 -g \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  validation/widescreen_edges_test.cpp -o /tmp/sma3-widescreen-edges-test
ASAN_OPTIONS=detect_leaks=0 /tmp/sma3-widescreen-edges-test
python3 -B validation/test_window_patch_stack.py
```

Un par de recorridos con activos y fixture privados:

```bash
python3 validation/validate_panorama_edges_gameplay.py \
  --baseline /ruta/runner-0.10.12 --candidate /ruta/runner-0.10.13 \
  --rom /ruta/juego.gba --bios /ruta/bios.bin \
  --state /ruta/borde-izquierdo.state --actions /ruta/acciones.json \
  --width 356 --changed-side left --require-change \
  --output /ruta/salida-privada-nueva
```

Para ancho 240 omite `--require-change`. Los anchos 347 y 356 ejercitan
centrados con márgenes asimétricos y simétricos, respectivamente. Las
capturas, ROM, BIOS, estados y claves permanecen privados; los informes
publicados contienen metadatos y resultados, no esos activos.
