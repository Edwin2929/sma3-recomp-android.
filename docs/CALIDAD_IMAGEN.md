# Calidad de imagen — Android 0.10.4 beta

En **0.10.4**, abre **Atrás del teléfono → Imagen**. El modo de imagen,
el filtro y la resolución de presentación se configuran por separado.

| Modo | Qué muestra | Proporciones |
| --- | --- | --- |
| **Original (3:2)** | El fotograma original completo, centrado y con bandas cuando hacen falta. | Conserva 240:160 = 3:2. |
| **Panorámica (beta)** | Escenario adicional a los lados en las escenas compatibles, con el centro original y bandas cuando falta cobertura de la escena. | Conserva la forma de los gráficos. Puede dejar bandas. |
| **Estirar a pantalla completa** | El mismo fotograma original ocupa toda el área disponible. | Puede deformarlo. No muestra más escenario. |

Original es el valor predeterminado en una instalación nueva. Al actualizar
desde 0.9/0.9.1, la antigua casilla desactivada se convierte en Original y la
casilla activada en Estirar. **La actualización nunca selecciona Panorámica
por su cuenta.** La opción elegida se aplica durante el juego y se guarda.

**Advertencia de Panorámica (beta):** es una fase temprana de desarrollo y
todavía puede presentar errores visuales o de compatibilidad. El aviso está
visible en Imagen. Original (3:2) sigue disponible para las escenas que no
tengan una ampliación correcta. Cambiar el nombre de la opción no altera su
valor guardado ni implica que todo el juego esté validado.

<a id="filtros-de-imagen-en-0103"></a>

## Filtros de imagen en 0.10.4

Abre **Atrás → Imagen → Filtros de imagen**. Elige una opción y vuelve al
juego para comparar el resultado. La selección se aplica al instante y
permanece guardada entre sesiones. En una instalación nueva se utiliza
**Equilibrado**; al actualizar desde 0.10.3 se conserva el filtro elegido.

| Filtro | Aspecto y uso | Qué conviene tener en cuenta |
| --- | --- | --- |
| **Equilibrado (recomendado)** | Mantiene definidos los píxeles y suaviza los bordes fraccionarios al ajustar la imagen a la pantalla. | Un punto de partida para conservar el aspecto actual. |
| **Píxel nítido** | Amplía con el vecino más cercano, sin mezclar colores. | Los píxeles y los escalones de los contornos se ven más marcados. |
| **Suave bilineal** | Mezcla los colores vecinos al ampliar para dar un acabado más suave. | Puede difuminar letras y detalles pequeños. |
| **Scale2x** | Reconstruye cada píxel como un bloque de 2 × 2, usando los vecinos para suavizar algunos contornos. | Una alternativa para reducir escalones sin un desenfoque general. |
| **Scale3x** | Reconstruye cada píxel como un bloque de 3 × 3 con reglas adicionales de contorno. | Puede modificar detalles pequeños y requiere más procesamiento que Scale2x. |
| **CRT suave** | Añade líneas horizontales suaves y fijas sobre el escalado equilibrado. | Es un acabado retro, oscurece ligeramente la imagen y no añade parpadeo temporal. |
| **Cinematográfico** | Escalado Equilibrado con dos franjas negras fijas, arriba y abajo. | Cada franja cubre el 10 % de la altura del juego. Ocultan imagen e indicadores situados en esas zonas; no añaden escenario. |

Puedes empezar por Equilibrado y comparar con Scale2x. Si prefieres ver los
píxeles claramente, selecciona Píxel nítido. La elección depende del aspecto
que te guste; una imagen más suave no siempre conserva mejor los detalles.
Para recuperar la presentación anterior basta con volver a Equilibrado.

**Cinematográfico** conserva el tamaño y la posición del centro de la imagen.
Las franjas tienen la misma altura, calculada con redondeo hacia abajo, y
se dibujan dentro del área del juego. En un destino de 1620 × 1080 miden
108 píxeles cada una. Los controles táctiles y el contador de FPS se dibujan
después y permanecen visibles. Para recuperar toda la imagen, selecciona
cualquier otro filtro. Solo se elige un filtro a la vez: Cinematográfico
usa Equilibrado como base, sin combinarlo con Scale2x o CRT.

Los filtros se aplican al fotograma terminado, antes de los controles y el
contador de FPS. Funcionan con Original (3:2), Panorámica (beta) y Estirar,
y con la mezcla temporal activada o desactivada. No cambian la velocidad,
la lógica del juego ni la selección de encuadre. No añaden detalle dibujado
originalmente a mayor resolución, ni corrigen escenas fuera de la cobertura
panorámica. Las bandas de seguridad durante Fuzzy siguen siendo necesarias.

Scale2x y Scale3x siguen las reglas públicas de la
[especificación original de Scale2x](https://www.scale2x.it/algorithm), con
repetición del píxel más próximo en los bordes. Las líneas de CRT permanecen
quietas y se omiten cuando no hay altura suficiente
para representarlas sin aliasing.

[Validación de filtros](../validation/ANDROID_0.10.3_FILTER_VALIDATION.md) ·
[Cinematográfico y siete filtros](../validation/CINEMATIC_0.10.4_VALIDATION.md) ·
[Selector y persistencia](../validation/ANDROID_0.10.4_MODS_SETTINGS.md).

## Mods de color

**Atrás → Mods** permite importar perfiles compatibles `.gbamod`. El perfil
activo transforma los colores del fotograma terminado y puede usarse con
cualquiera de los siete filtros. Solo se aplica uno a la vez. El ejemplo
**Tono cálido** aumenta el rojo y reduce el azul; desactivarlo recupera los
colores originales sin cambiar el filtro, el modo de imagen o la partida.
Las franjas negras de Cinematográfico y de las escenas panorámicas sin
cobertura siguen negras. [Instalación y creación de paquetes](MODS.md).

## Correcciones adicionales en 0.10.2

Un objeto ordinario sin posición lateral resuelta ya no apaga los dos
márgenes completos: se conserva el escenario comprobado y ese objeto se
limita al centro original. En los fondos BG3 completos compatibles se usa
el mapa actualizado por el juego, incluyendo sus animaciones, en lugar de
reconstruir una versión anterior de sus texturas.

El efecto de terreno del **Fuzzy** se conserva en el centro original. Como
el juego calcula ese efecto únicamente para su encuadre nativo, durante
ese intervalo se muestran bandas negras estables; la panorámica vuelve al
restaurarse los mapas. La selección sigue siendo Panorámica (beta) y no
hay que volver a activarla. El cierre por rutinas ARM faltantes se corrige
por separado, también en Original.

[Pruebas de panorámica](../validation/PANORAMA_0.10.2_VALIDATION.md) ·
[Corrección de Fuzzy](../validation/FUZZY_VALIDATION.md).

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

## Usar Panorámica (beta)

Selecciona **Atrás → Imagen → Panorámica (beta)**. El modo amplía
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

**Límite de la beta:** la panorámica no modifica los límites originales de aparición
y dibujo de objetos. Algunos enemigos, objetos o efectos pueden aparecer
tarde o faltar en los márgenes. La comprobación de los objetos que ya están
en OAM no demuestra que el juego haya generado todos los que deberían verse
fuera del encuadre original. Las rutas verificadas del castillo y del coche
no cubren todos los niveles, jefes ni transformaciones. Si una escena muestra
un defecto lateral, selecciona **Original (3:2)** para usar su encuadre nativo.
[Cobertura e investigación pendiente](PANORAMICA_EN_DESARROLLO.md).

## Resolución de presentación

En **Atrás → Imagen → Resolución de salida** puedes elegir:

- **Automática:** adapta el escalado al tamaño de salida de Android.
- **720p · HD:** utiliza una imagen intermedia de 1280 × 720 píxeles.
- **1080p · Full HD:** utiliza una imagen intermedia de 1920 × 1080 píxeles.

Los controles y el contador se dibujan a la resolución de salida de Android,
después de escalar el juego. 720p y 1080p no redibujan los sprites ni amplían
por sí solos el campo de visión. El filtro determina cómo se remuestrea la
imagen. Equilibrado amplía primero por un factor entero sin suavizado y
después ajusta los bordes fraccionarios con filtrado lineal.
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

Estas comprobaciones históricas no sustituyen una prueba física de 0.10.4. El reporte de
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
