# Android 0.10.1 — corrección del titileo y de la imagen panorámica

Fecha de validación: **25 de octubre de 2026 (UTC)**.
Base de código y APK afectados: **0.10**, commit
[`66436d1b2687afcee0ea89af94dd05ef6dadda43`](https://github.com/Edwin2929/sma3-recomp-android./commit/66436d1b2687afcee0ea89af94dd05ef6dadda43).

## Resultado

Se reprodujo y corrigió un fallo de presentación de 0.10 que provocaba
oscilaciones de anchura y texturas de otro momento del juego. El mismo
recorrido de movimiento produce ahora **600 centros de imagen idénticos a
Original**, con anchura panorámica estable. También pasan los cambios de
superficie y la ruta de mezcla temporal. Se entrega el APK firmado
**`sma3-recomp-0.10.1-panorama-fix-arm64.apk`**, código de versión **12**.

Esto acredita las correcciones y los casos descritos; no acredita todos los
niveles ni una ejecución física en el Pixel 7 Pro. Los límites de aparición
y dibujo de objetos de la panorámica experimental siguen abiertos.

## Defecto de producción reproducido

La prueba anterior de 0.10 comprobaba la presentación sobre una escena quieta
al final de 120 fotogramas. Para investigar el reporte se capturaron ahora
**todas las subidas RGB a SDL durante 600 fotogramas con movimiento**, utilizando
los callbacks de producción y la misma cinta de entradas en Original y
Panorámica. No se activa la ruta de diagnóstico que fuerza una anchura fija.

En el runner intacto de 0.10 la anchura cambió así, usando índices de subida
desde cero:

| Subida | Anchura | Píxeles centrales diferentes de Original |
| ---: | ---: | ---: |
| 271 | 356 | 10.278 |
| 272 | 240 | 16.693 |
| 287 | 356 | 22.543 |
| 288 | 240 | 22.604 |
| 291 | 356 | 22.478 |
| 292 | 356 | 129 |

Los primeros cinco casos coinciden con cambios de anchura. El sexto es el
fotograma siguiente a un cambio que ocurrió cuando ya se había dibujado una
línea con el paso de fila anterior. Las otras 594 imágenes centrales eran
idénticas. La comparación usa el centro completo de 240 × 160 de cada subida,
no muestras aisladas del recorrido.

### Causa comprobada

La escena y los objetos se validaban al principio del fotograma. Después,
al presentar, `presentation_ready()` volvía a comparar esa observación con
OAM en vivo. Durante VBlank el juego ya podía haber cargado los objetos de
la **siguiente** imagen, mientras la PPU conservaba correctamente la imagen
terminada. La comparación tardía rechazaba esa imagen válida y solicitaba
otra anchura.

Una traza independiente conservó exactamente las subidas de la versión intacta:
la política era válida en la línea 0, sin objetos sin resolver; al presentar
en las líneas 182/189, el único rechazo era la comparación con OAM en vivo.
El cambio de tamaño invalidaba la imagen terminada y el runtime llamaba a
`ppu.render` con memoria de esa fase posterior. Eso mezclaba el momento de
las texturas con el de la imagen que debía presentarse. La superficie de
trabajo tampoco se recolocaba al cambiar de paso de fila.

## Correcciones

1. **Geometría estable mientras Panorámica está seleccionada.** La política
   de escena sigue aplicándose durante el dibujo del fotograma. Las escenas
   sin cobertura conservan el centro original con márgenes negros, dentro de
   la misma superficie panorámica. Se elimina la decisión tardía basada en
   OAM en vivo; no se conservan posiciones de objetos antiguos para ocultar
   un fallo de coherencia.
2. **Redimensionado que conserva la imagen terminada.** La PPU recoloca el
   centro nativo de sus dos buffers al nuevo paso de fila: el terminado y
   el que todavía se está dibujando. Conserva el indicador de imagen válida.
   Los márgenes se limpian y los dos caminos de presentación copian esa imagen
   terminada en lugar de reconstruirla con memoria de VBlank.
3. **Restauración que conserva la imagen guardada.** Cuando la partida tiene
   una imagen nativa válida, la carga en panorámica la coloca en el centro y
   deja negros los márgenes que el archivo no contiene. Conserva también
   la fase del dibujo en curso y el estado de temporización.
4. **Bandas negras durante el blanqueo forzado.** Si la política había cerrado
   los márgenes, el modo blanco del juego ya no los pinta de blanco. El centro
   conserva el comportamiento original y las políticas laterales se respetan.

Los cambios de la PPU y del runtime forman el octavo parche,
[`gbarecomp-panorama-stability.patch`](../patches/gbarecomp-panorama-stability.patch).
La política estable está en [`src/main.cpp`](../src/main.cpp). No se modifican
la lógica del juego, RAM, CPU, límites de aparición ni sus rutinas generadas.

Durante un cambio real de tamaño, los márgenes pueden permanecer negros
hasta que se dibujen de nuevo; no se inventa una imagen lateral a partir de
memoria de otra fase. Original, Panorámica y Estirar siguen siendo opciones
independientes, y se conserva la preferencia guardada al actualizar.

## Pruebas de presentación de producción

| Prueba | Antes, 0.10 | Después, 0.10.1 |
| --- | --- | --- |
| 600 fotogramas con movimiento | Cinco cambios de anchura y seis centros corruptos. | Cero cambios de anchura y 600 centros exactos. |
| Ruta real de mezcla temporal compilada para host | Seis presentaciones completas y nueve intermedias con centro incorrecto. | 600 completas y 600 intermedias correctas; sin errores de la fórmula de mezcla. |
| Cambios de superficie 356 → 240 → 347 → 356 | Cubiertos por la regresión sintética del defecto. | Los 600 centros coinciden con Original, incluidos los cambios y sus fotogramas siguientes. |

En el recorrido corregido, los márgenes permanecen negros durante la entrada
no admitida, aparecen en la subida 264 y no vuelven a desaparecer hasta el
final de la prueba. Los 600 fotogramas de **Original** del candidato también
coinciden exactamente con Original de 0.10.

La secuencia de tamaños se aplica a las consultas de superficie SDL, sin
modificar memoria del juego. Las comparaciones temporales distinguen las
presentaciones completas de las intermedias y comprueban los contadores
nativos. Todos los recorridos terminan con cobertura `FULLY_STATIC`, cero
faltas de despacho, cero instrucciones interpretadas y cero fallos de bus.

El host que ejercita la mezcla se compila con el mismo camino de runtime
`SMA3_FRAME_INTERPOLATION`; no se afirma que sea el binario Android ni una
medición de 120 Hz en el teléfono. El planificador puede omitir un punto
intermedio si vence su plazo: el número bruto de subidas no es una medición
de velocidad del juego.

[Resultados y huellas de las capturas](android-0.10.1-production-stability.json).
El runner de producción corregido tiene SHA-256:

```text
397aa0f28b608c410c8614dbd3a0373035cc0a9f3b153cd5a799215cdf7fde39
```

## Regresiones de PPU, escalador y parches

| Grupo | Resultado |
| --- | --- |
| Redimensionado, restauración y blanqueo | **87 escenarios**, 699.005 comparaciones internas; ASan/UBSan. |
| Ciclo de vida actualizado | **40 escenarios**, 2.413.802 comparaciones internas; ASan/UBSan. |
| Compositor real de ventanas | **488 escenarios**, 8.102.978 comparaciones internas; ASan/UBSan. |
| Escalador y límites del mezclador | **36 presentaciones y 255 comprobaciones**, incluidas ocho del mezclador; ASan/UBSan. |
| Python | **169 pruebas normales y 169 con `-O`**, todas aprobadas. |
| Aplicador de parches | **Ocho parches, nueve etapas reconocidas**, 18 aplicaciones/repeticiones aprobadas por modo; rechazo de ediciones ajenas. |

La misma fuente final de la prueba nueva falla contra la PPU preservada de
0.10 en tres subgrupos independientes: pérdida de la imagen terminada al
redimensionar, márgenes blancos indebidos y pérdida de la imagen guardada al
restaurar. Los tres pasan con la PPU corregida. Las comprobaciones internas
son comparaciones de píxeles/estado, no cientos de miles de escenas distintas.
LeakSanitizer permanece desactivado por las limitaciones del entorno; los
sanitizadores de direcciones y comportamiento indefinido están activos.

[PPU y fallos antes de la corrección](android-0.10.1-ppu-checks.json) ·
[Escalador y mezcla](android-0.10.1-presentation-audit.json) ·
[Python y parches](android-0.10.1-python-checks.json).

### Diagnóstico adicional sobre 0.10

Para separar el fallo de presentación del decodificador de terreno, se
compararon 1.800 fotogramas del castillo y 900 de cada uno de otros dos niveles
con anchuras fijas. Los 3.600 pares conservaron el centro, el recorrido y la
E/S observada. Las diferencias de entradas en cinco estados del castillo eran variaciones
de paleta en una pieza totalmente transparente; no cambiaban los píxeles.
Los otros dos niveles también pasan el contraste entre el mapa reconstruido
y el nativo en 211.200 muestras de píxeles de capa de 22 estados.

Este trabajo se realizó **con el runner antiguo de 0.10 para localizar el
fallo**. No se presenta como una nueva calificación completa de esos niveles
con 0.10.1. Los recorridos largos históricos del castillo y del coche tampoco
se han repetido en esta entrega.
[Registro de diagnóstico](android-0.10.1-baseline-diagnostics.json).

## APK y procedencia

El archivo entregado mide **31.787.975 bytes** y tiene SHA-256:

```text
aca710f4cf4585ffb2acec1702d4c5f4d6b4c0aa270bed8ddea6727801ec7558
```

Paquete `org.sma3.recomp.experimental`, versión
`0.10.1-panorama-fix-experimental`, código **12**, ARM64, Android 9/API 28 mínimo
y API 35 de destino. La variante de aplicación es `debug`, con nativo
`Release`, como las entregas experimentales anteriores.

Se recompilaron las fuentes nativas modificadas. El archivo generado ARM64,
compilado completamente para 0.10, se reutilizó con sus 66 miembros y su hash
verificado. No se afirma una nueva generación desde ROM/BIOS ni otra
compilación completa de los 64 fragmentos. El ensamblado definitivo limpia
la salida y el estado incremental del empaquetador; se verifican versión,
firma v2, CRC, alineación, DEX consecutivos, enlace JNI y las tres bibliotecas
exactas de la compilación actual.

El certificado de firma sigue siendo:

```text
2390f57b35d436ff37b2a303189d18d463c8f5114a5ff809b2f2e6df60f58da3
```

La misma identidad y el código superior permiten preparar una actualización
sobre 0.10. No se incluyen archivos originales de ROM/BIOS, claves de firma
ni partidas. [Registro del paquete](android-0.10.1-build.json) ·
[Instalación y notas](../docs/releases/v0.10.1-experimental.md).

## Reproducción y alcance

Las fuentes de captura/comparación están junto a los informes de validación.
El registro de producción identifica ejecutables, entradas y resultados.
Las pruebas sintéticas no requieren ROM ni BIOS; las sesiones del juego
requieren archivos compatibles aportados localmente. No se publican los
RGB capturados, las partidas ni los archivos privados del juego.

Queda contrastar este APK en el teléfono, incluida su GPU, rotación real,
segundo plano y ajustes usados al observar el fallo. No se dispone de un
video ni de un nivel exacto aportados con el reporte. La corrección elimina
los defectos reproducidos; no permite garantizar que cualquier otro efecto
visual del juego completo esté cubierto. Tampoco amplía los límites originales
de aparición de objetos ni convierte la mezcla temporal en interpolación
por movimiento o en 120 estados independientes del juego.
