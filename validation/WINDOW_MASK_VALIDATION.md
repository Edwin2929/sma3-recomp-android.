# Máscara de iluminación panorámica — 23 de octubre de 2026 (UTC)

La máscara OBJ-window del prototipo de escritorio se extiende ahora a los
márgenes horizontales con posiciones verificadas. La ruta de las dos tuberías
vuelve a pasar en **240 × 160 y 356 × 160**: mantiene el centro original,
el estado comparado, la salud y la cobertura de posiciones. La sala iluminada
del subnivel 153 deja de mostrar las franjas negras que la política anterior
introducía en los márgenes de ciertas filas.

La base es el commit `820685309644c274deb2095a6174574fdabb48b0`. Sus informes
y capturas anteriores no se sobrescriben. Se reutilizan sus dos sesiones
completadas como referencia antes del cambio y se ejecutan dos sesiones nuevas
del castillo y dos del coche con el nuevo ejecutor.

## Qué cambia y cuándo se permite

El [parche del motor](../patches/gbarecomp-widescreen-window.patch) añade
`g_ws_obj_window_x_provider`. El [adaptador](../src/widescreen.cpp) utiliza
la presentación coherente y las coordenadas completas ya verificadas para
los objetos normales y las copias de ventana. Cada llamada contrasta el índice
y los tres atributos OAM; el motor exige resultado 1, X dentro de un entero
de 16 bits con signo y coincidencia de los nueve bits bajos de X con OAM.

La ampliación solo se autoriza si:

- La fila está dentro de las 160 originales y la escena no está en pillarbox.
- OBJ y OBJ-window están activos; WIN0 y WIN1 están desactivados.
- El adaptador ha publicado el proveedor durante juego normal con una
  presentación completa y coherente.
- Todos los objetos de ventana habilitados que intersectan la fila resuelven
  su posición. Una posición desconocida invalida la máscara lateral completa
  de esa fila, aunque otros objetos ya se hayan dibujado en su almacenamiento.

La máscara nativa y su constructor quedan intactos. El almacenamiento adicional
solo escribe fuera de `[0,240)`, utiliza los texeles originales, sus volteos,
sus matrices afines y el tamaño doble cuando corresponde. Una X completa que
difiera en 512 de su alias OAM no desplaza ni altera la máscara del centro.

Las filas autorizadas consultan los controles reales de WINOUT y OBJ-window
en cada píxel lateral. No copian el valor uniforme del centro: una ventana
puede estar completamente dentro de un margen. Si la fila no está autorizada,
se conserva la política anterior de control uniforme o margen negro.
`g_ws_authored_margin_layers` permanece en 0.

En la sala final se observan `DISPCNT=0x9f00`, `WINOUT=0x173f`,
`BLDCNT=0x3748` y `BLDALPHA=0x0408`. Fuera de las siluetas se permiten las
capas y sus efectos; dentro, el control `0x17` excluye BG3 y el efecto de color.
Por eso la comprobación debe demostrar tanto escenario visible como siluetas
correctas de iluminación. Eliminar el negro de toda la fila no bastaría.

## Recorrido nativo y conservación del juego

El [agregado del castillo](widescreen-castle-window-mask.json) vuelve a evaluar
el contrato publicado de las dos tuberías, sin modificar las acciones,
el fixture, las duraciones ni los criterios de salud. Cada sesión carga
la entrada sintética una sola vez y reproduce los **130 pasos y 6.801
fotogramas** sin restauraciones intermedias.

| Comprobación | Resultado |
| --- | --- |
| Observaciones de entradas, escena, movimiento y salud | 6.802, incluida la inicial |
| Centros RGB comparados entre 240 y 356 | 250, todos idénticos |
| Estados comparados entre anchuras | 251, todos equivalentes |
| Huecos de posiciones brutos / visibles / incoherentes | 0 / 0 / 0 |
| Resultado del contrato de las dos tuberías | Correcto |
| Salud | Tres vidas, cero muertes y cero separaciones de Baby Mario durante juego normal |
| Reposo final | 418 fotogramas sin botones, posición fija y velocidad cero |
| Fallos de despacho / instrucciones interpretadas / reparaciones nativas | 0 / 0 / 0 |

El [diferencial antes/después](widescreen-window-mask-effect.json) compara
las nuevas sesiones con las de la revisión anterior. Las entradas, las trazas
y el calendario coinciden entre las cuatro sesiones. Las **250 imágenes
completas de 240 píxeles** y los **250 centros de 356 píxeles** se conservan
exactamente; los **251 estados por anchura** también superan la comparación.

Cambian únicamente los márgenes de nueve capturas, en los fotogramas
transcurridos **6420, 6480, 6501, 6540, 6600, 6660, 6720, 6780 y 6801**.
La suma es **32.142 píxeles RGB a la izquierda y 31.088 a la derecha**,
63.230 en total. Las otras 241 capturas panorámicas permanecen idénticas.
Estos recuentos corresponden a imágenes programadas; no equivalen a comparar
cada imagen producida durante el recorrido.

La comparación de estado mantiene las excepciones anteriores: excluye PPU0,
que depende del área de presentación, y ocho bytes del reloj RTC inactivo.
Se exige que el indicador del RTC siga inactivo; no se añaden excepciones
a CPU0, BUS0, IO_0, AUD0, SAV0 o META. El centro RGB se verifica por separado.

El diferencial demuestra conservación y un efecto lateral real. Por sí solo
no califica la geometría de la máscara; esa evidencia procede de las pruebas
del compositor descritas abajo.

## Regresión de transformación

El [agregado nuevo del coche](widescreen-car-window-mask.json) pasa con las
acciones y el contrato históricos: **3.506 fotogramas, 3.507 observaciones,
61 centros RGB idénticos y 62 estados equivalentes**. Completa el ciclo
normal → coche → normal, recupera el modo normal en el fotograma 2216 y
termina con 1.200 fotogramas sin botones. El avance tras la recuperación es
58,83984375 píxeles. Ambas sesiones mantienen cero huecos de posiciones
y terminan con los diagnósticos nativos en cero.

## Pruebas independientes y procedencia

[La prueba del compositor](widescreen_obj_window_render_test.cpp) utiliza
directamente `gba_ppu.cpp`, sin ROM, BIOS, código generado ni partidas. Sus
texturas sintéticas permiten comprobar texeles opacos y transparentes.
El oráculo de geometría traslada los objetos dentro de la vista nativa y usa
el compositor original; no reproduce la fórmula del nuevo rasterizador para
decidir qué píxeles esperar. También compara el centro original y valores
de color explícitos.

La cobertura incluye tamaños y formas de objetos, 4/8 bits por píxel,
disposiciones 1D/2D, volteos, transformaciones afines y tamaño doble, uniones
de ventanas, ventanas exteriores, X completa frente a alias, rechazo de
posiciones y procedencia inválidas, filas parcialmente resueltas, prioridades
WIN0/WIN1, activación de efectos de color, límites de VRAM y filas verticales
no autorizadas. Se ejecuta con AddressSanitizer y UndefinedBehaviorSanitizer;
LeakSanitizer está desactivado.

El resultado es **488 escenarios sintéticos y 8.102.978 aserciones internas**,
sin hallazgos de ASan/UBSan. Las aserciones cuentan comparaciones internas;
no representan millones de pruebas independientes ni fotogramas del juego.
El oráculo asegura equivalencia con el compositor nativo existente, sin
pretender validar por separado toda su emulación del hardware.

Una [segunda prueba](widescreen_native_window_mask_test.cpp) utiliza las
copias OBJ-window de nueve estados reales de la sala final guardados en la
sesión base de 356 píxeles. Los vuelve a dibujar con el PPU nuevo. El
[extractor](prepare_window_mask_fixtures.py) contrasta el búfer de copias,
la entrega normal compactada y las posiciones completas. Para las piezas
afines 076/077 verifica también propietario, asignación, metadatos y el
desplazamiento de ocho píxeles de la caja. No infiere X de los nueve bits OAM.

El [agregado de esas máscaras](widescreen-native-window-mask.json) registra
**99 copias verificadas, 345.600 píxeles centrales iguales y 167.040 píxeles
laterales iguales al compositor nativo trasladado**. En cuatro estados hay
181, 313, 739 y 843 píxeles opacos en el margen izquierdo; los otros cinco
no tienen máscara lateral. No hay píxeles opacos de máscara a la derecha
en esos estados. El comprobador pasa ASan/UBSan.

Esta prueba conserva OAM, matrices y texturas OBJ, pero muestra la máscara
sobre un fondo sintético. No supone que los registros de un estado guardado
correspondan exactamente a la fase del PNG capturado ni valida todo el dibujo
del escenario a partir de ese estado. Los datos binarios permanecen privados.

[La prueba de parches](test_window_patch_stack.py) invoca el aplicador real
en clones locales temporales. Las siete etapas reconocidas alcanzan el mismo
resultado y pueden reaplicarse sin cambios. Las modificaciones ajenas en
cada uno de los seis archivos protegidos y una revisión de motor incorrecta
se rechazan sin alterar el árbol. Un archivo local ajeno permanece intacto.

El [registro de comprobaciones](window-mask-checks-2026-10-23.txt) conserva
los comandos, los recuentos y los resultados. El
[manifiesto](window-mask-native-build.json) registra las huellas del motor,
los seis parches, las fuentes, el archivo generado reutilizado y el ejecutor.
Se comprueba el target `sma3_runner` de escritorio; no se afirma una nueva
compilación Android ni la regeneración completa del código del juego.

## Reproducir la ruta

Se necesitan el motor fijado, las fuentes generadas privadas y los activos
propios compatibles descritos en el manifiesto. Utilizar un directorio nuevo
para cada sesión; conservar los contratos publicados.

```sh
cmake -S . -B build -DSMA3_WIDESCREEN_DIAGNOSTICS=ON
cmake --build build --target sma3_runner -j2
python3 validation/run_panorama_session.py \
  --rom /ruta/juego.gba --bios /ruta/bios.bin --state /ruta/entrada-castillo.state \
  --actions validation/panorama-castle-post-pipe-actions.json --castle-status \
  --width 240 --output /ruta/control-nuevo
python3 validation/run_panorama_session.py \
  --rom /ruta/juego.gba --bios /ruta/bios.bin --state /ruta/entrada-castillo.state \
  --actions validation/panorama-castle-post-pipe-actions.json --castle-status \
  --width 356 --output /ruta/panoramica-nueva
python3 validation/compare_castle_post_pipe_progress.py \
  /ruta/control-nuevo /ruta/panoramica-nueva /ruta/informe-nuevo.json \
  --contract validation/castle-post-pipe-expectations.json
python3 validation/test_window_patch_stack.py
```

La prueba sintética del PPU no necesita activos del juego:

```sh
mask_engine=../gbarecomp
c++ -std=c++20 -O1 -g -Wall -Wextra \
  -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all \
  -I"$mask_engine/src/gba" -I"$mask_engine/src/debug" -I"$mask_engine/src/runtime" \
  validation/widescreen_obj_window_render_test.cpp "$mask_engine/src/gba/gba_ppu.cpp" \
  -o /tmp/obj-window-render-test
ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /tmp/obj-window-render-test
```

La comprobación de máscaras reales requiere los nueve estados de una sesión
compatible. El directorio de salida debe ser nuevo y permanecer privado:

```sh
python3 validation/prepare_window_mask_fixtures.py \
  --sessions /ruta/panoramica-base --rom /ruta/juego.gba \
  --output /ruta/mascaras-privadas-nuevas
c++ -std=c++20 -O1 -g -Wall -Wextra \
  -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all \
  -I"$mask_engine/src/gba" -I"$mask_engine/src/debug" -I"$mask_engine/src/runtime" \
  validation/widescreen_native_window_mask_test.cpp "$mask_engine/src/gba/gba_ppu.cpp" \
  -o /tmp/native-window-mask-test
ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /tmp/native-window-mask-test /ruta/mascaras-privadas-nuevas/native-mask-*.bin
```

## Alcance pendiente

La entrada inicial sigue siendo sintética. Se valida esta ruta y el caso
OBJ-window sin ventanas rectangulares simultáneas. Permanecen pendientes
otros efectos de ventana, otras escenas y salidas, jefes, aparición lateral
ampliada y la adaptación del ancho a Android. No se amplían márgenes verticales.

La panorámica no está integrada en los APK **0.9 ni 0.9.1**. El ajuste
«Estirar a pantalla completa» conserva su significado de escalado del
fotograma original; no activa este prototipo. Esta fase no mide FPS ni
califica 120 Hz en el teléfono.
