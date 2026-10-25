# Controles de Android 0.10.5-beta

Fecha de validación: 25 de octubre de 2026.

## Resultado

Los controles táctiles incorporan símbolos de acciones y conservan una opción de
letras clásicas. La opacidad se ajusta entre el 20 % y el 100 %, con el 75 % como
valor inicial. El editor de diseños utiliza los mismos colores y símbolos y lee
las mismas preferencias que el juego.

Se corrigió una limitación comprobada del renderizador de Android: el backend
SDL declara `text_supported() == false`, por lo que el código anterior omitía
las etiquetas A/B/L/R/SELECT/START. Ahora ambas variantes usan trazos vectoriales
y funcionan sin una fuente o una biblioteca adicional de texto.

Ruta del ajuste: **Atrás → Controles → Símbolos y opacidad**. El estilo y la
opacidad se guardan al cambiarlos. La visibilidad completa de los controles se
ajusta por separado en **Controles táctiles**; una opacidad mínima del 20 % evita
crear por accidente botones invisibles que todavía reciben toques.

## Símbolos y alcance

| Control | Símbolo | Acción principal en Yoshi's Island |
| --- | --- | --- |
| Cruceta | Cuatro flechas | Mover y mirar; abajo también permite agacharse, tragar o hacer un salto bomba según el contexto. |
| A | Flecha ascendente | Saltar; mantener el botón permite aletear. |
| B | Lengua | Comer o escupir; la acción depende de lo que lleve Yoshi en la boca. |
| L | Mira | Fijar o liberar el cursor al apuntar un huevo. |
| R | Huevo con manchas | Apuntar y lanzar huevos según el estilo de lanzamiento configurado en el juego. |
| SELECT | Lista | Abrir la pantalla de estado. |
| START | Dos barras | Pausar el juego. |

Fuente del mapa de acciones: [manual oficial de Nintendo para Yoshi's Island:
Super Mario Advance 3](https://www.nintendo.com/eu/media/downloads/games_8/emanuals/game_boy_advance_8/Manual_GameBoyAdvance_YoshisIslandSuperMarioAdvance3_EN_DE_FR_ES_IT.pdf),
sección inglesa, páginas impresas 6–7, página 4 del PDF. La página fue inspeccionada
visualmente. El menú explica que las acciones pueden cambiar en transformaciones
o en Mario Bros. Los símbolos describen las acciones principales y no reasignan
los botones del juego.

El formato de los diseños guardados sigue siendo el array de 28 valores: centro
x/y y ancho/alto de cada uno de los siete controles. La apariencia se almacena en
preferencias independientes (`touch-style` y `touch-opacity`). Las zonas que
detectan los toques, el arrastre del editor y las asignaciones de bits de entrada
conservan su implementación anterior.

## Implementación validada

- `src/touch_appearance.h` valida los valores y publica estilo y opacidad juntos
  en una sola variable atómica. El renderizador lee una única instantánea por
  dibujo completo de los controles.
- `src/touch_appearance_renderer.h` dibuja los símbolos, las letras, la cruceta y
  las superficies de los botones. Los estados pulsados mantienen un color visible.
- `android/java/org/gbarecomp/TouchAppearance.java` dibuja las variantes del editor.
  `TouchDesignEditor.java` utiliza ese helper y nombres de acciones en español,
  inglés y portugués. El contorno de selección conserva su intensidad para que
  un control al 20 % siga siendo fácil de arrastrar.
- `patches/gbarecomp-touch-appearance.patch` es el parche número 12 y se aplica
  después de `gbarecomp-frame-pacing.patch`. Integra el dibujo y restaura el modo
  de mezcla y el color SDL del llamador después de renderizar las capas de la
  interfaz. También se verifican las restauraciones existentes del tamaño lógico
  y del viewport.

Las esquinas y el suavizado pueden diferir entre Canvas y SDL: el backend SDL
existente no utiliza el parámetro de redondeo de rectángulos. Los colores,
símbolos, etiquetas, preferencias y geometría de los controles corresponden a la
misma configuración; no se afirma identidad píxel por píxel entre ambos backends.

## Prueba con el renderizador real

`validation/touch_appearance_test.cpp` incluye el `host_window.cpp` de producción
del motor parcheado y ejecuta sus funciones reales `compute_pad_layout`,
`pad_buttons_at`, `SdlOverlay` y `render_host_layers`, sobre superficies SDL de
software. No sustituye esas funciones por implementaciones de prueba.

| Verificación | Casos y resultado |
| --- | --- |
| Tamaños y orientaciones | 960 × 540, 540 × 960 y 641 × 401; áreas seguras con márgenes distintos en cada lado. |
| Apariencia | Dos estilos × tres opacidades (20/75/100 %) × tres superficies: 18 presentaciones. |
| Controles ocultos | Una comprobación adicional por superficie: la imagen de fondo queda intacta. |
| Geometría y detección de toques | Misma geometría antes y después; comparación de los bits de entrada en una cuadrícula de puntos en toda la superficie para cada combinación. |
| Glifos sin fuentes | Se detecta tinta visible dentro de A, B, L, R, SELECT y START en ambas variantes. |
| Opacidad | El relleno de A al 100 % coincide con su RGB opaco; el cambio de 20 a 75 y a 100 % modifica progresivamente el resultado visible. |
| Diseño personalizado | A cambiado de posición y tamaño; dos estilos × tres opacidades, con y sin pulsación: 12 dibujos adicionales. Se conserva la detección del botón A. |
| Estado SDL | Se conservan color, modo de mezcla, viewport personalizado y tamaño lógico no nulo. |
| Cambios concurrentes | 100 cuadros dibujados mientras otro hilo alterna clásico/20 % y símbolos/100 %. Cada cuadro coincide con una de las dos configuraciones completas. |
| Preferencias inválidas | Valores negativos, extremos de `int`, estilos desconocidos y opacidad fuera de rango se normalizan sin desbordamientos. |

Resultado de la compilación optimizada:

```text
appearance presentations=21
PASS touch appearance assertions=66685 concurrent_frames=100
```

La misma prueba compilada con AddressSanitizer y UndefinedBehaviorSanitizer
produjo exactamente el mismo resultado, sin diagnósticos. Se utilizó
`-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
-fno-sanitize-recover=all`, `ASAN_OPTIONS=detect_leaks=0:abort_on_error=1` y
`UBSAN_OPTIONS=halt_on_error=1`. La biblioteca SDL del sistema no está instrumentada
y esta ejecución no incluye LeakSanitizer ni ThreadSanitizer.

El contador de 21 presentaciones corresponde a la matriz de 18 combinaciones más
los tres casos ocultos. La prueba ejecuta además los dibujos del diseño
personalizado, el caso de conservación del estado y las referencias y cuadros
concurrentes; no deben confundirse con ese contador parcial.

## Compilación y reproducción

Se compilaron `TouchAppearance.java` y `TouchDesignEditor.java` con JDK 17,
`-source 17 -target 17`, el SDK de Android API 35 y las clases de dependencia del
proyecto. Resultado: código de salida 0, sin mensajes del compilador. La
compilación completa del APK y del resto de la interfaz se valida en el informe
general de la versión.

Para reproducir la prueba nativa desde la raíz del repositorio, con SDL2 de
desarrollo y el motor `../gbarecomp` ya preparado con los 12 parches:

```bash
c++ -std=c++20 -O2 -ffunction-sections -fdata-sections -pthread \
  -DSMA3_FRAME_INTERPOLATION=1 -DGBARECOMP_HAVE_SDL2=1 \
  -I src -I ../gbarecomp/src/runtime \
  $(pkg-config --cflags sdl2) \
  validation/touch_appearance_test.cpp \
  ../gbarecomp/src/runtime/touch_input.cpp \
  -Wl,--gc-sections $(pkg-config --libs sdl2) -o /tmp/sma3-touch-test
/tmp/sma3-touch-test /tmp/sma3-touch-preview
```

En el entorno de esta validación no estaba instalado `pkg-config`; se usaron las
cabeceras SDL2 ya disponibles en la caché de compilación y la biblioteca del
sistema `libSDL2-2.0.so.0` (SDL 2.30.0). El ejecutable optimizado y el instrumentado
se compilaron contra el motor compartido final, sin redefinir
`SMA3_HOST_WINDOW_SOURCE`.

La prueba genera `classic.bmp` y `symbols.bmp` a 960 × 540. Se convirtieron a PNG
y se inspeccionaron visualmente: ambas variantes muestran sus etiquetas o
símbolos y un fondo transparente sobre el color de prueba. Son capturas del
renderizador SDL de software, no capturas de una sesión Android ni pruebas de
funcionamiento en un Pixel 7 Pro.

## Trazabilidad y límites

Motor fijado: `e7728148c6829ba526f682876430a0c9022dc6c0`, con la secuencia de parches
del proyecto. SHA-256 de `host_window.cpp` después del parche 11:
`827cf216700669cceb9ae51bbded264ee475c0578ec280a0edef24189b149143`.
Después del parche 12:
`a94be0ce9e8a041f4a41d8940c120b4c31ca0cb42e336428e159bea10d90993e`.

Los hashes de los archivos del componente y los resultados estructurados están
en `validation/android-0.10.5-controls.json`. Estas pruebas cubren el dibujo real,
el estado de SDL y las zonas de entrada del código nativo. No reemplazan una
prueba de gestos multitáctiles, persistencia después de reiniciar la aplicación
y legibilidad sobre escenarios reales en un teléfono. Tampoco establecen una
garantía de frecuencia de cuadros en hardware físico.
