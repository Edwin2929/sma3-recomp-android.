# Panorámica real — Android 0.10.3 beta

## Filtros de presentación en 0.10.3

0.10.3 añade **Imagen → Filtros de imagen**. Los seis filtros cambian el
acabado del fotograma terminado, conservando la cobertura y las restricciones
panorámicas de 0.10.2 descritas debajo. El modo continúa como **Panorámica
(beta)**, con el aviso de fase temprana. Las bandas durante el efecto de
terreno del Fuzzy se mantienen. Un filtro no amplía por sí mismo la escena.
[Guía de filtros](CALIDAD_IMAGEN.md#filtros-de-imagen-en-0103) ·
[Notas de 0.10.3](releases/v0.10.3-beta.md).

## Correcciones y límites actuales en 0.10.2

El menú usa **Panorámica (beta)** y avisa de que está en una fase temprana,
con posibles errores visuales o de compatibilidad. Se activa en
**Atrás → Imagen**, junto a Original (3:2) y Estirar a pantalla completa.
Cambiar el nombre no altera las preferencias guardadas.

Se corrige otra causa de titileo que no aparecía en el recorrido anterior
del castillo: un objeto ordinario sin posición lateral comprobada apagaba
los dos márgenes completos. Ahora se conservan el terreno y los objetos
verificados, mientras ese objeto se dibuja únicamente en el centro original.
Se mantienen la coherencia de la observación de OAM y las restricciones de
las ventanas de iluminación; no se inventan posiciones laterales.

En los fondos BG3 con una imagen completa de 512 × 512 y la distribución
de mapa nativa comprobada se usa el mapa que actualiza el propio juego.
Así se conservan sus entradas de tiles, bancos de paleta y animaciones.
Los mapas mayores o cargados por partes conservan su reconstrucción
específica; la regla no se aplica a todos los fondos de forma indiscriminada.

**Fuzzy:** el cierre al tocarlo tenía una causa independiente del ancho de
imagen: faltaban dos rutinas ARM asignadas dinámicamente en RAM. 0.10.2 las
recompila y comprueba su asignación y sus bytes antes de ejecutarlas. El
efecto original del Fuzzy se mantiene. Mientras está activo el desplazamiento
de terreno, se conservan márgenes negros estables porque el juego solo
calcula esa deformación para su encuadre original. Al restaurarse los mapas,
vuelve la panorámica. Esto es una limitación explícita de la beta.

[Pruebas de panorámica](../validation/PANORAMA_0.10.2_VALIDATION.md) ·
[Pruebas de Fuzzy](../validation/FUZZY_VALIDATION.md) ·
[Notas de entrega](releases/v0.10.2-beta.md).

Las secciones siguientes conservan la historia de las versiones anteriores;
sus cifras y limitaciones se refieren a cada entrega indicada.

## Corrección de presentación en 0.10.1

El reporte de titileo en 0.10 se reprodujo con movimiento: cinco cambios
transitorios de anchura y seis fotogramas con el centro corrupto en 600
presentaciones. La comprobación tardía de OAM usaba datos de la siguiente
imagen; al cambiar la geometría se descartaba la imagen terminada y se
redibujaba memoria de otra fase del fotograma.

0.10.1 conserva la geometría panorámica mientras ese modo está seleccionado.
La política de cada escena se aplica durante el dibujo del fotograma y
mantiene negros los márgenes no autorizados. Al redimensionar o restaurar,
conserva el centro de la imagen terminada y limpia los márgenes pendientes.
También respeta las bandas negras durante el blanqueo forzado del juego.

El mismo recorrido conserva ahora los 600 centros originales, sin oscilación
de anchura. Pasan también los cambios 356 → 240 → 347 → 356 y la ruta real de
mezcla temporal, compilada para host. Las verificaciones no amplían los límites
de aparición ni equivalen a una prueba física de Android o de todo el juego.
[Notas de 0.10.1](releases/v0.10.1-experimental.md) ·
[Diagnóstico, pruebas y reproducción](../validation/ANDROID_0.10.1_VISUAL_FIXES.md).

## Integración inicial en 0.10

**0.10 integra la vista panorámica opcional en Android.** El APK
`sma3-recomp-0.10-panorama-arm64.apk` se compila con esa función y conserva
la firma de 0.9/0.9.1. Su entrega al mantenedor y su publicación manual en
GitHub Releases son pasos distintos: la release pública «Descarga» sigue
conteniendo 0.9 al preparar esta versión.
[Notas e instalación de 0.10](releases/v0.10-experimental.md) ·
[Informe completo de compilación y validación](../validation/ANDROID_0.10_VALIDATION.md).

## Activación y comportamiento de 0.10

Abre **Atrás → Imagen → Panorámica (experimental)**. La opción aparece junto
a **Original (3:2)** y **Estirar a pantalla completa**. El menú explica en
ES/EN/PT que Original conserva los 240 × 160, Estirar deforma el mismo
fotograma y Panorámica dibuja escenario adicional donde tiene cobertura.
La actualización conserva la elección anterior; una instalación nueva usa
Original y ninguna migración activa Panorámica automáticamente.

El ancho lógico sigue la superficie de presentación, entre **240 y 356
píxeles**, con altura **160**. Una superficie 3120 × 1440 calcula 347 × 160;
2400 × 1080 calcula 356 × 160. En vertical se conserva 240 × 160 centrado.
Puede haber bandas por el límite de ancho, el redondeo o una escena no
admitida. `fullUser` permite la orientación del usuario en Android; la
rotación física todavía requiere prueba en el teléfono.

La vista amplia se autoriza únicamente en juego normal compatible, con un
mapa válido, fondos habilitados reconstruibles y una observación completa
coincidente con los objetos mostrados. Si falta esa información, vuelve a
la vista nativa. Al cargar una partida, invalida el historial y conserva
Original hasta reconstruir datos coherentes. WIN0/WIN1 y otras ventanas no
cubiertas pueden mantener márgenes negros por fila. Original y Estirar
retiran el observador de panorámica y sus proveedores.

## Qué se ha comprobado para 0.10

- Dos recorridos emparejados 240/356: **6.801 fotogramas del castillo**, con
  dos transiciones naturales por tubería, y **3.506 del ciclo normal → coche
  → normal**. Los 311 centros de imagen y 313 estados comparados coinciden;
  no hay huecos de procedencia de objetos en esos recorridos.
- Frente a la revisión anterior de iluminación: **622 imágenes completas y
  626 estados conservados**, incluyendo los márgenes y el estado de PPU.
- Siete casos de presentación de producción SDL, sin activar la ruta de
  captura del prototipo: panorámica 347/356, vertical, Original, fuente de
  Estirar y transición no admitida. Los cinco centros comparados coinciden
  con su referencia y no se observa oscilación de anchura.
- **40 casos de ciclo de vida** y **488 del compositor real de ventanas**,
  con ASan/UBSan. Se corrigió un defecto reproducible al restaurar una
  partida a mitad de fotograma: quedaban restos laterales de la escena
  anterior. Ahora se limpia la superficie ampliada antes de restaurar el
  centro nativo.
- **169 pruebas Python** normales y con `-O`, incluida la aplicación y
  repetición de los siete parches sobre las ocho etapas reconocidas.

Los informes y las instrucciones de reproducción están reunidos en
[ANDROID_0.10_VALIDATION.md](../validation/ANDROID_0.10_VALIDATION.md).
La prueba del Pixel 7 Pro comunicada anteriormente por Edwin corresponde a
**0.9.1**; no acredita la panorámica ni el rendimiento de 0.10.

## Límites de esta entrega experimental

**Los límites originales de aparición y dibujo de objetos no se amplían.**
La coherencia de OAM comprueba los objetos que el juego ya decidió mostrar;
no detecta necesariamente uno que el juego omitió fuera del encuadre
original. Por eso pueden aparecer tarde o faltar objetos en los márgenes,
aunque una escena tenga fondo lateral. Reducir arbitrariamente el máximo
no demuestra que esos filtros dejen de intervenir.

Queda ampliar y validar esos límites, cubrir más niveles, jefes, mensajes,
transformaciones y ventanas, y probar instalación, rotación, segundo plano,
controles y rendimiento en el Pixel 7 Pro. **0.10 es una compilación
experimental utilizable para esas pruebas, no una validación del juego
completo ni de 120 FPS.** Original (3:2) permite volver al encuadre nativo.

## Qué pueden hacer los APK 0.9 y 0.9.1

Para conservar las proporciones, abre **Atrás → Imagen → Estirar a pantalla
completa** y desactiva la casilla. El fotograma original de **240 × 160** tiene
proporción **3:2**; se centra y deja bandas cuando la superficie tiene otra
proporción. Activar Estirar llena el área de juego con ese mismo fotograma,
puede deformarlo y **no muestra más escenario**.

La panorámica real descrita aquí necesita dibujar contenido adicional fuera
de ese fotograma. No se activa con Estirar ni al elegir 720p o 1080p.
La aclaración de la ayuda del menú está incluida en el APK 0.9.1 y
**no integra este prototipo en Android**. La confirmación de funcionamiento
de 0.9.1 en un Google Pixel 7 Pro corresponde al APK con el campo de visión
original, no a una compilación panorámica.
Más detalles en la [guía de calidad de imagen](CALIDAD_IMAGEN.md).

## Base de renderizado implementada

- Reconstrucción del terreno desde el mapa completo del nivel, fuera del anillo
  parcial de VRAM. Incluye las cuatro piezas de cada bloque y las máscaras de
  la capa frontal.
- Lectura de los fondos completos, con sus desplazamientos y volteos.
- Lecturas acotadas sin escribir RAM ni modificar CPU, temporización o estado
  del bus. La vista admite hasta 356 × 160, frente a 240 × 160 originales.
- Captura de diagnóstico por fotograma y pruebas independientes del decodificador.
- `SMA3_PANORAMA=ON` incluye la presentación opcional en producción.
  `SMA3_WIDESCREEN_DIAGNOSTICS` es independiente, exclusivo de escritorio y
  permanece desactivado en Android.

## Historial de investigación anterior a 0.10

Los apartados siguientes conservan los avances y límites de cada fase del
prototipo de escritorio. Las afirmaciones sobre APK no generados se refieren
a esas fases; el estado actual de 0.10 se describe al principio. Las pruebas
y contratos históricos no se han sustituido por los nuevos resultados.

## Reconstrucción del 23 de octubre de 2026 (UTC)

Se reconstruyeron los observadores de la nube puente `0x0BB`, los 17 componentes
del sprite `0x064`, la tercera pieza afín de `0x076/0x077` y las copias de OAM
usadas por el efecto OBJ-window. Las pruebas independientes de esos productores
y la regresión de objetos giratorios pasan de nuevo con C++20, ASan y UBSan.
El historial sigue admitiendo una sola presentación completa y coherente;
los observadores no escriben RAM ni registros del juego.

Las cuatro ejecuciones nuevas reproducen la misma cinta de **130 acciones y
6.801 fotogramas**, desde el fixture original y sin restauraciones intermedias.
Conservan la primera tubería y continúan por la zona superior hasta una segunda
salida, del subnivel 3 al 153. La pareja corregida de **240 × 160 y 356 × 160**
supera el contrato de recorrido y salud, con **250 centros RGB idénticos,
251 comparaciones de estado equivalentes y cero huecos de posiciones**.
Termina con tres vidas, sin muertes ni separación de Baby Mario durante el
juego normal, y 418 fotogramas finales quietos y sin botones.

La comparación antes/después conserva las trazas, las imágenes de 240 píxeles,
los centros panorámicos y los estados comparados. Los 759 fotogramas con
problemas de posiciones de la base pasan a cero en ambas anchuras. Las
diferencias de imagen aparecen únicamente en los márgenes de 21 capturas.
La [regresión del coche](../validation/widescreen-car-post-pipe-fix.json)
también pasa: ciclo normal → coche → normal, 3.506 fotogramas y 1.200 fotogramas
finales sin botones. Las 166 pruebas Python pasan normal y con `-O`.
[Resultados, criterios y límites](../validation/CASTLE_POST_PIPE_VALIDATION.md) ·
[Procedencia de la reconstrucción](../validation/castle-post-pipe-native-build.json).

**Límite de aquella revisión:** la máscara OBJ-window todavía se construía
en los 240 píxeles originales. Las filas con controles de ventana distintos
conservaban los márgenes negros, aunque la procedencia de los objetos ya
estuviera corregida. Las capturas de esa fase y su informe se conservan como
base histórica; la ampliación de la máscara se comprueba por separado abajo.

## Iluminación en los márgenes, 23 de octubre de 2026 (UTC)

El nuevo parche `gbarecomp-widescreen-window.patch` añade una máscara exclusiva
para las columnas adicionales. Usa la X completa verificada, los atributos,
las matrices afines y la transparencia del objeto original. La máscara central
conserva su decodificador y sus coordenadas de hardware; el adaptador no escribe
memoria, registros ni estado del juego.

La autorización es por fila y exige OBJ y OBJ-window activos, WIN0/WIN1
desactivados, juego normal, una presentación coherente y posiciones comprobadas
para todos los objetos de ventana que tocan esa fila. Si falta una posición,
se conserva la política anterior de esa fila. No se activa globalmente
`g_ws_authored_margin_layers` ni se amplían ventanas rectangulares, escenas
de transición o márgenes verticales.

La sala iluminada del subnivel 153 cumple estas condiciones. Sus siluetas
deciden qué capas y efectos se ven en cada píxel lateral, incluso cuando
una ventana queda completamente fuera de la imagen central. La nueva pareja
de ejecuciones de **6.801 fotogramas** supera de nuevo el contrato de las dos
tuberías: **250 centros idénticos, 251 estados equivalentes y cero huecos de
posiciones**. Frente al ejecutor anterior, cambian únicamente los márgenes de
**nueve capturas** de esa sala, con **63.230 píxeles RGB** distintos sumados.
La regresión del coche también pasa con el ejecutor nuevo.

Las pruebas independientes usan el compositor real con texturas sintéticas
y comparan la máscara ampliada con objetos trasladados dentro del compositor
nativo. Cubren transparencias, colores de 4/8 bits, mapas 1D/2D, volteos,
transformaciones afines, coordenadas completas, prioridades de ventanas y
rechazo de procedencia inválida, bajo ASan/UBSan. El aplicador reconoce las
siete etapas de la pila de seis parches y rechaza modificaciones ajenas.
[Resultados, reproducción y alcance exacto](../validation/WINDOW_MASK_VALIDATION.md).

Esta ampliación no califica todas las ventanas, otras salidas, el jefe ni el
castillo completo. La panorámica continúa fuera de los APK **0.9/0.9.1** y
esta comprobación no mide FPS.

## Comprobaciones y límites de la evidencia

Durante la investigación previa, una prueba de 9.600 fotogramas terminó con
`dispatch_misses=0 interpreted_insns=0 healed_native=0`, `unmapped=0`.
La imagen ampliada mostró terreno adicional y su centro de 240 × 160 coincidió
píxel a píxel con el control. La reconstrucción coincidió con 600 muestras por
capa en los fotogramas 7200, 8000 y 9599 del recorrido del tutorial.

La comparación nueva de 18.000 fotogramas enfrenta un control de 240 × 160
con el prototipo de 356 × 160 usando la misma partida nueva y las mismas entradas.
El informe `validation/widescreen-regression-before-sync.json` conserva el
resultado anterior a la sincronización de objetos: las 30 capturas de memoria
coinciden byte a byte y el centro de la imagen final coincide píxel a píxel.
Los dos recorridos registran exactamente los mismos 448 accesos no mapeados,
con las mismas direcciones y valores. Esto demuestra que esos avisos no fueron
introducidos por el dibujo ampliado en este recorrido; no demuestra que sean
inofensivos ni valida el juego completo.

El resolvedor de objetos conserva los índices originales durante la compactación
de OAM. Solo extiende objetos cuyos atributos y coordenadas completas coinciden;
los demás conservan el recorte original. La lectura directa al comienzo del
fotograma resultó insuficiente, porque la lista temporal puede estar preparando
el siguiente. El observador de `080004A0` captura las posiciones antes de
compactar, sin modificar registros ni memoria del juego. El consumidor vuelve
a contrastar los atributos con OAM y descarta los datos al cambiar de época de
estado. No sustituye otros observadores instalados por el motor.

Las pruebas independientes cubren compactación con huecos, posiciones negativas,
la ambigüedad +272/−240, datos discordantes, reinicio y capacidad de 128 objetos.
Se ejecutan con AddressSanitizer y UndefinedBehaviorSanitizer. LeakSanitizer se
desactiva por las limitaciones de inspección de procesos del entorno.

## Plan histórico previo a la integración Android

El prototipo planteó completar posiciones, ampliar límites de dibujo y
aparición, probar escenas especiales e integrar el ajuste y el ancho en
Android. 0.10 completa la integración y añade retorno conservador a Original;
la cobertura completa, la ampliación de aparición y las mediciones físicas
continúan abiertas. El alcance actual se detalla al principio de esta guía.

## Puntos de investigación para continuar

SMA3 USA, SHA-1 `7352d2bd064d9ebaec579e264228aa21c7345b80`:

| Dato o rutina | Dirección | Uso |
| --- | --- | --- |
| Puntero del mapa del nivel | `03007010` → `0200000C` | 0x200 bytes por pantalla |
| Índices de pantalla | `0201B800` | Índice YX; filtrar bits bajos 0–5 |
| Punteros de bloques de terreno | `081BAD20` | Índice por byte alto del bloque |
| Máscaras de capa frontal | `081BC444` | Bits 3–0, una pieza de 8 × 8 por bit |
| Mapas completos de fondos | `0201BC00`, `0201DC00` | Bloques de 16 × 16 |
| Punteros de fondos comprimidos | `081675E4`, `0816766C` | Tamaño descomprimido en cabecera |
| Cámara / copias del fotograma | `030069D4–69F3` | Desplazamientos X e Y |
| Estado / modo de juego | `03006B05`, `03006D64` | Juego normal: estado 0D, modo 00 |
| Filtro de dibujo de objetos pequeños | `0804CE30`, `0804CE36` | X+16 comparado con 255 |
| Búsqueda de objetos para aparecer | `08000868–08000914` | Escaneo de filas y columnas |
| Límites de aparición | `0817209E`; lector `0804E710` | +288 / −48 originales |

`0202C8B0` conserva coordenadas completas por índice de la lista temporal
`03005A00`, no por índice final de OAM. `080004A0` compacta 256 posiciones,
omitiendo Y=160, hacia `0201A800`. La captura debe realizarse antes de esa
compactación; leer la lista al comenzar el fotograma no garantiza sincronía.
El límite de salida del resolvedor es 128 objetos. No amplía aún la aparición
ni los límites de dibujo que aplica el propio juego.

## Reproducir

Se requieren los archivos generados privados y los activos propios compatibles,
además del motor fijado por el proyecto. No se suben ROM, BIOS, capturas de RAM,
partidas, claves de firma ni archivos generados del juego.

```sh
cmake -S . -B build -DSMA3_WIDESCREEN_DIAGNOSTICS=ON
cmake --build build --target sma3_runner -j2
python3 validation/run_widescreen_probe.py --rom /ruta/juego.gba --bios /ruta/bios.bin --output /ruta/prueba-nueva
# Para la comparación larga: usar --frames 18000 en ambos recorridos.
# Control: --generic --width 240; candidato: --width 356.
# Los avisos heredados hacen terminar el ejecutor con código 1; revisar el informe.
python3 validation/compare_widescreen_runs.py /ruta/control /ruta/candidato --report /ruta/informe.json
g++ -std=c++20 -Wall -Wextra -Werror -fsanitize=address,undefined -g validation/widescreen_tiles_test.cpp -o /tmp/widescreen_tiles_test
ASAN_OPTIONS=detect_leaks=0 /tmp/widescreen_tiles_test
```

Referencia de ingeniería: [sma3-disasm](https://github.com/KarisaAdvynia/sma3-disasm),
especialmente `LevelCode.asm`, `CodeStart.asm` y `SpriteShared08049E80.asm`.

El parche `gbarecomp-widescreen-objects.patch` añade el recorte por objeto al motor;
`gbarecomp-widescreen-window.patch` añade la máscara lateral autorizada.
En las fases anteriores a 0.10 esos callbacks solo se activaban desde el
diagnóstico. Desde 0.10 los habilita también la opción de producción
`SMA3_PANORAMA`; el parche `gbarecomp-panorama-android.patch` completa la
integración y la corrección de restauración. El aplicador verifica la revisión
del motor y rechaza
cambios locales que no correspondan a una etapa reconocida de los parches.

## Resultado del observador sincronizado

`validation/widescreen-regression.json` registra otra comparación completa de
18.000 fotogramas con el observador activo: 30 capturas idénticas al control,
centro final idéntico y los mismos 448 avisos heredados. Los contadores de fallos
de despacho, instrucciones interpretadas y reparación dinámica permanecen en cero.
Objetos resueltos en los fotogramas 7200, 8000, 9599, 12000, 16000 y 17999:
6, 6, 1, 3, 3 y 3 respectivamente. **La cobertura sigue siendo parcial.**
Esto valida la ausencia de cambios observados en ese recorrido, no todos los
objetos ni todas las transiciones. En aquella fase todavía no se había generado un APK panorámico.

## Selección coherente de OAM

Ahora se compara la lista completa de atributos con OAM antes de aceptar sus
coordenadas. Se conservan hasta tres envíos recientes, se elige un único envío
completo y se vacía el historial al cambiar la época del estado. No se mezclan
coincidencias de objetos de distintos envíos. Los atributos de objetos sin
coordenadas verificadas también intervienen en la comprobación; esos objetos
siguen recortados a la vista original.

`validation/widescreen-coherent-regression.json` registra 18.000 fotogramas:
30 capturas idénticas al control, centro final idéntico, despacho nativo sin
fallos y los mismos 448 accesos no mapeados heredados. En las seis muestras se
eligió el envío más reciente; este recorrido no demuestra que los otros dos
sean necesarios. La prueba independiente comprueba además el rechazo de una
lista con un atributo discordante.

| Fotograma | Objetos OAM activos | Posiciones verificadas |
| --- | ---: | ---: |
| 7200 | 23 | 6 |
| 8000 | 17 | 6 |
| 9599 | 16 | 1 |
| 12000 | 10 | 3 |
| 16000 | 10 | 3 |
| 17999 | 10 | 3 |

Una prueba adicional observando X/Y en `080007A8` antes de dibujar produjo la
misma cobertura en las seis muestras. Se descartó ese añadido por no resolver
los objetos restantes. Los activos incluyen componentes de personajes y HUD;
no todos deben extenderse a los márgenes. Queda identificar sus rutinas de
dibujo y ampliar los límites de los objetos pequeños y de aparición, después
validar transiciones y Android. En aquella fase la panorámica todavía no estaba lista para un APK.

## Límite experimental de dibujo de objetos pequeños

Se añadió un parche optativo de las constantes de `0804CE30` y `0804CE36`.
El intervalo original X=[−16,239] se conserva por defecto. Con una vista de
356 píxeles y 58 adicionales por lado, el diagnóstico usa X=[−74,297]. Las
operaciones que calculan las banderas de CPU reciben los mismos operandos
que las operaciones aritméticas modificadas. No se cambia el límite vertical.

El script `tools/patch_widescreen_bounds.py` comprueba el SHA-256 del fragmento
generado original, admite repetir su aplicación y rechaza otras modificaciones.
Los archivos generados siguen siendo privados. El parche requiere reconstruir
el archivo de código generado; activar solo la variable de entorno no modifica
un archivo precompilado anterior. El ejecutor de diagnóstico acepta
`--object-bounds` y `--replay`, y el reconstructor acepta `--object-bounds`.

```sh
python3 tools/rebuild_host.py --object-bounds --cmake /ruta/cmake
python3 validation/run_widescreen_probe.py --rom /ruta/juego.gba --bios /ruta/bios.bin --output /ruta/nueva --frames 18000 --width 356 --object-bounds
```

El código sigue desactivado en Android. `widescreen-bounds-default-regression.json`
comprueba que los valores por defecto conservan el resultado del control anterior.
`widescreen-bounds-expanded-regression.json` compara el ancho original con el
ampliado: 30 capturas y centro final idénticos en el recorrido original. La prueba
instrumentada posterior detectó cero entradas al filtro en ese recorrido: por
ello estos resultados **no demuestran todavía el dibujo de objetos adicionales**.
Se añadió `panorama-long-jumps.csv` para investigar un recorrido distinto; no se
considera una prueba de cobertura solo por terminar sin errores.

Resultado del segundo recorrido: `widescreen-long-jumps-regression.json` pasa
la comparación de 18.000 fotogramas (30 capturas y centro final idénticos).
Registra 1.568 accesos no mapeados en ambos lados, con las mismas direcciones y
valores, frente a 448 del primer recorrido. No son fallos nuevos del parche,
pero siguen pendientes de explicación. `widescreen-bounds-coverage.json`
confirma cero llamadas observadas a `0804CE1C` en ambos recorridos: el parche
está preparado y compilado, pero su efecto de ampliar objetos no está validado.
No se debe activar en una entrega Android basándose en estas pruebas.

## Pruebas cortas desde estados guardados

`validation/run_panorama_session.py` inicia su propio servidor de diagnóstico
local, carga un estado privado y aplica una lista acotada de entradas. Valida
los hashes de ROM/BIOS, permite como máximo 7.200 fotogramas por sesión y guarda
una captura y posición de Yoshi por acción, además del estado final. Cierra su
proceso al terminar o fallar. Los archivos de salida deben permanecer privados.
Requiere Python y Pillow, además del ejecutable Linux compilado.

```sh
python3 validation/run_panorama_session.py --rom /ruta/juego.gba --bios /ruta/bios.bin --state /ruta/estado.state --actions validation/panorama-hill-route.json --output /ruta/nueva
```

El estado privado inicial se obtuvo mediante las entradas originales hasta el
fotograma 6000, durante el mensaje inicial. Las sesiones cortas verificaron que
A produce el salto y que retroceder permite tomar altura en la pendiente.
`panorama-session-progress.json` conserva sus resultados: se alcanzó X≈860,
superando el tramo previo, y después X≈993, donde falta resolver otro desnivel.
`panorama-hill-route.json` reúne las acciones realizadas hasta X≈860; incluye
intentos fallidos conservados para investigación. Las sesiones se ejecutaron
por segmentos con restauraciones entre ellos; la lista concatenada todavía no
ha sido reproducida de una sola vez. No demuestra un recorrido completo, la
activación del filtro de objetos pequeños ni funcionamiento en Android.

## Posiciones durante animaciones — 7 de octubre de 2026

El observador conserva ahora el índice de origen de cada objeto y registra
las coordenadas completas de los componentes de Yoshi, sus dos emisores por
franjas, el emisor genérico y los ayudantes de transformación afín. Reconstruye
las posiciones a partir de los parámetros y las tablas de animación; no decide
el signo de X a partir de los nueve bits de OAM. Las observaciones se descartan
tras cada compactación y al restaurar un estado.

Se corrigió además `080D8CB4`: después de la transformación afín, esa rutina
ajusta Y según la escala. La predicción anterior dejaba de coincidir con el
objeto real. El observador reproduce ese ajuste vertical y conserva la X
verificada. Antes de admitir cualquier posición, exige que los tres atributos
completos coincidan con la lista terminada y, después, con el fotograma mostrado.
Los objetos no reconocidos conservan el recorte original.

Rutinas cubiertas por los observadores:

| Rutina | Observación |
| --- | --- |
| `08042D28` | Componentes de la animación de Yoshi y orientación |
| `08041CBC` | Matriz afín y desplazamiento de doble tamaño de Yoshi |
| `0804211C`, `080421A8` | Emisión de franjas de la animación |
| `080007A8` | Coordenadas completas del emisor genérico |
| `0804CAB8`, `0804CB64` | Objeto afín a partir de su ancla de pantalla |
| `080D8CB4` | Ajuste vertical posterior según la escala |

Pruebas independientes con ASan y UBSan: coordenadas negativas, ambigüedad
+272/−240, límites de slots, volteo, matrices, doble tamaño, ajuste vertical y
rechazo de atributos que no coinciden. LeakSanitizer permanece desactivado por
la limitación del entorno indicada arriba. Resultado en
`validation/widescreen-provenance-test.txt`.

La comparación final de 18.000 fotogramas conserva las 30 capturas de memoria,
el centro de 240 × 160 píxel a píxel y los mismos 448 avisos de acceso no mapeado
del control. Los contadores de fallos de despacho, instrucciones interpretadas
y recuperación nativa permanecen en cero. Informe:
`validation/widescreen-provenance-regression.json`.

En los fotogramas 12000, 16000 y 17999 se verifican los 10 objetos activos,
frente a 9 antes del ajuste vertical y 3 antes de observar las piezas de Yoshi.
Las primeras tres muestras mantienen objetos sin resolver; entre ellos hay
componentes del HUD y del mensaje inicial. El desglose sin datos del juego está
en `validation/widescreen-provenance-coverage.json`.

**El paso 1 sigue pendiente de cobertura completa.** Resolver todos los objetos
de estas tres capturas no demuestra todas las escenas, enemigos o efectos.
Tampoco demuestra que el juego genere objetos fuera de sus límites originales;
esa ampliación sigue siendo el paso 2. En aquella fase todavía no se había generado un APK panorámico.

## Cobertura continua: interfaz, lengua y Toadies

Se añadieron observadores específicos para distinguir el HUD, los indicadores
limitados a la pantalla y los componentes de los mensajes. Esta clasificación
requiere observar su rutina productora y comprobar los tres atributos finales;
no depende únicamente del número de slot. Estos elementos conservan el recorte
nativo, incluso si un metadato antiguo coincide accidentalmente con ellos.

La revisión de **cada fotograma de juego normal admitido** detectó casos que
no aparecían en las seis capturas anteriores. Se corrigieron:

- El indicador para avanzar el mensaje (`080E9124`).
- La punta horizontal y vertical de la lengua (`08042380`, `0804244C`).
- Las piezas de los Toadies que se llevan a Baby Mario: posición base, traslación
  de cada integrante, piezas compartidas y cambios de orientación (`0804F44A`,
  `0804F51C`, `0804F5AC`).

Los observadores de interfaz cubren además `0802D0CC`, `080DFDC2` y `080E98A0`.
Los tests con ASan/UBSan incluyen reutilización de slots, plantillas truncadas,
punteros fuera de rango, coordenadas negativas y traslaciones de objetos compuestos.
Los resultados están en `widescreen-ui-test.txt` y `widescreen-components-test.txt`.

Dos recorridos de 18.000 fotogramas aprobaron la comparación contra sus respectivos
controles de 240 × 160: 30 capturas de memoria idénticas por recorrido y centro
final idéntico píxel a píxel. Mantienen respectivamente 448 y 1.568 avisos de
accesos no mapeados del control, sin fallos nuevos de despacho ni ejecución
interpretada. Informes: `widescreen-components-regression.json` y
`widescreen-components-jumps-regression.json`.

| Cobertura de posiciones | Recorrido original | Recorrido con saltos |
| --- | ---: | ---: |
| Fotogramas con posiciones sin resolver antes de estas correcciones | 1.604 | 2.471 |
| Fotogramas con posiciones sin resolver después | 44 | 170 |
| Fotogramas con objetos sin resolver que pueden intersectar la pantalla verticalmente | 0 | 0 |
| Fotogramas de juego admitido sin una lista coherente de objetos | 0 | 0 |

Los 44 y 170 casos restantes son objetos fuera del área vertical visible.
**Sus coordenadas X siguen sin resolverse** y mantienen el recorte original.
El diagnóstico de visibilidad respeta el tamaño, el ajuste vertical de OAM y el
doble tamaño afín; no cambia su dibujo ni los declara resueltos.

`validation/report_object_coverage.py` genera el desglose y rechaza registros
antiguos o incompletos. Los informes son `widescreen-visible-coverage.json` y
`widescreen-visible-jumps-coverage.json`. El segundo recorrido atraviesa escenas
no admitidas por el prototipo: las muestras 8000 y 16000 quedan fuera de esta
cobertura y mantienen el comportamiento de bordes existente.

Estos resultados cierran los casos visibles encontrados en ambos recorridos.
**No completan el paso 1 para todo el juego:** quedan otros niveles, efectos y
escenas por cubrir. No amplían por sí mismos los límites de aparición del juego,
no prueban rendimiento en el Pixel 7 Pro y no constituyen un APK panorámico.

## Ciclo completo de coche — 18 de octubre de 2026

El control de 240 píxeles de ancho y el candidato de 356 completaron el mismo
recorrido nativo de **3.506 fotogramas**. La secuencia observada en cada fotograma
fue exactamente **0 → 2 → 0** y terminó con **1.200 fotogramas consecutivos de
Yoshi normal, en juego y sin botones pulsados**. El resultado
`complete_cycle_pass` es `true` en el nuevo
[informe del ciclo de coche](../validation/widescreen-car-cycle.json).

| Comprobación | Resultado |
| --- | --- |
| Fotograma inicial / final | 6.599 / 10.105 |
| Primera transformación en coche | Fotograma 7.535; 936 ejecutados desde el inicio |
| Primera recuperación de Yoshi normal | Fotograma 8.815; 2.216 ejecutados desde el inicio |
| Desplazamiento a la derecha desde la recuperación hasta el final | 58,83984375 píxeles; mínimo exigido 32 |
| Imágenes centrales RGB de 240 × 160 idénticas | 61 pares |
| Comparaciones de estado, incluida la final | 62 pares |
| Fotogramas con huecos de posición, posiciones visibles sin resolver o envíos incoherentes | 0 / 0 / 0 |
| Fallos de despacho, instrucciones interpretadas o reparaciones nativas | 0 / 0 / 0 |

El estado de entrada es sintético e idéntico al utilizado para la prueba
histórica de coche. La transformación se activa mediante entradas del juego y
la burbuja original; no se fuerza el campo de transformación ni se restaura un
estado durante el recorrido. La ruta calibrada conserva las primeras siete
acciones, mueve a la izquierda durante 105 fotogramas, espera como máximo 2.400
hasta detectar el primer fotograma normal, avanza 90 a la derecha y termina
con 1.200 de espera. Su presupuesto máximo es de 5.265 fotogramas.

Las 61 imágenes comparan el centro del candidato panorámico con la imagen
original en puntos de captura; no son una comparación de imágenes de todos
los fotogramas. Las 62 comparaciones de estado cubren CPU, memoria del juego,
E/S, audio, guardado y metadatos, conservando la excepción acotada a ocho bytes
de reloj RTC inactivo. Incluyen 61 puntos de captura y un guardado final en el
mismo fotograma que la última captura. El estado PPU dependiente del área de presentación se
excluye. La traza de transformación, entradas y movimiento sí se comprueba en
cada fotograma.

El resultado del **8 de octubre** permanece intacto en
`validation/widescreen-car.json`: 3.360 fotogramas, 38 imágenes centrales y
secuencia 0 → 2 → 0 → 2. La revisión del **9 de octubre** añadió el contrato de
ciclo completo y superó sus pruebas sintéticas, pero registró la ejecución
nativa como pendiente. La ejecución del 18 de octubre completa esa fase para
la ruta calibrada; no cambia las conclusiones de los informes anteriores.
La calibración y los criterios se detallan en
[CAR_VALIDATION.md](../validation/CAR_VALIDATION.md).

La [evidencia de compilación nativa](../validation/car-cycle-native-build.json)
acompaña al resultado. Se corrigió el reconstructor de Linux para incluir
`SMA3_WIDESCREEN_DIAGNOSTICS=ON`, necesario para exponer la interfaz de las
pruebas. No se modificaron el motor ni el renderizador del coche para esta
validación, ni se repitieron pruebas con sanitizadores o de rendimiento Android.

Este resultado valida un ciclo acotado desde una entrada sintética. La espera
final demuestra que no hubo reactivación durante esos 1.200 fotogramas; no
establece seguridad permanente bajo otras entradas. **No completa la cobertura
de niveles, jefes ni todas las transformaciones.** La ampliación lateral de
aparición de objetos sigue desactivada. Aquella validación por sí sola no generó un APK; la panorámica real sigue
ausente de Android 0.9 y 0.9.1.

## Avance del castillo 1-4 — 18 de octubre de 2026

La nueva ruta continua desde la misma entrada sintética supera el bloqueo
histórico en X=1872.99609375, cruza el primer muro y utiliza el elevador.
Termina en X=2662.93359375 dentro del corredor inferior: **789.9375 píxeles
más adelante**, con tres vidas, cero muertes y Baby Mario sobre Yoshi.
El [informe del avance](../validation/widescreen-castle-progress.json) y su
[explicación reproducible](../validation/CASTLE_PROGRESS_VALIDATION.md)
documentan el criterio y sus límites.

| Comprobación | Resultado |
| --- | --- |
| Ruta nativa en 240 × 160 y 356 × 160 | 2.238 fotogramas por ejecución |
| Imágenes centrales RGB idénticas | 73 pares |
| Comparaciones de estado, incluida la final | 74 pares |
| Espera final estable, sin entradas y con el bebé seguro | 337 fotogramas consecutivos |
| Huecos brutos / visibles sin resolver / envíos incoherentes | 0 / 0 / 0 |
| Fallos de despacho / instrucciones interpretadas / reparaciones nativas | 0 / 0 / 0 |

El recorrido ampliado descubrió 132 fotogramas con posiciones visibles sin
resolver en las dos paletas del sprite `0x13C`. La corrección observa sus
coordenadas completas después del dibujo y de los ajustes afines, y conserva
la comprobación de los atributos finales. La repetición eliminó esos huecos
sin modificar la trayectoria ni el estado del juego. El
[resultado anterior a la corrección](../validation/widescreen-castle-progress-before-fix.json)
permanece separado y rechazado por su cobertura insuficiente.

La [comparación antes/después](../validation/widescreen-castle-fix-effect.json)
confirma cambios reales de las paletas en los márgenes de tres capturas,
sin cambios en los centros ni en los estados comparados. La
[regresión del coche](../validation/widescreen-car-post-castle-fix.json)
también vuelve a superar el ciclo completo con el mismo binario corregido.

Las imágenes y los estados se comparan en puntos de captura; el guardado final
comparte fotograma con la última captura. Las entradas, el movimiento y la
salud sí se observan en cada fotograma. La excepción de comparación de estado
sigue limitada al reloj RTC inactivo y al estado de presentación PPU descritos
en la documentación de la prueba.

La ruta termina antes de la siguiente pared, situada en X=2720. **Todavía no
acredita una transición de sala natural ni completar el castillo.** El cambio
de subnivel de la inicialización sintética se registra, pero no cuenta como
transición jugada. La aparición lateral ampliada sigue desactivada y esta
validación de escritorio no añade panorámica a Android 0.9 ni 0.9.1.

## Una transición natural por tubería del castillo 1-4

La continuación conserva las 37 acciones del avance anterior y se reproduce
de una sola vez desde la misma entrada sintética, sin restaurar estados durante
el recorrido. La ruta completa de **65 acciones y 3.907 fotogramas** atraviesa
la tubería del extremo derecho del corredor inferior y regresa al juego en la
zona superior del **nivel 3/subnivel 3**.

El identificador de subnivel permanece en 3. La prueba reconoce la transición
por la secuencia **13 → 9 → 10 → 11 → 13**, el índice de salida 1016 y la
reubicación de Yoshi en **X=368, Y=1024** durante la carga. Termina en
X=368, Y=1152.99609375, con velocidad cero y Baby Mario sobre Yoshi.
El [informe agregado](../validation/widescreen-castle-transition.json) supera
`complete_castle_transition_pass` y `natural_room_transition_qualified`.

| Comprobación | Resultado |
| --- | --- |
| Ejecuciones de 240 × 160 y 356 × 160 | 3.907 fotogramas y 3.908 observaciones por sesión |
| Imágenes centrales RGB idénticas | 133 pares |
| Comparaciones de estado, incluida la final | 134 pares |
| Espera final estable, sin entradas y con el bebé seguro | 354 fotogramas consecutivos |
| Vidas iniciales / finales; muertes; episodios de separación del bebé | 3 / 3; 0; 0 |
| Huecos brutos / visibles sin resolver / envíos incoherentes | 0 / 0 / 0 |

Las entradas, el movimiento, la escena y la salud se comparan en cada fotograma.
Las imágenes y los estados se comparan en los puntos programados; el guardado
final comparte fotograma con la última captura. Los criterios, la procedencia
y las excepciones de comparación están en el
[informe de validación y reproducción](../validation/CASTLE_TRANSITION_VALIDATION.md).

Este hito acredita **una sola transición por tubería**, no el castillo completo
ni otras salidas. La aparición lateral ampliada sigue desactivada. Es una
validación de escritorio: **los APK Android 0.9 y 0.9.1 siguen sin panorámica
real** y esta prueba no aporta una medición de FPS.
