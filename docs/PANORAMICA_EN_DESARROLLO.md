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

La prueba posterior de 18.000 fotogramas terminó con los mismos tres contadores
de despacho a cero, pero **registró `unmapped=448`**. Falta compararla con un
control equivalente; no se considera una prueba de estabilidad aprobada.
Las capturas temporales de esas sesiones no se incluyen en el repositorio.

La versión del código guardada aquí incorpora una corrección conservadora:
limita los objetos a la vista original durante el diagnóstico del terreno.
Por tanto, las observaciones anteriores no equivalen a validar este código
completo en Android. La prueba actual independiente del decodificador supera
16 casos de bloques, límites y volteos, además de entradas inválidas, con
AddressSanitizer y UndefinedBehaviorSanitizer. LeakSanitizer se desactiva porque
el entorno de ejecución no permite inspeccionar los procesos que necesita.

## Pendiente antes de entregar el APK

1. Resolver las posiciones completas de los objetos. OAM conserva solo nueve
   bits de X; interpretar todos los valores 256–319 como positivos puede mover
   objetos ocultos al lado contrario. Se descartó esa suposición.
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

La zona `0202C8B0` parecía conservar coordenadas completas de OAM, pero no
coincidió con ninguno de los diez objetos visibles de la captura examinada.
No debe usarse como solución sin localizar primero su correspondencia real.

## Reproducir

Se requieren los archivos generados privados y los activos propios compatibles,
además del motor fijado por el proyecto. No se suben ROM, BIOS, capturas de RAM,
partidas, claves de firma ni archivos generados del juego.

```sh
cmake -S . -B build -DSMA3_WIDESCREEN_DIAGNOSTICS=ON
cmake --build build --target sma3_runner -j2
python3 validation/run_widescreen_probe.py --rom /ruta/juego.gba --bios /ruta/bios.bin --output /ruta/prueba-nueva
# Repetir con --generic y otra carpeta para obtener el control.
g++ -std=c++20 -Wall -Wextra -Werror -fsanitize=address,undefined -g validation/widescreen_tiles_test.cpp -o /tmp/widescreen_tiles_test
ASAN_OPTIONS=detect_leaks=0 /tmp/widescreen_tiles_test
```

Referencia de ingeniería: [sma3-disasm](https://github.com/KarisaAdvynia/sma3-disasm),
especialmente `LevelCode.asm`, `CodeStart.asm` y `SpriteShared08049E80.asm`.
