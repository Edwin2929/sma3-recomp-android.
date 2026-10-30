# Fantasma del récord — validación de 0.10.17 beta

Base publicada: **0.10.16**, commit
`ba2d2193fa36c5eba1e468b85068ee8ab721a36b`.
Motor GBARecomp: `e7728148c6829ba526f682876430a0c9022dc6c0`, con los mismos
19 parches y 20 estados reconocidos. Disassembly:
`KarisaAdvynia/sma3-disasm`, commit `c8532ec9a8d0038c3bfeb003dd0c7ea89d7e1071`.
ROM USA admitida: SHA-1 `7352d2bd064d9ebaec579e264228aa21c7345b80`.

Esta entrega implementa únicamente el punto 11: **fantasma de tu mejor
recorrido**. No modifica el comportamiento de arranque o recuperación de
sesión. Las pruebas con observaciones controladas, la ejecución nativa y
la interfaz Android en host tienen alcances distintos; ninguna acredita
instalación o rendimiento sostenido en un teléfono físico.

## 1. Reloj, elegibilidad y grabación

El servicio consume la instantánea de 74 valores del contrarreloj existente.
No cambia su reconocimiento de entradas, metas, muertes, pausas o estados
restaurados. El índice de cada muestra es el número de intervalos del
contrarreloj: 280.896 ciclos a 16.777.216 Hz. Las presentaciones adicionales
de 120 y las publicaciones repetidas del mismo frame no añaden muestras.

Se requiere una entrada completa desde el principio, un contador en cero y
un identificador de intento nuevo. Activar a mitad de un nivel no permite
guardar un sufijo como recorrido completo. Los intervalos de pausa no
adelantan la grabación; la muestra del extremo actual se reemplaza al
reanudar. Una finalización válida contiene exactamente `duration` muestras,
con índices `[0, duration)`.

Java sigue siendo la fuente de las mejores marcas persistidas. Su trabajador
publica 60 tiempos validados y monótonos mediante `syncTimeTrialGhost`.
Solo un fantasma cuyo tiempo coincide exactamente con la marca correspondiente
es elegible. Un archivo de grabación no crea ni corrige una marca por sí solo.
Una marca antigua sin movimientos necesita una nueva carrera que la iguale
o mejore. Las marcas más largas que el límite de grabación siguen siendo
tiempos válidos, pero no pueden generar un fantasma de esa duración.

Morir, salir, desactivar, restaurar un estado o romper la continuidad del
reloj cancela la captura parcial y retira la imagen de reproducción. Las
grabaciones completas se conservan. La selección del récord queda fijada al
empezar cada intento; una carga tardía compatible puede incorporarse, pero
no se cambia de referencia a un nuevo récord en mitad de la carrera.

[Servicio](../src/time_trial_ghost.cpp) ·
[Contrato](../src/time_trial_ghost.h) ·
[Captura y formato](../src/time_trial_ghost_detail.h).

## 2. Almacenamiento y límites

Los recorridos se guardan por nivel en `time-trials/ghosts-v1`, separados
del archivo Java de marcas y de los datos del juego. El formato little endian
incluye una cabecera de 64 bytes con esquema, identidad ROM, escala del reloj,
curso, duración y cantidades; CRC32 de cabecera y cuerpo; un atlas de imágenes
4 bpp con paleta RGB555; y poses de 12 bytes con posición, habitación e imagen.

Se comprueban longitudes, curso, esquema, identidad, rangos, referencias al
atlas, píxeles transparentes y ambas sumas antes de reproducir. Los límites
son 72.000 intervalos, 512 imágenes distintas y archivos de hasta 4 MiB. Una
captura que alcanza su límite se cancela sin invalidar el cronómetro ni una
marca completa. Las imágenes se deduplican con comparación exacta tras hash.

Un trabajador realiza la lectura, prepara los buffers y escribe un temporal
en el mismo directorio. Sincroniza el archivo, lo renombra, sincroniza el
directorio y verifica los bytes escritos. Los archivos existentes inválidos
o de versión desconocida se conservan. Los fallos de escritura mantienen
trabajo pendiente para reintento, con una cola acotada; la interfaz distingue
preparación, guardado, lectura fallida y escritura pendiente.

El callback del juego no hace operaciones de disco ni reserva un vector por
fotograma. Los buffers se reservan en el trabajador. La liberación de rutas
retiradas se difiere también al trabajador mediante una cola acotada. La
instantánea de 72 valores está protegida por el mismo mutex breve que el
estado del servicio. El cierre se realiza después de que termine `run_game`;
cancela lo parcial y termina el trabajo pendiente de guardado. Una nueva
entrada a SDL en el mismo proceso puede reiniciar el trabajador.

[Operaciones de archivo](../src/time_trial_ghost_storage.h) ·
[Prueba de núcleo y persistencia](time_trial_ghost_test.cpp).

El núcleo y codec pasaron **1.043 comprobaciones** tanto con optimización
`-O2 -Wall -Wextra -Werror` como con ASan/UBSan. Se probaron corrupción de
cada byte, truncamientos, versiones futuras, límites, empate/mejora, fallo
real de escritura y reintento, continuidad, cancelación y reapertura en el
mismo proceso. LeakSanitizer estaba desactivado. La cola cuenta el trabajo
en escritura y el pendiente bajo el mismo mutex, con máximo de dos, de modo
que cada escritura en curso conserva un hueco para volver a la cola si falla.

[Resultados del núcleo](time-trial-ghost-0.10.17-results.json).

## 3. Captura y dibujo

La imagen de Yoshi procede de los OBJ, tiles y paleta reales del juego. La
captura se limita a su forma normal; Baby Mario y los huevos no forman parte
del atlas. Conserva el color aplicado durante la grabación y se presenta con
alfa 112/255. La capa se dibuja después de la imagen y sus filtros, antes de
los controles; no altera el framebuffer fiel, la memoria ni las entradas del
juego.

La muestra y la cámara usan el contexto del fotograma anterior, coincidente
con la imagen fijada en VBlank que presenta SDL. El latch solo avanza ante
un frame real nuevo. Se comprueba el recorte en Original, Estirar, Panorámica,
los márgenes cerrados por la política panorámica y las franjas cinematográficas.
El límite es de 512 rectángulos por presentación, con regiones repetidas
en caché.

Se oculta en otra habitación, transformaciones, Fuzzy, pausas, transiciones,
escenas incompatibles, ajustes Android y edición táctil. Las pruebas separan
el decoder puro, los datos de una observación nativa de Yoshi normal y el
callback real de overlay con reproducción controlada.

| Comprobación visual | Resultado |
| --- | --- |
| Decoder y geometría, sin ROM | 2.545 comprobaciones, optimizado y ASan/UBSan |
| Decoder con captura nativa privada | 2.556 comprobaciones totales en optimizado |
| Callback de overlay de producción | 38 comprobaciones, optimizado e instrumentado |
| Yoshi normal observado | 26 × 31, 444 texeles opacos y 216 rectángulos |
| Overlay desactivado | Cero llamadas de dibujo |

La ejecución instrumentada del callback excluye `vptr` porque se descartan
las funciones de bus y su RTTI. El decoder instrumentado usa fixtures sin
ROM. LeakSanitizer está desactivado en ambos casos. Las mediciones repetidas
de componentes en host no representan el coste SDL total ni los FPS de Android.

[Informe visual detallado y comandos](GHOST_VISUAL_0.10.17.md) ·
[Resultados visuales](ghost-visual-checks-0.10.17.json).

## 4. Interfaz y sincronización de marcas

La página añade **Fantasma de mi récord**, desactivado inicialmente. La
preferencia es independiente de Mostrar cronómetro y requiere el contrarreloj
para grabar o reproducir. Los estados de guardado y la disponibilidad por
nivel se muestran en español, inglés y portugués. La tabla no se reconstruye
en cada consulta si sus marcas y fantasmas no han cambiado.

El modelo Java valida tamaño, esquema, flags, enum de almacenamiento y motivo,
identificadores de curso y límites antes de presentar los datos. La sincronización
solo se intenta después de leer correctamente el archivo de marcas. Se copia
el array de tiempos antes de pasarlo al servicio; un error del fantasma no
reemplaza ni borra los récords.

La prueba Android final pasó **115 comprobaciones del modelo de contrarreloj
y 13 pruebas JUnit** de interfaz y almacenamiento. Incluye preferencia inicial,
activación independiente, marcas antiguas, mezcla de una nueva marca, reintento
tras fallo, idioma, recreación, datos inválidos, límites, errores del fantasma,
aislamiento de arrays y detención de consultas en segundo plano. Se inspeccionaron
las capturas verticales de los tres idiomas y la navegación horizontal, cuyo
botón Volver queda fuera del cuerpo desplazable.

La regresión del generador de reportes pasó **671 comprobaciones de modelo
y ocho pruebas JUnit**. La lista permitida añade únicamente el estado booleano
de la preferencia del fantasma. Los archivos de recorridos no se adjuntan.
Los destinos externos están interceptados: no se enviaron reportes ni se
publicaron issues.

Se utilizan Robolectric 4.16.1, JUnit 4.13.2, API 34 y JDK 17, con compilación
contra `android.jar` de API 35. Estas pruebas usan instantáneas JNI controladas;
no ejecutan el juego ARM64 en un teléfono.

[Resultados de la UI](ghost-ui-0.10.17-results.json) ·
[Regresión de reportes](bug-report-0.10.17-results.json) ·
[Pruebas UI](TimeTrialUiTest.java).

## 5. Recorrido completo, guardado y reapertura nativos

La cinta final contiene **123 acciones de botones y 4.805 frames solicitados**.
Se ejecutó íntegra desde Try Again en 0.10.16 y en el candidato 0.10.17 con
grabación activada, con una única carga inicial por proceso y ninguna carga
intermedia. Ambos terminaron exactamente un intento válido de **1-1**, con
meta real y **4.127 intervalos cronometrados**, equivalentes a
**69,09714889526367 segundos**. No hubo otra muerte durante esos intentos;
la pérdida y recuperación de Bebé Mario se produjeron mediante el juego.

Los **48 pares de checkpoints** conservaron imágenes completas de 240 × 160
idénticas y las mismas secciones `CPU0`, `BUS0`, `IO_0`, `AUD0`, `SAV0`,
**`PPU0`** y `META`. Solo se excluyen ocho bytes de reloj de pared del RTC
inactivo en `BUS0`. Las instantáneas completas del contrarreloj también
coincidieron; activar el grabador no cambió el resultado del juego ni su tiempo.

Se escribió `course-0.ghost`, de **107.723 bytes**, con SHA-256
`738b46cf9a0b424945d9b21c7116973c264917d0961e0fe61d4f59434160d631`.
El trabajador confirmó almacenamiento listo, ninguna escritura pendiente
y un fantasma elegible de 4.127 intervalos. El archivo de movimientos permanece
privado y no se incluye en el repositorio ni en el APK.

Una tercera sesión abrió el archivo en un proceso nuevo con la marca sincronizada.
Entró de nuevo desde el principio y añadió 60 frames neutros para separar
el intento nuevo de la referencia. Tras 634 frames solicitados, el nuevo
intento seguía válido y el fantasma guardado estaba cargado y era elegible.
Cerrar ese recorrido parcial conservó exactamente el archivo anterior.

Las **tres sesiones** suman **10.244 frames solicitados**. Todas terminaron
**FULLY_STATIC**, con cero misses, instrucciones interpretadas, bloques
sanados y fallos. El runner candidato tiene SHA-256
`9e5068fe31818e340d9cf508f61e00a8b4ed09f6aa3e353cbbc298084f8d55a9`
y comparte las fuentes nativas congeladas para el APK.

### Procedencia y límites de la prueba

El checkpoint de Try Again procede de la validación anterior. Su origen fue
un fixture de entrada que modificó solo selector de curso `03006288`, entrada
principal `03006A52` y estado de inicialización `03006B05`. Después, botones
normales causaron daño y muerte hasta Try Again. El hash del checkpoint y
los valores iniciales están en `entry_provenance` del informe. El driver de
esta entrega no modifica RAM, posiciones, puntuación o meta.

La exploración de la ruta utilizó checkpoints intermedios para preparar los
botones. Esa exploración no se cuenta como un récord continuo. Una primera
cinta completa terminó antes del aro y fue rechazada correctamente, pese
a conservar la equivalencia entre versiones. Se añadieron los movimientos
finales y se repitió la cinta íntegra que produce los resultados anteriores.

Esta evidencia corresponde a **un recorrido completo desde una reentrada
normal preparada**, no a una campaña desde arranque nuevo ni a la cobertura
de los 60 niveles. La nota de limitación dentro de la procedencia antigua
describe la prueba incompleta de 0.10.16; esta entrega añade la finalización
continua documentada arriba.

Las capturas TCP representan el framebuffer del juego y excluyen el overlay
del fantasma. La visibilidad de la capa requiere la comprobación SDL separada.

[Driver nativo](ghost_native_test.py) ·
[Cinta de botones](ghost-course-1-1.json) ·
[Resultados y procedencia](ghost-native-0.10.17-results.json).

Para repetir la validación con los recursos privados y su procedencia:

```bash
python3 validation/ghost_native_test.py \
  --reference /ruta/runner-0.10.16 \
  --candidate /ruta/runner-0.10.17 \
  --rom /ruta/sma3-usa.gba --bios /ruta/gba_bios.bin \
  --entry-state /ruta/try-again.state \
  --entry-provenance /ruta/entry-provenance.json \
  --private-output /tmp/ghost-native \
  --report /tmp/ghost-native-results.json
```

El directorio de evidencia debe estar vacío y fuera del repositorio. La
cinta se incluye en el port; la ROM, BIOS, SRAM, estados y capturas permanecen
privados.

### Presentación real con el fantasma activado y desactivado

Se ejecutó otra pareja con el mismo runner, checkpoint, botones y una demora
de 30 frames. Esta fase usa la ventana SDL, el replay CSV y **604 presentaciones
por proceso**, sin servidor de stepping. El tap lee el drawable terminado,
incluido el overlay. La captura final corresponde al índice 603 en ambos casos.

Con modo Original, filtro Nearest y escala entera 4×, la salida es **960 × 640**.
El fantasma cambia **6.976 píxeles**, todos dentro de `(460, 184)–(560, 316)`,
una región equivalente a 25 × 33 píxeles del juego. La imagen se inspeccionó
visualmente: Yoshi aparece separado del jugador, con la pose grabada y
transparencia. El framebuffer fiel sigue siendo idéntico, al igual que el
cronómetro de 241 intervalos y los contadores finales. El archivo del récord
permanece byte a byte igual.

Ambos procesos terminaron con código cero, **FULLY_STATIC** y cero misses de
despacho, instrucciones interpretadas, bloques sanados y fallos de ejecución.
`io_unhandled` vale cero. El contador de bus **`unmapped` vale 448** en ambos
casos y se registra expresamente por separado. Una pasada adicional de
referencia con 0.10.16 y el mismo CSV produjo también 448, todos los mismos
contadores, el mismo cronómetro y la misma imagen SDL que 0.10.17 con el
fantasma apagado. Este contador no se presenta como cero.

La pareja SDL compara reloj, framebuffer y contadores; las comparaciones
completas de CPU/BUS/PPU son las de la cinta íntegra anterior. Las comprobaciones
de otras geometrías y recortes son de componentes. El readback altera los
tiempos de presentación, por lo que estas pasadas **no son un benchmark**.
La prueba del tap usa SDL software y vídeo dummy; no equivale a una GPU Android.

[Driver de presentación](ghost_presentation_test.py) ·
[Resultados SDL](ghost-presentation-0.10.17-results.json) ·
[Referencia del contador y la imagen base](ghost-window-reference-0.10.17-results.json).

Para repetir esta fase, primero genera el recorrido y `persisted-bests.txt`
con el driver nativo. Compila el tap de lectura de SDL y proporciona su ruta:

```bash
cc -shared -fPIC -O2 -Wall -Wextra -Werror -I/ruta/SDL2 \
  validation/sdl_ghost_present_capture.c -ldl -o /tmp/ghost-present-tap.so

python3 validation/ghost_presentation_test.py \
  --runner /ruta/runner-0.10.17 \
  --rom /ruta/sma3-usa.gba --bios /ruta/gba_bios.bin \
  --entry-state /ruta/try-again.state \
  --ghost-directory /tmp/ghost-native/ghost-store \
  --bests /tmp/ghost-native/persisted-bests.txt \
  --tap /tmp/ghost-present-tap.so \
  --private-output /tmp/ghost-presentation \
  --report /tmp/ghost-presentation-results.json
```

El driver fija la identidad del runner y del checkpoint usados en esta
calificación. La documentación y los hashes permiten distinguir este binario
de una recompilación con otros inputs o herramientas.

## 6. Arranque conservado

Se compararon contra la base publicada los hashes de `runtime.toml`,
`runtime.cpp`, `runtime.h` y el bootstrap de actividad Android del motor.
Permanecen idénticos. La ruta Android conserva explícitamente
`resume_suspend_state_on_launch = true` y `GBARECOMP_BIOS_SKIP_INTRO = 1`.
La inicialización y el cierre del fantasma rodean la llamada existente al
runtime sin cambiar sus argumentos de arranque.

[Comprobación de arranque](ghost-startup-0.10.17-results.json).

## 7. Compilación y APK

La entrega es **0.10.17-beta**, código **28**, payload **0.10.17**,
paquete `org.sma3.recomp.experimental`, ARM64 y Android 9 o posterior.
Conserva la clave de firma de 0.10.16, con incremento del código. La variante
Android sigue siendo depurable como las betas anteriores; el código nativo
se compila en Release. Las instrumentaciones de escritorio se excluyen.

Se congelaron **1.797 entradas de producción**, de las que **1.666** son
entradas nativas compartidas con el host. Permanecieron idénticas antes y
después de ambas compilaciones. El archivo generado ARM64 se reutiliza tras
comprobar su hash y las 71 dependencias originales, todas sin cambios. El
port se recompila; no se editó código generado ni se añadió un parche al motor.

La primera ejecución Gradle terminó con éxito, pero la comprobación del
artefacto rechazó su salida: el APK y la caché incremental de empaquetado
seguían correspondiendo a 0.10.16, aunque los manifiestos intermedios eran
0.10.17. Se conservaron esos resultados y se regeneraron únicamente la salida
APK y la caché `packageDebug`, sin cambios de producción. La segunda ejecución
offline produjo la versión correcta. El log no demuestra la causa exacta de
la salida antigua. Los avisos sobre archivos temporales durante el strip no
se tradujeron en entradas adicionales del APK final.

El verificador del archivo entregado comprobó firma v2 y certificado,
manifest/versiones, alineación de cuatro bytes, integridad ZIP, tres DEX,
clases Java y las **25 correspondencias JNI** entre DEX y ELF, conservando
las 23 anteriores. Verificó que las tres bibliotecas ARM64 son idénticas a
sus salidas de compilación y que el contenedor no incluye ROM, BIOS, claves,
partidas ni archivos de grabación. SDL y libc++ permanecen byte a byte
iguales a 0.10.16.

El APK final mide **28.124.440 bytes**, SHA-256
`d6ea81becf93fdfc222a46ac2d81a55adfc097e123d486aa63579f023f6b5940`.
El certificado conserva SHA-256
`2390f57b35d436ff37b2a303189d18d463c8f5114a5ff809b2f2e6df60f58da3`.

[Informe completo de compilación](android-0.10.17-build.json) ·
[Notas e instalación](../docs/releases/v0.10.17-beta.md).

## 8. Repetir las pruebas de componentes

Desde la raíz del port:

```bash
c++ -std=c++20 -O2 -Wall -Wextra -Werror -pthread -Isrc \
  validation/time_trial_ghost_test.cpp src/time_trial_ghost.cpp \
  -o /tmp/time-trial-ghost-test
/tmp/time-trial-ghost-test /tmp/ghost-test-output

python3 validation/run_time_trial_ui_test.py \
  --java-home /ruta/jdk-17 \
  --android-jar /ruta/android-sdk/platforms/android-35/android.jar \
  --dependencies-from /ruta/dependencias-robolectric \
  --robolectric-cache /ruta/cache-robolectric \
  --work-dir /tmp/ghost-ui

python3 validation/run_bug_report_ui_test.py \
  --java-home /ruta/jdk-17 \
  --android-jar /ruta/android-sdk/platforms/android-35/android.jar \
  --dependencies-from /ruta/dependencias-robolectric \
  --robolectric-cache /ruta/cache-robolectric \
  --work-dir /tmp/ghost-report-ui
```

El directorio de salida del núcleo debe estar vacío o no existir y quedar
fuera del repositorio. La variante instrumentada sustituye `-O2` por
`-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer` y se ejecuta con
`ASAN_OPTIONS=detect_leaks=0`. Las dependencias Android deben estar preparadas,
incluido el runtime instrumentado de API 34. Los comandos del decoder y del
callback están en el [informe visual](GHOST_VISUAL_0.10.17.md).
