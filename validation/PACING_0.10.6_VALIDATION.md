# Presentación — diagnóstico y validación de 0.10.6 beta

## Resultado comprobado

Se reprodujo una regresión de **0.10.5 en el modo de presentación 120**:
se preparaban imágenes intermedias cuyo plazo ya había vencido. La ruta
ejecutaba perfil de color, subida de textura, escalado, controles y envío de
órdenes SDL; solo después comprobaba si debía descartar la imagen. Ese trabajo
inútil podía retrasar también la imagen completa y reducir la velocidad del
juego. La ruta anterior de 0.10.4 descartaba el punto intermedio antes de
renderizarlo.

La corrección comprueba primero si cabe la imagen adicional, usando el coste
observado de preparación y de intercambio de buffers. Si no cabe, conserva
el fotograma anterior para futuras mezclas y presenta únicamente la imagen
completa. **El reloj del juego continúa a 59,7275 Hz; no se acelera la lógica
para aumentar el contador.**

El defecto se reprodujo con las clases reales `HostWindow` y `FramePacer`
en un equipo **x86-64 con SDL 2.30.0 software**. El escenario utiliza una
espera explícita en lugar de ejecutar una ROM. No es una ejecución del APK
ni una medida de GPU, batería, temperatura o FPS del Pixel 7 Pro.

## Comparación de las tres rutas

La prueba [`pacing_render_benchmark.cpp`](pacing_render_benchmark.cpp) usa
las ramas Android de presentación, una superficie de **2400 × 1080**, filtro
**Nearest**, controles visibles y ninguna modificación de color activa. SDL
ejecuta realmente las subidas de textura, escalado y dibujo sobre su backend
software; su cola de órdenes está activada. La carga del juego se sustituye
por una espera de **2 ms o 12 ms por iteración**. Cada caso tiene 30 iteraciones
de calentamiento y 120 medidas. Se ejecutaron dos rondas, invirtiendo el orden
de versiones en la segunda.

Las tres fuentes comparadas son:

- **0.10.4:** `HostWindow` y `FramePacer` originales del motor fijado
  `e7728148c6829ba526f682876430a0c9022dc6c0`, con parches 1–10. Las cinco
  unidades archivadas se reconstruyeron por separado desde ese commit y
  coinciden byte por byte. No es el renderer nuevo con una etiqueta antigua.
- **0.10.5:** las fuentes con parches 1–12 del estado de referencia
  `40849aeef03a5cdd569a19f35034c8233c9520d2` del proyecto.
- **Candidato 0.10.6:** las mismas fuentes y los mismos filtros/controles de
  0.10.5, más la corrección de presupuesto e historial. Las optimizaciones
  independientes de filtros de 0.10.6 no se incluyen en esta comparación.

El código común de filtros, escalador y mezcla era idéntico entre los commits
de proyecto 0.10.4 (`65de9e6f72820e9253bb56004ebc5f6f837a9a08`) y 0.10.5.
La apariencia de los controles sí pertenece a cada renderer histórico; la
comparación 0.10.5 → candidato mantiene esa apariencia idéntica.

### Caso que reproduce la regresión

Objetivo 120, carga artificial de 12 ms:

| Ruta | Iteraciones completas/s, dos rondas | Subidas de textura / llamadas SDL_RenderPresent por ronda | CPU del hilo por iteración |
| --- | ---: | ---: | ---: |
| 0.10.4 | 59,55–59,73 | 120 / 120 | 4,66–4,72 ms |
| 0.10.5 | **48,35–48,91** | **240 / 120** | **8,37–8,61 ms** |
| Candidato 0.10.6 | **59,68–59,73** | **120 / 120** | **4,18–4,19 ms** |

En 0.10.5 se calculaban y dibujaban 120 imágenes adicionales que no llegaban
a mostrarse. El candidato evita ese trabajo. En esta prueba la media de CPU
por iteración baja aproximadamente **50,7 %**. Ese tiempo de CPU incluye
renderizado, mezcla, instrumentación y la parte activa de la espera; no
incluye como CPU los 12 ms dormidos de la carga artificial.

### Casos que limitan la conclusión

Con carga artificial de 2 ms y objetivo 60, las tres rutas mantuvieron
aproximadamente **59,727 iteraciones/s**. Con objetivo 120 y esa carga,
0.10.5 registró **119,45 presentaciones SDL/s** y el candidato **116,83–117,96**:
el presupuesto conservador omitió algunos extras ante picos medidos de
preparación. Sus iteraciones completas permanecieron en **59,66–59,73/s**.
La prioridad de la corrección es no retrasar la imagen completa; **no se
afirma una mejora universal del número de presentaciones**.

En el escenario de 12 ms y objetivo 60, cercano al presupuesto disponible
del renderer software, hubo variación del host: el candidato obtuvo
57,50–59,48 iteraciones/s y 0.10.5 obtuvo 59,10–59,73. Esta muestra no
demuestra una mejora del modo 60. Se conserva su orden de espera y su reloj;
los datos completos se incluyen al final y en el JSON, sin omitir estos casos.

## Cambios de producción

`OptionalPresentationBudget`, en
[`presentation_pacing.h`](../src/presentation_pacing.h), mantiene estimaciones
de preparación y de tiempo transcurrido dentro de `SDL_RenderPresent`. Una
muestra lenta eleva inmediatamente la estimación; las muestras siguientes
permiten reducirla en un octavo por observación. Se añade un margen de
preparación de 100 µs y se acotan las muestras a 100 ms para que una pausa
larga no mantenga indefinidamente el presupuesto alto.

La admisión exige que la preparación prevista termine dentro del plazo
intermedio y que, después del intercambio intermedio previsto, aún quede
tiempo para preparar la imagen completa. No confunde una solicitud de 120
con la capacidad real de ejecutar dos presentaciones.

`PresentationPacing` consulta ese presupuesto antes de calcular la mezcla.
`HostWindow::present_paced` lo comprueba otra vez después de la mezcla y
antes de cualquier trabajo de color, textura, escalador o controles. La
comprobación tardía existente permanece como respaldo: un coste inesperado
todavía puede superar la estimación.

[`FrameBlend::capture`](../src/frame_interpolation.h) actualiza el historial
sin calcular diferencias ni promedios de una imagen que no se mostrará.
Cuando vuelve a haber presupuesto, la siguiente mezcla usa los dos estados
consecutivos correctos. Se conservan los cortes de escena, cambios de tamaño
y discontinuidades del contador de fotogramas.

El motor incorpora tres métodos nuevos en `FramePacer` para consultar el
presupuesto y observar costes. Los métodos previos y `HostWindow::present`
conservan sus firmas. La integración corresponde al parche 13
`gbarecomp-render-performance.patch`; el parche histórico 11 no se reescribe.

## Revisión de SDL, bloqueo de intercambio y primera medida de FPS

La implementación SDL fijada vacía la cola con `RunCommandQueue` y la deja
vacía. `SDL_RenderPresent` vuelve a comprobar esa cola, pero no ejecuta
otra vez las mismas órdenes. En esta prueba con batching, el trabajo del
renderer software se concentró en el flush; el `RenderPresent` posterior
consumió habitualmente unos **0,003–0,006 ms por iteración de presentación
completa**. No se observó un segundo trabajo equivalente de renderizado.

La [documentación de SDL_RenderFlush](https://wiki.libsdl.org/SDL2/SDL_RenderFlush)
desaconseja su uso habitual fuera de la combinación con APIs gráficas nativas.
En esta corrección se conserva la colocación existente para ejecutar la cola
CPU antes de la espera. Retirarla movería ese trabajo después del plazo sin
un ahorro demostrado en este diagnóstico. **La llamada no implica
`glFinish` ni garantiza que la GPU termine antes de dormir.**

El motor solicita un renderer sin `SDL_RENDERER_PRESENTVSYNC` por defecto;
el backend GLES2 de SDL pide intervalo de intercambio 0. Esto descarta la
afirmación de que el código haya activado explícitamente dos relojes VSync.
No descarta bloqueos reales del driver: la
[guía de bucles de Android](https://developer.android.com/games/develop/gameloops)
explica que `eglSwapBuffers` puede esperar por buffers disponibles. Por eso
el nuevo presupuesto observa también el tiempo del swap. **No se midió EGL
ni SurfaceFlinger de un teléfono en este trabajo.**

El contador de 0.10.5 corrigió además un sesgo de la primera ventana: la
primera muestra iniciaba el intervalo pero antes se contaba como un intervalo
completado. Una ventana ideal de 30 intervalos nativos podía aparecer cerca
de 61,75 en el código antiguo, frente a 59,73 intervalos reales/s. Eso cambia
la primera cifra, no la velocidad del juego. La corrección del contador se
conserva; no se infla el resultado para mostrar 60 o 120 redondos.

## Verificaciones

- **4.995 comprobaciones** del planificador, presupuesto, historial y
  contador aprobadas en compilación optimizada y con **ASan/UBSan**.
- 360 fotogramas simulados conservan exactamente una espera completa por
  fotograma. Se cubren pausa, cambio de modo, salto de fotogramas, avance
  rápido, textura perdida y llegada tardía.
- El historial tras omitir extras produce mezclas idénticas píxel por
  píxel a las del historial completo, incluidos cortes de escena y cambio
  240 × 160 → 356 × 160.
- Los casos de presupuesto verifican el rechazo temprano, la reserva del
  render completo tras un swap lento, las muestras inválidas, la cota de
  coste y la recuperación después de un pico.
- Las tres versiones del `FramePacer` real avanzaron 90 periodos en unos
  **1,50684 s**, tanto con una como con dos ventanas de presentación. La
  política de espera de 0.10.5 se conserva en el candidato; diferencias de
  CPU entre ejecuciones del temporizador no se atribuyen a una optimización
  nueva del reloj.
- `host_platform.cpp`, `host_window.cpp` y `runtime.cpp` del candidato pasan
  comprobación de sintaxis con los argumentos reales **Android ARM64**.
  Permanecen advertencias previas de función sin usar, etiquetas class/struct
  y comparación de signos. No se modificaron para esta corrección.

LeakSanitizer permanece desactivado por las restricciones del entorno;
AddressSanitizer y UndefinedBehaviorSanitizer están activos. La biblioteca
SDL del sistema no está instrumentada. La construcción completa del APK y
las pruebas de otros componentes se registran por separado.

## Reproducción

Con el motor fijado y los 13 parches aplicados, desde la raíz del proyecto:

```sh
c++ -std=c++20 -O2 -Wall -Wextra -Werror -Wno-error=misleading-indentation \
  -pthread -DSMA3_FRAME_INTERPOLATION=1 -I src -I ../gbarecomp/src/runtime \
  validation/presentation_pacing_test.cpp ../gbarecomp/src/runtime/host_platform.cpp \
  -o /tmp/sma3-pacing-test
/tmp/sma3-pacing-test
```

El benchmark de renderer requiere JDK 17 para los encabezados JNI, los
encabezados SDL del motor y una biblioteca SDL2 del host. Define
`SMA3_BENCH_JDK` con la ruta del JDK y `SMA3_BENCH_SDL` con la biblioteca
compartida. Se compilan ramas de código Android sobre el host, sin ejecutar
JNI, EGL Android ni una ROM:

```sh
c++ -std=c++20 -O3 -pthread -ffunction-sections -fdata-sections \
  -Wl,--gc-sections -D__ANDROID__=1 -DGBARECOMP_HAVE_SDL2=1 \
  -DSMA3_FRAME_INTERPOLATION=1 -DSMA3_MOD_PROFILES=1 -DBENCH_VERSION=6 \
  -I src -I ../gbarecomp/src/runtime \
  -I ../gbarecomp/platform/android/third_party/SDL/include \
  -I "$SMA3_BENCH_JDK/include" -I "$SMA3_BENCH_JDK/include/linux" \
  validation/pacing_render_benchmark.cpp \
  ../gbarecomp/src/runtime/host_platform.cpp \
  ../gbarecomp/src/runtime/color_lut.cpp ../gbarecomp/src/runtime/touch_input.cpp \
  "$SMA3_BENCH_SDL" -o /tmp/sma3-render-benchmark
/tmp/sma3-render-benchmark 2400 1080 1 1 120 12000
```

Los argumentos son ancho, alto, controles visibles, filtro persistido,
objetivo de presentación y microsegundos de carga artificial dormida.
Para las comparaciones históricas se requieren copias independientes de
las fuentes descritas arriba, el `BENCH_VERSION` correspondiente y los
defines `SMA3_HOST_WINDOW_HEADER` y `SMA3_HOST_WINDOW_SOURCE` que apunten
a esas copias; también deben usarse sus propios encabezados y `host_platform.cpp`.
La prueba pequeña [`frame_pacer_benchmark.cpp`](frame_pacer_benchmark.cpp)
permite repetir por separado los 90 periodos del temporizador real.

Resultados, configuración y hashes completos:
[`android-0.10.6-pacing.json`](android-0.10.6-pacing.json).

## Todas las mediciones del renderer

Las cifras siguientes son mediciones del harness de host descrito, no del
APK. «CPU» es tiempo del hilo por iteración; «subidas» y «salidas» son los
conteos de llamadas de textura y de `SDL_RenderPresent`.

| Ronda | Ruta | Objetivo | Carga artificial | Ciclos/s | Presentaciones SDL/s | CPU ms/ciclo | Subidas / salidas |
| ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 0.10.4 | 60 | 2 ms | 59.727 | 59.727 | 5.022 | 120 / 120 |
| 1 | 0.10.5 | 60 | 2 ms | 59.727 | 59.727 | 4.686 | 120 / 120 |
| 1 | Candidato 0.10.6 | 60 | 2 ms | 59.727 | 59.727 | 4.764 | 120 / 120 |
| 1 | 0.10.4 | 120 | 2 ms | 59.231 | 117.475 | 10.551 | 238 / 238 |
| 1 | 0.10.5 | 120 | 2 ms | 59.728 | 119.455 | 9.184 | 240 / 240 |
| 1 | Candidato 0.10.6 | 120 | 2 ms | 59.660 | 116.834 | 9.371 | 237 / 235 |
| 1 | 0.10.4 | 60 | 12 ms | 59.640 | 59.640 | 4.678 | 120 / 120 |
| 1 | 0.10.5 | 60 | 12 ms | 59.101 | 59.101 | 4.406 | 120 / 120 |
| 1 | Candidato 0.10.6 | 60 | 12 ms | 57.503 | 57.503 | 5.177 | 120 / 120 |
| 1 | 0.10.4 | 120 | 12 ms | 59.554 | 59.554 | 4.715 | 120 / 120 |
| 1 | 0.10.5 | 120 | 12 ms | 48.351 | 48.351 | 8.605 | 240 / 120 |
| 1 | Candidato 0.10.6 | 120 | 12 ms | 59.675 | 59.675 | 4.178 | 120 / 120 |
| 2 | Candidato 0.10.6 | 60 | 2 ms | 59.727 | 59.727 | 4.151 | 120 / 120 |
| 2 | 0.10.5 | 60 | 2 ms | 59.727 | 59.727 | 4.271 | 120 / 120 |
| 2 | 0.10.4 | 60 | 2 ms | 59.727 | 59.727 | 4.820 | 120 / 120 |
| 2 | Candidato 0.10.6 | 120 | 2 ms | 59.728 | 117.962 | 8.557 | 238 / 237 |
| 2 | 0.10.5 | 120 | 2 ms | 59.727 | 119.455 | 8.604 | 240 / 240 |
| 2 | 0.10.4 | 120 | 2 ms | 59.738 | 119.476 | 9.198 | 240 / 240 |
| 2 | Candidato 0.10.6 | 60 | 12 ms | 59.477 | 59.477 | 4.365 | 120 / 120 |
| 2 | 0.10.5 | 60 | 12 ms | 59.727 | 59.727 | 4.258 | 120 / 120 |
| 2 | 0.10.4 | 60 | 12 ms | 59.382 | 59.382 | 4.766 | 120 / 120 |
| 2 | Candidato 0.10.6 | 120 | 12 ms | 59.728 | 59.728 | 4.194 | 120 / 120 |
| 2 | 0.10.5 | 120 | 12 ms | 48.914 | 48.914 | 8.370 | 240 / 120 |
| 2 | 0.10.4 | 120 | 12 ms | 59.730 | 59.730 | 4.660 | 120 / 120 |
