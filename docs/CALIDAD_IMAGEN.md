# Calidad de imagen — Android 0.10.1 experimental

En **0.10 y 0.10.1**, abre **Atrás del teléfono → Imagen** y elige uno de los tres
modos. La resolución de presentación se configura por separado.

| Modo | Qué muestra | Proporciones |
| --- | --- | --- |
| **Original (3:2)** | El fotograma original completo, centrado y con bandas cuando hacen falta. | Conserva 240:160 = 3:2. |
| **Panorámica (experimental)** | Escenario adicional a los lados en las escenas compatibles, con retorno a la vista original cuando falta cobertura. | Conserva la forma de los gráficos. Puede dejar bandas. |
| **Estirar a pantalla completa** | El mismo fotograma original ocupa toda el área disponible. | Puede deformarlo. No muestra más escenario. |

Original es el valor predeterminado en una instalación nueva. Al actualizar
desde 0.9/0.9.1, la antigua casilla desactivada se convierte en Original y la
casilla activada en Estirar. **La actualización nunca selecciona Panorámica
por su cuenta.** La opción elegida se aplica durante el juego y se guarda.

## Corrección del titileo en 0.10.1

La versión 0.10.1 corrige cambios transitorios de anchura que descartaban una
imagen terminada y la reconstruían con texturas de otro momento. Ahora la
superficie panorámica permanece estable; las escenas no admitidas muestran
el centro original con bandas negras dentro de esa superficie. Los cambios
reales de tamaño y las cargas de partidas conservan la imagen ya terminada.
También se evitan destellos blancos en márgenes que debían permanecer negros.
[Pruebas fotograma a fotograma](../validation/ANDROID_0.10.1_VISUAL_FIXES.md).

## Conservar la proporción original

La imagen original de Game Boy Advance mide **240 × 160 píxeles**:
240 ÷ 160 = 1,5, es decir, una proporción **3:2**, no 4:3. Original amplía el
fotograma hasta que cabe entero y lo centra. La dimensión no limitante se
redondea hacia abajo menos de un píxel; los márgenes opuestos pueden diferir
un píxel por ese redondeo.

En superficies más anchas que 3:2 aparecen bandas laterales; en superficies
más estrechas aparecen arriba y abajo. Por ejemplo, en **2400 × 1080**, el
fotograma Original ocupa **1620 × 1080**, con **390 píxeles a cada lado**.
La aplicación continúa a pantalla completa: las bandas son parte del ajuste
de la imagen a una superficie con otra proporción.

## Usar la panorámica de 0.10

Selecciona **Atrás → Imagen → Panorámica (experimental)**. El modo amplía
el campo de visión en las escenas que el adaptador puede reconstruir; no
estira los 240 píxeles originales para simular una vista más ancha.

La altura lógica permanece en **160** y el ancho se adapta a la superficie,
entre **240 y 356**. Estos son tamaños del fotograma lógico, no la resolución
del panel ni una promesa de ausencia de bandas:

| Superficie disponible | Vista lógica panorámica compatible |
| --- | --- |
| 3120 × 1440 o 2340 × 1080 | 347 × 160 |
| 2400 × 1080 | 356 × 160 |
| 1920 × 1080 | 284 × 160 |
| 1080 × 2400 o 1440 × 3120 | 240 × 160, vista original centrada |

El ancho máximo limita las pantallas más alargadas; el redondeo y el ajuste
final también pueden dejar bandas pequeñas. Se utiliza el área que Android
entrega al juego, que puede diferir de la resolución nominal del teléfono.
0.10 admite la orientación elegida por el usuario mediante `fullUser`; la
rotación física y los controles en cada orientación quedan por comprobar
en el dispositivo.

La ampliación necesita un estado de juego admitido, un mapa válido y datos
coherentes para los objetos y fondos habilitados. Tras cargar una partida,
muestra temporalmente el centro original con márgenes negros dentro de la
misma superficie panorámica hasta reunir esos datos. En escenas no
admitidas muestra el centro original con bandas, manteniendo estable la
superficie panorámica; algunos efectos de ventana conservan márgenes
negros en las filas que no se pueden ampliar con seguridad.

**Límite experimental:** 0.10 no modifica los límites originales de aparición
y dibujo de objetos. Algunos enemigos, objetos o efectos pueden aparecer
tarde o faltar en los márgenes. La comprobación de los objetos que ya están
en OAM no demuestra que el juego haya generado todos los que deberían verse
fuera del encuadre original. Las rutas verificadas del castillo y del coche
no cubren todos los niveles, jefes ni transformaciones. Si una escena muestra
un defecto lateral, selecciona **Original (3:2)** para usar su encuadre nativo.
[Cobertura e investigación pendiente](PANORAMICA_EN_DESARROLLO.md).

## Resolución de presentación

En **Atrás → Imagen** también puedes elegir:

- **Automática:** adapta el escalado al tamaño de salida de Android.
- **720p · HD:** utiliza una imagen intermedia de 1280 × 720 píxeles.
- **1080p · Full HD:** utiliza una imagen intermedia de 1920 × 1080 píxeles.

Los controles y el contador se dibujan a la resolución de salida de Android,
después de escalar el juego. 720p y 1080p no redibujan los sprites ni amplían
por sí solos el campo de visión. Se amplía primero por un factor entero sin
suavizado y después se ajustan los bordes fraccionarios con filtrado lineal.
El ajuste no cambia la resolución física ni la configuración del panel.
1080p puede consumir más GPU y batería.

Se conservan el mapeo del mando, los controles táctiles y la mezcla temporal
opcional de 120 FPS. Esa mezcla no crea 120 estados independientes del juego;
0.10 no acredita una mejora de rendimiento ni 120 FPS estables.

## Verificación y alcance

La revisión del escalador de 0.9.1 aprobó **45 casos de geometría** y
**110 comprobaciones de renderizado SDL2 software**, incluidos destinos
horizontales, verticales, impares y cambios de calidad. Corrigió un error de
redondeo en tamaños como 241 × 160 y 1081 × 720.
[Informe](../validation/VIDEO_ASPECT_RATIO.md) ·
[Resultados](../validation/video-scaler-test.txt).

Para 0.10 se ejecutaron siete casos con la ruta de presentación de producción
y superficies SDL simuladas: Original, panorámica 347/356, vertical, fuente
original para Estirar y una transición no admitida en ambos modos. Los cinco
centros comparados coinciden exactamente con su referencia. También pasan
40 casos de ciclo de vida y 488 casos del compositor de ventanas con
AddressSanitizer y UndefinedBehaviorSanitizer. Los recorridos emparejados
comprueban 6.801 fotogramas del castillo y 3.506 del coche.
[Informe de 0.10 y reproducción](../validation/ANDROID_0.10_VALIDATION.md).

Estas comprobaciones no sustituyen una prueba física de 0.10.1. El reporte de
Edwin del 18 de octubre de 2026 sobre su **Google Pixel 7 Pro** corresponde
al APK **0.9.1**, con el campo de visión original. No se traslada a la
panorámica, la rotación o el rendimiento de esta nueva versión.

## Instrucciones para 0.9 y 0.9.1

En esas versiones, abre **Atrás → Imagen → Estirar a pantalla completa**
y desactiva la casilla para conservar 3:2. Activarla llena el área de juego
y puede deformar el mismo fotograma. **Ninguna de las dos incluye panorámica
real.** La ayuda aclarada en ES/EN/PT y la corrección de redondeo están en
0.9.1; el APK 0.9 conserva sus textos y escalador anteriores.
[Notas de 0.9.1](releases/v0.9.1-experimental.md).
