# Una transición natural del castillo 1-4 — 19 de octubre de 2026 (UTC)

## Resultado y alcance

La ruta continua de **65 acciones y 3.907 fotogramas** supera la pared posterior
al corredor inferior, utiliza el trampolín y las plataformas sobre lava, y entra
por la tubería del extremo derecho. La salida conduce a otra zona del mismo
**nivel 3/subnivel 3**. Las ejecuciones de escritorio a **240 × 160 y 356 × 160**
superan la comparación y el contrato de transición natural.

El [informe final](widescreen-castle-transition.json) acredita esta ruta y una
sola salida. La primera entrada sigue siendo el fixture sintético revisado;
la transición posterior se realiza mediante entradas de mando. Cada ejecución
de calificación carga el estado una vez al iniciar y continúa sin restauraciones
intermedias ni alteraciones artificiales de posición, colisiones o memoria del juego.

La panorámica real continúa fuera de los APK Android 0.9 y 0.9.1. El ajuste de
Android para conservar **3:2**, correspondiente a los 240 × 160 píxeles de GBA,
mantiene el campo de visión original. Esta prueba de escritorio amplía el área
dibujada; no constituye una nueva compilación Android ni una medición de FPS.

## Continuación de la ruta revisada

La [secuencia final](panorama-castle-transition-actions.json) conserva literalmente
las primeras **37 acciones y 2.238 fotogramas** del
[avance anterior](CASTLE_PROGRESS_VALIDATION.md). El nuevo contrato vuelve a
comprobar el primer muro, el elevador y el reposo en el corredor inferior antes
de aceptar la continuación. También conserva los primeros 2.989 fotogramas de
la primera repetición de esta fase.

El trampolín permite superar la siguiente pared, cuyo lado izquierdo está en
X=2720. La continuación cruza las plataformas sobre lava hasta el último apoyo.
Una pausa antes del salto final evita el enemigo situado cerca de X=3808; la
ruta pasa por el anillo intermedio, aterriza sobre la tubería y pulsa Down.
El contador interno de estrellas termina en 209 décimas, frente a 109 al inicio.
Esa variación se registra por separado de vidas, muertes y estado del bebé.

La calibración privada ayuda a encontrar entradas, pero sus restauraciones no
constituyen evidencia de continuidad. La aceptación depende de reproducir la
secuencia íntegra desde el fixture original en cada una de las dos anchuras.

## Primer intento rechazado

La primera ruta, de 3.793 fotogramas, terminó con **127 pares de centros iguales,
128 comparaciones de estado equivalentes y cero huecos de posición**. Sin
embargo, Yoshi recibió un golpe en el fotograma relativo 3.017; Baby Mario quedó
separado durante 629 observaciones y se recuperó en 3.646. La ruta terminó en
X=3926.65234375, Y=1888.99609375, sin seleccionar la salida ni entrar en la tubería.

El [informe rechazado](widescreen-castle-transition-first-replay.json) mantiene
`differential_pass=true` y `coverage_pass=true`, pero
`complete_castle_transition_pass=false`. Así queda documentado un caso real en
el que la igualdad entre resoluciones no demuestra que se haya completado el
recorrido. También se conservan sus
[acciones](panorama-castle-transition-first-replay-actions.json) y su
[contrato](castle-transition-first-replay-expectations.json), sin sustituirlos
por los de la ruta aprobada.

En la calibración restaurada, el enemigo `0x080` tenía una altura y gravedad
distintas de las observadas en la repetición completa. Su rutina selecciona la
gravedad mediante un valor del RNG del juego. Esto explica la diferencia de
posición del enemigo; esta prueba no determina la causa completa de la
divergencia temporal tras restaurar. No se fuerza ese valor ni se modifica al
enemigo. La recalibración parte de un checkpoint obtenido del recorrido
completo, y la nueva secuencia se valida después desde la entrada original.

## Evidencia de una salida jugada

El [contrato final](castle-transition-expectations.json) exige una única secuencia
**13 → 9 → 10 → 11 → 13** después del calentamiento y del prefijo revisado. Un
cambio de coordenadas o los estados de la entrada sintética no bastan para
aprobar. La salida seleccionada debe ser **1016**, correspondiente a la pantalla
127, y debe conservarse hasta el final.

| Evidencia | Observación de la ruta final |
| --- | --- |
| Inicio de la salida, estado 9 | Fotograma relativo 3.374; Down efectivo y movimiento descendente continuo dentro de la boca de la tubería |
| Desvanecimiento de salida | Estado 9 durante 48 observaciones, de 3.374 a 3.421 |
| Inicio de la carga, estado 10 | Fotograma 3.422; la posición de entrada persiste hasta la inicialización |
| Posición escrita por el cargador | X=368, Y=1024 desde 3.423; 31 observaciones consecutivas dentro del estado 10 |
| Entrada de destino, estado 11 | Fotogramas 3.454 a 3.520; 67 observaciones |
| Retorno al juego normal, estado 13 | Fotograma 3.521, dentro de la zona superior de destino |
| Reposo final sobre el suelo | X=368, Y=1152.99609375; 354 fotogramas consecutivos con velocidad cero y sin botones |

La duración del estado 10 es de 32 observaciones. El contrato admite intervalos
acotados para las fases de salida y entrada, en lugar de exigir que cualquier
evidencia válida copie exactamente estas tres duraciones. Comprueba que el
tipo de entrada sea 1 durante los estados 9 y 10, y 0 antes y después; la carga
debe permanecer en (368,1024) hasta el retorno previsto.

El nivel y subnivel siguen siendo 3 porque la tubería enlaza dos zonas de ese
subnivel. La transición se demuestra mediante las entradas, el índice de
salida, la secuencia de estados, la carga de coordenadas y el retorno jugable.
Los estados iniciales 10 y 12, y el subnivel heredado 57 de la observación cero,
pertenecen a la entrada sintética y no se contabilizan como otra salida natural.

Las direcciones y su interpretación se contrastaron con
[sma3-disasm, revisión c8532ec9](https://github.com/KarisaAdvynia/sma3-disasm/tree/c8532ec9a8d0038c3bfeb003dd0c7ea89d7e1071):
`Sub0804429C` comprueba la entrada por tubería, `Sub0804189C` selecciona la salida,
y el cargador escribe X/Y en `0802C8BA` y `0802C8CA`. La lógica del trampolín está
en `Sub08064190`; la selección de gravedad del enemigo, en `Sub08062EB8`.
Estas referencias explican los campos observados; no se parchean para crear
la evidencia de transición.

## Comparación nativa final

| Comprobación | Resultado por pareja |
| --- | --- |
| Resoluciones | 240 × 160 y 356 × 160 |
| Acciones / fotogramas ejecutados por sesión | 65 / 3.907 |
| Observaciones por sesión, incluida la inicial | 3.908 |
| Fotogramas absolutos inicial / final | 6.599 / 10.506 |
| Imágenes centrales RGB idénticas | 133 de 133 pares |
| Comparaciones de estado equivalentes | 134 de 134 |
| Huecos brutos / huecos visibles / envíos incoherentes | 0 / 0 / 0 fotogramas |
| Vidas iniciales / finales; muertes | 3 / 3; 0 |
| Episodios de separación de Baby Mario en juego normal | 0 |
| Reposo final seguro y estable | 354 fotogramas; mínimo exigido 300 |
| Fallos de despacho / instrucciones interpretadas / reparaciones nativas | 0 / 0 / 0 en ambas sesiones |
| `complete_castle_transition_pass` | `true` |
| `natural_room_transition_qualified` | `true` |

La traza de entradas, movimiento, escena y salud se compara en cada fotograma.
Las imágenes y estados se comparan en los puntos programados: cambios de
escena, finales de acción y cada 60 fotogramas. La comparación de estado número
134 es el guardado final y comparte fotograma con la última captura.

Se comparan CPU0, BUS0, IO_0, AUD0, SAV0 y META. La excepción permanece limitada
a ocho bytes del reloj RTC inactivo; se excluye PPU0 por depender del área de
presentación y se comprueba por separado el centro RGB de 240 × 160. En la
captura de 356 píxeles, ese centro comienza 58 píxeles después del borde izquierdo.
Por tanto, los recuentos de imágenes no significan que se hayan comparado todas
las imágenes producidas por el juego.

Los límites globales de coordenadas del JSON contienen todos los valores raw
observados, incluidos los del prefijo. Por ejemplo, `y_max_pixels` alcanza
132928.796875 en un registro de ese prefijo: no representa la altura física del
escenario. La evidencia de la tubería usa sus propias ventanas de coordenadas,
continuidad descendente, carga y zona final, comprobadas en los fotogramas
correspondientes.

La [procedencia nativa](castle-transition-native-provenance.json) identifica el
binario de escritorio reutilizado, su configuración, las fuentes, el motor y
el fixture. El hash del ejecutor coincide con el de la compilación documentada
en [la fase anterior](castle-progress-native-build.json). No fue necesario
reconstruirlo para resolver esta ruta: el cambio está en las entradas y en los
criterios de validación.

## Salud, pruebas del validador y publicación

El perfil `castle-status-v1` registra la salud en cada fotograma. El nuevo
contrato rechaza descensos de vidas, vidas negativas, cualquier cambio o valor
no nulo del contador de muertes, inhibición de ese contador y cualquier
separación de Baby Mario durante el juego normal, aunque luego se recupere.
Durante el estado 9 exige al bebé sobre Yoshi, identificado como 97/5; en
10 y 11 permite además el marcador vacío 0/0 del cargador.

Las pruebas sintéticas comprueban tanto el contrato como el agregador: igualdad
sin salida, coordenadas de retorno falsas, fases omitidas o repetidas, Down
ausente, salida equivocada, carga incompleta, salud transitoriamente incorrecta,
recuperación del bebé después de un golpe y reposo insuficiente. También
rechazan sesiones incompletas, capturas omitidas, anchuras incorrectas,
procedencia diferente, contadores nativos no nulos y contratos debilitados.
El [registro de comprobaciones](castle-transition-checks-2026-10-19.txt)
separa estas pruebas sintéticas de las ejecuciones reales del juego.

Los agregados públicos contienen contratos, mediciones y hashes. No incluyen
ROM, BIOS, código generado del juego, APK, ejecutor, partidas, imágenes ni las
trazas completas de las sesiones privadas.

## Reproducción

Se necesitan los activos propios compatibles, las fuentes generadas privadas
y el motor fijado por el proyecto. El estado inicial debe tener SHA-256
`02f131a013d8bc2c352b05502ce0f420136e6736c8b4864cbfb51c4307de11f2`.
La huella canónica de la secuencia aprobada es
`e87a2b74b03b8ac9871254617ab2d7a3698f54a966f4b53f55a51aa0e5c12d93`.

```sh
cmake -S . -B build -DSMA3_WIDESCREEN_DIAGNOSTICS=ON
cmake --build build --target sma3_runner -j2
python3 validation/run_panorama_session.py \
  --rom /ruta/juego.gba --bios /ruta/bios.bin --state /ruta/castillo.state \
  --actions validation/panorama-castle-transition-actions.json \
  --width 240 --castle-status --output /ruta/control-nuevo
python3 validation/run_panorama_session.py \
  --rom /ruta/juego.gba --bios /ruta/bios.bin --state /ruta/castillo.state \
  --actions validation/panorama-castle-transition-actions.json \
  --width 356 --castle-status --output /ruta/panoramica-nueva
python3 validation/compare_castle_transition.py \
  /ruta/control-nuevo /ruta/panoramica-nueva /ruta/informe-nuevo.json \
  --contract validation/castle-transition-expectations.json
```

Los directorios de salida y el informe deben ser nuevos. Para reproducir el
caso rechazado, se usan en las dos sesiones las acciones terminadas en
`first-replay-actions.json` y el contrato `castle-transition-first-replay-expectations.json`.
El comparador debe terminar con código 1 y con la calificación completa en falso.

## Límite y continuación

Esta fase acredita la pared siguiente, el tramo de lava y una tubería hasta
el reposo inicial en el corredor superior. El recorrido posterior de esa zona,
las demás salidas y el jefe quedan pendientes. La aparición lateral ampliada
continúa desactivada. La prueba no certifica el castillo completo, todos los
objetos o transformaciones, restauraciones arbitrarias en caliente, Android
ni rendimiento. El [estado general](../docs/PANORAMICA_EN_DESARROLLO.md) conserva
la distinción entre el ajuste de proporción del APK y el prototipo panorámico.
