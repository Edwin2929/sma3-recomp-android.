# Panorámica real: prototipo, aún sin APK

Objetivo: sustituir el estiramiento por una vista que muestre más escenario,
mantenga las proporciones y se adapte a la pantalla del teléfono.

La versión Android entregada al mantenedor es 0.9.1, con la corrección del
escalado 3:2 y la ayuda del menú actualizada. El APK publicado en la release
«Descarga» sigue siendo 0.9 mientras se prepara la subida manual de 0.9.1.
La panorámica real **no está activada en ninguna de estas versiones** y
no demuestra todavía pantalla completa correcta en todas las escenas ni
rendimiento a 120 FPS.

## Qué pueden hacer los APK 0.9 y 0.9.1

Para conservar las proporciones, abre **Atrás → Imagen → Estirar a pantalla
completa** y desactiva la casilla. El fotograma original de **240 × 160** tiene
proporción **3:2**; se centra y deja bandas cuando la superficie tiene otra
proporción. Activar Estirar llena el área de juego con ese mismo fotograma,
puede deformarlo y **no muestra más escenario**.

La panorámica real descrita aquí necesita dibujar contenido adicional fuera
de ese fotograma. No se activa con Estirar ni al elegir 720p o 1080p.
La aclaración de la ayuda del menú está incluida en el APK 0.9.1 y
**no integra este prototipo en Android**. La confirmación de funcionamiento
de 0.9.1 en un Google Pixel 7 Pro corresponde al APK con el campo de visión
original, no a una compilación panorámica.
Más detalles en la [guía de calidad de imagen](CALIDAD_IMAGEN.md).

## Implementado

- Reconstrucción del terreno desde el mapa completo del nivel, fuera del anillo
  parcial de VRAM. Incluye las cuatro piezas de cada bloque y las máscaras de
  la capa frontal.
- Lectura de los fondos completos, con sus desplazamientos y volteos.
- Lecturas acotadas sin escribir RAM ni modificar CPU, temporización o estado
  del bus. El prototipo admite hasta 356 × 160, frente a 240 × 160 originales.
- Captura de diagnóstico por fotograma y pruebas independientes del decodificador.
- Opción CMake `SMA3_WIDESCREEN_DIAGNOSTICS`, desactivada por defecto y exclusiva
  de escritorio. Permanece desactivada en las compilaciones Android.

## Comprobaciones y límites de la evidencia

Durante la investigación previa, una prueba de 9.600 fotogramas terminó con
`dispatch_misses=0 interpreted_insns=0 healed_native=0`, `unmapped=0`.
La imagen ampliada mostró terreno adicional y su centro de 240 × 160 coincidió
píxel a píxel con el control. La reconstrucción coincidió con 600 muestras por
capa en los fotogramas 7200, 8000 y 9599 del recorrido del tutorial.

La comparación nueva de 18.000 fotogramas enfrenta un control de 240 × 160
con el prototipo de 356 × 160 usando la misma partida nueva y las mismas entradas.
El informe `validation/widescreen-regression-before-sync.json` conserva el
resultado anterior a la sincronización de objetos: las 30 capturas de memoria
coinciden byte a byte y el centro de la imagen final coincide píxel a píxel.
Los dos recorridos registran exactamente los mismos 448 accesos no mapeados,
con las mismas direcciones y valores. Esto demuestra que esos avisos no fueron
introducidos por el dibujo ampliado en este recorrido; no demuestra que sean
inofensivos ni valida el juego completo.

El resolvedor de objetos conserva los índices originales durante la compactación
de OAM. Solo extiende objetos cuyos atributos y coordenadas completas coinciden;
los demás conservan el recorte original. La lectura directa al comienzo del
fotograma resultó insuficiente, porque la lista temporal puede estar preparando
el siguiente. El observador de `080004A0` captura las posiciones antes de
compactar, sin modificar registros ni memoria del juego. El consumidor vuelve
a contrastar los atributos con OAM y descarta los datos al cambiar de época de
estado. No sustituye otros observadores instalados por el motor.

Las pruebas independientes cubren compactación con huecos, posiciones negativas,
la ambigüedad +272/−240, datos discordantes, reinicio y capacidad de 128 objetos.
Se ejecutan con AddressSanitizer y UndefinedBehaviorSanitizer. LeakSanitizer se
desactiva por las limitaciones de inspección de procesos del entorno.

## Pendiente antes de entregar el APK

1. Completar la cobertura de posiciones de objetos en todas las escenas.
   La resolución por metadatos es conservadora: cualquier objeto no verificado
   permanece limitado a la vista original.
2. Ampliar y comprobar los límites de dibujo y aparición. No basta con ampliar
   el fondo: enemigos y coleccionables deben aparecer correctamente.
3. Probar cambios de nivel, cuadros de texto, transformaciones, jefes y modos
   gráficos especiales. El prototipo conserva bordes en escenas no admitidas.
4. Adaptar el ancho a la superficie de Android, integrar el ajuste de imagen y
   medir la presentación con 120 Hz en el Pixel 7 Pro.

## Puntos de investigación para continuar

SMA3 USA, SHA-1 `7352d2bd064d9ebaec579e264228aa21c7345b80`:

| Dato o rutina | Dirección | Uso |
| --- | --- | --- |
| Puntero del mapa del nivel | `03007010` → `0200000C` | 0x200 bytes por pantalla |
| Índices de pantalla | `0201B800` | Índice YX; filtrar bits bajos 0–5 |
| Punteros de bloques de terreno | `081BAD20` | Índice por byte alto del bloque |
| Máscaras de capa frontal | `081BC444` | Bits 3–0, una pieza de 8 × 8 por bit |
| Mapas completos de fondos | `0201BC00`, `0201DC00` | Bloques de 16 × 16 |
| Punteros de fondos comprimidos | `081675E4`, `0816766C` | Tamaño descomprimido en cabecera |
| Cámara / copias del fotograma | `030069D4–69F3` | Desplazamientos X e Y |
| Estado / modo de juego | `03006B05`, `03006D64` | Juego normal: estado 0D, modo 00 |
| Filtro de dibujo de objetos pequeños | `0804CE30`, `0804CE36` | X+16 comparado con 255 |
| Búsqueda de objetos para aparecer | `08000868–08000914` | Escaneo de filas y columnas |
| Límites de aparición | `0817209E`; lector `0804E710` | +288 / −48 originales |

`0202C8B0` conserva coordenadas completas por índice de la lista temporal
`03005A00`, no por índice final de OAM. `080004A0` compacta 256 posiciones,
omitiendo Y=160, hacia `0201A800`. La captura debe realizarse antes de esa
compactación; leer la lista al comenzar el fotograma no garantiza sincronía.
El límite de salida del resolvedor es 128 objetos. No amplía aún la aparición
ni los límites de dibujo que aplica el propio juego.

## Reproducir

Se requieren los archivos generados privados y los activos propios compatibles,
además del motor fijado por el proyecto. No se suben ROM, BIOS, capturas de RAM,
partidas, claves de firma ni archivos generados del juego.

```sh
cmake -S . -B build -DSMA3_WIDESCREEN_DIAGNOSTICS=ON
cmake --build build --target sma3_runner -j2
python3 validation/run_widescreen_probe.py --rom /ruta/juego.gba --bios /ruta/bios.bin --output /ruta/prueba-nueva
# Para la comparación larga: usar --frames 18000 en ambos recorridos.
# Control: --generic --width 240; candidato: --width 356.
# Los avisos heredados hacen terminar el ejecutor con código 1; revisar el informe.
python3 validation/compare_widescreen_runs.py /ruta/control /ruta/candidato --report /ruta/informe.json
g++ -std=c++20 -Wall -Wextra -Werror -fsanitize=address,undefined -g validation/widescreen_tiles_test.cpp -o /tmp/widescreen_tiles_test
ASAN_OPTIONS=detect_leaks=0 /tmp/widescreen_tiles_test
```

Referencia de ingeniería: [sma3-disasm](https://github.com/KarisaAdvynia/sma3-disasm),
especialmente `LevelCode.asm`, `CodeStart.asm` y `SpriteShared08049E80.asm`.

El parche `gbarecomp-widescreen-objects.patch` añade el recorte por objeto al motor.
Sin el diagnóstico activado, su callback permanece nulo y no habilita panorámica
en Android. El aplicador verifica la revisión del motor y rechaza cambios locales
que no correspondan a una etapa reconocida de los parches.

## Resultado del observador sincronizado

`validation/widescreen-regression.json` registra otra comparación completa de
18.000 fotogramas con el observador activo: 30 capturas idénticas al control,
centro final idéntico y los mismos 448 avisos heredados. Los contadores de fallos
de despacho, instrucciones interpretadas y reparación dinámica permanecen en cero.
Objetos resueltos en los fotogramas 7200, 8000, 9599, 12000, 16000 y 17999:
6, 6, 1, 3, 3 y 3 respectivamente. **La cobertura sigue siendo parcial.**
Esto valida la ausencia de cambios observados en ese recorrido, no todos los
objetos ni todas las transiciones. No se ha generado un APK panorámico.

## Selección coherente de OAM

Ahora se compara la lista completa de atributos con OAM antes de aceptar sus
coordenadas. Se conservan hasta tres envíos recientes, se elige un único envío
completo y se vacía el historial al cambiar la época del estado. No se mezclan
coincidencias de objetos de distintos envíos. Los atributos de objetos sin
coordenadas verificadas también intervienen en la comprobación; esos objetos
siguen recortados a la vista original.

`validation/widescreen-coherent-regression.json` registra 18.000 fotogramas:
30 capturas idénticas al control, centro final idéntico, despacho nativo sin
fallos y los mismos 448 accesos no mapeados heredados. En las seis muestras se
eligió el envío más reciente; este recorrido no demuestra que los otros dos
sean necesarios. La prueba independiente comprueba además el rechazo de una
lista con un atributo discordante.

| Fotograma | Objetos OAM activos | Posiciones verificadas |
| --- | ---: | ---: |
| 7200 | 23 | 6 |
| 8000 | 17 | 6 |
| 9599 | 16 | 1 |
| 12000 | 10 | 3 |
| 16000 | 10 | 3 |
| 17999 | 10 | 3 |

Una prueba adicional observando X/Y en `080007A8` antes de dibujar produjo la
misma cobertura en las seis muestras. Se descartó ese añadido por no resolver
los objetos restantes. Los activos incluyen componentes de personajes y HUD;
no todos deben extenderse a los márgenes. Queda identificar sus rutinas de
dibujo y ampliar los límites de los objetos pequeños y de aparición, después
validar transiciones y Android. La panorámica todavía no está lista para un APK.

## Límite experimental de dibujo de objetos pequeños

Se añadió un parche optativo de las constantes de `0804CE30` y `0804CE36`.
El intervalo original X=[−16,239] se conserva por defecto. Con una vista de
356 píxeles y 58 adicionales por lado, el diagnóstico usa X=[−74,297]. Las
operaciones que calculan las banderas de CPU reciben los mismos operandos
que las operaciones aritméticas modificadas. No se cambia el límite vertical.

El script `tools/patch_widescreen_bounds.py` comprueba el SHA-256 del fragmento
generado original, admite repetir su aplicación y rechaza otras modificaciones.
Los archivos generados siguen siendo privados. El parche requiere reconstruir
el archivo de código generado; activar solo la variable de entorno no modifica
un archivo precompilado anterior. El ejecutor de diagnóstico acepta
`--object-bounds` y `--replay`, y el reconstructor acepta `--object-bounds`.

```sh
python3 tools/rebuild_host.py --object-bounds --cmake /ruta/cmake
python3 validation/run_widescreen_probe.py --rom /ruta/juego.gba --bios /ruta/bios.bin --output /ruta/nueva --frames 18000 --width 356 --object-bounds
```

El código sigue desactivado en Android. `widescreen-bounds-default-regression.json`
comprueba que los valores por defecto conservan el resultado del control anterior.
`widescreen-bounds-expanded-regression.json` compara el ancho original con el
ampliado: 30 capturas y centro final idénticos en el recorrido original. La prueba
instrumentada posterior detectó cero entradas al filtro en ese recorrido: por
ello estos resultados **no demuestran todavía el dibujo de objetos adicionales**.
Se añadió `panorama-long-jumps.csv` para investigar un recorrido distinto; no se
considera una prueba de cobertura solo por terminar sin errores.

Resultado del segundo recorrido: `widescreen-long-jumps-regression.json` pasa
la comparación de 18.000 fotogramas (30 capturas y centro final idénticos).
Registra 1.568 accesos no mapeados en ambos lados, con las mismas direcciones y
valores, frente a 448 del primer recorrido. No son fallos nuevos del parche,
pero siguen pendientes de explicación. `widescreen-bounds-coverage.json`
confirma cero llamadas observadas a `0804CE1C` en ambos recorridos: el parche
está preparado y compilado, pero su efecto de ampliar objetos no está validado.
No se debe activar en una entrega Android basándose en estas pruebas.

## Pruebas cortas desde estados guardados

`validation/run_panorama_session.py` inicia su propio servidor de diagnóstico
local, carga un estado privado y aplica una lista acotada de entradas. Valida
los hashes de ROM/BIOS, permite como máximo 6.000 fotogramas por sesión y guarda
una captura y posición de Yoshi por acción, además del estado final. Cierra su
proceso al terminar o fallar. Los archivos de salida deben permanecer privados.
Requiere Python y Pillow, además del ejecutable Linux compilado.

```sh
python3 validation/run_panorama_session.py --rom /ruta/juego.gba --bios /ruta/bios.bin --state /ruta/estado.state --actions validation/panorama-hill-route.json --output /ruta/nueva
```

El estado privado inicial se obtuvo mediante las entradas originales hasta el
fotograma 6000, durante el mensaje inicial. Las sesiones cortas verificaron que
A produce el salto y que retroceder permite tomar altura en la pendiente.
`panorama-session-progress.json` conserva sus resultados: se alcanzó X≈860,
superando el tramo previo, y después X≈993, donde falta resolver otro desnivel.
`panorama-hill-route.json` reúne las acciones realizadas hasta X≈860; incluye
intentos fallidos conservados para investigación. Las sesiones se ejecutaron
por segmentos con restauraciones entre ellos; la lista concatenada todavía no
ha sido reproducida de una sola vez. No demuestra un recorrido completo, la
activación del filtro de objetos pequeños ni funcionamiento en Android.

## Posiciones durante animaciones — 7 de octubre de 2026

El observador conserva ahora el índice de origen de cada objeto y registra
las coordenadas completas de los componentes de Yoshi, sus dos emisores por
franjas, el emisor genérico y los ayudantes de transformación afín. Reconstruye
las posiciones a partir de los parámetros y las tablas de animación; no decide
el signo de X a partir de los nueve bits de OAM. Las observaciones se descartan
tras cada compactación y al restaurar un estado.

Se corrigió además `080D8CB4`: después de la transformación afín, esa rutina
ajusta Y según la escala. La predicción anterior dejaba de coincidir con el
objeto real. El observador reproduce ese ajuste vertical y conserva la X
verificada. Antes de admitir cualquier posición, exige que los tres atributos
completos coincidan con la lista terminada y, después, con el fotograma mostrado.
Los objetos no reconocidos conservan el recorte original.

Rutinas cubiertas por los observadores:

| Rutina | Observación |
| --- | --- |
| `08042D28` | Componentes de la animación de Yoshi y orientación |
| `08041CBC` | Matriz afín y desplazamiento de doble tamaño de Yoshi |
| `0804211C`, `080421A8` | Emisión de franjas de la animación |
| `080007A8` | Coordenadas completas del emisor genérico |
| `0804CAB8`, `0804CB64` | Objeto afín a partir de su ancla de pantalla |
| `080D8CB4` | Ajuste vertical posterior según la escala |

Pruebas independientes con ASan y UBSan: coordenadas negativas, ambigüedad
+272/−240, límites de slots, volteo, matrices, doble tamaño, ajuste vertical y
rechazo de atributos que no coinciden. LeakSanitizer permanece desactivado por
la limitación del entorno indicada arriba. Resultado en
`validation/widescreen-provenance-test.txt`.

La comparación final de 18.000 fotogramas conserva las 30 capturas de memoria,
el centro de 240 × 160 píxel a píxel y los mismos 448 avisos de acceso no mapeado
del control. Los contadores de fallos de despacho, instrucciones interpretadas
y recuperación nativa permanecen en cero. Informe:
`validation/widescreen-provenance-regression.json`.

En los fotogramas 12000, 16000 y 17999 se verifican los 10 objetos activos,
frente a 9 antes del ajuste vertical y 3 antes de observar las piezas de Yoshi.
Las primeras tres muestras mantienen objetos sin resolver; entre ellos hay
componentes del HUD y del mensaje inicial. El desglose sin datos del juego está
en `validation/widescreen-provenance-coverage.json`.

**El paso 1 sigue pendiente de cobertura completa.** Resolver todos los objetos
de estas tres capturas no demuestra todas las escenas, enemigos o efectos.
Tampoco demuestra que el juego genere objetos fuera de sus límites originales;
esa ampliación sigue siendo el paso 2. No se ha generado un APK panorámico.

## Cobertura continua: interfaz, lengua y Toadies

Se añadieron observadores específicos para distinguir el HUD, los indicadores
limitados a la pantalla y los componentes de los mensajes. Esta clasificación
requiere observar su rutina productora y comprobar los tres atributos finales;
no depende únicamente del número de slot. Estos elementos conservan el recorte
nativo, incluso si un metadato antiguo coincide accidentalmente con ellos.

La revisión de **cada fotograma de juego normal admitido** detectó casos que
no aparecían en las seis capturas anteriores. Se corrigieron:

- El indicador para avanzar el mensaje (`080E9124`).
- La punta horizontal y vertical de la lengua (`08042380`, `0804244C`).
- Las piezas de los Toadies que se llevan a Baby Mario: posición base, traslación
  de cada integrante, piezas compartidas y cambios de orientación (`0804F44A`,
  `0804F51C`, `0804F5AC`).

Los observadores de interfaz cubren además `0802D0CC`, `080DFDC2` y `080E98A0`.
Los tests con ASan/UBSan incluyen reutilización de slots, plantillas truncadas,
punteros fuera de rango, coordenadas negativas y traslaciones de objetos compuestos.
Los resultados están en `widescreen-ui-test.txt` y `widescreen-components-test.txt`.

Dos recorridos de 18.000 fotogramas aprobaron la comparación contra sus respectivos
controles de 240 × 160: 30 capturas de memoria idénticas por recorrido y centro
final idéntico píxel a píxel. Mantienen respectivamente 448 y 1.568 avisos de
accesos no mapeados del control, sin fallos nuevos de despacho ni ejecución
interpretada. Informes: `widescreen-components-regression.json` y
`widescreen-components-jumps-regression.json`.

| Cobertura de posiciones | Recorrido original | Recorrido con saltos |
| --- | ---: | ---: |
| Fotogramas con posiciones sin resolver antes de estas correcciones | 1.604 | 2.471 |
| Fotogramas con posiciones sin resolver después | 44 | 170 |
| Fotogramas con objetos sin resolver que pueden intersectar la pantalla verticalmente | 0 | 0 |
| Fotogramas de juego admitido sin una lista coherente de objetos | 0 | 0 |

Los 44 y 170 casos restantes son objetos fuera del área vertical visible.
**Sus coordenadas X siguen sin resolverse** y mantienen el recorte original.
El diagnóstico de visibilidad respeta el tamaño, el ajuste vertical de OAM y el
doble tamaño afín; no cambia su dibujo ni los declara resueltos.

`validation/report_object_coverage.py` genera el desglose y rechaza registros
antiguos o incompletos. Los informes son `widescreen-visible-coverage.json` y
`widescreen-visible-jumps-coverage.json`. El segundo recorrido atraviesa escenas
no admitidas por el prototipo: las muestras 8000 y 16000 quedan fuera de esta
cobertura y mantienen el comportamiento de bordes existente.

Estos resultados cierran los casos visibles encontrados en ambos recorridos.
**No completan el paso 1 para todo el juego:** quedan otros niveles, efectos y
escenas por cubrir. No amplían por sí mismos los límites de aparición del juego,
no prueban rendimiento en el Pixel 7 Pro y no constituyen un APK panorámico.

## Ciclo completo de coche — 18 de octubre de 2026

El control de 240 píxeles de ancho y el candidato de 356 completaron el mismo
recorrido nativo de **3.506 fotogramas**. La secuencia observada en cada fotograma
fue exactamente **0 → 2 → 0** y terminó con **1.200 fotogramas consecutivos de
Yoshi normal, en juego y sin botones pulsados**. El resultado
`complete_cycle_pass` es `true` en el nuevo
[informe del ciclo de coche](../validation/widescreen-car-cycle.json).

| Comprobación | Resultado |
| --- | --- |
| Fotograma inicial / final | 6.599 / 10.105 |
| Primera transformación en coche | Fotograma 7.535; 936 ejecutados desde el inicio |
| Primera recuperación de Yoshi normal | Fotograma 8.815; 2.216 ejecutados desde el inicio |
| Desplazamiento a la derecha desde la recuperación hasta el final | 58,83984375 píxeles; mínimo exigido 32 |
| Imágenes centrales RGB de 240 × 160 idénticas | 61 pares |
| Comparaciones de estado, incluida la final | 62 pares |
| Fotogramas con huecos de posición, posiciones visibles sin resolver o envíos incoherentes | 0 / 0 / 0 |
| Fallos de despacho, instrucciones interpretadas o reparaciones nativas | 0 / 0 / 0 |

El estado de entrada es sintético e idéntico al utilizado para la prueba
histórica de coche. La transformación se activa mediante entradas del juego y
la burbuja original; no se fuerza el campo de transformación ni se restaura un
estado durante el recorrido. La ruta calibrada conserva las primeras siete
acciones, mueve a la izquierda durante 105 fotogramas, espera como máximo 2.400
hasta detectar el primer fotograma normal, avanza 90 a la derecha y termina
con 1.200 de espera. Su presupuesto máximo es de 5.265 fotogramas.

Las 61 imágenes comparan el centro del candidato panorámico con la imagen
original en puntos de captura; no son una comparación de imágenes de todos
los fotogramas. Las 62 comparaciones de estado cubren CPU, memoria del juego,
E/S, audio, guardado y metadatos, conservando la excepción acotada a ocho bytes
de reloj RTC inactivo. Incluyen 61 puntos de captura y un guardado final en el
mismo fotograma que la última captura. El estado PPU dependiente del área de presentación se
excluye. La traza de transformación, entradas y movimiento sí se comprueba en
cada fotograma.

El resultado del **8 de octubre** permanece intacto en
`validation/widescreen-car.json`: 3.360 fotogramas, 38 imágenes centrales y
secuencia 0 → 2 → 0 → 2. La revisión del **9 de octubre** añadió el contrato de
ciclo completo y superó sus pruebas sintéticas, pero registró la ejecución
nativa como pendiente. La ejecución del 18 de octubre completa esa fase para
la ruta calibrada; no cambia las conclusiones de los informes anteriores.
La calibración y los criterios se detallan en
[CAR_VALIDATION.md](../validation/CAR_VALIDATION.md).

La [evidencia de compilación nativa](../validation/car-cycle-native-build.json)
acompaña al resultado. Se corrigió el reconstructor de Linux para incluir
`SMA3_WIDESCREEN_DIAGNOSTICS=ON`, necesario para exponer la interfaz de las
pruebas. No se modificaron el motor ni el renderizador del coche para esta
validación, ni se repitieron pruebas con sanitizadores o de rendimiento Android.

Este resultado valida un ciclo acotado desde una entrada sintética. La espera
final demuestra que no hubo reactivación durante esos 1.200 fotogramas; no
establece seguridad permanente bajo otras entradas. **No completa la cobertura
de niveles, jefes ni todas las transformaciones.** La ampliación lateral de
aparición de objetos sigue desactivada. No se ha generado un nuevo APK con esta
validación y la panorámica real sigue ausente de Android 0.9 y 0.9.1.

## Avance del castillo 1-4 — 18 de octubre de 2026

La nueva ruta continua desde la misma entrada sintética supera el bloqueo
histórico en X=1872.99609375, cruza el primer muro y utiliza el elevador.
Termina en X=2662.93359375 dentro del corredor inferior: **789.9375 píxeles
más adelante**, con tres vidas, cero muertes y Baby Mario sobre Yoshi.
El [informe del avance](../validation/widescreen-castle-progress.json) y su
[explicación reproducible](../validation/CASTLE_PROGRESS_VALIDATION.md)
documentan el criterio y sus límites.

| Comprobación | Resultado |
| --- | --- |
| Ruta nativa en 240 × 160 y 356 × 160 | 2.238 fotogramas por ejecución |
| Imágenes centrales RGB idénticas | 73 pares |
| Comparaciones de estado, incluida la final | 74 pares |
| Espera final estable, sin entradas y con el bebé seguro | 337 fotogramas consecutivos |
| Huecos brutos / visibles sin resolver / envíos incoherentes | 0 / 0 / 0 |
| Fallos de despacho / instrucciones interpretadas / reparaciones nativas | 0 / 0 / 0 |

El recorrido ampliado descubrió 132 fotogramas con posiciones visibles sin
resolver en las dos paletas del sprite `0x13C`. La corrección observa sus
coordenadas completas después del dibujo y de los ajustes afines, y conserva
la comprobación de los atributos finales. La repetición eliminó esos huecos
sin modificar la trayectoria ni el estado del juego. El
[resultado anterior a la corrección](../validation/widescreen-castle-progress-before-fix.json)
permanece separado y rechazado por su cobertura insuficiente.

La [comparación antes/después](../validation/widescreen-castle-fix-effect.json)
confirma cambios reales de las paletas en los márgenes de tres capturas,
sin cambios en los centros ni en los estados comparados. La
[regresión del coche](../validation/widescreen-car-post-castle-fix.json)
también vuelve a superar el ciclo completo con el mismo binario corregido.

Las imágenes y los estados se comparan en puntos de captura; el guardado final
comparte fotograma con la última captura. Las entradas, el movimiento y la
salud sí se observan en cada fotograma. La excepción de comparación de estado
sigue limitada al reloj RTC inactivo y al estado de presentación PPU descritos
en la documentación de la prueba.

La ruta termina antes de la siguiente pared, situada en X=2720. **Todavía no
acredita una transición de sala natural ni completar el castillo.** El cambio
de subnivel de la inicialización sintética se registra, pero no cuenta como
transición jugada. La aparición lateral ampliada sigue desactivada y esta
validación de escritorio no añade panorámica a Android 0.9 ni 0.9.1.
