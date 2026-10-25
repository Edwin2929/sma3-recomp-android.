# Rendimiento y objetivos de 60/120 FPS — 0.10.5 beta

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

## Qué cambia en 0.10.5

La versión anterior preparaba y presentaba la imagen de 60 FPS antes de
esperar el final del periodo. Si variaba el tiempo de preparación, también
variaba el instante visible de la presentación. Ahora se prepara la textura,
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

1. Instala 0.10.5 sobre la versión anterior y anota la versión de Android.
2. Activa el ajuste de pantalla fluida/alta frecuencia del teléfono y
   comprueba su modo de ahorro de batería.
3. Empieza con Original (3:2), filtro Equilibrado y resolución Automática.
   Activa Mostrar FPS y compara 60 y 120 en la misma zona del juego.
4. Repite con Panorámica (beta) y con los filtros que quieras utilizar.
   Anota objetivo, presentación, Hz de pantalla y, si aparece, tasa de app.
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
[android-0.10.5-build.json](../validation/android-0.10.5-build.json).
No se dispone de una prueba física de este APK ni de una medición de su GPU Android.

Referencias oficiales:

- [Android: optimizar cambios de refresco y solicitar tasas superiores a 60](https://developer.android.com/games/optimize/display-refresh-rate-change).
- [Android: API de frecuencia de fotogramas y condiciones del sistema](https://developer.android.com/media/optimize/performance/frame-rate).
- [Android: límites de frecuencia de Game Mode](https://developer.android.com/games/optimize/adpf/gamemode/fps-throttling).
- [Android 12: tasa de aplicación y modo físico de pantalla](https://developer.android.com/about/versions/12/reference/compat-framework-changes#display_info_nr_advanced_supported).

Las notas de [0.8](MENU_Y_120FPS.md) e [interpolación original](INTERPOLACION_120.md)
se conservan como historial; la política actual es la descrita aquí.
