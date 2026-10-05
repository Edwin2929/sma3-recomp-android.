# Menú e intento de corrección de 120 FPS — 0.8 experimental

## Idioma de las opciones

Atrás → **Idioma del menú…** permite elegir Español, English o Português.
Es independiente de **Idioma del juego…**. Traduce opciones de FPS, botones,
mapeo del mando, calidad de imagen, explicaciones y contador. El cambio es
inmediato al volver al menú y se conserva al reiniciar. Las instalaciones
anteriores mantienen español por defecto. Los nombres físicos A/B/L/R y de
los botones de las marcas se conservan para facilitar su identificación.

## Presentación

El reporte del usuario indica que el modo anterior no alcanza 120 FPS en su
Pixel 7 Pro. No tenemos todavía una captura con FPS y Hz ni una traza de ese
dispositivo que identifique la causa concreta.

Cambios implementados:

- Solicitud explícita del modo de pantalla cercano a 120 Hz entre los modos
  compatibles de la misma resolución; se libera al desactivar o suspender.
- Solicitud Surface de 119,455 imágenes/s y permiso de cambio de modo en Android 12+.
- Observación de cambios de refresco para actualizar los Hz del contador.
- Aviso cuando el modo está activo pero Android indica menos de 119 Hz.
- El temporizador anterior descartaba un punto intermedio con más de 1 ms de
  retraso. Ahora aprovecha un retraso moderado si aún queda al menos un cuarto
  del periodo del juego (~4,19 ms) antes de la siguiente imagen. Sigue
  descartando las presentaciones extra bajo carga excesiva, sin ráfagas de
  recuperación ni acelerar la simulación.
- En cortes de escena, presenta la imagen actual en lugar de mezclar imágenes
  incompatibles; evita perder una presentación únicamente por el cambio de escena.

La simulación sigue a 59,7275 Hz y el objetivo es 119,455 presentaciones/s.
La cifra puede rondar 119–120; no son 120 fotogramas de simulación distintos.
El contador mide llamadas de presentación SDL, no garantiza que el compositor
Android muestre todas las imágenes. La mezcla puede producir estelas.

## Pruebas y límites

`validation/interpolation-test.txt` comprueba mezcla, cortes, reinicios,
temporización y recuperación de un retraso de 10 ms. La prueba de carga
`validation/presentation-load-test.txt` mide las oportunidades del temporizador
con 0, 6, 10 y 14 ms de trabajo simulado. No usa una GPU Android ni mide FPS
visibles. Conserva un solo periodo del juego por iteración.

No se ha probado este APK en un teléfono físico. Estos cambios corrigen
limitaciones observadas en el código, pero todavía no demuestran 120 FPS
estables en el Pixel 7 Pro. Android puede limitar el refresco por ahorro de
batería u otras condiciones. Referencia:
https://developer.android.com/media/optimize/performance/frame-rate

Para validar: instalar como actualización, activar Pantalla fluida, desactivar
ahorro de batería, iniciar en calidad Automática y activar Mostrar FPS y el
modo 120. Jugar al menos cinco minutos y registrar por separado Salida (FPS)
y Pantalla (Hz). Comparar después con 720p/1080p, y probar apagar/encender la
pantalla y volver desde otra aplicación. Si la pantalla marca 120 Hz pero la
salida ronda 60 FPS, sigue habiendo un límite de ejecución/presentación que
requiere la medición del teléfono.
