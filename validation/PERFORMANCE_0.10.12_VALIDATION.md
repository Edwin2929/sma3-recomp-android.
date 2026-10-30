# Rendimiento y recuperación de 120 — validación de 0.10.12 beta

Base: **0.10.11**, commit `42b7b6e463a9fdfe258e4a1cd3ea260b6235276e`.
Motor fijado en `e7728148c6829ba526f682876430a0c9022dc6c0`, con 18 parches
ordenados. Los cambios corrigen trabajo redundante y una recuperación lenta
del presupuesto. **No acreditan 120 FPS sostenidos en un Pixel 7 Pro.**

## 1. Problema reproducido y corrección del presupuesto

El modo de mezcla 2× mantiene un periodo de juego de **16,742706 ms** y dos
plazos de presentación por periodo. Las estimaciones de trabajo reservan
tiempo para el juego, historial, preparación, intercambio y márgenes. Si no
cabe, se omite la imagen adicional.

Antes, un tirón aislado podía conservar hasta 100 ms en una estimación que
decae un octavo por muestra. Aunque las siguientes cargas fueran normales,
se rechazaban imágenes adicionales durante muchos fotogramas. Una muestra
de un periodo completo ya basta para impedir esa admisión: retener un valor
superior no aporta protección adicional a esa decisión.

Se limita el horizonte de las estimaciones al periodo que comunica el
temporizador. Se mantienen el decaimiento, los márgenes, el servicio y la
tolerancia del punto intermedio. Las observaciones nuevas siguen invalidando
el trabajo adicional que no cabe. También se limita el historial previo al
cambiar de horizonte.

**El contador recibe los tiempos reales completos.** El límite afecta a la
estimación para decidir trabajo futuro, no al tiempo transcurrido, las llamadas
SDL completadas, la velocidad del juego ni las cifras mostradas como FPS.

### Comparación determinista

Carga habitual: 10 ms de juego, 0,75 ms de preparación y 0,25 ms de intercambio
por imagen. Se introduce un único pico de 68 ms y se vuelve a la carga habitual.

| Fuente del pico | Primer fotograma que recupera la imagen extra, antes → después | Extras en los siguientes 30 fotogramas, antes → después |
| --- | ---: | ---: |
| Juego | 13 → **3** | 18 → **28** |
| Preparación | 26 → **16** | 5 → **15** |
| Intercambio | 28 → **17** | 3 → **14** |

Tras el pico, los seis casos conservan **59,727502 fotogramas de juego/s**.
Con 25 ms sostenidos en cualquiera de las tres fuentes se admiten **cero
extras en 120 fotogramas**, tanto antes como después. Esto no garantiza un
comportamiento idéntico bajo todas las cargas posibles: una discontinuidad
superior a 100 ms conserva el reinicio de historial que ya existía.

Pasan **88 comprobaciones** de presentación cooperativa y **4.992** de
temporización. El benchmark público reproduce exactamente los seis registros
guardados. Datos y método:
[presentation-cost-recovery-0.10.12.json](presentation-cost-recovery-0.10.12.json),
[benchmark](presentation_cost_recovery_benchmark.cpp) y
[regresiones](cooperative_presentation_test.cpp).

## 2. Reloj PPU: eliminar el recorrido por cada punto

`GbaPpu::tick` convierte los ciclos en puntos completos y avanza entre límites
de línea. Conserva los eventos HBlank, VBlank, VCOUNT y final de fotograma,
el residuo de ciclos y todos los contadores. El scheduler sigue deteniéndose
en los mismos eventos. No cambia la frecuencia de CPU/PPU/audio del juego.

La prueba diferencial enlaza la función de producción y conserva como
referencia el bucle anterior. Pasan **2.993.496 comparaciones**, que incluyen
las 280.896 fases válidas del raster, llamadas fragmentadas, bordes, grandes
entradas, desbordamientos y 1.680 estados con campos inválidos. Una regresión
del primer prototipo —paso cero para un punto inválido de un estado legado—
se corrigió y quedó cubierta antes de compilar el APK.

En un microbenchmark del componente, 600 fotogramas y mediana de cinco
pasadas, el patrón de llamadas mezcladas baja de **65,021 a 11,336 ms**.
El ahorro es aproximadamente **0,089 ms por fotograma de juego**. Es útil,
pero no equivale a multiplicar los FPS del juego por la aceleración del
componente. [Prueba reproducible](ppu_tick_test.cpp).

## 3. Renderizado: menos subidas y copias

Scale2x/Scale3x pasan de dos subidas de textura a una cuando el filtro tiene
éxito. Si falla, se carga la imagen nativa actual antes de presentar el
fallback. Una sola instantánea del filtro evita mezclar decisiones durante
un cambio de opciones. Las salidas que ya son exactamente 1280 × 720 o
1920 × 1080 omiten su copia final 1:1.

Se compararon **568 salidas completas**, **4.564.684.800 bytes**, sin una sola
diferencia. Incluyen los siete filtros, tres calidades, seis tamaños de
pantalla, orientaciones, panorámica, controles y fallos de textura. También
pasan 699 comprobaciones de upload/fallback y 45 casos geométricos más 130
de renderizado del escalador.

Los conteos de trabajo se reducen de forma verificable. Los tiempos completos
del renderizador software fueron variables, con mejoras y regresiones, por
lo que no se atribuye un incremento general de FPS a esos tiempos.
[Informe íntegro, incluidos resultados desfavorables](RENDER_0.10.12_PERFORMANCE.md).

## 4. Juego real con la ruta de presentación Android

Se añadió `SMA3_PRESENTATION_DIAGNOSTICS=ON` para compilar en Linux la misma
planificación y las ramas de geometría/filtros de Android. El modo diagnóstico
está prohibido en compilaciones Android; no añade sondeos ni escritura de
diagnóstico al APK. Usa histogramas fijos y emite el resumen al terminar.

La referencia conserva los algoritmos de 0.10.11 y añade únicamente los mismos
puntos de medición y selección de opciones. Ambas ejecutan la ROM recompilada
con cobertura `FULLY_STATIC`, sin intérprete ni funciones reconstruidas en
caliente. Se crea una superficie SDL real del tamaño indicado; no se simula
la GPU del teléfono.

Cada fila contiene 360 presentaciones obligatorias, controles visibles,
filtro Equilibrado, calidad Automática y objetivo 120:

| Escena y superficie SDL software | Presentaciones/s antes → después | Extras antes → después |
| --- | ---: | ---: |
| Entrada de Fuzzy, original, 480 × 320 | 95,247 → 99,959 | 232 → 252 |
| Coche, panorámica, 960 × 540 | 50,861 → 53,361 | 0 → 0 |

Son ejecuciones individuales en un host compartido. **No constituyen una
medición de mejora sostenida ni de FPS del Pixel.** La segunda escena sigue
sin presupuesto para las imágenes extra en este renderizador software. No
se alteran los números para presentar ese resultado como 120 FPS.

Los estados finales y los píxeles finales coinciden entre versiones. La
escena corta de entrada de Fuzzy termina antes del contacto; la prueba de
1.900 fotogramas de la siguiente sección sí incluye el efecto y la recuperación.
Las medias de estimaciones de presupuesto usan horizontes distintos entre
versiones y no deben interpretarse como ahorro de CPU. El hook SDL cuenta
llamadas reales y mide tiempo real por separado.

[Resultados completos](native-performance-0.10.12-results.json) ·
[Herramienta reproducible](benchmark_native_presentation.py) ·
[Medición SDL](sdl_presentation_timing.c).

## 5. Regresiones del juego ejecutadas con el candidato final

Se reutilizó la evidencia de referencia verificada de 0.10.11 y se ejecutó de
nuevo cada recorrido con el candidato final de 0.10.12. No se contabiliza la
referencia histórica como una nueva ejecución.

| Recorrido | Traza idéntica | Estados comparados | Capturas idénticas |
| --- | ---: | ---: | ---: |
| Fuzzy, 240 × 160, 1.900 fotogramas | 1.901 filas | 41 | 40 |
| Coche, 356 × 160, 1.526 fotogramas | 1.527 filas | 34 | 33 |

Coinciden las secciones CPU, bus, I/O, audio, guardado y metadatos, junto con
la temporización PPU y todos los píxeles muestreados. Solo se excluye el campo
de reloj de pared del RTC inactivo, igual que en la referencia.

Fuzzy permanece activo 782 fotogramas, se recupera y conserva al menos 100
fotogramas normales al final. No hay muerte ni reinicio y no se desactiva el
efecto para lograrlo. La transformación de coche empieza y termina, con
progreso de logros idéntico. Son replays Linux de zonas concretas; no equivalen
a una campaña completa ni a pruebas de controles/audio físicos de Android.

## 6. APK y límites prácticos

APK **0.10.12-beta**, código **23**, ARM64, Android 9 o posterior. Se verifican
firma v2 con el certificado de la entrega anterior, alineación, CRC, versión,
21 enlaces JNI, los 26 IDs de logros y ausencia de ROM/BIOS/clave/partidas
sueltas. Las bibliotecas del APK coinciden con esta compilación. Sus 1.824
entradas de fuente permanecieron invariables y las 1.778 compartidas con el
runner de validación coinciden. El runtime se recompila; el archivo generado
principal del juego se reutiliza después de verificar sus 102 entradas.

Un primer intento falló durante `llvm-strip` por un temporal desaparecido en
los intermedios del empaquetado. Se recrearon esos intermedios y la segunda
compilación terminó correctamente, sin cambios de fuente entre intentos.
[Verificación del paquete](android-0.10.12-build.json).

El objetivo ideal sigue siendo **119,455 presentaciones/s**, dos por cada
fotograma original de aproximadamente 59,7275 Hz. El contador no mide la
salida física del compositor. Android puede conceder menos frecuencia a la
app que al panel; se conserva esa comprobación y la solicitud de 120 Hz.

Falta la prueba física de esta entrega en Pixel 7 Pro, incluida la estabilidad
durante una sesión larga, temperatura y distintos filtros. No se asegura
estabilidad de 120 en todas las escenas o configuraciones. La selección de
filtro, calidad y modo de imagen se conserva. Panorámica continúa marcada
**beta** con su advertencia de fase temprana.
