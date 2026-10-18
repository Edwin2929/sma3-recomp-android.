# Proporción de imagen y pantalla completa — incidencia #1

Revisión del 18 de octubre de 2026, sobre la base
`fab4fb618b9d6aa5a48e4cc3a2227309779723b5`.
Incidencia: [#1](https://github.com/Edwin2929/sma3-recomp-android./issues/1).

## Opción disponible en Android 0.9

**Atrás → Imagen → Estirar a pantalla completa → desactivar.**
La app sigue a pantalla completa; el fotograma completo se centra y deja
bandas cuando la superficie tiene otra proporción. La preferencia
`stretch-screen` se guarda y su valor predeterminado es `false`.

La fuente original mide **240 × 160**, por lo que su proporción es **3:2**.
El motor fijado define esas dimensiones en
[`gba_ppu.h`](https://github.com/mstan/gbarecomp/blob/e7728148c6829ba526f682876430a0c9022dc6c0/src/gba/gba_ppu.h).
Una relación 4:3 no corresponde al fotograma original de GBA.

Se inspeccionó el APK de la
[release Descarga](https://github.com/Edwin2929/sma3-recomp-android./releases/tag/Descarga):

- Archivo: `sma3-recomp-0.9-layouts-arm64.apk`, 31.838.319 bytes.
- SHA-256 verificado: `7110024294051368945fd4ceb127cfaf4edb3a09f0e138ae6b81ae350a3dee3d`.
- Las cadenas DEX incluyen `Estirar a pantalla completa`,
  `Stretch to full screen`, `Esticar para tela inteira`, los nombres del
  submenú Imagen/Image/Imagem y `stretch-screen`.

Esta inspección del APK es estática: no se instaló ni se ejecutó en un teléfono.

## Corrección del cálculo del rectángulo

La implementación anterior redondeaba la altura antes de decidir qué eje
limitaba el escalado. En la frontera entre ambos ejes podía añadir un píxel
de ancho innecesario:

| Superficie | Resultado anterior | Resultado corregido |
| --- | --- | --- |
| 241 × 160 | 241 × 160 | 240 × 160 |
| 1081 × 720 | 1081 × 720 | 1080 × 720 |

Además, con una superficie de `INT_MAX × 1080` y una fuente de `160 × 240`,
el resultado intermedio se convertía a `int` antes de estar acotado. En el
compilador de esta revisión produjo una altura negativa.

[`src/video_scaler.h`](../src/video_scaler.h) compara ahora los productos
en 64 bits antes de dividir. Elige primero el eje limitante y convierte a
`int` únicamente una dimensión que cabe en la superficie. En tamaños
habituales, como 2400 × 1080, el resultado no cambia.

Al representar el rectángulo con píxeles enteros, la dimensión no limitante
puede redondearse hacia abajo menos de un píxel. Los márgenes opuestos pueden
diferir un píxel cuando el espacio sobrante es impar. No se recorta el
fotograma ni se añade escenario.

## Comprobar el escalador

Resultado: **45 casos de geometría y 110 comprobaciones de renderizado
aprobados**. Los 45 casos agrupan 21 configuraciones válidas, comprobadas con
y sin estirar, y 24 variantes de entradas inválidas. Los 110 renderizados
recorren diez tamaños, vuelven al tamaño inicial y cambian entre Automática,
720p, 1080p, 720p y Automática, con ambos valores de Estirar.

Se comprueban los píxeles de un patrón de cuatro cuadrantes y un cuadrado
central: imagen completa, bordes, bandas, proporciones finales después del
buffer intermedio, restauración del destino de renderizado y reutilización
o liberación de texturas. Los cambios de orientación se simulan con destinos
SDL de distinto tamaño, manteniendo el mismo renderizador y escalador.

| Superficie comprobada | Imagen sin estirar | Márgenes |
| --- | --- | --- |
| 2400 × 1080 | 1620 × 1080 | 390 a izquierda y derecha |
| 1920 × 1080 | 1620 × 1080 | 150 a izquierda y derecha |
| 1080 × 2400 | 1080 × 720 | 840 arriba y abajo |
| 1080 × 1080 | 1080 × 720 | 180 arriba y abajo |
| 2048 × 1536 | 2048 × 1365 | 85 arriba, 86 abajo |
| 1536 × 2048 | 1536 × 1024 | 512 arriba y abajo |
| 2341 × 1081 | 1621 × 1081 | 360 a izquierda y derecha |

Entorno: Ubuntu 24.04.3, Linux x86_64, g++ 13.3.0, encabezados y biblioteca
SDL2 2.30.0; renderizador `software`. La suite nueva ejecutada con el
encabezado anterior falla con código de salida 1 en
`rounding boundary native` (241 × 160), reproduciendo el defecto. Con el
encabezado corregido termina con código de salida 0.

La compilación final usa `-Wall -Wextra -Wpedantic -Werror`, sin advertencias.
Una segunda ejecución completa con `-fsanitize=undefined
-fno-sanitize-recover=undefined` también aprueba los 45 casos de geometría y
110 renderizados, sin diagnósticos del sanitizador.

El código de la prueba está en
[`video_scaler_test.cpp`](video_scaler_test.cpp) y su salida en
[`video-scaler-test.txt`](video-scaler-test.txt).

Para reproducirla en Linux con un compilador C++20 y los encabezados y la
biblioteca de desarrollo de SDL2 disponibles, desde la raíz del repositorio:

```sh
c++ -std=c++20 -O2 -Wall -Wextra -Werror -I src \
  $(sdl2-config --cflags) validation/video_scaler_test.cpp \
  $(sdl2-config --libs) -o /tmp/sma3-video-scaler-test
/tmp/sma3-video-scaler-test
```

La prueba usa el renderizador software real de SDL2 y no requiere ROM, BIOS,
un servidor gráfico ni el motor completo.

## Revisión de la integración

- El menú aplica la preferencia a través de `GbaNative.setVideoOptions`;
  [`frame_interpolation_jni.cpp`](../src/frame_interpolation_jni.cpp) la pasa
  a `sma3::stretch_screen`.
- El [parche del renderizador](../patches/gbarecomp-controls.patch) calcula
  el destino con el tamaño actual de salida. El motor vuelve a consultar ese
  tamaño al presentar un fotograma. El camino de respaldo, si falla el
  escalador, utiliza el mismo rectángulo.
- Automática, 720p y 1080p comparten el rectángulo final. Los buffers
  intermedios de 1280 × 720 y 1920 × 1080 no definen la proporción final ni
  añaden campo de visión.
- Los controles y demás capas se dibujan después del juego. Las bandas usan
  el color oscuro del fondo; no tienen por qué ser negro puro.

## Alcance

Las superficies verticales comprueban el cálculo y el escalador. **No
habilitan ni validan orientación vertical en Android 0.9 o 0.9.1**, cuya
configuración del juego es horizontal. Sigue pendiente la prueba visual,
de GPU y de cambios de superficie en un dispositivo físico, incluido el
POCO X7 Pro mencionado en la incidencia.

La ayuda nueva del menú en ES/EN/PT y la corrección del cálculo se incorporaron
al APK `sma3-recomp-0.9.1-aspect-arm64.apk`, compilado y verificado después de
integrar la PR #2. El 18 de octubre de 2026, Edwin confirmó que ese APK
funciona en un Google Pixel 7 Pro. Es una confirmación general del usuario,
sin resultados separados de cada ajuste ni mediciones de FPS. Su subida a
GitHub Releases queda pendiente de publicación manual; el APK 0.9 publicado
no ha sido sustituido. Véase el
[registro de la compilación y la prueba reportada](ANDROID_0.9.1_VALIDATION.md).

La panorámica real sigue siendo un prototipo de escritorio.
[`CMakeLists.txt`](../CMakeLists.txt) la desactiva por defecto y rechaza su
activación para Android. **No está disponible en los APK 0.9 y 0.9.1**. Su estado
se documenta en [PANORAMICA_EN_DESARROLLO.md](../docs/PANORAMICA_EN_DESARROLLO.md).
