# Rendimiento — corrección y verificación de 0.10.6 beta

## Resultado y alcance

Se corrige una regresión comprobada de la ruta de presentación 120 de
**0.10.5 beta**, base `40849aeef03a5cdd569a19f35034c8233c9520d2`.
La imagen intermedia opcional se preparaba por completo antes de comprobar
si llegaba a tiempo. Cuando el trabajo previo consumía el intervalo disponible,
se pagaban conversión de color, subida de textura, filtros y controles para
una imagen que después se descartaba. Ese trabajo podía retrasar también
la imagen completa y el siguiente ciclo del juego.

La corrección comprueba el presupuesto **antes de mezclar y renderizar**.
Usa el tiempo observado de preparación y de presentación y reserva tiempo
para preparar la imagen completa. Un pico aumenta inmediatamente la estimación;
esta baja gradualmente cuando desaparece la carga. Si se omite una imagen
intermedia, se guarda el fotograma actual para que la siguiente mezcla utilice
dos imágenes consecutivas. La comprobación se repite después de mezclar y
antes de convertir colores, subir texturas o dibujar.

El fotograma completo conserva una sola espera. Las solicitudes de 60/120 Hz,
el contador real, la política de segundo plano, las preferencias de imagen,
los controles y sus zonas táctiles mantienen su comportamiento de 0.10.5.
La cadencia original sigue siendo aproximadamente 59,7275 Hz; el objetivo
120 utiliza mezcla temporal, no 120 estados independientes del juego.

## Reproducción de la regresión

Se ejecutaron los `HostWindow` y `FramePacer` reales con sus ramas de Android,
SDL por software en Linux x86-64, ventana de **2400 × 1080**, filtro Nearest
y controles visibles. Una espera sintética de 2 o 12 ms sustituye al trabajo
del juego. Cada caso mide 120 ciclos después del calentamiento; se intercalan
dos rondas de las versiones 0.10.4, 0.10.5 y del candidato final.

**Este arnés no ejecuta una partida, un APK ni la GPU Android.** Sus ciclos
por segundo permiten comparar el mismo renderizador y la misma carga
simulada, sin convertirlos en una medición de FPS del Pixel 7 Pro.

| Caso | 0.10.5 | 0.10.6 | Interpretación |
| --- | ---: | ---: | --- |
| Objetivo 120, carga sintética de 12 ms: ciclos completos/s | 48,35–48,91 | 59,68–59,73 | Dejar de preparar imágenes descartadas recupera el intervalo de la imagen completa en este caso. |
| Mismo caso: subidas y flushes por 120 presentaciones reales | 240 | 120 | Se elimina la preparación duplicada que no llegaba a mostrarse. |
| Mismo caso: CPU de render/temporización por ciclo | 8,37–8,61 ms | 4,18–4,19 ms | Excluye la espera sintética y no representa tiempo GPU. |
| Objetivo 120, carga sintética de 2 ms: presentaciones/s | 119,45 | 116,83–117,96 | El presupuesto conservador omite algunas imágenes extra ante picos; no promete elevar siempre el contador 120. |
| Objetivo 60, carga sintética de 2 ms: ciclos completos/s | Aproximadamente 59,73 | Aproximadamente 59,73 | La ruta de una imagen mantiene su cadencia en este caso. |

La comparación a 60 con 12 ms se aproxima al límite del host y varía entre
rondas: 59,10–59,73 ciclos/s en 0.10.5 y 57,50–59,48 en el candidato. No se
oculta esa variación ni se deduce una mejora universal. Las **24 filas**,
los tiempos de preparación/flush/presentación y las seis ejecuciones del
temporizador están en [el informe de pacing](PACING_0.10.6_VALIDATION.md)
y [su JSON](android-0.10.6-pacing.json).

Se mantiene `SDL_RenderFlush` antes de esperar: en el SDL fijado, el flush
ejecuta la cola y la presentación posterior no vuelve a ejecutar los mismos
comandos. Retirarlo trasladaría esa preparación detrás de la espera sin un
ahorro demostrado. El envío de comandos no demuestra que la GPU haya terminado.
No se atribuye la regresión a dos esperas VSync ni al número de llamadas SDL.

## Optimizaciones adicionales de renderizado

Se especializan Scale2x y Scale3x por factor y recorrido de filas. Para
perfiles diagonales de al menos 256 píxeles, tres tablas locales de 256 bytes
conservan exactamente redondeo y saturación. Las matrices generales conservan
su bucle anterior. En calidad nativa y escalado entero exacto se evita la
textura intermedia y se pasa de dos copias SDL a una; los tamaños fraccionarios
y las resoluciones explícitas 720p/1080p conservan su procesamiento.

Medianas de CPU del host para una imagen sintética de 356 × 160 con bloques
de pixel art:

| Componente | 0.10.5 | 0.10.6 |
| --- | ---: | ---: |
| Scale2x | 0,4553 ms | 0,2476 ms |
| Scale3x | 0,6177 ms | 0,5221 ms |
| Perfil cálido diagonal | 0,2785 ms | 0,0450 ms |

Son costes de componentes concretos, no porcentajes de mejora de toda la app.
La composición SDL entera 2160 × 1440 mejoró de 24,1060 a 21,6960 ms; la
fraccionaria 1620 × 1080 permaneció en 17,2664/17,2809 ms. También se conserva
el caso Scale2x no elegible que varió +2,7 %. Véanse
[método, resultados y equivalencia](RENDER_0.10.6_PERFORMANCE.md) y
[datos completos](android-0.10.6-render.json).

## Verificaciones

- **4.995 comprobaciones de pacing:** límites de admisión, reserva del
  fotograma completo, costes de preparación y bloqueo, recuperación de picos,
  historial al omitir/reanudar mezclas, cambios de modo y ciclo de vida.
  Pasan optimizadas y con ASan/UBSan.
- **2.322.138 aserciones de filtros** sobre 42.294 vecindarios y 18 imágenes,
  y **12.722 de color** sobre 636 matrices. Los oráculos pasan también con
  ASan/UBSan.
- **714 presentaciones pareadas**, con coincidencia exacta de los
  3.669.932.868 bytes RGBA comparados. Incluyen los siete filtros, tres calidades,
  fuentes de 240/356 × 160 y escalas enteras/fraccionarias; 120 casos
  elegibles eliminan una pasada sin cambiar los píxeles.
- Pila real de **13 parches**, **14 prefijos** reconocidos, aplicación
  idempotente y rechazo sin escrituras de cambios ajenos en **8 archivos**
  protegidos. Los doce parches anteriores se conservan; el nuevo es
  `gbarecomp-render-performance.patch`.
- Compilación ARM64 y revisión del APK final, incluidas firma, identidad,
  versión, bibliotecas, enlaces JNI y activos, registradas en
  [android-0.10.6-build.json](android-0.10.6-build.json).

La revisión de controles encontró que 318 llamadas a geometría SDL no
equivalen a 318 drawcalls GPU: el backend ya agrupa comandos compatibles.
El candidato privado de caché ahorraba aproximadamente **0,034 ms** en
encolado CPU por cuadro a 2400 × 1080. Esa magnitud no justifica atribuirle
la regresión. **No se integra la caché ni la agrupación propuestas**; se
conservan símbolos, letras, opacidad y editor existentes. El diagnóstico y
sus límites están en [CONTROLS_0.10.6_PERFORMANCE.md](CONTROLS_0.10.6_PERFORMANCE.md).

## Límites y prueba pendiente en dispositivo

No se midieron FPS sostenidos, GPU, consumo ni temperatura en el Pixel 7 Pro.
Las optimizaciones no pueden garantizar una tasa fija contra carga del juego,
limitación térmica o decisiones de Android. La simulación y el audio no se
aceleran para maquillar el contador. La elección de filtro o resolución del
usuario no se reduce automáticamente.

La prueba física debe comparar la misma zona con 0.10.5/0.10.6, los mismos
ajustes y condiciones térmicas, en 60/120 y con pantalla/app a la frecuencia
solicitada. Hay una secuencia práctica en
[Rendimiento y FPS](../docs/RENDIMIENTO_Y_FPS.md#comprobación-en-el-teléfono).
Las fuentes de Fuzzy, la cobertura panorámica, la lógica del juego y los
formatos de guardado permanecen iguales; no se declara una nueva campaña
completa ni una nueva validación física de esos componentes.
