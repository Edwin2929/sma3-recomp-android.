# Avance del castillo 1-4 — 18 de octubre de 2026

## Ruta y criterio

La [ruta histórica](CASTLE_ROUTE_VALIDATION.md) ejecutaba 1.920 fotogramas y
terminaba bloqueada en X=1872.99609375. La nueva ruta contiene **37 acciones y
2.238 fotogramas**. Parte del mismo estado de entrada sintético, atraviesa el
primer muro, utiliza el elevador y termina en el corredor inferior posterior.
El estado se carga una sola vez al iniciar cada ejecución de calificación.
La calibración empleó restauraciones privadas; la ruta congelada las sustituye
por una reproducción continua desde la entrada original.

El paso decisivo consiste en aterrizar sobre el escalón antes del muro y
encadenar `A+Right 35 → Right 1 → A+Right 100`. Se observa el desplazamiento real
del personaje; no se alteran su posición, transformación, colisiones ni memoria.
La secuencia completa está en
[panorama-castle-progress-actions.json](panorama-castle-progress-actions.json).

El [contrato](castle-route-progress-expectations.json) exige dos hitos ordenados
en nivel 3/subnivel 3, estado de juego 13 y transformación normal:

| Hito | Criterio mínimo |
| --- | --- |
| Al otro lado del primer muro | X entre 1930 y 2096, Y ≤1825, durante cinco fotogramas consecutivos |
| Después del elevador | X entre 2224 y 2360, Y ≤1825, durante cinco fotogramas consecutivos |
| Final del recorrido | X ≥2600 y 300 fotogramas consecutivos con posición fija, velocidad cero, ningún botón pulsado y Baby Mario sobre Yoshi |

Los límites usan las coordenadas sin redondear. Comparar únicamente
`X > 1872.996` aceptaría por error el antiguo bloqueo exacto
`1872.99609375`. La igualdad entre dos ejecuciones tampoco acredita por sí sola
que ambas hayan avanzado.

## Fallo de dibujo detectado al ampliar el recorrido

La primera comparación terminó con trayectorias y salud idénticas, **73 centros
RGB iguales y 74 comparaciones de estado equivalentes**, pero registró
**132 fotogramas con posiciones visibles sin resolver**. Los dos componentes
del sprite `0x13C`, situados en los slots temporales 37 y 38, carecían de una
posición panorámica verificada entre los fotogramas absolutos 8356 y 8487.
El resultado completo fue rechazado pese a superar el criterio de avance.

Este resultado se conserva en
[widescreen-castle-progress-before-fix.json](widescreen-castle-progress-before-fix.json).
Las imágenes, partidas y trazas completas permanecen privadas.

## Corrección y resultado nativo

El observador de `src/widescreen_rotating.h` obtiene las coordenadas completas
de las dos paletas del sprite `0x13C` a partir de sus anclas, ángulo, tablas
trigonométricas y cámara. Se ejecuta en el retorno común `08075E7A`, después
del dibujo, la asignación de matrices y el ajuste de prioridad. Comprueba
ambos componentes antes de publicar sus posiciones y exige la coincidencia
de sus atributos finales. La compactación vuelve a comprobar esos atributos.
No escribe memoria ni registros del juego.

La repetición con el binario corregido supera el contrato completo. El
[informe final](widescreen-castle-progress.json) registra:

| Comprobación | Resultado |
| --- | --- |
| Resoluciones comparadas | 240 × 160 y 356 × 160 |
| Fotogramas ejecutados / observaciones por sesión | 2.238 / 2.239, incluida la entrada |
| Fotogramas absolutos inicial / final | 6.599 / 8.837 |
| Imágenes centrales RGB idénticas | 73 pares |
| Comparaciones de estado equivalentes | 74 pares |
| Posiciones sin resolver / visibles sin resolver / envíos incoherentes | 0 / 0 / 0 fotogramas |
| Primer hito completado / segundo hito completado | Fotogramas relativos 1.459 / 1.745 |
| Posición final de Yoshi | X=2662.93359375, Y=1952.99609375 |
| Avance desde el bloqueo histórico | 789.9375 píxeles |
| Espera final estable con Baby Mario seguro | 337 fotogramas consecutivos; mínimo exigido 300 |
| Vidas iniciales / finales; muertes; pérdidas del bebé | 3 / 3; 0; 0 |
| Fallos de despacho / instrucciones interpretadas / reparaciones nativas | 0 / 0 / 0 en ambas ejecuciones |
| `complete_castle_progress_pass` | `true` |

Las imágenes se comparan en los 73 puntos programados, no en cada fotograma.
Las 74 comparaciones de estado incluyen esos puntos y un guardado final en
el mismo fotograma que la última captura. La traza de entradas, movimiento,
escena y salud sí se compara en cada fotograma. Los límites globales del JSON
incluyen valores transitorios de la inicialización sintética; no deben
interpretarse como límites del movimiento jugable.

La [evidencia de compilación](castle-progress-native-build.json) identifica el
commit base, las fuentes modificadas y los hashes de ambos binarios. Se
reconstruyó el ejecutor de diagnóstico de escritorio con el mismo motor y
archivo de juego generado. No fue necesario modificar el motor ni regenerar
las funciones del juego.

### Efecto real del cambio y regresión del coche

La [comparación independiente antes/después](widescreen-castle-fix-effect.json)
comprueba por cada ancho la misma traza completa, 73 capturas y 74 estados.
No cambia ningún píxel de las imágenes de control ni del centro de las
panorámicas. Sí cambian **1.129 píxeles sumados entre tres capturas**, todos
dentro de los 58 píxeles adicionales de cada margen:

| Fotograma relativo / absoluto | Margen izquierdo | Margen derecho |
| --- | ---: | ---: |
| 1.758 / 8.357 | 0 píxeles | 339 píxeles |
| 1.860 / 8.459 | 429 píxeles | 0 píxeles |
| 1.878 / 8.477 | 361 píxeles | 0 píxeles |

La inspección de las capturas de 1.758 y 1.860 confirma que las piezas de las
paletas se dibujan más allá del área original. Estas cifras describen las
capturas comparadas, no un recuento de píxeles de todos los fotogramas.

Como el despachador de observadores es compartido, se repitió también el ciclo
completo del coche con el binario corregido. La
[regresión del coche](widescreen-car-post-castle-fix.json) vuelve a superar
`complete_cycle_pass`: 3.506 fotogramas, secuencia `0 → 2 → 0`, 1.200 fotogramas
finales de espera normal, 61 centros iguales, 62 comparaciones de estado y
cero huecos o fallos nativos. No sustituye el informe histórico del ciclo.

Las pruebas C++ del observador pasan con `-Wall -Wextra -Werror`, ASan y UBSan;
LeakSanitizer permanece desactivado por las limitaciones del entorno. Los
validadores pasan sus 66 pruebas tanto en Python normal como con `-O`.
El [registro de comprobaciones](castle-progress-checks-2026-10-18.txt)
separa estas pruebas sintéticas de las sesiones reales del juego.

## Evidencia de salud y continuidad

`--castle-status` registra en cada fotograma vidas con signo, contador de
muertes, inhibición de ese contador, estrellas internas, tipo de entrada,
índice de salida y estado de Baby Mario. Valida el puntero de juego antes de
leer campos relativos. El parser es el mismo al observar el proceso y al
comprobar los estados guardados; los endpoints deben coincidir con la traza.

El contrato rechaza cualquier descenso de vidas respecto al fotograma anterior,
vidas negativas, cualquier cambio o valor no nulo del contador de muertes y
la inhibición del contador. Así, ganar una vida antes de morir no oculta una
pérdida posterior. Tampoco admite estados de muerte o reintento durante la
entrada, y desde el fotograma relativo 300 exige juego normal en la misma zona.
Registra los episodios de separación del bebé; un golpe recuperado no sustituye
el requisito de terminar con Baby Mario seguro.

El contador de estrellas se almacena en décimas. Un HUD rojo con «1» no basta
para concluir que el bebé está perdido: el final histórico tenía Baby Mario
de nuevo sobre Yoshi, tres vidas, cero muertes y 12 décimas internas.

Las direcciones y la interpretación se contrastaron con
[sma3-disasm, revisión c8532ec9](https://github.com/KarisaAdvynia/sma3-disasm/tree/c8532ec9a8d0038c3bfeb003dd0c7ea89d7e1071):
`Sub08035690` para vidas y muertes, `YIGameStatePtrs` para los estados,
`NormalGameplayInit` para la entrada, y `BabyMario_Main` / `SpriteStatus5` para
identificar al bebé sobre Yoshi. No se escriben esos campos durante la prueba.

El ejecutor ahora captura los cambios de nivel, subnivel y estado de juego,
además de transformación, los finales de acción y cada 60 fotogramas. El
comparador calcula los estados observados a partir de toda la traza. El booleano
general `supported_gameplay_only` incluye la entrada sintética y puede ser
`false` aunque el segmento posterior al calentamiento sí esté admitido.
En el contrato del coche esa segunda comprobación se denomina
`cycle_supported_gameplay_only`; sus requisitos originales se conservan.
En el informe del castillo, `route_checks.supported_gameplay_only` corresponde
al segmento desde el fotograma relativo 300; el campo del nivel superior
también incluye los estados de entrada 10 y 12.

## Reproducción

Se necesitan los activos propios compatibles, las fuentes generadas privadas y
el motor fijado por el proyecto. El fixture de entrada debe tener SHA-256
`02f131a013d8bc2c352b05502ce0f420136e6736c8b4864cbfb51c4307de11f2`.
Se puede reconstruir desde la fuente normal compatible con
`make_level_fixture.py --level 3`; el hash del resultado debe coincidir.
La huella canónica de las acciones es
`af302618d4858bbd150047e1db7b6c637e0dd011cf8b4f990ef56eade64e7a37`.

```sh
cmake -S . -B build -DSMA3_WIDESCREEN_DIAGNOSTICS=ON
cmake --build build --target sma3_runner -j2
python3 validation/run_panorama_session.py \
  --rom /ruta/juego.gba --bios /ruta/bios.bin --state /ruta/castillo.state \
  --actions validation/panorama-castle-progress-actions.json \
  --width 240 --castle-status --output /ruta/control-nuevo
python3 validation/run_panorama_session.py \
  --rom /ruta/juego.gba --bios /ruta/bios.bin --state /ruta/castillo.state \
  --actions validation/panorama-castle-progress-actions.json \
  --width 356 --castle-status --output /ruta/panoramica-nueva
python3 validation/compare_castle_progress.py \
  /ruta/control-nuevo /ruta/panoramica-nueva /ruta/informe-nuevo.json \
  --contract validation/castle-route-progress-expectations.json
```

Las salidas y el informe deben ser nuevos. El proceso guarda sus cachés y
diagnósticos dentro del directorio privado de cada sesión. La comparación
completa exige igualdad de entradas y observaciones por fotograma, todas las
capturas programadas, estado de CPU/memoria/E/S/audio/guardado equivalente,
contadores nativos a cero, progreso y **cero huecos de posición brutos**.
La excepción de estado permanece limitada a ocho bytes de reloj RTC inactivo;
el estado PPU dependiente del área de presentación se excluye y se compara
el centro de la imagen por separado.

## Límite de esta fase

La ruta termina en el corredor inferior, antes de la siguiente pared, cuya
cara izquierda está en X=2720. La continuación sobre esa pared no está validada.
La inicialización sintética pasa por los estados 10 y 12 antes de 13; el
subnivel heredado 57 se sustituye por 3 al cargar la entrada. Ese arranque
**no es una transición de sala jugada de forma natural**.

Esta prueba no certifica el castillo completo, jefes, otras transformaciones,
aparición lateral ampliada, Android ni rendimiento. La aparición ampliada
continúa desactivada. La panorámica real sigue fuera de los APK 0.9 y 0.9.1;
el APK comprobado por el mantenedor en un Pixel 7 Pro conserva el campo de
visión original. Véase el [estado general del prototipo](../docs/PANORAMICA_EN_DESARROLLO.md).
