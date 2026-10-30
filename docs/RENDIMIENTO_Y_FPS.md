# Rendimiento y objetivos de 60/120 FPS — 0.10.12 beta

Abre **Atrás → Rendimiento y FPS → Objetivo de presentación** y elige
**60 FPS** o **120 FPS · mezcla 2×**. La elección se guarda al tocarla.
Activa **Mostrar FPS** para comparar la salida medida con el modo de pantalla.

| Opción | Comportamiento |
| --- | --- |
| **60 FPS** | Solicita un modo de pantalla próximo a 60 Hz y presenta al ritmo original del juego: aproximadamente 59,73 imágenes por segundo. |
| **120 FPS · mezcla 2×** | Solicita 120 Hz y, cuando Android concede esa tasa, intercala mezclas de imágenes consecutivas: aproximadamente 119,45 presentaciones por segundo. |

La simulación mantiene su cadencia original, aproximadamente **59,7275 Hz**.
La opción 120 añade imágenes de presentación; no genera 120 estados
independientes del juego. La mezcla puede producir estelas. El código conserva
la detección de cambios de escena para evitar mezclar imágenes incompatibles.

## Correcciones de 0.10.12

Un tirón aislado podía seguir suprimiendo imágenes adicionales después de
volver a una carga normal, porque su duración se conservaba en una estimación
que decae lentamente. Ahora esa estimación se limita al periodo del juego:
un coste de ese tamaño ya impide admitir trabajo adicional. Se mantienen los
márgenes, los plazos y el rechazo de sobrecarga continua.

En una prueba controlada con un único pico de 68 ms, la recuperación pasa de
13 a 3 fotogramas si el pico ocurre en el cálculo del juego; de 26 a 16 si
ocurre al preparar la imagen; y de 28 a 17 si ocurre al intercambiar buffers.
Son resultados del planificador con reloj controlado. **Los FPS y tiempos
mostrados siguen usando mediciones reales completas.**

Además, se reduce el trabajo del reloj de vídeo, Scale2x/Scale3x evitan una
subida de textura redundante y las salidas que ya coinciden con 720p/1080p
omiten una copia final 1:1. La comparación de 568 salidas conserva exactamente
los mismos píxeles. El filtro y la calidad seleccionados se mantienen.

La ruta de Android sigue solicitando 120 Hz y verificando la tasa concedida a
la app. No se encontró un bloqueo fijo a 60 Hz que justificara quitar esa
comprobación. La capacidad de mantener mezcla 2× depende del trabajo de cada
escena, filtros y dispositivo. No se ha certificado esta entrega en el Pixel.

[Pruebas completas de 0.10.12](../validation/PERFORMANCE_0.10.12_VALIDATION.md) ·
[Notas y APK](releases/v0.10.12-beta.md).

## Planificación incorporada en 0.10.7

Se corrige una limitación de la planificación anterior. Después de presentar
la imagen completa, el mismo hilo calculaba el siguiente fotograma del juego.
Para añadir su mezcla intermedia, ese trabajo y la preparación de la imagen
debían terminar antes de unos **9,42 ms**. Un cálculo de 10–12 ms podía mantener
el juego a su ritmo original y, aun así, perder casi todas las imágenes extra.
Tener una pantalla a 120 Hz no elimina esa restricción del programa.

El modo 120 distribuye ahora el trabajo así:

1. En cada plazo del juego se presenta la mezcla de las dos imágenes terminadas.
   Se utiliza la imagen actual sin mezcla al comenzar, ante un cambio de escena,
   o si la imagen completa anterior no pudo presentarse.
2. Una copia inmutable de la imagen completa se presenta aproximadamente
   **8,37 ms después**, durante un punto de servicio del cálculo del siguiente
   fotograma. Todo el dibujo permanece en el mismo hilo de SDL.
3. El juego conserva **una sola espera de periodo completo por fotograma**.
   El siguiente cálculo puede ocupar más de medio periodo sin perder por ello
   todas las presentaciones adicionales.

Esta distribución retrasa la salida de la imagen completa medio periodo
respecto a la ruta anterior; es una compensación de latencia del modo 120,
no una aceleración de la simulación. La mezcla sigue pudiendo producir estelas.

La admisión considera el trabajo observado del juego, la copia/mezcla,
la preparación, el intercambio de buffers y la pequeña espera de servicio.
Una carga que no cabe en el periodo reduce las imágenes extra para conservar
el ritmo del juego. Las imágenes que ya llegan tarde se descartan. No se
presentan ráfagas atrasadas para aumentar el contador.

Cambiar de modo de imagen, pausar/reanudar, cargar una partida, reiniciar el
reloj o recrear la superficie invalida las imágenes pendientes. Antes de
cambiar el tamaño lógico se resuelve o descarta la imagen del tamaño anterior.
El servicio adicional no vuelve a ejecutar CPU/PPU/audio del juego ni procesa
entradas o cargas de partida dentro de una instrucción.

La [validación de 0.10.7](../validation/PERFORMANCE_0.10.7_VALIDATION.md) separa
la reproducción con reloj simulado, el renderizado real de SDL en host y las
comprobaciones del APK. La captura del usuario muestra **65,2 FPS**, objetivo
120 y pantalla a **120,0 Hz**; no revela la versión de Android, el APK exacto,
los filtros ni los costes de cada fase. Las pruebas de desarrollo no son una
medición del nuevo APK en el Pixel 7 Pro.

## Optimizaciones conservadas de 0.10.6

0.10.6 corrigió una regresión de 0.10.5: una imagen intermedia podía pasar por
perfiles de color, subida de textura, filtros y controles para ser descartada
después por llegar tarde. Se comprueba el plazo **antes de preparar
esa imagen opcional**, usando el coste observado de preparación y presentación
y reservando tiempo para la imagen completa. Si no hay presupuesto, solo se
conserva el historial necesario para la próxima mezcla.

La presentación obligatoria mantiene su única espera y el juego conserva su velocidad.
Al recuperarse de un pico de carga, el presupuesto vuelve a permitir imágenes
intermedias. No se cambian automáticamente el filtro, la resolución, el modo
de imagen ni la preferencia de 60/120 elegidos por el usuario.

También se especializan las operaciones de Scale2x/Scale3x, se evita una
textura intermedia cuando el escalado es un múltiplo entero exacto y se
aceleran los perfiles de color que actúan sobre cada canal por separado.
Las rutas fraccionarias y de resolución 720p/1080p conservan su tratamiento.

Las mejoras y sus mediciones se documentan en
[PERFORMANCE_0.10.6_VALIDATION.md](../validation/PERFORMANCE_0.10.6_VALIDATION.md).
Son comparaciones de componentes en el equipo de desarrollo; no son una
medición de FPS del Pixel 7 Pro.

## Historial de la temporización incorporada en 0.10.5

Antes de 0.10.5 se preparaba y presentaba la imagen de 60 FPS antes de
esperar el final del periodo. Si variaba el tiempo de preparación, también
variaba el instante visible de la presentación. Desde 0.10.5 se prepara la textura,
el filtro y los controles, se envían los comandos de dibujo pendientes y
se espera **justo antes de presentar**. Existe una sola espera de periodo
completo por fotograma simulado.

Las esperas de ambos modos comparten un margen adaptativo acotado. Se duerme
durante la mayor parte del intervalo y se cede el procesador durante el
tramo final. Un punto intermedio que llega demasiado tarde se descarta en
lugar de comprimir dos presentaciones en un intervalo corto. La tolerancia
del punto intermedio pasa a un dieciseisavo del periodo, aproximadamente
1,05 ms. Esto puede reducir el número de imágenes extra bajo carga: el
contador muestra el resultado real.

Cambiar de modo, reanudar la aplicación o recrear la superficie restablece
la fase de presentación y el historial de mezcla. Una pausa larga no se
intenta recuperar con una ráfaga de imágenes. El contador inicia una nueva
ventana al reanudar o cambiar de modo; las caídas normales durante la partida
sí forman parte de la medición.

La solicitud de refresco se vuelve a evaluar cuando cambia la pantalla o
la superficie y al entrar/salir de primer plano. No se envía una solicitud
nueva por fotograma. Se eligen modos de la **misma resolución física** y,
en Android 12 o posterior, la solicitud de Surface limita los cambios a los
que pueden realizarse sin una transición no continua de pantalla.

## Si Android no concede 120 Hz

La preferencia de 120 se conserva. Mientras el modo físico o la tasa
informada para la aplicación sea inferior a 119 Hz, se utiliza la
presentación normal de 60. Si Android vuelve a conceder 120 Hz, se
reactivan las imágenes intermedias mediante el aviso de cambio de pantalla.

El menú y el contador distinguen:

- **Objetivo:** la opción elegida, 60 o 120.
- **Presentación:** llamadas de presentación SDL completadas por segundo,
  calculadas con un reloj de alta resolución.
- **Base y extra:** descomposición de las llamadas de presentación completadas
  en la misma ventana de medida. Base corresponde al plazo del juego; en el
  modo 120 puede ser una mezcla. Extra corresponde al plazo de medio periodo.
  No son contadores de estados independientes de la simulación.
- **Preparar:** promedio en milisegundos de conversión/perfiles, subida, filtros,
  controles y envío de comandos SDL de las imágenes que se presentaron.
- **Presentar:** promedio en milisegundos dentro de la llamada de intercambio
  de buffers. Ambos tiempos excluyen la espera intencional del temporizador;
  no representan una medición aislada de la GPU.
- **Modo de pantalla:** los Hz del modo físico informado por Android.
- **App:** la tasa informada para la aplicación, cuando difiere del modo
  físico. Android puede mantener el panel a 120 Hz y conceder menos a una app.

El contador de presentación no mide qué imágenes termina mostrando el
compositor de Android. Un panel de 120 Hz tampoco demuestra que el juego
mantenga 120 presentaciones por segundo.

**La aplicación no puede imponer una tasa estable contra límites del
dispositivo, temperatura, ahorro de batería o decisiones de Android.**
Estos cambios corrigen problemas comprobables de programación de la
presentación; no acreditan estabilidad sostenida en un Pixel 7 Pro.

## Comprobación en el teléfono

1. Instala 0.10.12 sobre la versión anterior, sin desinstalarla, y anota la versión de Android.
2. Activa el ajuste de pantalla fluida/alta frecuencia del teléfono y
   comprueba su modo de ahorro de batería.
3. Empieza con Original (3:2), filtro Equilibrado y resolución Automática.
   Activa Mostrar FPS y compara 60 y 120 en la misma zona del juego.
4. Repite con Panorámica (beta) y con los filtros que quieras utilizar.
   Anota objetivo, presentación, base/extra, preparar/presentar, Hz de pantalla
   y, si aparece, tasa de app. Repite la zona de la captura de 65,2 FPS.
5. Cambia varias veces de 60 a 120, gira el teléfono, abre otra aplicación,
   vuelve al juego y apaga/enciende la pantalla. Comprueba sonido y controles.
6. Mantén una sesión de al menos diez minutos y registra duración, temperatura
   percibida, ajustes y caídas. No extrapoles una lectura puntual a toda la sesión.

Si la pantalla y la app informan 120 Hz pero la presentación se mantiene
cerca de 60, hace falta medir la carga del juego y la presentación en ese
dispositivo. Los filtros Scale2x/Scale3x y la resolución de salida pueden
cambiar la carga; no se desactivan automáticamente ni se modifica tu selección.

## Evidencia y referencias

Las pruebas de reloj simulado, puntos intermedios tardíos, cambios de modo,
reinicios, contador y política de selección de pantalla se documentan en
[PERFORMANCE_0.10.5_VALIDATION.md](../validation/PERFORMANCE_0.10.5_VALIDATION.md).
La compilación ARM64 y las comprobaciones del APK están en
[android-0.10.12-build.json](../validation/android-0.10.12-build.json).
No se dispone de una prueba física de este APK ni de una medición de su GPU Android.

Referencias oficiales:

- [Android: optimizar cambios de refresco y solicitar tasas superiores a 60](https://developer.android.com/games/optimize/display-refresh-rate-change).
- [Android: API de frecuencia de fotogramas y condiciones del sistema](https://developer.android.com/media/optimize/performance/frame-rate).
- [Android: límites de frecuencia de Game Mode](https://developer.android.com/games/optimize/adpf/gamemode/fps-throttling).
- [Android 12: tasa de aplicación y modo físico de pantalla](https://developer.android.com/about/versions/12/reference/compat-framework-changes#display_info_nr_advanced_supported).

Las notas de [0.8](MENU_Y_120FPS.md) e [interpolación original](INTERPOLACION_120.md)
se conservan como historial; la política actual es la descrita aquí.
