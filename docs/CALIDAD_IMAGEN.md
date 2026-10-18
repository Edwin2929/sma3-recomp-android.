# Calidad de imagen — guía para Android 0.9

El escalado se incorporó en 0.6 y sigue disponible en el APK 0.9.

## Conservar las proporciones originales

Durante el juego, abre **Atrás del teléfono → Imagen → Estirar a pantalla
completa** y **desactiva** la casilla. Esta opción ya existe en el APK 0.9
publicado y está desactivada de forma predeterminada en una instalación nueva.
Si la activaste antes, se conserva esa preferencia hasta que la cambies.

La app continúa a pantalla completa. La casilla controla cómo encaja la imagen
dentro de esa superficie; las bandas no significan que haya salido de ese modo.

La imagen original de Game Boy Advance mide **240 × 160 píxeles**:
240 ÷ 160 = 1,5, es decir, una proporción **3:2**, no 4:3. Sin estirar, el juego
se amplía hasta caber entero en el área disponible y queda centrado, con el
redondeo necesario a píxeles enteros. La dimensión no limitante se redondea
hacia abajo menos de un píxel y los márgenes opuestos pueden diferir un píxel.
En superficies más anchas que 3:2 quedan
bandas laterales; en superficies más estrechas quedan bandas arriba y abajo.

Por ejemplo, en una superficie de **2400 × 1080**, la imagen sin estirar ocupa
**1620 × 1080** y deja **390 píxeles a cada lado**. Para conservar toda la
imagen y sus proporciones en esa superficie, esas bandas son necesarias.

## Tres comportamientos distintos

| Modo | Resultado | Disponible en el APK 0.9 |
| --- | --- | --- |
| **Conservar 3:2:** Estirar desactivado | Imagen completa, centrada y con bandas cuando la superficie tiene otra proporción. | Sí |
| **Estirar:** Estirar activado | El mismo fotograma llena el área de juego. Puede deformarse al cambiar su proporción; no muestra más escenario. | Sí |
| **Panorámica real:** ampliar el campo de visión | Dibuja escenario adicional fuera de los 240 × 160 originales, manteniendo las proporciones de personajes y objetos. | **No.** Es un prototipo de escritorio en desarrollo. |

El prototipo panorámico requiere renderizar terreno, fondos y objetos adicionales
y comprobar sus límites de aparición. Activar Estirar no lo habilita.
Consulta su [estado y tareas pendientes](PANORAMICA_EN_DESARROLLO.md).

## Resolución de presentación

En **Atrás → Imagen** también puedes elegir:

- **Automática:** adapta el escalado al tamaño de salida de Android.
- **720p · HD:** imagen intermedia de 1280 × 720 píxeles.
- **1080p · Full HD:** imagen intermedia de 1920 × 1080 píxeles.

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
Resultados en [validation/video-scaler-test.txt](../validation/video-scaler-test.txt).
La [revisión de la incidencia #1](../validation/VIDEO_ASPECT_RATIO.md) documenta
45 casos de geometría y 110 comprobaciones de renderizado aprobados, junto
con una corrección del redondeo en tamaños como 241 × 160 y 1081 × 720.

Las comprobaciones del escalador en superficies horizontales y verticales son
pruebas de escritorio. No validan la rotación ni la presentación en un teléfono:
la configuración actual del juego en Android sigue siendo horizontal.
Queda pendiente la comprobación visual y de rendimiento en un dispositivo físico.

## Aclaración del menú en el código fuente

La ayuda del ajuste se ha aclarado en español, inglés y portugués: desactivado
conserva 3:2 con bandas; activado llena el área de juego y puede deformar la
imagen, sin mostrar más escenario. Se mantienen el nombre de la casilla y la
preferencia guardada.

Estos textos nuevos corresponden a una próxima compilación desde las fuentes.
El APK 0.9 publicado conserva su ayuda anterior; esta revisión no entrega un
nuevo APK ni incorpora el prototipo panorámico a Android.
