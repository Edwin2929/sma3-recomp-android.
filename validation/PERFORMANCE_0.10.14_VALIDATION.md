# Eficiencia de panorámica y presentación — 0.10.14 beta

Base: **0.10.13**, commit `8852595d7d0344ee264cbd6943b8551363de4b5d`.
Motor fijado en `e7728148c6829ba526f682876430a0c9022dc6c0`, con **19 parches
ordenados y 20 estados reconocidos**. La entrega reduce búsquedas repetidas
de terreno y sondeos opcionales del reloj. **No acredita una mejora general
de FPS ni 120 FPS sostenidos en un Pixel 7 Pro.**

Los resultados de componentes, recorridos del juego, presentación SDL y APK
se distinguen porque responden a preguntas diferentes. Las mediciones de
escritorio no sustituyen una prueba de GPU, temperatura o batería en Android.

## 1. Caché de bloques para el raster panorámico

El proveedor de los laterales resolvía el mismo bloque del mapa para varios
píxeles consecutivos. `RasterTileCache` conserva una entrada por capa en
**128 bytes**, sin asignaciones dinámicas durante el dibujo. Reutiliza una
entrada solo para X → X+1, en la misma fila del raster y celda del mundo de
8 × 8 píxeles; los fondos también comprueban la altura del mapa.

Una nueva fila, celda, fotograma o época invalida la reutilización. Repetir,
saltar o retroceder X vuelve a consultar. Las cargas de estado y el borrado
de observaciones descartan la caché. Se conservan los fallbacks, el mapa BG3
completo y la caché decorativa exterior de 0.10.13. No se escribe en el estado
del juego para evitar trabajo ni se cambia la cobertura de las escenas.

### Corrección y coste del componente

La prueba diferencial realiza **14.311.186 consultas** y compara sus valores
y condiciones de salida con la ruta anterior. Pasa con ASan/UBSan. Las
lecturas del proveedor en esa carga sintética bajan de **33.146.480 a
4.450.980**. En cinco instantáneas verificadas también coinciden las
**352.640 entradas** consultadas.

Cada medición temporal usa siete rondas de 200 fotogramas, orden AB/BA
alternado, afinidad a una CPU y `CLOCK_THREAD_CPUTIME_ID`. Los contadores de
lecturas se miden por separado, fuera de los bucles temporizados.

| Instantánea | Lecturas del recorrido de consultas antes → después | Mediana de CPU del componente, ms/fotograma antes → después |
| --- | ---: | ---: |
| Exterior, borde izquierdo | 112.200 → 16.200 | 0,758047 → 0,632419 |
| Castillo, pared izquierda | 112.160 → 16.160 | 0,667749 → 0,531648 |
| Castillo, borde derecho | 114.008 → 16.456 | 0,748736 → 0,623125 |
| Interior del coche | 204.160 → 28.160 | 0,931876 → 0,693980 |
| Entrada de Fuzzy | 205.360 → 28.328 | 0,917737 → 0,646147 |

El **85–86 % de reducción de lecturas** pertenece a esta carga de consultas,
que recorre las capas habilitadas de las instantáneas. No equivale al ahorro
del renderizador completo, a un porcentaje de FPS ni a autonomía adicional.

Fuentes: [caché](../src/widescreen_tile_cache.h),
[integración](../src/widescreen.cpp),
[prueba diferencial](widescreen_tile_cache_test.cpp),
[benchmark](widescreen_tile_cache_benchmark.cpp) y
[datos completos](panorama-tile-cache-0.10.14-results.json).

## 2. Sondeo solo con presentación pendiente

`CooperativePresentationPoll` recibe una solicitud con el plazo de preparación
de la imagen adicional. El puente del runtime comprueba el reloj cada 1.024
ticks **activos** y llama al servicio al alcanzar ese plazo. Si no hay imagen
pendiente, no realiza lecturas opcionales del reloj ni callbacks de presentación.
Se elimina la compuerta independiente de 1 ms que retrasaba el servicio después
de algunos muestreos.

El planificador arma y cancela la solicitud en sus rutas de preparación,
servicio, descarte y reinicio. El runtime elimina el callback y su puntero
antes de destruir al propietario. Se mantienen los sondeos de eventos, el
presupuesto de carga, el rechazo por retraso, las comprobaciones de época,
pausa y modo, y una sola espera del juego por periodo. La mezcla, los filtros,
la resolución y los contadores de tiempo real no cambian.

### Comparación con reloj controlado

| Carga sintética | Antes | Después |
| --- | ---: | ---: |
| Demora media desde el plazo al callback, 480 fases | 561,772 µs | 316,980 µs |
| Demora máxima en el mismo barrido | 1.478,647 µs | 1.478,647 µs |
| Lecturas opcionales de reloj en el barrido | 30.032 | 10.163 |
| Callbacks en el barrido | 7.700 | 480 |
| Extras en 120 periodos con 10 ms de trabajo de juego | 120 | 120 |
| Lecturas opcionales con esa carga de 10 ms | 4.800 | 3.360 |
| Callbacks con esa carga de 10 ms | 1.320 | 120 |
| Extras con sobrecarga sostenida de 17 ms | 0 | 0 |
| Lecturas opcionales con sobrecarga de 17 ms | 8.160 | 0 |
| Callbacks con sobrecarga de 17 ms | 2.040 | 0 |

Las cargas admitidas conservan **59,727502 fotogramas de juego/s**. La
sobrecarga de 17 ms conserva la ralentización del modelo: **55,555556 Hz**
en ambas versiones. No se altera el reloj del juego para mejorar el contador.

El barrido de 480 fases parte de la misma fase inicial y no pierde ningún
caso que antes llegaba a tiempo. No demuestra que cada callback real vaya a
ocurrir antes: el divisor deja de avanzar mientras está desarmado y su fase
relativa puede cambiar al rearmarlo. Un barrido separado de **1.024 fases de
rearmado** verifica cero lecturas durante la inactividad y como máximo
**1.024 ticks activos** para muestrear una solicitud cuyo plazo ya venció.
Trabajo prolongado entre ticks todavía puede hacer perder un plazo.

Pasan **4.116 comprobaciones** del helper y planificador, también con
ASan/UBSan; **88** de presentación cooperativa; **4.992** de temporización;
y las **3 pruebas** de la serie completa de parches. LeakSanitizer se
desactivó por restricciones del entorno; ASan y UBSan estuvieron activos.

Fuentes: [helper](../src/cooperative_presentation.h),
[planificador](../src/presentation_pacing.h),
[parche del motor](../patches/gbarecomp-performance-stability.patch),
[prueba reproducible](cooperative_poll_test.cpp) y
[datos y límites](cooperative-poll-0.10.14-results.json).

## 3. Regresiones del juego nativo

Se ejecutaron **11 recorridos candidatos**: exterior, castillo izquierdo y
castillo derecho a anchos lógicos 240, 347 y 356; entrada/salida de Fuzzy a
356; y transformación de coche a 356. Las referencias de 0.10.13 se reutilizan
tras verificar sus entradas y hashes; no se presentan como nuevas ejecuciones.

| Comprobación | Resultado |
| --- | ---: |
| Fotogramas de juego recorridos | 4.395 |
| Filas de trazas comparadas | 4.406 |
| Capturas completas exactamente iguales | 112 |
| Estados equivalentes en puntos sincronizados | 123 |
| Misses, instrucciones interpretadas, funciones reparadas y fallos | 0 en cada ejecución aceptada |

Cada recorrido parte del mismo estado y acciones. Además de las imágenes,
coinciden CPU0, BUS0, IO_0, AUD0, SAV0, META y el framebuffer nativo y tiempos
de PPU. Solo se excluyen los ocho bytes de reloj de pared del RTC inactivo
que ya se excluían en las comparaciones anteriores. Las capturas de panorama
completas también coinciden, incluidos los laterales.

Todas las referencias aceptadas y candidatos informan `FULLY_STATIC`, con
`GBARECOMP_STRICT_STATIC=1`. No se modifica código generado, ROM, BIOS ni
estado del juego para obtener estos resultados. No se afirma que se hayan
recorrido todos los niveles ni que se haya repetido toda la comparación
histórica contra mGBA.

Los intentos anteriores bloqueados por sockets locales, directorios ocupados
o pérdida del permiso ejecutable no llegaron a ejecutar el juego y no están
incluidos en estos totales. [Informe por recorrido](performance-0.10.14-gameplay.json).

## 4. Presentación nativa con SDL software

Se miden **16 ejecuciones**, 600 presentaciones obligatorias cada una
(**9.600 en total**), en orden secuencial **antes/después/después/antes**
por caso. No se superpusieron compilaciones ni otros benchmarks. Se usan
filtro **Equilibrado**, calidad **Automática**, controles visibles y las
mismas entradas verificadas. Fuzzy usa Original, superficie 480 × 320;
el coche usa Panorámica, superficie 960 × 540.

El instrumento intercepta llamadas SDL reales, mide CPU del hilo y tiempo
monotónico, y añade un histograma fijo sin asignaciones ni escritura por
fotograma. P99 es un límite superior en intervalos de 0,1 ms; el intervalo
de desbordamiento usa el máximo observado. Son intervalos entre llamadas
SDL completadas, no medidas del barrido físico de una pantalla.

### Todas las pasadas, incluidos los resultados desfavorables

CPU corresponde a la ventana entre la primera y última presentación.
Las columnas de tiempos no incluyen el arranque completo del proceso.

| Escena / objetivo | Orden y versión | Presentaciones/s | CPU del hilo (s) | P99 entre presentaciones (ms) | Extras |
| --- | --- | ---: | ---: | ---: | ---: |
| Fuzzy / 60 | 1 · 0.10.13 | 54.88 | 6.386 | 71.400 | 0 |
| Fuzzy / 60 | 2 · 0.10.14 | 59.24 | 6.170 | 22.100 | 0 |
| Fuzzy / 60 | 3 · 0.10.14 | 54.68 | 7.149 | 65.300 | 0 |
| Fuzzy / 60 | 4 · 0.10.13 | 59.10 | 5.634 | 21.600 | 0 |
| Fuzzy / 120 | 1 · 0.10.13 | 101.86 | 7.115 | 18.300 | 430 |
| Fuzzy / 120 | 2 · 0.10.14 | 115.76 | 6.516 | 16.800 | 564 |
| Fuzzy / 120 | 3 · 0.10.14 | 103.84 | 7.296 | 18.200 | 452 |
| Fuzzy / 120 | 4 · 0.10.13 | 91.50 | 8.050 | 38.400 | 396 |
| Coche / 60 | 1 · 0.10.13 | 45.82 | 12.758 | 36.800 | 0 |
| Coche / 60 | 2 · 0.10.14 | 48.42 | 12.153 | 34.900 | 0 |
| Coche / 60 | 3 · 0.10.14 | 26.55 | 22.166 | 566.778 | 0 |
| Coche / 60 | 4 · 0.10.13 | 44.15 | 13.392 | 49.200 | 0 |
| Coche / 120 | 1 · 0.10.13 | 43.42 | 13.345 | 41.000 | 0 |
| Coche / 120 | 2 · 0.10.14 | 45.89 | 12.837 | 38.200 | 0 |
| Coche / 120 | 3 · 0.10.14 | 46.14 | 12.692 | 51.800 | 0 |
| Coche / 120 | 4 · 0.10.13 | 46.24 | 12.682 | 32.500 | 0 |

Las imágenes finales coinciden en las cuatro pasadas de cada caso. Todas
mantienen cobertura `FULLY_STATIC`, cero misses/interpretación/healing/fallos,
conteos de presentación coherentes y el mismo punto final del juego. **PASS
en el JSON corresponde a esas comprobaciones; no es una certificación de
mejora de rendimiento.**

Con dos pasadas por versión, la mediana de presentación de Fuzzy/120 pasa de
96,68 a 109,80; Fuzzy/60 queda prácticamente igual (56,99 → 56,96). En
coche/60 empeora de 44,99 a 37,49, con una pasada candidata de 26,55 y un P99
superior a 566 ms. Coche/120 pasa de 44,83 a 46,02, pero su P99 mediano empeora
(36,75 → 45,00 ms) y ninguna versión logra imágenes adicionales en esa carga.

**Los resultados son mixtos y la variación es grande.** No se atribuye una
mejora general de FPS o regularidad a esta matriz. Tampoco se ha establecido
la causa del resultado adverso del coche/60: no se descarta ni se atribuye
sin pruebas al servidor. Los datos completos conservan cada pasada y las
medianas de dos muestras, sin tratarlas como una estimación estadística
precisa de una sesión larga.

### Diagnósticos y recuperación del procedimiento

Ambos runners fueron verificados como compilaciones de diagnóstico. El
colector existente puede no emitir registro cuando no hubo actividad
opcional. Se añadió `--allow-empty-diagnostics`, desactivado por defecto,
para permitir esa ausencia **solo si SDL contó exactamente las 600
presentaciones obligatorias**. En ese caso, los datos internos quedan
`null`/no disponibles; no se convierten en ceros. Cuando hay registro, su
conteo de extras debe coincidir con `llamadas SDL − 600`. Las cifras cero de
sondeos del apartado 2 proceden de la prueba controlada, no de inferirlas de
un registro ausente.

La primera pasada exploratoria de Fuzzy/60 terminó el juego, pero el parser
anterior rechazó el registro vacío; no se incluye entre las 16. El primer
intento de coche fue rechazado antes de lanzar el runner porque la cinta
completa contiene metadatos condicionales después del fotograma 675. Para
estas pasadas de 600 presentaciones se usaron las cuatro primeras acciones
originales, todas de duración fija y que suman 675 fotogramas. No se cambió
ninguna tecla ni duración de ese prefijo. Las ocho pasadas Fuzzy ya
completadas se conservaron tras comprobar sus hashes.

[Datos completos](native-performance-0.10.14-results.json). Los hashes de
runners e instrumento quedan registrados. El runner candidato comparte
las entradas nativas de producción del APK; el entorno SDL software y los
diagnósticos de escritorio no son una GPU Android ni se incluyen en el APK.


## 5. APK, procedencia e identidad de actualización

| Campo | Valor |
| --- | --- |
| Archivo | `sma3-recomp-0.10.14-beta-arm64.apk` |
| Versión / código | `0.10.14-beta` / `25` |
| Payload | `0.10.14` |
| Paquete | `org.sma3.recomp.experimental` |
| Arquitectura / mínimo | ARM64 / Android 9, API 28 |
| Tamaño | 28.017.944 bytes |
| Firma | v2 verificada, certificado original |
| Código nativo | Release, diagnósticos de presentación y panorámica desactivados |

SHA-256 del APK:

```text
5718fc00aef58c10556a41b4f27a590ee7b93a85578d858251a1379a48abad67
```

Las **1.828 entradas Android** coinciden antes y después de compilar y las
**1.782 entradas compartidas** coinciden con el runner candidato. Una última
comprobación de las fuentes actuales tampoco encontró cambios, añadidos ni
ausencias respecto a esos manifiestos.

Se reutilizó el archivo estático del juego después de verificar su hash y
sus **102 entradas originales**. Se recompilaron el runtime y la presentación.
No se afirma que se regeneró todo el juego ni que se certificó toda su cobertura.

SDL2 también se recompiló: sus bytes difieren del APK anterior. Se verifican
**1.209 entradas fuente iguales**, **185 órdenes de compilación equivalentes**
tras normalizar rutas y las mismas cabeceras de configuración/revisión.
Conserva tamaño, símbolos dinámicos y dependencias; el archivo empaquetado
coincide con la salida nueva. Esto no acredita identidad binaria ni de GPU.
`libc++_shared.so` sí coincide exactamente con la versión anterior.

Pasan CRC, alineación, metadatos, firma, **21 enlaces JNI** y **26
identificadores de logros**. No se empaquetan ROM, BIOS, partidas o clave
privada. La identidad permite actualizar sobre la instalación anterior;
la instalación física aún no se ha probado en esta sesión.

El informe conserva también los intentos de infraestructura: directorio
multiarch SDL ausente en el primer build de host y socket local de Gradle
bloqueado en el primer ensamblado Android. Sus reintentos terminaron sin
cambiar las fuentes de producción. [Informe del APK](android-0.10.14-build.json).

## 6. Reproducir y comprobar en un teléfono

Las pruebas de componentes no requieren ROM ni BIOS. Con el motor fijado y
la serie de parches aplicada, desde la raíz del proyecto:

```bash
c++ -std=c++20 -O2 validation/widescreen_tile_cache_test.cpp -o /tmp/sma3-tile-cache-test
/tmp/sma3-tile-cache-test
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -Wno-misleading-indentation -I src validation/cooperative_poll_test.cpp -o /tmp/sma3-cooperative-poll-test
/tmp/sma3-cooperative-poll-test
python3 validation/test_window_patch_stack.py
```

La medición nativa utiliza [benchmark_native_presentation.py](benchmark_native_presentation.py)
y [sdl_presentation_timing.c](sdl_presentation_timing.c), con un runner de
diagnóstico verificado, ROM/BIOS propios de la revisión compatible, un estado
inicial sincronizado y una cinta de acciones. Los recursos privados y las
capturas no se publican en el repositorio. `--help` documenta los parámetros.

Para comprobar la experiencia en Android, instala el APK sobre la versión
anterior sin borrar datos, activa **Mostrar FPS** y usa la misma escena,
filtro, calidad y modo de imagen. Compara 60 y 120 por separado, incluyendo
una sesión prolongada y el principio/final de esa sesión. Temperatura,
brillo, carga de batería y aplicaciones de fondo deben mantenerse comparables.

**Pendiente de dispositivo físico:** instalación, cadencia visible, GPU,
sesiones largas, temperatura y consumo de batería. No hay una cifra de mejora
certificada para esos aspectos. El APK se entrega al mantenedor para su carga
manual a `Descarga`; publicar estas fuentes no sube el binario a esa release.
