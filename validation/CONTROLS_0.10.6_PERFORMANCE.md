# Diagnóstico de rendimiento de los controles para 0.10.6

## Decisión

**0.10.6 conserva el renderizador de controles de 0.10.5. No incorpora los
prototipos de agrupación ni de caché geométrica de esta investigación.**

Se comprobó que el modo de símbolos realiza muchas llamadas pequeñas a SDL,
pero su coste de preparación y encolado medido es pequeño en términos absolutos.
En 2400 × 1080, el candidato de caché reduce aproximadamente **63,43 a 29,50
microsegundos** de CPU por cuadro al encolar los controles: un ahorro de
**0,034 milisegundos**, aproximadamente el **0,4 %** del intervalo de 8,37 ms de
la presentación 2× de GBA. Estas medidas no justifican atribuir a los controles
la caída de FPS informada por el usuario.

La decisión fue conservar el comportamiento visual y las zonas táctiles actuales
y priorizar la corrección de cadencia y el trabajo de filtros/escalado. Los
candidatos quedan como experimentos privados, no como funciones del APK. Durante
este diagnóstico no se modificaron el motor compartido, los parches de
producción, el renderer de controles, la Activity ni el editor Java.

## Entorno y método

Base: commit `40849aeef03a5cdd569a19f35034c8233c9520d2` de 0.10.5. Se compiló una
copia del `host_window.cpp` real, conservando `SdlOverlay`, `render_host_layers`,
el cálculo de controles y el estado de entrada. No se ejecuta el juego ni se
requiere ROM o BIOS.

Las medidas corresponden a una máquina virtual Linux x86_64 identificada como
AMD EPYC 9V74, SDL **2.30.0** de software y C++20 con `-O2`. El SDL incluido en
Android es **2.32.10**; se inspeccionó su backend GLES2. No hubo mediciones de
CPU o GPU en un teléfono Android.

Se probaron letras y símbolos, opacidad del 75 %, estado estable y A pulsado/
soltado en cuadros alternos. Las superficies fueron 960 × 540, 2400 × 1080,
1080 × 2400 y 3120 × 1440, con densidades de prueba de 3, 16, 16 y 20 píxeles/mm.
Son parámetros del experimento, no medidas de la configuración de un Pixel 7 Pro.

Cada caso tuvo 30 cuadros de calentamiento. Las comparaciones finales usan tres
rondas intercaladas y la mediana de las medianas de CPU de cada ronda. Se usó
`CLOCK_THREAD_CPUTIME_ID`, se conservaron además tiempos de pared y los
resultados individuales para exponer la variación entre rondas.

## Llamadas a SDL y trabajo GPU

Con símbolos a 2400 × 1080, el código actual realiza **318 llamadas a
`SDL_RenderGeometry`**, 11 a `SDL_RenderFillRectF` y 330 solicitudes/cambios del
modo de mezcla por dibujo del overlay. A 3120 × 1440 son 324 llamadas de
geometría. El número depende del radio de los círculos y del estilo.

El backend GLES2 incluido, en
`platform/android/third_party/SDL/src/render/opengles2/SDL_render_gles2.c`,
función `GLES2_RunCommandQueue`, ya reúne comandos `GEOMETRY` consecutivos con
la misma textura y modo de mezcla antes de `glDrawArrays`. Por tanto,
**318 llamadas a la API SDL no significan 318 drawcalls GPU**. Reducirlas puede
ahorrar validación y encolado de CPU, sin reducir en la misma proporción el
trabajo de GPU ni los píxeles procesados.

Se ensayaron dos candidatos privados:

- **Agrupación simple:** conserva vértices, índices y orden; acumula geometría
  entre rectángulos. Reduce las llamadas de geometría de 318 a 5.
- **Caché por control:** conserva un paquete de geometría CPU por cada uno de
  los siete controles y reconstruye el control que cambia de geometría,
  apariencia o estado pulsado. Reduce las llamadas a 7. No usa texturas ni un
  render target de pantalla completa.

## Encolado real, con rasterización separada

`SDL_CreateSoftwareRenderer` sobre una superficie no activa por sí solo la cola
de batching. Para separar los costes se usó una ventana del driver `dummy`,
`SDL_CreateRenderer(..., SDL_RENDERER_SOFTWARE)` y `SDL_HINT_RENDER_BATCHING=1`.

El arnés verifica que el dibujo se difiere: lee directamente el píxel de A,
comprueba que permanece intacto después del dibujo y que cambia únicamente tras
`SDL_RenderFlush`. Esta comprobación pasó en las nueve ejecuciones: tres
variantes por tres rondas. Se cronometra `render_host_layers` y después el flush
por separado. El borrado del fondo y las capturas quedan fuera del tramo. Se
midieron 100 cuadros por caso y ronda.

### Símbolos: microsegundos de CPU por cuadro al encolar

| Superficie | Estado | Actual | Agrupación privada | Caché privada |
| --- | --- | ---: | ---: | ---: |
| 960 × 540 | Estable | 36,74 | 26,18 | 15,68 |
| 960 × 540 | A alternando | 36,79 | 26,24 | 16,64 |
| 2400 × 1080 | Estable | 63,43 | 48,03 | 29,50 |
| 2400 × 1080 | A alternando | 62,09 | 47,62 | 34,66 |
| 1080 × 2400 | Estable | 61,77 | 46,96 | 29,89 |
| 1080 × 2400 | A alternando | 61,55 | 46,55 | 35,85 |
| 3120 × 1440 | Estable | 61,76 | 48,27 | 30,92 |
| 3120 × 1440 | A alternando | 63,14 | 49,77 | 35,83 |

El flush de software a 2400 × 1080 costó aproximadamente 2,5 ms. Es trabajo de
CPU del renderer de software, no una estimación del coste GPU de Android. Las
tres variantes dibujan los mismos píxeles.

## Preparación aislada y rasterización completa

Otro experimento reemplazó exclusivamente las llamadas de dibujo de geometría
y rectángulos por contadores, conservando el cálculo del layout y el recorrido
del overlay. Se midieron 5.000 cuadros por caso y ronda. En símbolos a
2400 × 1080, la preparación pasó de **17,80 a 0,42 µs** para estado estable y
de **17,82 a 4,40 µs** con A alternando.

Este experimento excluye deliberadamente validación/cola de dibujo SDL,
rasterización y GPU. Los valores más pequeños están próximos al coste de la
instrumentación. No son tiempos completos de renderizado ni permiten prometer
una mejora equivalente de FPS.

También se midieron dibujo y flush juntos sobre superficies SDL, con 180 cuadros
por caso y ronda. En símbolos estables a 2400 × 1080, las medianas fueron
**2.831,31 µs** en la base y **2.710,48 µs** en la caché, una diferencia del 4,3 %.
Las diferencias no fueron uniformes: a 1080 × 2400 con A alternando, la caché
fue un 2,3 % más lenta. No se restan los tiempos de experimentos distintos para
inferir un coste GPU. Los resultados de cada ronda figuran en el JSON.

## Equivalencia visual y alcance

Se compararon **16 capturas completas por candidato**: cuatro tamaños, dos
estilos y dos estados de A. Todos los bytes RGBA normalizados coinciden con la
base tanto para agrupación como para caché. La comprobación incluye RGB y alfa;
no utiliza una tolerancia perceptiva ni solo el canal alfa.

Los candidatos no modifican la detección de toques. Al decidir no integrarlos,
no se amplió la certificación de sus invalidaciones o ciclo de vida: no se
presentan como cambios listos para producción. Los controles entregados siguen
siendo los ya validados en 0.10.5.

Se revisó también la reaplicación de preferencias: `applyPresentationMode` puede
volver a aplicar opciones al recibir un cambio de pantalla, pero no se ejecuta
por cada cuadro renderizado. No se obtuvo evidencia que justificara atribuirle
la regresión ni cambiar la Activity.

## Reproducción de la medida base

Se publica solamente
[`touch_render_queue_benchmark.cpp`](touch_render_queue_benchmark.cpp), un arnés
del renderer base. Desde la raíz del repositorio, con SDL2 de desarrollo y
`../gbarecomp` preparado con los parches de 0.10.5:

```bash
c++ -std=c++20 -O2 -ffunction-sections -fdata-sections -pthread \
  -DSMA3_FRAME_INTERPOLATION=1 -DGBARECOMP_HAVE_SDL2=1 \
  -I src -I ../gbarecomp/src/runtime $(pkg-config --cflags sdl2) \
  validation/touch_render_queue_benchmark.cpp \
  ../gbarecomp/src/runtime/touch_input.cpp \
  -Wl,--gc-sections $(pkg-config --libs sdl2) -o /tmp/touch-queue-benchmark
/tmp/touch-queue-benchmark 100
```

El programa usa vídeo `dummy`, verifica que el dibujo se difiere y escribe CSV
con tiempos de encolado, flush y contadores de API. `HOST_SOURCE` permite elegir
otra copia preparada del motor. En el entorno original, sin `pkg-config`, se
indicaron las cabeceras SDL de la caché de compilación y se enlazó la biblioteca
del sistema `libSDL2-2.0.so.0`.

Las magnitudes, rondas, hashes y decisión de no integrar los candidatos están en
[`android-0.10.6-controls.json`](android-0.10.6-controls.json).
