# Presentación cooperativa — validación de 0.10.7 beta

## Evidencia y problema reproducido

La captura del usuario muestra **65,2 FPS**, objetivo **120** y modo de
pantalla **120,0 Hz**. No identifica la versión del APK o de Android, filtros,
costes de cada fase ni duración de la sesión. El teléfono se identifica como
Pixel 7 Pro en el historial del usuario; no ha estado conectado al entorno
de desarrollo.

Se revisó la producción 0.10.6, commit
`a76be226174cf2ec9368bfb255054d9a39abe9ff`, sobre el motor fijado en
`e7728148c6829ba526f682876430a0c9022dc6c0`.

La simulación tiene un periodo de aproximadamente **16,742706 ms**. La
planificación anterior calculaba el siguiente fotograma después de presentar
el anterior e intentaba producir la imagen intermedia a **8,371353 ms**,
con unos **1,046419 ms** de tolerancia. El cálculo y la preparación debían
terminar antes de unos **9,42 ms**, aunque el periodo completo ofreciera
más margen.

La reproducción determinista utiliza las clases de producción anteriores,
con **10 ms** de trabajo de juego, **0,75 ms** de preparación por imagen y
**0,25 ms** de intercambio por imagen. Las dos presentaciones requieren
12 ms en total, pero se pierden todos los puntos intermedios: **59,7275 FPS**.
Si uno de cada once cálculos dura 2 ms, el mismo modelo produce
**65,1573 FPS**, que se redondea a 65,2.

Esta coincidencia demuestra un mecanismo compatible con el patrón observado.
**No mide los costes del Pixel ni demuestra que sea la única causa allí.**

## Corrección

En modo 120, la presentación obligatoria utiliza una mezcla de dos imágenes
terminadas. La imagen completa actual queda en una copia inmutable y se
presenta medio periodo después, desde puntos de servicio mientras se calcula
el siguiente fotograma. El orden visual es mezcla, completa, mezcla,
completa; los cambios grandes de escena usan la imagen actual sin mezclar.

Se conserva una espera obligatoria por fotograma del juego. Todo el dibujo
permanece en el hilo existente de SDL. El servicio nuevo no vuelve a avanzar
CPU, PPU ni audio y no procesa entradas o cargas de estado dentro de una
instrucción. El servicio habitual de eventos conserva su cadencia.

**Compensación:** la imagen completa se presenta aproximadamente **8,37 ms
más tarde** que en la ruta anterior. No se han creado 120 estados
independientes de la simulación. La mezcla puede producir estelas.

El presupuesto reserva trabajo del juego, preparación del historial,
preparación e intercambio de ambas imágenes, 600 µs de adelanto de servicio
y 100 µs de margen. Los sondeos anticipados no esperan ni dibujan. Si el
trabajo no cabe o la imagen ya llega tarde, se omite la presentación extra.
El historial se captura sin calcular una mezcla que no se utilizará.
El presupuesto total se vuelve a comprobar después del dibujo obligatorio
y antes de prestar servicio a la copia pendiente: una subida de coste que
ya se ha medido invalida la admisión anterior.

La copia pendiente se invalida por cambio de época o generación del reloj,
cambio de modo/estado, reloj hacia atrás o antigüedad superior a 100 ms.
Ambos caminos del runtime resuelven esa copia antes de cambiar dimensiones.
Los callbacks se limpian también ante salidas anticipadas del runner.

## Pruebas deterministas y sanitizadores

| Prueba | Resultado |
| --- | --- |
| Temporización existente adaptada | 4.992 comprobaciones, 360 fotogramas simulados, PASS |
| Presentación cooperativa | 71 comprobaciones, PASS |
| Revisión independiente de casos límite | 44 comprobaciones, PASS |
| Contador base/extra y tiempos | 8 comprobaciones, PASS |
| Interpretación Java del contador nativo | 13 comprobaciones, PASS |
| AddressSanitizer + UndefinedBehaviorSanitizer | Ambas pruebas de temporización pasan |
| Pila de parches | 14 parches aplicados desde sus 15 estados reconocidos; idempotencia y rechazo de cambios ajenos verificados |

LeakSanitizer no pudo inspeccionar las tareas de `/proc` en este entorno.
Las pruebas de ASan/UBSan se completaron con `detect_leaks=0`; no se afirma
una comprobación de fugas.

Con los costes declarados de preparación/intercambio, los modelos con
cálculos de **2, 10, 12 y 14 ms** conservan **59,7275 fotogramas del juego/s**
y completan **119,455 presentaciones/s**. Los casos que se acercan demasiado
a la saturación o la exceden omiten las imágenes extra. La ausencia del
servicio conserva el ritmo normal y no inventa presentaciones.

Las pruebas incluyen: orden de mezclas/copias, imagen pendiente inmutable,
sondeos anticipados, terminación rápida del juego, servicio atrasado,
recuperación, cambios de modo/época/tamaño, reinicio externo del reloj,
pausa larga, cambios de escena, avance rápido y fallo del renderizador.

La revisión detectó defectos del prototipo antes de la entrega: mostrar
una copia antigua después de reiniciar el reloj y omitir la pequeña espera
opcional al calcular el presupuesto. La revisión final detectó además una
admisión que conservaba costes anteriores al dibujo obligatorio, aunque ya
se conociera una subida. Los tres casos tienen pruebas de regresión.

## Comparación con SDL real en host

Se compilan las ramas de presentación Android de `HostWindow` con
**SDL 2.30 software**, vídeo dummy, Linux x86_64 y GCC 13.3. Se utiliza el
filtro Píxel nítido y se dibujan controles. El trabajo del juego es una
espera sintética dividida en tramos de 1 ms; **no es ejecución de la ROM**.
Ambas versiones usan la misma contabilidad de ese trabajo. La versión nueva
puede prestar servicio a la presentación entre tramos.

Cada caso tiene 30 fotogramas de calentamiento y 120 medidos. Las dos
rondas iniciales, 24 medidas, se conservan como resultados anteriores a
la última corrección de costes y con los hashes de esas fuentes. La primera
podía coincidir con compilación; en la segunda se pausaron compilación
nativa y pruebas Fuzzy.

Después de corregir la admisión con costes desactualizados se repite una
matriz completa de **12 medidas del código final**, con compilación y Fuzzy
pausados. Se conserva también una pareja puntual de comprobación del código
final: 38 medidas en total. Los datos de **todas las rondas y casos** están en
[`android-0.10.7-cooperative-pacing.json`](android-0.10.7-cooperative-pacing.json).

La matriz del código final produjo:

| Superficie, carga simulada y modo | Presentación 0.10.6 | Presentación 0.10.7 | Ritmo del juego 0.10.6 → 0.10.7 |
| --- | ---: | ---: | ---: |
| 480 × 320, 2 ms, modo 120 | 118,95 | 117,46 | 59,73 → 59,73 |
| 480 × 320, 10 ms, modo 120 | 59,73 | 118,46 | 59,73 → 59,73 |
| 480 × 320, 12 ms, modo 120 | 59,73 | 98,05 | 59,73 → 59,73 |
| 480 × 320, 12 ms, modo 60 | 59,73 | 59,23 | 59,73 → 59,23 |
| 960 × 540, 10 ms, modo 120 | 59,73 | 64,21 | 59,73 → 59,73 |
| 2400 × 1080, 12 ms, modo 120 | 59,66 | 59,46 | 59,66 → 59,46 |

Todas las tasas son imágenes por segundo; ninguna fila es una medida del
teléfono. El resultado principal es que el cálculo que excede medio periodo
ya no elimina necesariamente todas las presentaciones adicionales.

**Límites visibles:** no todos los casos mejoran. La muestra final del
modo 60 baja a 59,23 aunque ninguna versión intenta imágenes opcionales;
las rondas anteriores de ese modo estaban alrededor de 59,73. En la
superficie mayor ambas omiten las imágenes extra. La pareja puntual de
960 × 540 con el mismo código final dio 86,00 presentaciones/s y 59,31
fotogramas del juego/s, frente a 64,21 y 59,73 en la matriz final.

Los registros agregados no permiten atribuir toda esa variación a un único
origen. Se corrige el uso de un coste antiguo cuando ya se ha medido uno
nuevo, pero una estimación no anticipa todos los picos futuros ni retrasos
del sistema. Aumentar más el margen descartaría más imágenes adicionales;
no acreditaría 120 FPS sostenidos. Estos datos tampoco permiten trasladar
las tasas del renderer software a EGL y la GPU del teléfono.

## Contador y comprobación en el dispositivo

El contador cuenta llamadas SDL de presentación completadas. La primera
muestra establece el inicio de la ventana; no añade un fotograma ficticio.
Base/extra usan esa misma ventana. Preparar/presentar son promedios de sus
tiempos medidos, excluyendo las esperas intencionales. Se actualiza la
etiqueta al ritmo anterior, aproximadamente dos veces por segundo.

Se verifican cifras bajas reales, separación de base/extra, reinicio de
ventana al reanudar/cambiar de modo, precisión decimal, títulos antiguos y
rechazo de títulos incompletos. El resultado no se redondea al objetivo 120.

Para comprobar el APK en el Pixel, instala 0.10.7 encima del anterior y
repite la zona de la captura con los mismos filtros y modo de imagen.
Registra versión de Android, presentación, base/extra, preparar/presentar,
Hz de pantalla/app y lectura después de varios minutos. Compara también
el modo 60 y el comportamiento al girar, pausar y reanudar.

## Construcción y continuidad del juego

El APK usa el mismo paquete y certificado, versión `0.10.7-beta` y código
18. La construcción regenera el archivo ARM64 principal a partir de los
64 fragmentos de producción verificados; no utiliza el archivo antiguo
recuperado de un respaldo. Las fuentes de los dos efectos Fuzzy coinciden
con las de producción 0.10.6.

La comparación Fuzzy usa dos ejecuciones de 1.500 fotogramas, en anchuras
240 y 356. En cada anchura coinciden las 1.501 filas del seguimiento y las
33 capturas PNG entre referencia y candidato, con recuperación del efecto.
Coinciden CPU, RAM, memoria de vídeo, PPU, audio y guardado. El archivo de
estado conserva una diferencia en el timestamp anfitrión de GbaRtc, que
está inactivo; no se afirma identidad binaria de todo el estado. Las pruebas
nativas de efectos pasan 144 escenarios y 3.422 comprobaciones frente al
intérprete. Son pruebas de host, no de contacto con Fuzzy en el APK físico.

La verificación exacta del APK, fuentes y pruebas de Fuzzy queda en
[`android-0.10.7-build.json`](android-0.10.7-build.json). Compilar, verificar
firmas y ejecutar pruebas de host no equivale a instalar o jugar el APK
en un teléfono físico.

## Reproducción de las pruebas pequeñas

Desde la raíz del proyecto, con el motor fijado y los parches aplicados:

```bash
g++ -std=c++20 -O2 -DSMA3_FRAME_INTERPOLATION=1 -Isrc -I../gbarecomp/src/runtime validation/presentation_pacing_test.cpp ../gbarecomp/src/runtime/host_platform.cpp -pthread -o /tmp/sma3-pacing-test
/tmp/sma3-pacing-test
g++ -std=c++20 -O2 -Isrc validation/cooperative_presentation_test.cpp -o /tmp/sma3-cooperative-test
/tmp/sma3-cooperative-test
g++ -std=c++20 -O2 -Isrc validation/presentation_metrics_test.cpp -o /tmp/sma3-metrics-test
/tmp/sma3-metrics-test
javac -d /tmp/sma3-stats-classes android/java/org/gbarecomp/PresentationStats.java validation/PresentationStatsTest.java
java -cp /tmp/sma3-stats-classes org.gbarecomp.PresentationStatsTest
```

Las opciones y fuentes del banco SDL están en
[`pacing_render_benchmark.cpp`](pacing_render_benchmark.cpp); los hashes y
la procedencia de la referencia están en el informe JSON. SDL software no
reproduce EGL, el compositor, la GPU ni la temperatura del Pixel.
