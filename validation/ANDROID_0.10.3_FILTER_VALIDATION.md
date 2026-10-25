# Validación de filtros de imagen — Android 0.10.3 beta

## Resultado y alcance

Los seis filtros tienen una implementación real en la presentación de la
imagen. La opción **Equilibrado**, predeterminada al actualizar, conserva la
salida anterior: una comparación simultánea con el escalador de 0.10.2
encontró **576 imágenes idénticas, byte por byte**. Los filtros no escriben en
el framebuffer del juego ni cambian sus proporciones, su campo de visión o
sus contadores de ejecución.

La comprobación nueva utiliza C++ y el renderizador software de SDL 2.30.0 en
el equipo de construcción. **No se ejecutó este APK en un teléfono**; estas
pruebas no certifican los controladores gráficos de Android, la rotación del
sistema, el consumo de batería ni 60/120 FPS físicos. La compilación y firma
del APK se documentan por separado en
[`android-0.10.3-build.json`](android-0.10.3-build.json).

## Qué se comprobó

| Prueba | Resultado |
| --- | --- |
| Equilibrado frente al código de 0.10.2 | 576 presentaciones iguales en todos los bytes RGBA leídos de SDL. |
| Regresión existente del escalador | 45 casos de geometría y 110 presentaciones aprobados. |
| Integración de los seis filtros | 2.592 presentaciones y 24.250 comprobaciones, tanto en compilación normal como con ASan/UBSan. |
| Reglas Scale2x/Scale3x | 42.294 vecindarios y 18 imágenes completas; 2.322.137 comprobaciones por ejecución, normal y ASan/UBSan. |
| Mezcla temporal y cambios de anchura existentes | 8 comprobaciones de historial y 36 presentaciones; 255 comprobaciones totales. |
| Aplicación de parches del motor | Los 10 estados posibles de prefijo de los 9 parches llegan al resultado esperado; repetición idempotente y rechazo de cambios desconocidos o revisión incorrecta. |

Los recuentos de comprobaciones internas no representan niveles o recorridos
independientes del juego. Datos y hashes de las entradas comprobadas:
[`android-0.10.3-filter-validation.json`](android-0.10.3-filter-validation.json).

### Equilibrado y geometría

[`image_filter_default_regression.cpp`](image_filter_default_regression.cpp)
ejecuta los dos escaladores en el mismo renderizador y compara todos los
bytes de salida. La referencia se extrae del commit
`fe0cf01aba8c4a1375b29cf0289c3055be8d9fb1`, correspondiente a 0.10.2.

La secuencia de fuentes es 240, 347, 356 y de nuevo 240 píxeles de anchura,
siempre con 160 de alto. Los destinos incluyen 2400 × 1080, 1080 × 2400,
1921 × 1081, 1081 × 1921, 321 × 217 y 3 × 2. Se prueban Automática, 720p,
1080p y la vuelta a Automática, tanto conservando proporciones como
estirando, con un fotograma alternativo entre dos copias del mismo patrón.

Esto comprueba que activar esta versión sin cambiar el filtro conserva el
escalado previo, que se mantiene el centrado y que los recursos reutilizados
no muestran píxeles de un fotograma anterior.

### Filtros, controles y mezcla temporal

[`image_filter_integration_test.cpp`](image_filter_integration_test.cpp)
recorre los seis IDs con las fuentes 240/347/356/240, destinos 961 × 541,
541 × 961, 120 × 80 y 3 × 2, las tres resoluciones y los dos tipos de destino.
Comprueba que:

- Los buffers de origen y de mezcla temporal permanecen intactos.
- Un fotograma repetido produce exactamente los mismos bytes después de
  presentar una imagen intermedia creada por el `FrameBlend` real.
- Volver a Equilibrado restaura exactamente la imagen inicial después de
  cada filtro. Los IDs inválidos también utilizan Equilibrado.
- Se conservan el destino, el color y el modo de mezcla del renderizador,
  de modo que el filtro no deja un estado gráfico distinto para dibujar los
  controles. El motor dibuja sus capas de interfaz después del juego.
- Los márgenes exteriores conservan su color. El interior negro de los
  márgenes panorámicos de protección sigue negro; los contornos de un píxel
  pueden suavizarse intencionadamente según el filtro.
- `reset()` libera las texturas y el buffer de los filtros.

La prueba ASan/UBSan instrumenta el código nuevo y el ejecutable de pruebas.
La biblioteca SDL instalada no está instrumentada y LeakSanitizer está
desactivado. No se atribuyen a este ensayo pruebas de pérdida real de un
contexto gráfico Android ni pulsaciones físicas sobre los controles.

### Scale2x y Scale3x

Las fórmulas se contrastaron con la
[especificación original de Andrea Mazzoleni](https://www.scale2x.it/algorithm).
El código RGB24 es propio, no incorpora una biblioteca externa. Repite el
píxel más cercano cuando un vecino cae fuera de la imagen.

[`image_filter_pixels_test.cpp`](image_filter_pixels_test.cpp) comprueba
todas las 21.147 particiones de igualdad de un vecindario de 3 × 3 para cada
algoritmo. También verifica imágenes completas de una fila o columna,
tamaños pequeños, impares, 240 × 160, 347 × 160 y 356 × 160; alineación,
límites de buffers, origen intacto, entradas inválidas y solapamiento.
Los resultados y órdenes de reproducción están en
[`image-filter-pixels-test.txt`](image-filter-pixels-test.txt).

### CRT suave y revisión visual

CRT aplica un oscurecimiento fijo a la parte inferior de cada fila de píxeles
después del escalado Equilibrado. No usa reloj, fase alterna ni historial.
Las líneas se omiten cuando la altura disponible no alcanza tres píxeles de
salida por fila original, para evitar un patrón demasiado pequeño.

Se renderizó el mismo fotograma privado del juego con las seis opciones a
840 × 560 y se revisaron las salidas. Equilibrado conserva los detalles;
Píxel nítido marca los escalones; Bilineal suaviza también los detalles;
Scale2x/Scale3x modifican los contornos, y CRT muestra líneas regulares. No
se observó corrupción en esa muestra. Es una comparación de la presentación
de un fotograma guardado, **no un nuevo recorrido del juego ni una captura
del APK en Android**. Las imágenes del juego permanecen privadas.

## Reproducción de las comprobaciones SDL

Se necesita un compilador C++20 y los archivos de desarrollo de SDL2.
Desde la raíz del repositorio, estas órdenes funcionan en un sistema con
`pkg-config sdl2` disponible:

```sh
git show fe0cf01aba8c4a1375b29cf0289c3055be8d9fb1:src/video_scaler.h > /tmp/sma3-video-scaler-0102.h
c++ -std=c++20 -O2 -I src $(pkg-config --cflags sdl2) \
  '-DSMA3_BASELINE_VIDEO_SCALER_HEADER="/tmp/sma3-video-scaler-0102.h"' \
  validation/image_filter_default_regression.cpp \
  $(pkg-config --libs sdl2) -o /tmp/sma3-filter-default
/tmp/sma3-filter-default

c++ -std=c++20 -O2 -I src $(pkg-config --cflags sdl2) \
  validation/image_filter_integration_test.cpp \
  $(pkg-config --libs sdl2) -o /tmp/sma3-filter-integration
/tmp/sma3-filter-integration

python -B -m unittest discover -s validation -p test_window_patch_stack.py -v
```

Para repetir la integración con sanitizadores, sustituir `-O2` por
`-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
-fno-sanitize-recover=all` y ejecutar con `ASAN_OPTIONS=detect_leaks=0`.
La interfaz traducida y la preferencia persistente están descritas en
[`ANDROID_0.10.3_FILTER_SETTINGS.md`](ANDROID_0.10.3_FILTER_SETTINGS.md).
