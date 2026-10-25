# Mods de color — validación de 0.10.4 beta

## Resultado y alcance

El apartado **Atrás → Mods** administra paquetes `.gbamod` que aplican un
perfil de color a la imagen presentada. Permite importar un archivo o
instalar el ejemplo incluido, consultar sus datos, activarlo, desactivarlo
y eliminarlo. Las importaciones empiezan **desactivadas**. Activar otro
perfil sustituye al anterior y la selección se guarda para el siguiente
inicio.

Esta versión admite el plugin declarativo `sma3.color-profile`, con la
función `colors`, para la versión USA de SMA3 identificada en el manifiesto.
La matriz contiene hasta nueve coeficientes enteros de −200 a 200, con
denominador 100. Los coeficientes omitidos conservan la matriz de identidad.
No se admiten bibliotecas ejecutables, parches IPS/BPS, cambios de ROM ni
mods de otros juegos. La guía de autores y el ejemplo editable están en
[`MODS.md`](../docs/MODS.md) y
[`mods/examples/tono-calido`](../mods/examples/tono-calido).

**Las pruebas descritas aquí pasan.** Comprueban el catálogo real, archivos
inválidos, persistencia, restauración y composición gráfica en el equipo de
desarrollo. La construcción del APK y su inspección se registran por
separado. No se ha probado este apartado en un teléfono físico.

## Catálogo real y archivos adversos

La revisión independiente usa
[`mod_adversarial_probe.cpp`](mod_adversarial_probe.cpp), enlazado con el
`mod_runtime.cpp` real del motor, y
[`mod_adversarial_test.py`](mod_adversarial_test.py). No sustituye las
operaciones nativas por respuestas simuladas. Las nueve pruebas pasan en
compilación optimizada y con AddressSanitizer/UndefinedBehaviorSanitizer.

| Área | Comprobación y resultado |
| --- | --- |
| Importación | Un manifiesto que pide activación por defecto se instala desactivado. Se conserva correctamente el nombre Unicode `Revisión 🌈` y el JSON destinado a JNI es ASCII válido con escapes Unicode. |
| Activación | El perfil cálido modifica los píxeles esperados. Al activar un segundo perfil, solo ese segundo aparece activo. |
| Desactivar y eliminar | Desactivar devuelve exactamente los píxeles originales. Eliminar el perfil activo también los restaura y retira su carpeta. |
| Esquema del manifiesto | Se rechazan 20 variantes: versiones de formato inválidas, rutas en la versión, juego o SHA incorrectos, plugin u opción desconocidos, coeficientes fuera de rango o desbordados, límites/tipo/paso incorrectos, claves u opciones duplicadas, texto UTF-8 inválido y función incompatible. |
| Archivo ZIP | Se rechazan 12 variantes: rutas ascendentes, absolutas, de Windows o anidadas; entradas duplicadas; carga binaria ajena; manifiesto excesivo; expansión excesiva; directorios; CRC incorrecto; ZIP truncado; archivo comprimido demasiado grande. |
| Identificadores | Se rechaza la importación de un identificador ya instalado y 15 llamadas de activación/desactivación/eliminación con identificadores vacíos o inválidos. |
| Fallos de persistencia | Se bloquea deliberadamente la escritura de `state.toml.tmp`. Fallan activar otro perfil, desactivar, importar y eliminar; el catálogo, los archivos previos y la imagen activa quedan iguales. La importación fallida no deja un paquete instalado. |
| Inicio y recuperación | La entrada de producción verifica la ROM autorizada y recupera la selección. Un manifiesto instalado dañado queda visible como incompatible y eliminable; la imagen vuelve a sus colores normales. |
| Selección dañada | Versión desconocida, clave `enabled` duplicada, cadena truncada y sección malformada provocan recuperación con aviso y todos los perfiles desactivados. No se activa una selección leída parcialmente. |
| ROM incorrecta | Una reinicialización que no supera la comprobación SHA borra la autorización anterior y no permite volver a activar su perfil. |
| Concurrencia | Se realizan 100 cambios reales del catálogo en un hilo mientras otro transforma al menos 100 fotogramas completos de 240 × 160. Cada fotograma utiliza íntegramente una sola matriz; no mezcla canales o filas de dos perfiles. |

Las 20 variantes de manifiesto, las 12 variantes ZIP y el identificador
duplicado suman **33 importaciones rechazadas**. En cada una se comprueban
el catálogo, los hashes de archivos persistentes y los píxeles activos
antes y después. Estos casos son entradas sintéticas; no representan
recorridos distintos del juego.

Los límites aplicados son 2 MiB por archivo comprimido, 1 MiB total
descomprimido, 64 KiB para `manifest.toml`, hasta 32 entradas ZIP y hasta
64 perfiles instalados. El paquete solo admite el manifiesto y archivos de
documentación de los nombres permitidos. La validación del motor comprueba
las rutas y los datos del archivo antes de publicar el paquete.

### Prueba concreta del efecto

La revisión importa un paquete real con `rr=108`, `gg=100` y `bb=88`, lo
activa por la misma fachada nativa utilizada desde Android y consulta la
transformación de presentación:

| RGB de origen | Perfil cálido activo | Tras desactivar |
| --- | --- | --- |
| `(120, 150, 200)` | `(130, 150, 176)` | `(120, 150, 200)` |
| `(255, 1, 2)` | `(255, 1, 2)` | `(255, 1, 2)` |
| `(0, 255, 99)` | `(0, 255, 87)` | `(0, 255, 99)` |

La matriz redondea al entero más cercano y limita cada canal a 0–255. El
perfil solo transforma la copia de presentación: no modifica los píxeles
de origen, la memoria de la partida ni sus archivos de guardado.

La batería del implementador,
[`test_mod_profiles.py`](test_mod_profiles.py), aporta **otras nueve pruebas**
con un punto de inicialización sin ROM. También pasa con ASan/UBSan y
cubre valores extremos, identidad, rechazo de operaciones de ROM, cambios
externos del manifiesto, fallo de renombrado y 10.000 publicaciones
concurrentes de matrices. Ambas baterías se solapan en varios casos; sus
recuentos no deben interpretarse como 18 funcionalidades independientes.

## Composición con filtros y mezcla temporal

[`mod_render_contract_test.cpp`](mod_render_contract_test.cpp) usa los
encabezados reales `FrameBlend`, `ColorProfile` y `VideoScaler`, junto con
el renderizador de software SDL. Comprueba **896 presentaciones**,
**7.612.832 comparaciones de píxeles de franjas** y **7.907.723 aserciones**.
La ejecución optimizada y la instrumentada con ASan/UBSan pasan.

La matriz de casos incluye los siete filtros, fuentes de 240 y 356 × 160,
salidas de 643 × 401 y 321 × 679, proporción conservada o estirada y calidad
Automática/720p. Cada combinación recibe tanto el fotograma normal como el
punto intermedio producido por el `FrameBlend` real. Se verifica que:

- El perfil procesa los valores RGB del fotograma actual, incluido el
  intermedio; una referencia aritmética independiente produce la misma
  imagen para cada filtro.
- Desactivar el perfil recupera exactamente la presentación anterior, sin
  textura del mod retenida en la caché del escalador.
- Las franjas del filtro Cinematográfico permanecen negras y opacas; el
  centro y los márgenes coinciden con Equilibrado aplicado al mismo perfil.
- Un marcador dibujado después de los filtros conserva su color y aparece
  sobre la franja. El color y la mezcla de SDL quedan restaurados para los
  controles superpuestos.
- Los buffers originales permanecen intactos.

La revisión del parche confirma el orden de producción:
`FrameBlend → transformación del perfil → textura/filtro → controles`.
El renderizador toma una instantánea inmutable del perfil para cada
presentación. Esta prueba de composición usa un marcador de control;
no ejecuta eventos táctiles de Android.

La validación específica de las dos franjas, de los tamaños pequeños y de
la igualdad exacta de los filtros anteriores se encuentra en
[`CINEMATIC_0.10.4_VALIDATION.md`](CINEMATIC_0.10.4_VALIDATION.md).

## Correcciones verificadas durante la revisión

Se corrigieron tres problemas concretos antes de congelar las fuentes:

1. El guardado heredado intentaba eliminar el destino si fallaba un
   renombrado. Ahora un fallo de publicación no elimina el estado anterior.
   La instalación nueva se revierte y la eliminación aparcada se restaura
   cuando falla la confirmación.
2. Una comprobación de ROM fallida podía dejar el catálogo anterior
   disponible en una reinicialización. Ahora se limpian tanto el estado
   autorizado como la instantánea de color antes de verificar la ROM.
3. La lectura tolerante del archivo de selección ignoraba versiones,
   duplicados y líneas malformadas, y podía conservar una activación parcial.
   El lector limitado para perfiles exige un formato completo y coherente;
   si falla, recupera el catálogo con todos los perfiles desactivados y
   muestra un aviso.

También se endurecieron las claves duplicadas del manifiesto, los
componentes de versión usados como rutas, los desbordamientos al validar
opciones y las entradas ZIP duplicadas o con NUL. La gestión mantiene los
paquetes incompatibles visibles para poder retirarlos.

## Android y aplicación de los parches

La revisión de Java confirma la cola única de trabajo para copias y
operaciones nativas, el límite medido en bytes leídos, la limpieza de la
copia temporal, la cancelación del selector y el descarte de respuestas
tardías cuando se abandona la página o se destruye la actividad. Si Android
recrea la actividad con el selector abierto, se reconstruye el controlador
antes de entregarle el resultado. La navegación reutiliza el bloqueo de
entrada del menú de Ajustes.

Estas son comprobaciones del código. La compilación aislada de las cinco
clases Java con API 35 y las **26 pruebas del ayudante de transferencia**
pasan. La revisión adicional del empaquetador y del ejemplo completa
**37 comprobaciones** e importa también el manifiesto exacto de la guía.
Los detalles y las limitaciones de idioma están en
[`ANDROID_0.10.4_MODS_SETTINGS.md`](ANDROID_0.10.4_MODS_SETTINGS.md).

[`test_window_patch_stack.py`](test_window_patch_stack.py) pasa sus tres
pruebas con los diez parches: los **11 prefijos posibles** terminan en el
mismo resultado y una segunda ejecución no cambia archivos. Los cambios
locales desconocidos en cualquiera de los **7 archivos protegidos** se
rechazan sin escritura; también se rechaza un motor de versión incorrecta.
El motor requerido sigue siendo
`e7728148c6829ba526f682876430a0c9022dc6c0`.

## Límites de la evidencia

No se ha ejecutado el selector de documentos, la navegación o los gestos
en un dispositivo Android físico. Tampoco se han medido FPS, batería ni
controladores gráficos de un Pixel 7 Pro. La comprobación de arranque nativo
verifica la ROM local autorizada, pero no ejecuta una nueva partida completa
ni amplía la cobertura de niveles del modo Panorámica beta.

Las pruebas gráficas usan SDL 2.30.0 de software. La biblioteca SDL del
sistema no está instrumentada. LeakSanitizer se desactiva por las
restricciones del entorno; AddressSanitizer y UndefinedBehaviorSanitizer
permanecen activos. La inclusión de `frame_interpolation.h` trae un aviso
preexistente de indentación en `touch_layout.h`; se conserva el aviso y se
exceptúa únicamente esa categoría de `-Werror` al compilar la prueba de
composición. Los fallos de E/S probados son deterministas; no se simulan
todas las posibles pérdidas de alimentación o condiciones de falta de memoria.

Los resultados y hashes de las fuentes revisadas están en
[`android-0.10.4-mods-validation.json`](android-0.10.4-mods-validation.json).

## Reproducción

Desde la raíz del repositorio, con C++20, Python 3 y el motor parcheado en
`../gbarecomp`:

```sh
c++ -std=c++20 -O2 -pthread -DSMA3_MOD_PROFILES=1 \
  -I src -I ../gbarecomp/src/runtime -I ../gbarecomp/src/gba \
  -I ../gbarecomp/src/armv4t \
  validation/mod_adversarial_probe.cpp \
  ../gbarecomp/src/runtime/mod_runtime.cpp \
  ../gbarecomp/tests/mod_runtime/ppu_provider_stub.cpp \
  ../gbarecomp/src/runtime/mod_function_hooks.cpp \
  ../gbarecomp/src/runtime/asset_picker.cpp \
  ../gbarecomp/src/runtime/host_platform.cpp \
  ../gbarecomp/src/gba/mod_audio.cpp ../gbarecomp/src/gba/crc32.cpp \
  ../gbarecomp/src/gba/sha1.cpp -o /tmp/sma3-mod-review
SMA3_MOD_PROBE=/tmp/sma3-mod-review python3 -B validation/mod_adversarial_test.py

c++ -std=c++20 -O2 -Wall -Wextra -Werror -Wno-error=misleading-indentation \
  -pthread -I src $(pkg-config --cflags sdl2) \
  validation/mod_render_contract_test.cpp $(pkg-config --libs sdl2) \
  -o /tmp/sma3-mod-render-test
SDL_VIDEODRIVER=dummy /tmp/sma3-mod-render-test

python3 -B validation/test_window_patch_stack.py
```

Las tres pruebas que pasan por la inicialización de producción se omiten
si no se proporciona `SMA3_MOD_TEST_ROM` con una ROM local autorizada y del
SHA esperado. En esta revisión se ejecutaron las nueve, sin omisiones. No
se distribuye el archivo de juego con las pruebas.

Para repetir con sanitizadores, sustituye `-O2` por
`-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer` y usa
`ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1`.
