# Traducción ampliada — validación de 0.10.15 beta

Base publicada: **0.10.14**, commit
`09146420df7bc6e184f3bc6901187744c5562230`.
Motor GBARecomp: `e7728148c6829ba526f682876430a0c9022dc6c0`, con la misma
serie de **19 parches**. Disassembly:
`KarisaAdvynia/sma3-disasm`, commit `c8532ec9a8d0038c3bfeb003dd0c7ea89d7e1071`.
La identidad del juego se comprueba contra la ROM USA SHA-1
`7352d2bd064d9ebaec579e264228aa21c7345b80`.

La entrega añade textos de créditos y seis rótulos de la interfaz del juego
en español latinoamericano y portugués de Brasil. Las comprobaciones que
siguen separan recursos, lectura del runtime, ejecución nativa y paquete
Android. **No constituyen una prueba física de teléfono ni una traducción
completa de todo el juego.**

## 1. Cobertura y compatibilidad de textos anteriores

| Elemento | Antes | 0.10.15 |
| --- | ---: | ---: |
| Paquetes de texto por idioma | 187 | 207 |
| Paquetes nuevos de créditos por idioma | 0 | 20 |
| Posiciones verificadas de punteros de texto | 267 | 287 |
| Rótulos gráficos añadidos por idioma | 0 | 6 |
| Punteros a recursos gráficos nuevos | 0 | 4 |
| Punteros a la copia de fuente de créditos | 0 | 2 |

El generador emite primero los 187 paquetes anteriores en su orden histórico
y añade los créditos al final. Una ordenación global por dirección de origen
habría desplazado los bancos anteriores y roto las referencias de mensajes
que ya estuvieran abiertos en una partida guardada.

Se verifican los **267 punteros anteriores** y los prefijos completos de los
bancos ES/PT, de **16.915 / 16.499 bytes**, contra el recurso usado en 0.10.14:
todos son idénticos. Los glifos portugueses anteriores también se mantienen.
Los bancos finales de texto ocupan **17.750 / 17.309 bytes**.

Los bancos de cada idioma permanecen legibles independientemente de la
selección actual. Cambiar de idioma afecta a la próxima resolución de un
puntero, sin reemplazar los datos de un paquete ya abierto. Los nuevos
rótulos aparecen cuando el juego vuelve a cargar sus recursos de pantalla.

[Datos de cobertura](../localization/coverage.json) ·
[Generador](../tools/generate_localization.py).

## 2. Créditos: fuente, acentos y comandos

Se contrastan con la ROM los **39 paquetes originales**. Se traducen veinte:
encabezado, cargos, equipos, agradecimientos, aviso de derechos y narración
final. Los otros diecinueve, con nombres propios o separación, conservan sus
punteros originales. Los nombres, SRD, Nintendo y el orden del equipo no se
reemplazan por autores de esta traducción.

Los paquetes `0x00–0x23` usan una fuente que resta `0x80` al código del carácter.
Los códigos acentuados de la fuente principal son inferiores a ese límite;
reutilizarlos habría indexado fuera del comienzo de la tabla. Los paquetes
`0x24–0x26` usan la fuente principal y se codifican con sus reglas correspondientes.

Se genera una **copia privada e inmutable de 1.664 bytes**: 1.536 bytes de
fuente y 128 de anchos. Siete glifos acentuados se añaden a posiciones que
ninguno de los 39 paquetes originales usa. Las posiciones originales no
estaban vacías, por lo que sus datos en ROM no se sobrescriben.

Dos literales Thumb verificados redirigen el renderer de créditos a esa copia:

| Literal | Fuente original | Destino privado |
| --- | --- | --- |
| `08033D80` | Fuente `0816D509` | `09D00000` |
| `08033E58` | Anchos `0816D489` | `09D00600` |

Los **53 glifos utilizados por los créditos originales en inglés** y sus
anchos son idénticos. La redirección de fuente es estable incluso al cambiar
el idioma, para que un crédito traducido abierto mantenga sus acentos. Esto
puede cambiar valores de punteros temporales durante la ejecución inglesa
de los créditos; no se promete igualdad binaria de todos esos registros.

La prueba de recursos pasa **19.623 comprobaciones**: **80 filas traducidas**,
comandos, filas y posiciones verticales, centrado, límites, anchos de tinta,
los siete acentos y compatibilidad anterior. La mayor línea ocupa **228
píxeles**, con seis de margen a cada lado sobre el ancho original de 240.
También se rechazan catálogos incompletos, duplicados o que introduzcan
comandos del renderer.

La hoja de glifos se revisó visualmente como recurso; no se presenta como
una captura de un recorrido del juego. El header privado completo
`src/localization_data.h`, que incluye textos, gráficos y fuente, se regeneró
dos veces de forma idéntica. Su SHA-256 es:

```text
86c2cb00567a566521c2a71835a78a68b66744fb2f41147b8b5441345de8e544
```

[Catálogo de créditos](../localization/credits.tsv) ·
[Generación de fuente y paquetes](../tools/localization_credits.py) ·
[Prueba](localization_credits_test.py) ·
[Resultados](localization-credits-0.10.15-results.json).

## 3. Rótulos de archivos e inventario

| Rótulo | Español | Portugués |
| --- | --- | --- |
| Archivos | ARCHIVOS | ARQUIVOS |
| Copiar | COPIAR | COPIAR |
| Borrar | BORRAR | APAGAR |
| Mundo | MUNDO | MUNDO |
| Mejor puntuación | MEJOR | RECORDE |
| Puntos totales | PUNTOS TOTALES | PONTOS TOTAIS |

Las tres cabeceras de archivos proceden del recurso LZ77 de `081A42D8`.
Solo se redibujan las regiones de letras con la fuente del propio juego;
el resultado conserva **3.584 bytes descomprimidos**, tilemap, paleta, bordes
y geometría. Los anchos resultantes son 61, 45 y 48 píxeles, dentro de cajas
de 72, 88 y 88 píxeles. El descompresor original del juego procesa los datos.

El inventario utiliza tres tablas de 24 caracteres. Se conservan los huecos
dinámicos de mundo, nivel y puntuación, sus separadores y unidades. La inicial
estática W de la fila de récord pasa a M, como abreviatura de Mundo. Los
números siguen siendo escritos por el juego en sus posiciones anteriores.

| Literal verificado | Recurso original |
| --- | --- |
| `080FE930` | Cabeceras `081A42D8` |
| `080E75C0` | Puntos totales `08194868` |
| `080E75D4` | Récord `08194880` |
| `080E75E0` | Mundo `08194898` |

Los bancos privados de gráficos ocupan **1.120 bytes ES** y **1.140 bytes PT**,
en `09C00000` y `09C80000`. Sus cuatro punteros se redirigen únicamente con
lectura de cuatro bytes y valor original verificado. Ambos bancos permanecen
legibles al cambiar de idioma.

Pasan **78 comprobaciones** de recurso, **15 casos** de compresión, un
decodificador independiente, límites y rechazo de ROM incompatible. En los
ocho recursos comparados se conservan **3.696 bytes exteriores a las regiones
de letras** y **992 píxeles de bordes**. Las vistas de recurso revisadas
utilizan una paleta de contraste y no sustituyen las capturas nativas.

[Catálogo de rótulos](../localization/graphics.tsv) ·
[Generador](../tools/localization_graphics.py) ·
[Prueba](localization_graphics_test.py) ·
[Resultados](localization-graphics-0.10.15-results.json).

## 4. Lecturas del runtime y selección de idioma

La prueba enlaza la función de producción `localized_read` con el header
generado y realiza **2.164.740 comprobaciones**, tanto en compilación normal
como con **AddressSanitizer y UndefinedBehaviorSanitizer**. LeakSanitizer
se desactiva por las restricciones del entorno de ejecución.

Se comprueba cada byte de los bancos, lecturas de uno/dos/cuatro bytes,
orden little-endian, relleno de final, tres espejos ROM, límites, punteros
ordenados y únicos, selección de los tres idiomas y cambios de selección
entre lecturas. También se verifican valores originales alterados, lecturas
parciales de punteros, direcciones ajenas a ROM, anchos inválidos y salida
nula. Una lectura rechazada conserva el valor de salida del llamador.

La prueba anterior de localización también pasa con **287 posiciones por
tres idiomas**. La ROM y sus datos originales no se escriben; estas rutas
solo sirven datos privados o resuelven los punteros verificados.

[Runtime](../src/localization.cpp) ·
[Prueba ampliada](localization_resources_test.cpp) ·
[Resultados](localization-runtime-0.10.15-results.json).

## 5. Ejecución nativa de las pantallas


Se completaron **ocho sesiones nativas por TCP**: referencia 0.10.14 en
inglés y candidato 0.10.15 en inglés, español y portugués, con dos recorridos
por variante. Todas terminaron **FULLY_STATIC**, con cero misses,
instrucciones interpretadas, bloques sanados y fallos. Las fuentes de
producción permanecieron congeladas durante estas ejecuciones.

| Recorrido | Entrada | Intervalos solicitados por sesión | Capturas por sesión |
| --- | --- | ---: | ---: |
| Archivos → copiar → volver → borrar → volver | Arranque nuevo, partida privada vacía | 1.302 | 5 |
| Juego → inventario → juego | GBAS privado verificado, Select y B | 274 | 3 |

Son **6.304 intervalos de frame** y **32 capturas privadas** en total.
Los menús de copiar y borrar muestran el estado de partida vacía; esta
prueba acredita navegación, presentación y retorno, sin copiar ni borrar
una partida poblada. Cada ejecución escribe únicamente en una ruta nueva
de guardado privado. El inventario restaura el contenido del GBAS en esa
sesión aislada, espera 92 intervalos antes de abrirse y conserva el archivo
de origen sin cambios. Su SHA-256 es:

```text
f810aff15a8bda2b1187bcc8ca3ceac71793b0c0ec8e1c7b682d5421b3b823d4
```

Este punto procede de las comprobaciones acotadas anteriores. No se
presenta como un recorrido natural desde el principio hasta ese nivel.

**Regresión inglesa:** los ocho pares de checkpoints coinciden en la
imagen completa de 240 × 160 y en todas las secciones serializadas:
`CPU0`, `BUS0`, `IO_0`, `AUD0`, `SAV0`, **`PPU0`** y `META`. Se comprueban
también identidad y versión del contenedor. La única exclusión son los
ocho bytes de segundos del reloj de host que el motor guarda con el RTC
inactivo; no se excluye la PPU ni se compara únicamente el centro de la imagen.
El binario de referencia reproduce el SHA-256 del runner 0.10.14 publicado;
ambos hashes están incluidos en el reporte.

**Recursos realmente usados:** en los veinte checkpoints de archivos se
verifican los **3.584 bytes descomprimidos en VRAM `0600E000`**, los 45 tiles
del encabezado activo y su índice de paleta. Esto confirma tanto la lectura
del nuevo recurso como su descompresión por el código nativo y la selección
de la pantalla correspondiente.

En cada una de las cuatro capturas de inventario se contrastan **68 tiles
de rótulos** contra los caracteres y la fuente esperados, más **76 tiles
intactos**, incluidos los campos dinámicos de mundo, nivel y puntuaciones.
Los **1.792 bytes** del tilemap de EWRAM `02015400` coinciden con su copia
en VRAM `0600D000`. Las paletas completas, de **1.024 bytes**, coinciden
entre idiomas tanto en los menús de archivos como en el inventario.

En la captura del inventario, español cambia **1.132 píxeles** y portugués
**1.052**, todos dentro de las celdas previstas para las letras. Los
**34.048 píxeles exteriores** a esas celdas son idénticos al inglés en
ambos casos, incluidos números, iconos y campos de puntuación. Las capturas
nativas de los seis rótulos se revisaron visualmente: son legibles y no
recortan letras ni desplazan cifras. No se exige igualdad completa de CPU
entre idiomas, porque los textos traducidos pueden ejecutar cantidades
distintas de trabajo del renderer.

[Validador y cintas de botones](validate_localization_gameplay.py) ·
[Resultados numéricos y hashes](localization-gameplay-0.10.15-results.json).

Para repetir las ocho sesiones y sus comparaciones, con ambos runners
preparados y el GBAS privado de la identidad indicada:

```bash
python3 validation/validate_localization_gameplay.py suite \
  --reference /ruta/runner-0.10.14 --candidate /ruta/runner-0.10.15 \
  --rom /ruta/sma3-usa.gba --bios /ruta/bios.bin \
  --state /ruta/checkpoint-inventario.state --disasm /ruta/sma3-disasm \
  --output /ruta/privada/nueva-suite --report /ruta/resultados-nuevos.json
```

`--output` debe estar fuera del repositorio y ser nuevo; guarda imágenes,
estados y saves privados. El reporte contiene únicamente metadatos,
contadores y hashes. El subcomando `qualify` repite las comparaciones sobre
una suite ya capturada, sin ejecutar de nuevo el juego. Esta comprobación
no acredita todos los diálogos, una campaña completa, rendimiento ni un
teléfono Android físico.


## 6. Renderer nativo de créditos


La prueba focal invoca `CreditsText_ProcessMsg` (`08033E94`) mediante el
dispatch estático del archivo original recompilado, con descriptores y RAM
controlados. Enlaza `localized_read` y el header de recursos de esta versión.
**Pasan 186 ejecuciones nativas con `GBARECOMP_STRICT_STATIC=1`, sin misses,
instrucciones de fallback ni código sanado.** El hook de entrada confirma
que se ejecutaron las funciones generadas en cada caso.

| Comprobación nativa | Casos aceptados |
| --- | ---: |
| Paquetes ingleses, píxeles iguales con fuente original y copia ampliada | 39 pares |
| Paquetes traducidos, registros/flags/RAM completa/ciclos iguales a la referencia | 40 |
| Paquetes ya resueltos, mismo resultado al cambiar la selección a inglés | 40 |
| Siete acentos, ambas paridades de X y ambas filas de créditos | 28 |

Los 28 casos de acentos contrastan el búfer real con una composición
independiente del formato de tiles y los bits de paleta. Se observan los
dos literales verificados y, en los cuarenta paquetes traducidos,
**8.220 lecturas de fuente y 685 de anchos** en el banco `09D`. Los canarios
de RAM fuera del búfer, descriptor y espacio de pila permitido se conservan
en todas las ejecuciones. Las 39 comparaciones inglesas comprueban los
píxeles; la copia de fuente puede cambiar punteros temporales del procesador.

La referencia se ejecuta **por separado**, exclusivamente dentro de esta
prueba: **68 ejecuciones offline interpretadas y 1.297.059 instrucciones**.
Esas ejecuciones no se cuentan como aceptación nativa. El reporte de cobertura
se captura al terminar una ejecución nativa; el test verifica los contadores
de misses y fallback después de cada caso. Resultado: **38.737
comprobaciones**, con el header privado SHA-256 indicado en la sección 2.

Esto acredita la función original y sus lecturas de los recursos nuevos con
entradas controladas. **No acredita un recorrido natural hasta el final,
frames de PPU, GPU ni ejecución en Android.** La referencia interpretada no
forma parte del ejecutable Android ni cambia su ruta de ejecución.

[Harness C++](localization_credits_native_test.cpp) ·
[Resultados y hashes](localization-credits-native-0.10.15-results.json).

Para reproducirlo, generar los recursos privados y preparar la compilación
host de esta versión, incluido el target existente `sma3_credits_test`.
El siguiente bloque reutiliza las opciones y bibliotecas de ese target
sin modificar CMake ni el código generado. Ejecutarlo desde la raíz del
repositorio, sustituyendo la ruta del build host:

```bash
python3 - /ruta/build-host <<'PY'
from pathlib import Path
import shlex, subprocess, sys
repo = Path.cwd()
build = Path(sys.argv[1]).resolve()
target = build / 'CMakeFiles/sma3_credits_test.dir'
flags = dict(line.split(' = ', 1) for line in
             (target / 'flags.make').read_text().splitlines() if ' = ' in line)
options = sum((shlex.split(flags[key]) for key in
               ['CXX_DEFINES', 'CXX_INCLUDES', 'CXX_FLAGS']), [])
objects = []
for source in ['validation/localization_credits_native_test.cpp',
               'src/localization.cpp', 'src/widescreen_bounds.cpp']:
    obj = build / ('qualification_' + Path(source).stem + '.o')
    subprocess.run(['c++', *options, '-I' + str(repo / 'src'), '-c',
                    str(repo / source), '-o', str(obj)], cwd=build, check=True)
    objects.append(str(obj))
link = shlex.split((target / 'link.txt').read_text())
libraries = link[link.index('-Wl,--start-group'):]
subprocess.run(['c++', *objects, '-o', str(build / 'sma3_localization_credits_native_test'),
                *libraries], cwd=build, check=True)
PY
GBARECOMP_STRICT_STATIC=1 /ruta/build-host/sma3_localization_credits_native_test \
  /ruta/sma3-usa.gba /ruta/credits-native-coverage.json
```


## 7. APK y procedencia de compilación

El APK **`sma3-recomp-0.10.15-beta-arm64.apk`** tiene **28.022.040 bytes**,
versión **0.10.15-beta**, código **26**, payload **0.10.15** y paquete
`org.sma3.recomp.experimental`. Requiere Android 9 o posterior y ARM64.

| Identidad verificada | SHA-256 |
| --- | --- |
| APK final | `46fadaed4004cfea641b3ee10783cc884da134c170c504a9070cc9112e6ddc41` |
| Certificado conservado | `2390f57b35d436ff37b2a303189d18d463c8f5114a5ff809b2f2e6df60f58da3` |

La firma v2, el contenedor, la alineación, las tres bibliotecas AArch64,
los tres DEX y las 21 entradas JNI pasan la verificación. El paquete contiene
los dos assets previstos y no incluye archivos ROM, BIOS, claves ni partidas.
La recomprensión final conserva los 17 contenidos de Gradle byte a byte.

Se recompiló el archivo estático del juego: **66 objetos AArch64 y 49.507
funciones declaradas/definidas, ninguna ausente**. Las **1.775 entradas de
producción** coinciden antes y después de las compilaciones host y Android;
el informe registra 307 hashes seleccionados. El runner de referencia
reproduce exactamente el hash publicado para 0.10.14.

Se usaron JDK 17.0.21+9, Gradle 8.9, AGP 8.7.3, SDK 35, NDK
27.1.12297006 y CMake 3.22.1. Gradle utiliza su variante `debug` con el
certificado de esta línea de entregas; el código nativo se compila en
**Release**, con panorámica activada y diagnósticos de escritorio desactivados.
Las bibliotecas de soporte se compararon con 0.10.14: libc++ es idéntica,
y las secciones ELF de SDL son idénticas salvo el identificador de compilación.

Estas verificaciones acreditan identidad y contenido del instalador. No
sustituyen la instalación, los menús ni la GPU de un teléfono físico. El
APK se entrega por separado para la publicación manual del mantenedor.

[Informe del APK, herramientas y procedencia](android-0.10.15-build.json) ·
[Notas de instalación](../docs/releases/v0.10.15-beta.md).

## 8. Reproducir las comprobaciones

Se necesitan la ROM/BIOS propias de las revisiones verificadas y el
disassembly fijado. Desde la raíz del proyecto:

```bash
python3 tools/generate_localization.py --rom /ruta/sma3-usa.gba --disasm /ruta/sma3-disasm
python3 validation/localization_credits_test.py --rom /ruta/sma3-usa.gba --disasm /ruta/sma3-disasm --legacy-header /ruta/localization_data_0.10.14.h
python3 validation/localization_graphics_test.py --rom /ruta/sma3-usa.gba --disasm /ruta/sma3-disasm
g++ -std=c++20 -O2 -Wall -Wextra -Werror -I src validation/localization_resources_test.cpp src/localization.cpp -o /tmp/sma3-localization-test
/tmp/sma3-localization-test
```

La comparación con el header anterior requiere generarlo desde las fuentes
0.10.14; no se toma un header nuevo como referencia de compatibilidad.
Los archivos privados derivados de ROM, capturas, estados y recursos de
fuentes no se incluyen en el repositorio. Se publican los generadores,
catálogos propios, pruebas y resúmenes de resultados.

## 9. Límites

La cobertura sigue siendo parcial: otros rótulos, botones ilustrados,
estrellas/monedas/flores, el menú de suspensión y Mario Bros. permanecen
pendientes. El trabajo no añade idiomas nuevos ni certifica una traducción
oficial. La revisión visual de recursos no equivale a recorrer todos los
diálogos, niveles y transiciones.

La instalación, la navegación Android y el cambio de idioma en un teléfono
físico todavía requieren comprobación. No se hacen afirmaciones nuevas de
FPS, temperatura o batería. El APK conserva la publicación manual a cargo
del mantenedor en la release `Descarga`.
