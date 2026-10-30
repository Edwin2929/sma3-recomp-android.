# Fantasma: captura y presentación de Yoshi

## Alcance

El componente visual de 0.10.17 obtiene la imagen del Yoshi normal de los OBJ,
tiles y paleta que ya produce el juego. No incluye una hoja de sprites ni dibuja
un icono en sustitución del personaje. Guarda la posición mundial de la esquina
de la imagen, la habitación y los texeles de la pose. El servicio de
`time_trial_ghost` decide qué recorrido corresponde al récord personal y qué
pose debe reproducirse; estas pruebas se centran en su captura y presentación.

La imagen conserva los colores capturados, incluido el color personalizado de
Yoshi cuando el proveedor de paleta está activo, y se dibuja con alfa 112/255
(aproximadamente 44 % de opacidad). No se aplica un tinte cian. Solo incluye el
cuerpo y la pose de Yoshi; Baby Mario y los huevos tienen reservas distintas de
tiles y no se incorporan al atlas.

No se ha cambiado el arranque, la intro ni la reanudación de partidas.

## Contrato de captura

`src/ghost_visual.h` define una imagen de hasta 64 × 64, índices de 4 bits y
16 colores RGB555. El índice cero es transparente. Los bordes transparentes se
recortan y los bytes y colores sin uso se ponen a cero para que poses iguales
se puedan deduplicar. La memoria de la imagen tiene tamaño fijo; el componente
visual no escribe archivos ni reserva memoria en cada cuadro.

La selección usa el dominio del proveedor de color ya existente: Yoshi normal,
paleta OBJ 5, formato 4 bpp y tiles base 0–63. La decodificación contempla los
tamaños GBA, el orden de los OBJ, la transparencia, las inversiones horizontal
y vertical, los mapas de tiles 1D/2D y las matrices afines, incluido el cuadro
doble. Se limita la composición a 32 OBJ y una imagen de 64 × 64; una muestra
que no cabe queda oculta.

La imagen se obtiene únicamente cuando el contexto es compatible. Queda
oculta durante transformaciones, estados distintos del juego normal,
inventario, pausa Start, Fuzzy, pantalla forzosamente vacía, transiciones de
brillo que afectan a OBJ, mosaico OBJ activo o ventanas que restringen OBJ en
alguna región habilitada. Las ventanas normales que permiten OBJ en todas sus
regiones no bloquean la captura. La reproducción también requiere que coincida
la habitación y se oculta al abrir los ajustes Android o el editor de controles.
Las formas transformadas no tienen sprites inventados: la reproducción vuelve
a la pose correspondiente al tiempo de juego cuando regresa una forma
compatible.

La cámara se lee de `030069D4`/`030069DC`. El código original que prepara la
posición en pantalla, `Sub08042FF8`, resta esas coordenadas de la posición de
Yoshi. Por ello la esquina de la imagen en mundo es su esquina OBJ más la
cámara. La observación nativa descrita abajo contrastó ambas representaciones.

## Sincronización y geometría

El callback de inicio de cuadro se publica en la línea cero. La presentación
inmediatamente posterior usa la imagen terminada que se fijó en el VBlank
anterior. `ContextLatch`, utilizado directamente por producción y por las
pruebas, conserva el contexto anterior para esa presentación. El servicio de
reproducción aplica el mismo desplazamiento a la pose. Una publicación
duplicada por cambio de tamaño y una presentación adicional a 120 Hz no
avanzan esta historia. Un cambio de estado restaurado, de bus o un retroceso de
cuadros invalida el contexto anterior.

El orden de integración en `main.cpp` es: cronómetro, proveedor de color,
política panorámica, contexto visual y grabador/reproductor. Así se conserva
también la decisión panorámica del cuadro correspondiente.

`HostOverlay` dibuja después de la imagen y sus filtros, antes de los controles
táctiles. La capa no modifica el framebuffer fiel, la RAM ni las entradas del
juego. Las coordenadas se convierten al rectángulo real de la imagen en el
drawable, con factores horizontal y vertical independientes para Estirar.
Panorámica añade el desplazamiento del viewport nativo. Se recorta contra los
límites de la imagen y del drawable; si la política panorámica ha cerrado los
márgenes, se restringe al centro nativo. Cinemático conserva sus dos barras
exactas, cada una de `alto / 10` píxeles redondeados hacia abajo.

Las regiones de un mismo color se combinan horizontalmente y entre filas
iguales. El límite es de 512 rectángulos por presentación. Una imagen que
superaría el límite se rechaza antes del primer dibujo, evitando un fantasma
parcial. Las regiones se almacenan en caché hasta que cambia la imagen.

## Pruebas ejecutadas

`validation/ghost_visual_test.cpp` completó **2.545 comprobaciones** sin ROM:
decodificación, inversiones, matrices afines, mapeo 1D/2D, prioridad, recorte,
canonicidad, exclusión de Baby Mario, condiciones de ocultación, límites,
geometría Original/Estirar/Panorámica/Cinemático y la historia de contextos ante
duplicados, pausa/reanudación, restauración y cambio de origen. La ejecución
optimizada con la captura privada añade 11 comprobaciones y terminó con
**2.556 comprobaciones**. La ejecución sin ROM con ASan y UBSan terminó sin
diagnósticos; LeakSanitizer estaba desactivado.

`validation/ghost_overlay_test.cpp` ejecuta el callback de dibujo de producción
con un `HostOverlay` que registra las llamadas. La reproducción y el contexto
ya fijado son fixtures; la supresión, caché, geometría y llamadas son el código
real. Completó **38 comprobaciones**, incluidas:

- Cero llamadas de dibujo con reproducción desactivada.
- Color original y alfa 112; la segunda presentación conserva la posición.
- Ajustes, editor táctil, habitación distinta y pose/contexto ocultos.
- Estirar con escalas distintas, márgenes panorámicos y barras cinematográficas.
- Cero llamadas para una imagen que excede el presupuesto; no reaparece la
  imagen anterior desde la caché.
- Exactamente 512 llamadas para la imagen de prueba en el límite.

La ejecución de este callback con ASan y UBSan también completó sus 38
comprobaciones sin diagnósticos. La instrumentación omite la comprobación UBSan `vptr`, porque
el enlazado descarta las funciones de bus ajenas al dibujo y no incluye su
RTTI. LeakSanitizer está desactivado. La captura nativa privada solo se procesó
en la prueba optimizada; la ejecución ASan/UBSan del decoder usa los fixtures
sin ROM. No se han realizado pruebas en un teléfono físico.

### Observación nativa privada

Se utilizó el ejecutable de referencia 0.10.16 con la ROM USA admitida, BIOS
auténtica y un estado previo de juego en 1-1, avanzando dos cuadros neutros por
el protocolo TCP. Los datos OAM, VRAM, PAL, IWRAM e IO, la captura y el estado
permanecen fuera del repositorio. La ejecución terminó con `FULLY_STATIC`,
`distinct_misses=0`, `interpreted_insns=0`, `healed_native=0` y `failed=0`.

En el cuadro 10.104 se observaron seis OBJ de Yoshi, paleta 5 y tiles base
0, 2, 4, 6, 8 y 10; la cámara era (47, 1812), la posición del jugador
(119,25; 1904,99609375) y su ancla en pantalla (72, 92). El decoder produjo una
imagen recortada **26 × 31**, en mundo **(119, 1905)**, con **444 texeles opacos**
y **216 rectángulos**. La imagen se renderizó aparte y se inspeccionó
visualmente: representa la pose real de Yoshi, con silueta, cara, cuerpo y pies
conservados, sin Baby Mario. Esta es evidencia del componente con datos reales,
no una captura de un recorrido fantasma completo.

Una medición optimizada de 20.000 repeticiones sobre esa misma muestra dio una
media de aproximadamente **7,1 μs** para decodificar y **2,85 μs** para construir
las regiones. Son costes de componentes en el host, con datos repetidos en
caché. No incluyen SDL, el resto del juego, almacenamiento ni Android; no
justifican una afirmación de FPS en un dispositivo.

## Captura de la salida SDL

`validation/sdl_ghost_present_capture.c` es una herramienta de diagnóstico
Linux para observar la presentación real, incluidas las capas que no aparecen
en la captura del framebuffer TCP. Intercepta `SDL_RenderPresent`, lee el
drawable y llama a la función original. No altera memoria ni entradas del
juego. El readback perturba los tiempos y debe mantenerse separado de cualquier
benchmark.

Se activa con `LD_PRELOAD` y `SMA3_GHOST_PRESENT_TAP`, que indica un archivo
privado. `SMA3_GHOST_PRESENT_FROM` y `SMA3_GHOST_PRESENT_THROUGH` limitan los
índices de presentación. Se conserva la última imagen del intervalo: cuatro
enteros de 32 bits little endian (índice, ancho, alto, bytes), seguidos de los
píxeles RGBA32. Esta herramienta se compiló con `-Wall -Wextra -Werror`.
La prueba completa de grabación, reapertura y reproducción nativa corresponde
al informe de integración de la versión; no se sustituye por esta prueba del
componente.

## Reproducción de pruebas sin ROM

Desde la raíz del port y con el motor en `../gbarecomp`:

```bash
c++ -std=c++17 -O2 -Wall -Wextra -Werror -Isrc \
  validation/ghost_visual_test.cpp -o /tmp/ghost-visual-test
/tmp/ghost-visual-test

c++ -std=c++17 -O2 -ffunction-sections -fdata-sections \
  -Wall -Wextra -Werror -Wno-misleading-indentation \
  -Isrc -I../gbarecomp/src/gba -I../gbarecomp/src/runtime \
  -I../gbarecomp/src/armv4t validation/ghost_overlay_test.cpp \
  -Wl,--gc-sections -o /tmp/ghost-overlay-test
/tmp/ghost-overlay-test
```

Para instrumentar, sustituir `-O2` por
`-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer` y ejecutar con
`ASAN_OPTIONS=detect_leaks=0`; añadir `-fno-sanitize=vptr` en el caso del
callback que descarta el bus. La supresión de indentación solo evita una
advertencia preexistente en `touch_layout.h`; no se cambió ese archivo.

## Fuentes visuales verificadas

| Archivo | SHA-256 |
| --- | --- |
| `src/ghost_visual.h` | `8994b64b7e14e9bf4a79aa5e0cb7ae57b27c93fb4a3090179f47a4db45b86ea5` |
| `src/ghost_visual_detail.h` | `aeff07288fddf5b585fdf9372459e0e45062249ee505dc8df72a17cd529ec2d9` |
| `src/ghost_visual.cpp` | `9c824ff8292dd8c97886d9f80a7c44430abc56297780bc684ecebcd6db9ac9df` |
