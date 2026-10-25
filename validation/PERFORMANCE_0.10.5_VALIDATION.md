# Presentación 60/120 — validación de 0.10.5 beta

## Resultado y alcance

Se corrigen causas verificables de irregularidad en la planificación de
la presentación. Las opciones **60 FPS** y **120 FPS** conservan el reloj
original de GBA: aproximadamente **59,7275 estados por segundo** y, con
mezcla temporal, hasta **119,455 imágenes por segundo**. No se ejecutan dos
pasos de juego por seleccionar 120 y no se modifica el reloj para que el
contador muestre un número redondo.

Pasan las pruebas de las reglas de planificación, del contador y de la
selección de frecuencia. También compilan las unidades nativas modificadas
con los argumentos ARM64 de la construcción Android y los ayudantes Java
con API 35. Estas comprobaciones **no certifican 60/120 FPS sostenidos en
el Pixel 7 Pro**: no hay prueba física de GPU, temperatura o batería en esta
entrega. La construcción completa del APK se registra por separado.

## Problemas encontrados

1. El modo de presentación normal intercambiaba la imagen **antes** de
   esperar al siguiente plazo. Una duración variable del trabajo del juego
   o de los filtros se convertía en variación de los intervalos de salida.
2. La ruta de mezcla esperaba antes de preparar el dibujo. SDL además puede
   acumular órdenes hasta `SDL_RenderPresent`, trasladando trabajo variable
   después del momento planificado.
3. Un punto intermedio podía aceptarse hasta aproximadamente **4,19 ms**
   tarde, acercándolo demasiado a la imagen completa siguiente.
4. La solicitud Android escogía explícitamente un modo de 120 Hz, pero no
   uno de 60 Hz. Un cambio de pantalla solo actualizaba el texto; podía
   mantenerse la doble presentación cuando Android limitaba la app a 60 Hz.
5. El contador incluía la primera muestra como si ya hubiera transcurrido
   un intervalo. Tampoco distinguía una nueva sesión de presentación tras
   permanecer en segundo plano con el modo de 60 seleccionado.

## Cambios de producción

### Preparar, esperar y presentar

El undécimo parche, [`gbarecomp-frame-pacing.patch`](../patches/gbarecomp-frame-pacing.patch),
añade `HostWindow::present_paced`. La entrada anterior `present` conserva
su firma y delega sin plazo. La nueva ruta prepara textura, filtro y
controles, envía las órdenes acumuladas mediante `SDL_RenderFlush` y espera
**inmediatamente antes** de `SDL_RenderPresent`. La espera puede solaparse
con el trabajo enviado a la GPU; no se introduce un `glFinish` ni una espera
ocupada de finalización de GPU.

[`PresentationPacing`](../src/presentation_pacing.h) coordina las dos
selecciones. Cada fotograma del juego consume **exactamente una espera
completa**. El punto intermedio es opcional y no avanza el reloj del juego.
Si su preparación termina demasiado tarde, se omite y se conserva el plazo
de la imagen completa. La tolerancia del punto intermedio se limita a un
dieciseisavo del periodo de GBA, aproximadamente **1,05 ms**. Eso no promete
que siempre exista presupuesto para 120 imágenes.

El planificador reinicia su fase y el historial de mezcla cuando cambia
el modo efectivo, se reanuda una sesión, cambia la superficie, hay una
discontinuidad del contador de fotogramas o un intervalo largo. Una textura
o ventana perdida no deja al juego sin límite: la espera completa sigue
ejecutándose. La ruta de avance rápido conserva la espera que ya controlaba
el bucle exterior.

La espera usa sueño para la mayor parte del intervalo y una cola de
`yield` adaptativa de **100–1.500 microsegundos**. Las observaciones de
despertar tardío aumentan ese margen; los intervalos tranquilos lo reducen.
Se mantiene el tratamiento existente de retrasos largos, sin intentar
recuperarlos mediante una ráfaga ilimitada de estados del juego. No se
cambian la prioridad del proceso ni los controles térmicos del sistema.

### Selección Android

[`DisplayRefreshController`](../android/java/org/gbarecomp/DisplayRefreshController.java)
utiliza [`RefreshRatePolicy`](../android/java/org/gbarecomp/RefreshRatePolicy.java)
para elegir un modo de 60 o 120 Hz con **la misma resolución física**. Si
no existe uno exacto, puede usar un múltiplo entero; no fuerza un cambio de
resolución para encontrar una frecuencia mayor. Las solicitudes de ventana
y superficie se deduplican para evitar cambios repetidos en los callbacks.

La frecuencia del modo físico y la informada para la aplicación se
mantienen separadas. La segunda puede incluir una limitación de Android.
La doble presentación solo se habilita con actividad reanudada, superficie
válida y ambas frecuencias por encima de 119 Hz. La elección guardada sigue
siendo 120 aunque el modo efectivo temporal sea 60; el menú puede explicar
esa diferencia. La migración conserva la preferencia `interpolation-120`.

El controlador usa las API públicas `Surface.setFrameRate` con compatibilidad
normal para juegos y, desde API 31, cambio de frecuencia sin interrupción.
`invalidateSurface` permite volver a solicitar la frecuencia cuando Android
reutiliza el objeto Java de una superficie recreada; no se usan métodos
ocultos ni reflexión.

La [guía oficial de frecuencia de fotogramas](https://developer.android.com/media/optimize/performance/frame-rate)
explica que estas solicitudes orientan al sistema y que deben evitarse
llamadas continuas. La [documentación de cambios de compatibilidad de Android 12](https://developer.android.com/about/versions/12/reference/compat-framework-changes)
describe la distinción entre el modo físico y las frecuencias informadas
para la aplicación. El sistema conserva la decisión sobre el modo disponible.

### Contador medido

`PresentationRateCounter` usa el contador monotónico de alta resolución
de SDL y los intervalos entre llamadas de presentación completadas. La
primera muestra establece el inicio; no añade una imagen ficticia a la
medición. El cambio de modo y el contador de sesión `presentation_epoch`
inician una ventana nueva. `resetPresentationTiming()` se llama desde los
eventos de reanudación y creación real de superficie, sin reiniciarse
continuamente desde las actualizaciones del contador.

Un retraso ordinario durante el juego **permanece** en la medición. Solo
las transiciones explícitas de sesión o modo descartan la ventana anterior.
El número mide salida de SDL; no mide directamente los barridos físicos
del panel ni la cantidad de estados independientes del juego.

## Pruebas ejecutadas

| Prueba | Resultado |
| --- | --- |
| Planificador, espera y contador C++ | **4.897 comprobaciones PASS**, optimizado y ASan/UBSan. |
| Carga variable de preparación | **360 fotogramas de juego simulados**, mitad en cada modo, con trabajo variable antes del intercambio. Los plazos completos permanecen separados por un periodo y cada fotograma espera exactamente una vez. |
| Punto intermedio | Límite de tolerancia, llegada tardía, omisión y conservación de la imagen completa comprobados. |
| Cambios de estado | Cambio 120→60, reanudación, salto de fotogramas, avance rápido y textura perdida comprobados. |
| Espera adaptativa | 1.200 intervalos con despertares simulados variables y 1.000 tranquilos; nunca retorna antes del plazo, mantiene los límites y reduce la cola en los intervalos tranquilos. |
| Contador | Primera muestra, 59,7275/119,455, reanudación sin tiempo de fondo, cambio de modo, reloj regresivo y caída real dentro de una sesión comprobados. |
| Temporizador del motor | Prueba de humo con `FramePacer` real: 20 periodos en aproximadamente 0,335 s. Es una comprobación del reloj del equipo de desarrollo. |
| Política Java | **31 comprobaciones PASS**: 60/120, resoluciones distintas, tasas fraccionarias, múltiplos, panel limitado, valores inválidos y actividad/superficie no disponibles. |
| Compilación Android | `host_platform.cpp`, `host_window.cpp` y `runtime.cpp` pasan comprobación de sintaxis con sus argumentos reales ARM64; los dos ayudantes Java compilan con JDK 17/API 35. |

La simulación de preparación usa los encabezados de producción y una
superficie/temporizador deterministas para poder comprobar el orden y los
intervalos sin depender del planificador del equipo. La prueba real de
`FramePacer` comprueba por separado la espera de reloj. No se transforma
ese resultado en una afirmación de FPS de Android.

AddressSanitizer y UndefinedBehaviorSanitizer permanecen activos;
LeakSanitizer está desactivado por las restricciones del entorno. El
encabezado histórico `touch_layout.h` produce un aviso de indentación;
se conserva y únicamente se exceptúa esa categoría de `-Werror` en la
compilación de la prueba. No se modifica el código de Fuzzy ni la
reconstrucción panorámica como parte de estos cambios.

Resultados y hashes:
[`android-0.10.5-performance.json`](android-0.10.5-performance.json).

## Reproducción

Con el motor fijado y los parches aplicados, desde la raíz del repositorio:

```sh
c++ -std=c++20 -O2 -Wall -Wextra -Werror -Wno-error=misleading-indentation \
  -pthread -DSMA3_FRAME_INTERPOLATION=1 -I src -I ../gbarecomp/src/runtime \
  validation/presentation_pacing_test.cpp ../gbarecomp/src/runtime/host_platform.cpp \
  -o /tmp/sma3-pacing-test
/tmp/sma3-pacing-test

mkdir -p /tmp/sma3-refresh-test
javac -encoding UTF-8 -source 17 -target 17 -d /tmp/sma3-refresh-test \
  android/java/org/gbarecomp/RefreshRatePolicy.java validation/RefreshRatePolicyTest.java
java -cp /tmp/sma3-refresh-test org.gbarecomp.RefreshRatePolicyTest
```

Para repetir C++ con sanitizadores, sustituye `-O2` por
`-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
-fno-sanitize-recover=all` y ejecuta con
`ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1`.
