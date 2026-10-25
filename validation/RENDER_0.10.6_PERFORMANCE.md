# Renderizado — equivalencia y coste de 0.10.6 beta

## Cambios y alcance

Se compara con **0.10.5 beta**, commit
`40849aeef03a5cdd569a19f35034c8233c9520d2`. Se conserva el filtro, la calidad,
la proporción y el perfil de color elegidos. No se reduce automáticamente la
resolución ni se cambia la emulación, el audio o el ritmo del juego.

| Componente | Cambio |
|---|---|
| Scale2x y Scale3x | Especialización de los dos factores y avance por filas; se elimina el bloque genérico temporal y el cálculo de coordenadas para cada píxel de salida. Se conservan las reglas de igualdad y la réplica de los bordes. |
| Perfiles diagonales | Tres tablas locales de 256 bytes sustituyen la aritmética repetida por canal en imágenes de al menos 256 píxeles. Se conserva exactamente el redondeo y la saturación. La matriz general mantiene su bucle aritmético anterior. |
| Escalado equilibrado entero | En calidad nativa, si el destino es exactamente un múltiplo entero en ambos ejes, una copia nearest produce los mismos píxeles que el prescalado nearest seguido de la copia lineal 1:1 anterior. Se evita una textura intermedia del tamaño final y una pasada. |

Los tamaños fraccionarios y las rutas explícitas de 720p/1080p conservan sus
pasadas anteriores. Los filtros CRT y cinematográfico siguen aplicando sus
líneas o bandas después de escalar. Los perfiles continúan aplicándose antes
de los filtros, sobre el RGB24 de cada presentación, incluidos los puntos
intermedios de 120 FPS.

Las tablas diagonales ocupan **768 bytes en la pila**, se completan para la
instantánea actual y no son una caché global mutable. No se cambia la
publicación de perfiles ni su persistencia. Los perfiles desactivados y la
matriz identidad mantienen la ruta directa, sin modificar el framebuffer ni
redimensionar el búfer temporal.

## Hallazgos sobre el coste existente

Los búferes y texturas de estos tres componentes ya se reutilizaban cuando
no cambiaban las dimensiones o el filtro. No se encontró una asignación
persistente por fotograma en sus búferes de aplicación. Scale2x/3x procesan
la imagen nativa de 240/356 × 160; el escalado al tamaño de pantalla utiliza
las operaciones de renderizado de SDL. El motor solicita un renderizador
acelerado y puede recurrir al renderizador de software si no está disponible.

Estos headers no cambiaron en 0.10.5: su coste no demuestra por sí mismo la
causa de la regresión de esa entrega. La revisión también encontró que el
camino de 120 FPS podía preparar color, texturas, filtros y controles para un
punto intermedio que después se descartaba por llegar tarde. La corrección
de ese trabajo descartado pertenece al coordinador de presentaciones y a su
informe de rendimiento, no a estos tres headers.

En el caso exacto 240 × 160 → 2160 × 1440, se evita una textura intermedia
RGBA de **12.441.600 bytes** y una copia de pantalla completa. Si una textura
ya estaba reservada por un tamaño anterior, se conserva para reutilizarla;
el reinicio del escalador continúa liberando todos sus recursos.

## Equivalencia comprobada

- **Scale2x/Scale3x:** 42.294 casos de vecindario, correspondientes a las
  21.147 particiones de igualdad de nueve píxeles para cada algoritmo;
  18 imágenes completas y **2.322.138 aserciones**. Incluye bordes, fuentes
  pequeñas, píxeles desalineados, guardas, solapamiento y entradas inválidas.
- **Perfiles:** **636 casos de matriz y 12.722 aserciones**. Se cubren todos
  los coeficientes permitidos de −200 a 200 y todos los valores de entrada
  de cada canal independiente; también matrices generales, saturación,
  imágenes de 255/256/257 píxeles, 240/356 × 160 y uso in-place.
- **Renderizado:** **714 presentaciones pareadas** contra el escalador de
  0.10.5. Coinciden los **3.669.932.868 bytes de salida** comparados, con los
  siete filtros, calidades nativa/720p/1080p, fuentes 240/356 × 160,
  escalas enteras 1/2/3/6/9, tamaños fraccionarios, márgenes y patrones que
  alternan. Los **120 casos elegibles** pasan de dos a una llamada a
  `SDL_RenderCopy` y de dos a cero cambios de target.
- Los oráculos de píxeles y color pasan también con **ASan/UBSan**.
  LeakSanitizer está desactivado por las restricciones del entorno; la
  biblioteca SDL del sistema no está instrumentada.

El archivo [`android-0.10.6-render.json`](android-0.10.6-render.json) conserva
los resultados, medidas por patrón, revisión base y hashes de las fuentes.

## Método de medición

Se usan patrones RGB24 sintéticos: color uniforme, bloques de pixel art y
ruido de cuatro colores. Se prueban anchuras 240 y 356 con altura 160. Las
mediciones alternan antes/después y después/antes, con tiempo de CPU del
hilo (`CLOCK_THREAD_CPUTIME_ID`) y mediana de las rondas. Los kernels usan
nueve rondas; el renderizado SDL, siete. Las matrices se mantienen dinámicas
en el benchmark mediante funciones sin especialización interprocedural,
evitando que el compilador convierta los valores de prueba en constantes.

Las tablas de medidas separan Scale2x, Scale3x, dos perfiles diagonales y
una matriz general. Una comparación inicial permitía que el compilador
especializara de forma distinta las matrices constantes del benchmark y
producía una aparente regresión del camino general; se corrigió ese sesgo
antes de aceptar el cambio. El bucle general de producción permanece igual.

Medianas de la ronda registrada, en milisegundos por imagen:

| Componente y caso | 0.10.5 | 0.10.6 | Lectura del resultado |
|---|---:|---:|---|
| Scale2x, 356 × 160, bloques de pixel art | 0,4553 | 0,2476 | 45,6 % menos CPU en este kernel y patrón. |
| Scale3x, 356 × 160, bloques de pixel art | 0,6177 | 0,5221 | 15,5 % menos CPU en este kernel y patrón. |
| Perfil cálido diagonal, mismo patrón | 0,2785 | 0,0450 | 83,8 % menos CPU para esta transformación independiente de canales. |
| Matriz general, mismo patrón | 0,2788 | 0,2700 | Control del camino que conserva su aritmética. |
| Equilibrado, SDL software, 2160 × 1440 entero | 24,1060 | 21,6960 | Una pasada menos; 10,0 % menos CPU de render en este caso. |
| Equilibrado, SDL software, 1620 × 1080 fraccionario | 17,2664 | 17,2809 | Misma ruta; diferencia de 0,08 %. |

Las medidas completas incluyen los casos sin mejora relevante. Por ejemplo,
Scale2x a 2160 × 1440 no tiene factor entero después del filtro y no activa
el bypass: esa composición varió de 34,3108 a 35,2292 ms (+2,7 %) en el host,
aunque su kernel aislado se reduce. No se interpreta esa variación como una
mejora ni se extrapolan estos porcentajes a toda la aplicación.

**Son medidas de componentes en un host x86-64, con SDL por software.** No
representan el coste de la GPU del Pixel 7 Pro, fotogramas de una partida ni
una garantía de 60/120 FPS. Tampoco miden consumo o temperatura. La revisión
del APK, las pruebas de pacing y una prueba física del teléfono se registran
por separado.

## Reproducción

Con GCC compatible con `noipa`, C++20 y los archivos de desarrollo de SDL2:

```sh
mkdir -p /tmp/sma3-render-baseline
git show 40849aeef03a5cdd569a19f35034c8233c9520d2:src/image_filters.h > /tmp/sma3-render-baseline/image_filters.h
git show 40849aeef03a5cdd569a19f35034c8233c9520d2:src/mod_color_profile.h > /tmp/sma3-render-baseline/mod_color_profile.h
git show 40849aeef03a5cdd569a19f35034c8233c9520d2:src/video_scaler.h > /tmp/sma3-render-baseline/video_scaler.h

c++ -std=c++20 -O2 -I src validation/image_filter_pixels_test.cpp -o /tmp/sma3-filter-oracle
c++ -std=c++20 -O2 -I src validation/color_profile_equivalence_test.cpp -o /tmp/sma3-color-oracle
/tmp/sma3-filter-oracle
/tmp/sma3-color-oracle

c++ -std=c++20 -O3 -DNDEBUG -I src \
  '-DSMA3_BASELINE_FILTER_HEADER="/tmp/sma3-render-baseline/image_filters.h"' \
  '-DSMA3_BASELINE_COLOR_HEADER="/tmp/sma3-render-baseline/mod_color_profile.h"' \
  validation/render_cpu_comparison.cpp -o /tmp/sma3-render-cpu
/tmp/sma3-render-cpu

c++ -std=c++20 -O3 -DNDEBUG -I src -I /usr/include/SDL2 \
  '-DSMA3_BASELINE_SCALER_HEADER="/tmp/sma3-render-baseline/video_scaler.h"' \
  validation/render_scaler_comparison.cpp -lSDL2 -o /tmp/sma3-render-scaler
/tmp/sma3-render-scaler --validate-only
/tmp/sma3-render-scaler --benchmark-only
```

Las rutas de SDL pueden variar según la distribución. Para los oráculos
con sanitizadores, usar `-O1 -g -fsanitize=address,undefined
-fno-omit-frame-pointer` en lugar de `-O2`. No se necesita ROM ni BIOS.

## Referencias técnicas

- [Reglas originales Scale2x/Scale3x y réplica del borde](https://www.scale2x.it/algorithm).
- [SDL_RenderCopy: copia al target actual y rectángulo de destino](https://wiki.libsdl.org/SDL2/SDL_RenderCopy).
- [SDL_SetTextureScaleMode](https://wiki.libsdl.org/SDL2/SDL_SetTextureScaleMode).
