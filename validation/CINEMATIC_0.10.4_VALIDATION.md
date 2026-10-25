# Filtro cinematográfico — Android 0.10.4 beta

## Comportamiento

El nuevo filtro **Cinematográfico** utiliza el ID **6**. Los IDs 0–5 mantienen
su significado anterior. Se parte del escalado Equilibrado y se dibujan dos
franjas negras opacas y fijas sobre la parte superior e inferior de la imagen
del juego. Cada franja mide `floor(altura_del_destino / 10)` píxeles; ambas
tienen siempre el mismo grosor. Si el destino mide menos de 10 píxeles de
alto, no se dibujan franjas.

La imagen visible conserva su escala, posición y proporciones. Las franjas
**cubren contenido**, incluido cualquier indicador del juego que esté en
esas zonas; no añaden campo de visión. Por ejemplo, con un destino de
1620 × 1080, cada franja mide 108 píxeles de alto. Los márgenes exteriores
del modo Original o Panorámica quedan fuera de la operación.

El filtro no utiliza reloj, fase alterna ni historial. No escribe en los
buffers de píxeles del juego. El color y el modo de mezcla del renderizador
se restauran antes de que el motor dibuje los controles. La preferencia usa
el enlace `setVideoFilter(I)V` ya existente; no necesita otro parche del
motor ni un cambio de firma JNI.

Código: [`image_filters.h`](../src/image_filters.h) y
[`video_scaler.h`](../src/video_scaler.h).

## Pruebas realizadas

| Comprobación | Resultado |
| --- | --- |
| Franjas, centro y márgenes | **108 casos** de geometría, resolución y aspecto. Se comprueba cada píxel de salida frente a Equilibrado y una máscara independiente. |
| Filtros anteriores | **648 presentaciones** de los IDs 0–5 idénticas byte por byte a las fuentes de 0.10.3. |
| Cambios entre siete filtros | **2.976 presentaciones y 27.826 comprobaciones** de integración. |
| Reglas de Scale2x/Scale3x | **42.294 patrones de vecindario y 18 imágenes completas**, con el nuevo intervalo válido de IDs 0–6. |
| Sanitizadores | Las tres pruebas pasan con AddressSanitizer y UndefinedBehaviorSanitizer. |

La prueba específica utiliza fuentes de **240, 347 y 356 × 160**, destinos
de 961 × 541, 541 × 961, 321 × 217, 3 × 2, 21 × 9 y 23 × 10, las resoluciones
Automática/720p/1080p y destinos con proporciones conservadas o estirados.
También verifica el recorte de SDL, la opacidad, los tamaños inválidos y
la restauración completa al desactivar las franjas. La referencia histórica
corresponde al commit `4d68bccbdffbc2ee55d7d1fb833cfc266f9005d3`.

La integración de siete filtros intercala imágenes intermedias creadas por
el `FrameBlend` real, conserva los buffers de origen, recupera Equilibrado
exactamente después de cada selección y mantiene el interior de los
márgenes panorámicos negros. Los IDs inválidos vuelven a Equilibrado.

Los millones de comparaciones de píxeles internas no representan millones
de escenas o recorridos de juego. Los recuentos, hashes y resultados están
en [`android-0.10.4-cinematic.json`](android-0.10.4-cinematic.json).

## Revisión visual y límites

Se procesó el mismo fotograma privado de juego con Equilibrado y
Cinematográfico, a **840 × 560**, y se revisaron ambas salidas. Las dos
franjas miden 56 píxeles, el centro coincide y no se observó corrupción en
esa muestra. La imagen de origen procede de una captura de desarrollo de
0.10.2; esta comparación no representa un nuevo recorrido del juego.

Las pruebas usan SDL 2.30.0 software en el entorno de construcción.
**No son pruebas físicas del APK en Android ni mediciones de FPS.** Los
sanitizadores instrumentan el código de los filtros y de las pruebas;
la biblioteca SDL instalada no está instrumentada y LeakSanitizer está
desactivado. La prueba de integración incluye un encabezado anterior del
editor táctil con un aviso de indentación; se conserva el aviso y solo se
exceptúa de `-Werror` esa categoría al compilar dicha prueba.

## Reproducción

Se necesitan C++20 y los archivos de desarrollo de SDL2. Desde la raíz
del repositorio:

```sh
mkdir -p /tmp/sma3-0103-filter-reference
git show 4d68bccbdffbc2ee55d7d1fb833cfc266f9005d3:src/video_scaler.h > /tmp/sma3-0103-filter-reference/video_scaler.h
git show 4d68bccbdffbc2ee55d7d1fb833cfc266f9005d3:src/image_filters.h > /tmp/sma3-0103-filter-reference/image_filters.h
c++ -std=c++20 -O2 -Wall -Wextra -Werror -I src $(pkg-config --cflags sdl2) \
  '-DSMA3_BASELINE_VIDEO_SCALER_HEADER="/tmp/sma3-0103-filter-reference/video_scaler.h"' \
  validation/cinematic_filter_test.cpp $(pkg-config --libs sdl2) \
  -o /tmp/sma3-cinematic-test
/tmp/sma3-cinematic-test

c++ -std=c++20 -O2 -Wall -Wextra -Werror -Wno-error=misleading-indentation \
  -I src $(pkg-config --cflags sdl2) validation/image_filter_integration_test.cpp \
  $(pkg-config --libs sdl2) -o /tmp/sma3-seven-filters-test
/tmp/sma3-seven-filters-test

c++ -std=c++20 -O2 -Wall -Wextra -Werror -I src \
  validation/image_filter_pixels_test.cpp -o /tmp/sma3-filter-pixels-test
/tmp/sma3-filter-pixels-test
```

Para repetir con sanitizadores, sustituye `-O2` por
`-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
-fno-sanitize-recover=all` y ejecuta con
`ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1`.
