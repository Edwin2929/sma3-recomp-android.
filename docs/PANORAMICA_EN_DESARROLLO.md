# Panorámica real: prototipo, aún sin APK

Objetivo: sustituir el estiramiento por una vista que muestre más escenario,
mantenga las proporciones y se adapte a la pantalla del teléfono.

La versión Android entregada sigue siendo 0.9. Este cambio **no está activado
en Android** y no demuestra todavía pantalla completa correcta en todas las
escenas ni rendimiento a 120 FPS.

## Implementado

- Reconstrucción del terreno desde el mapa completo del nivel, fuera del anillo
  parcial de VRAM. Incluye las cuatro piezas de cada bloque y las máscaras de
  la capa frontal.
- Lectura de los fondos completos, con sus desplazamientos y volteos.
- Lecturas acotadas sin escribir RAM ni modificar CPU, temporización o estado
  del bus. El prototipo admite hasta 356 × 160, frente a 240 × 160 originales.
- Captura de diagnóstico por fotograma y pruebas independientes del decodificador.
- Opción CMake `SMA3_WIDESCREEN_DIAGNOSTICS`, desactivada por defecto y exclusiva
  de escritorio. No cambia la compilación Android de 0.9.

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

## Pendiente antes de entregar el APK

1. Completar la cobertura de posiciones de objetos en todas las escenas.
   La resolución por metadatos es conservadora: cualquier objeto no verificado
   permanece limitado a la vista original.
2. Ampliar y comprobar los límites de dibujo y aparición. No basta con ampliar
   el fondo: enemigos y coleccionables deben aparecer correctamente.
3. Probar cambios de nivel, cuadros de texto, transformaciones, jefes y modos
   gráficos especiales. El prototipo conserva bordes en escenas no admitidas.
4. Adaptar el ancho a la superficie de Android, integrar el ajuste de imagen y
   medir la presentación con 120 Hz en el Pixel 7 Pro.

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

El parche `gbarecomp-widescreen-objects.patch` añade el recorte por objeto al motor.
Sin el diagnóstico activado, su callback permanece nulo y no habilita panorámica
en Android. El aplicador verifica la revisión del motor y rechaza cambios locales
que no correspondan a una etapa reconocida de los parches.

## Resultado del observador sincronizado

`validation/widescreen-regression.json` registra otra comparación completa de
18.000 fotogramas con el observador activo: 30 capturas idénticas al control,
centro final idéntico y los mismos 448 avisos heredados. Los contadores de fallos
de despacho, instrucciones interpretadas y reparación dinámica permanecen en cero.
Objetos resueltos en los fotogramas 7200, 8000, 9599, 12000, 16000 y 17999:
6, 6, 1, 3, 3 y 3 respectivamente. **La cobertura sigue siendo parcial.**
Esto valida la ausencia de cambios observados en ese recorrido, no todos los
objetos ni todas las transiciones. No se ha generado un APK panorámico.

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
validar transiciones y Android. La panorámica todavía no está lista para un APK.

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
