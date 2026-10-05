# Calidad de imagen — 0.6 experimental

Abre **Atrás → Imagen** y elige:

- **Automática:** adapta el escalado al tamaño de salida de Android.
- **720p · HD:** imagen intermedia de 1280 × 720 píxeles.
- **1080p · Full HD:** imagen intermedia de 1920 × 1080 píxeles.
- **Estirar a pantalla completa:** ocupa todo el espacio de juego de Android,
  ensanchando la imagen en pantallas panorámicas. Desactivado conserva 3:2.

Los ajustes se aplican sin reiniciar y se guardan. Los controles y el contador
se dibujan a la resolución de salida de Android, después de escalar el juego.
Se mantienen el mapeo del mando, el interruptor de controles táctiles, la
corrección de transformaciones y la mezcla opcional de 120 FPS.

El juego sigue generando los gráficos originales de GBA (240 × 160).
720p/1080p son resoluciones de presentación, no sprites redibujados ni detalle
nuevo. Primero se amplía por un factor entero sin suavizado; después se ajustan
los bordes fraccionarios con filtrado lineal. La imagen se presenta al tamaño
de pantalla que Android proporciona; el ajuste no modifica la resolución del
panel ni los ajustes del teléfono. 1080p puede consumir más GPU y batería.

## Verificación

`validation/video_scaler_test.cpp` usa el renderizador software SDL real.
Comprueba los buffers 1280×720/1920×1080, proporciones, pantalla completa,
colores interiores sin degradación, restauración del destino de renderizado,
cambios repetidos de calidad y liberación del buffer al volver a Automática.
Resultados en `validation/video-scaler-test.txt`.

El APK se compila para ARM64 con la misma firma e identificador que 0.5.
Pendiente de prueba visual y de rendimiento en el Pixel 7 Pro del usuario.
