# Segunda tubería del castillo 1-4 — reconstrucción del 23 de octubre de 2026 (UTC)

La reconstrucción supera la validación nativa de una ruta acumulada de
**130 acciones y 6.801 fotogramas**, con dos transiciones naturales por tubería
y **cero huecos de posiciones de objetos** en el ejecutor corregido. Se repitió
desde el fixture original en 240 × 160 y 356 × 160, antes y después del arreglo.
La máscara OBJ-window conserva el ancho original de 240 píxeles; la sala final
sigue mostrando franjas negras en parte de los márgenes.

## Alcance y estado

Esta fase valida una ruta acumulada desde el mismo estado de entrada sintético
del castillo. Conserva las **65 acciones y 3.907 fotogramas** de la
[primera transición por tubería](CASTLE_TRANSITION_VALIDATION.md), continúa
por el corredor superior y reproduce una segunda transición natural desde el
**nivel 3/subnivel 3 al nivel 3/subnivel 153**. La segunda tubería corresponde
a la pantalla `0x48` y al índice de salida **576**; su posición de carga observada
es **X=464, Y=1824**. El contrato confirma el recorrido, la salud y la estabilidad
final en ambas anchuras.

La entrada inicial sigue siendo sintética. Las cuatro sesiones la cargan una
sola vez y reproducen toda la cinta sin restauraciones intermedias ni cambios
artificiales de posición, colisiones, RNG o salud. La calibración
por segmentos sirve para encontrar las entradas y no acredita por sí sola
la continuidad de la ruta completa.

La [cinta final](panorama-castle-post-pipe-actions.json) tiene digest canónico
SHA-256 `87bfcf6f8143bd0fc2436cb2d6b610749c1dc9183eaf688e52c2375174aa2c2e`.
El [contrato](castle-post-pipe-expectations.json) la vincula al fixture y al
prefijo anterior. Los datos publicados proceden de las nuevas ejecuciones
del **23 de octubre de 2026 (UTC)**; no reutilizan los tiempos ni los resultados
de una compilación anterior.

Esta fase no califica otras salidas, la obtención de una llave, el jefe ni el
castillo completo. La aparición lateral ampliada sigue fuera de su alcance.
La panorámica continúa sin integrarse en los APK Android **0.9 y 0.9.1** y
esta validación de escritorio no mide FPS.

## Correcciones reconstruidas

Los productores se contrastaron con
[sma3-disasm fijado en c8532ec9](https://github.com/KarisaAdvynia/sma3-disasm/tree/c8532ec9a8d0038c3bfeb003dd0c7ea89d7e1071).
Los observadores conservan las coordenadas completas que no caben en los
campos de OAM y exigen que coincidan los tres atributos finales. No deducen
el signo de X de los nueve bits de su representación gráfica.

| Productor | Punto observado | Corrección y comprobación |
| --- | --- | --- |
| Nube puente `0x0BB` | Retorno `080DC02A` de `080DBF6C` | Conserva el desplazamiento de ocho píxeles y la X completa durante el encogimiento; comprueba propietario, fase, escala, matriz y atributos terminados. |
| Objeto compuesto `0x064`, variante revisada | Entrada `0808E144` y retorno `0808E33C` | Captura los atributos iniciales que permanecen intactos en los componentes recortados y valida los 17 componentes antes de publicar el grupo completo. |
| Sprites `0x076/0x077` | `080784CA` | Observa la tercera pieza tras su cambio de tamaño afín, conserva la procedencia del emisor y comprueba las coordenadas, el tile dinámico y la matriz. |
| Copias OBJ-window | `0805EE4E`, más la composición al compactar | Conserva la X completa de cada copia, reproduce `A0 \| 0x0800`, A1 intacto y `A2 & 0xF3FF`, y antepone las copias a los objetos originales con el límite de 128 entradas. |

La composición valida la lista copiada completa antes de sustituir un envío.
Los originales conservan su orden y los índices de matrices afines: VBlank
restaura los coeficientes por separado. Las copias no filtran Y=160, aunque
la compactación normal sí omite esos objetos. La comparación con OAM continúa
seleccionando **un único envío completo** entre los tres admitidos; no se
mezclan coincidencias de varios fotogramas ni se amplía la edad de la caché.

Las capturas de ventana y del sprite `0x064` se limpian en cada compactación
y al cambiar la época del estado. Un productor o cambio posterior de atributos
que no esté demostrado sigue rechazándose. La observación modifica únicamente
las estructuras auxiliares del adaptador; no escribe RAM, registros de CPU,
temporización ni estado del bus del juego.

## Límite real del efecto OBJ-window

El [motor fijado en e7728148](https://github.com/mstan/gbarecomp/blob/e7728148c6829ba526f682876430a0c9022dc6c0/src/gba/gba_ppu.cpp)
construye `obj_window_storage[GbaPpu::kScreenWidth]` en el área original de
**240 píxeles**. La consulta de esa máscara requiere `0 <= hx < kVanW`.
La política `black_nonuniform_window_margins` deja negros los márgenes cuando
los controles de ventana no son uniformes en la línea y
`g_ws_authored_margin_layers` es cero. El adaptador SMA3 mantiene ese valor.

Reconstruir las copias restablece su correspondencia con los objetos mostrados
y permite verificar su procedencia. **No extiende la máscara OBJ-window a las
columnas laterales.** Ni la igualdad del centro ni una cobertura de posiciones
sin huecos demostrarían por sí solas que todo el efecto de ventana del destino
esté extendido. La inspección de las imágenes del destino debe describir este
límite de forma expresa.

La inspección visual de las nuevas capturas confirma el límite. La comparación
antes/después del fotograma transcurrido **6177** restaura piezas rosas del
peligro en el margen izquierdo y conserva el centro. Las capturas corregidas
**6420 y 6801** muestran el centro del destino y franjas negras en ambos
márgenes a ciertas alturas. Estos PNG permanecen privados. El resultado de
cobertura acredita la procedencia de los objetos del recorrido, no una
extensión completa de la máscara de esa sala.

## Criterios de aceptación de la ruta

El perfil de observación es `castle-status-v1`. El
[validador de la segunda tubería](castle_post_pipe_checks.py) y su
[comparador](compare_castle_post_pipe_progress.py) exigen:

- Volver a validar el prefijo de 65 acciones y su contrato original, vinculados
  por sus huellas, dentro de la traza acumulada.
- Observar exactamente una secuencia adicional **13 → 9 → 10 → 11 → 13**
  después del prefijo, con fases de duración acotada.
- Demostrar Down efectivo, descenso medido desde el área de entrada y
  continuidad del movimiento al iniciar la salida, cambio del índice de
  salida de 1016 a 576, tipo de entrada 1 durante 9/10
  y 0 antes y después, carga sostenida en (464,1824) y retorno jugable.
- Mantener el nivel 3; pasar del subnivel 3 al 153 al cargar el destino,
  permaneciendo allí hasta el final. El subnivel de la entrada sintética no
  cuenta como otra transición natural.
- Mantener la salud en toda la traza, incluida la frontera del prefijo:
  sin pérdida de vidas, muertes, inhibición del contador ni separación de
  Baby Mario en juego normal, aunque después se recupere. En los estados
  de carga 10/11 se permite el marcador vacío 0/0 del cargador.
- Terminar con al menos **300 fotogramas consecutivos**, sin botones, posición
  fija, velocidad cero y el bebé seguro dentro de la zona final prevista.
- Comparar las dos anchuras, la traza completa, todas las capturas programadas,
  los estados guardados y la procedencia; superar los diagnósticos nativos
  y la cobertura, con **cero fotogramas con huecos brutos de posición**.

`complete_castle_post_pipe_progress_pass` y
`second_natural_room_transition_qualified` solo pueden ser `true` cuando
coinciden todos esos criterios. Dos ejecuciones iguales que no entren en
la tubería o que compartan el mismo fallo no bastan.

El presupuesto general de una sesión admite hasta 7.200 fotogramas. Ese
límite permite reproducir el prefijo y la continuación juntos; no cambia
los mínimos de salud, recorrido y reposo ni los contratos históricos.

## Nuevas ejecuciones nativas y efecto de la corrección

Los agregados [de la base](widescreen-castle-post-pipe-before.json) y
[del ejecutor corregido](widescreen-castle-post-pipe.json) registran los
resultados nuevos. La base reproduce correctamente las entradas y la salud,
pero no supera la cobertura de posiciones; por ello su calificación completa
es `false`, aunque coincidan las dos anchuras.

| Comprobación | Resultado nuevo |
| --- | --- |
| Ruta acumulada: acciones / fotogramas / observaciones | **130 / 6.801 / 6.802**, incluida la observación inicial |
| Pareja base, 240 × 160 frente a 356 × 160 | Diferencial y contrato de recorrido correctos; cobertura rechazada |
| Pareja corregida, 240 × 160 frente a 356 × 160 | Diferencial, recorrido, salud y cobertura correctos |
| Centros RGB idénticos / comparaciones de estado equivalentes | **250 / 251** en cada pareja |
| Huecos brutos / visibles / envíos incoherentes, base | **759 / 338 / 421** en cada anchura |
| Huecos brutos / visibles / envíos incoherentes, corregido | **0 / 0 / 0** en cada anchura |
| Observaciones entre las cuatro sesiones | Las 6.802 observaciones coinciden; misma traza SHA-256 |
| Comparación antes/después por anchura | 250 imágenes completas de 240 píxeles, 250 centros panorámicos y 251 estados equivalentes por anchura |
| Cambios de imagen antes/después | Solo en márgenes de **21 capturas**, 14.111 píxeles RGB sumados |
| Salud del recorrido | Tres vidas conservadas, cero muertes, cero episodios de separación de Baby Mario en juego normal |
| Final | Subnivel 153, X=464, Y=1842,99609375; velocidad cero; **418 fotogramas** quietos y sin botones |
| Fallos de despacho / instrucciones interpretadas / reparaciones nativas | **0 / 0 / 0** en las cuatro sesiones |
| Regresión del ciclo de coche con el ejecutor corregido | Correcta; detalle en el apartado siguiente |
| `complete_castle_post_pipe_progress_pass` | Base `false`; corregido **`true`** |
| `second_natural_room_transition_qualified` | Base `false`; corregido **`true`** |

La segunda entrada registra Down efectivo desde el fotograma **6212** y un
descenso de **31,75 píxeles**. Inicia la salida, estado 9, en **6247**; entra
en el estado de carga 10 en **6296** y observa la posición (464,1824) desde
**6297**. Pasa al estado 11 en **6313** y vuelve al juego normal, estado 13,
en **6379**. Las duraciones 49/17/66 de las fases 9/10/11 satisfacen los
intervalos del contrato. El índice final es 576 y el tipo de entrada vuelve a 0.

El [informe del efecto de la corrección](widescreen-castle-post-pipe-fix-effect.json)
compara las revisiones por anchura. Conserva las entradas, las observaciones,
el calendario, los centros y las secciones de estado comprobadas. Detecta
**13.611 píxeles RGB cambiados a la izquierda y 500 a la derecha**, sumados
entre las capturas; no son recuentos de canales ni de todos los fotogramas.
Los cambios marginales y la reducción de los huecos de 759 a cero están
ligados a dos ejecutables distintos y a los mismos activos de entrada.

Las entradas, el movimiento, la escena y la salud se comprueban en cada
fotograma. Las imágenes y estados se comparan en puntos programados, incluidos
los cambios de escena y finales de acción. El guardado final comparte fotograma
con la última captura; no debe contarse como otro fotograma observado.

El comparador conserva las excepciones existentes: ocho bytes del RTC inactivo
y la sección PPU0 dependiente del área de presentación. Las demás secciones
de estado comprobadas deben coincidir. El centro RGB de 240 × 160 se compara
por separado; en la imagen de 356 píxeles empieza a 58 píxeles del borde.
Los recuentos de capturas no equivalen a comparar todas las imágenes producidas.

## Regresión del ciclo de coche

El [agregado nuevo del coche](widescreen-car-post-pipe-fix.json) vuelve a
ejecutar la cinta y el contrato históricos sin modificarlos, usando el mismo
ejecutor corregido. Las dos anchuras completan **3.506 fotogramas**, con
**3.507 observaciones idénticas**, **61 centros RGB iguales** y **62
comparaciones de estado equivalentes**. El ciclo observado es **0 → 2 → 0**.

Yoshi vuelve al modo normal en el fotograma transcurrido 2216, termina con
**1.200 fotogramas consecutivos sin botones** y avanza **58,83984375 píxeles**
respecto de la primera observación normal de recuperación, superando el mínimo
de 32. Ambos registros tienen cero huecos de posiciones, fallos de despacho,
instrucciones interpretadas y reparaciones nativas.
`complete_cycle_pass` es `true`. Esta regresión acotada no amplía la cobertura
a otras transformaciones ni a niveles completos.

## Pruebas independientes reconstruidas

El 23 de octubre se compilaron y ejecutaron de nuevo estas pruebas con
**GCC 13.3, C++20, `-Wall -Wextra -Werror`, AddressSanitizer y
UndefinedBehaviorSanitizer**. Las cinco terminaron correctamente y no
produjeron diagnósticos de los sanitizadores. LeakSanitizer estuvo desactivado;
no se afirma una comprobación de fugas.

| Prueba | Cobertura relevante |
| --- | --- |
| [Nube puente](widescreen_cloud_test.cpp) | Coordenadas completas y sus alias, resta antes de truncar, escala, matrices, propietario, capacidad y rechazo de datos obsoletos. |
| [Pieza afín](widescreen_scaling_triplet_test.cpp) | Desplazamiento y límites verticales, escritura parcial de recorte, tiles, fases de matriz y paso de la X completa a la copia OBJ-window. |
| [Ventanas](widescreen_window_test.cpp) | Copias y originales exactos, Y=160, índices afines, presentación única, mutaciones de atributos, límites de capacidad y composición atómica. |
| [Sprite compuesto 0x064](widescreen_hazards_test.cpp) | Geometría literal y modelo independiente de las escrituras Thumb, 17 triples, metadatos, recortes parciales, vida de la captura y límites de punteros. |
| [Objetos giratorios existentes](widescreen_rotating_test.cpp) | Regresión de coordenadas, grupos afines, parejas finales, atributos obsoletos y capacidad. |

Las **166 pruebas Python** pasan con Python 3.12.14, tanto en ejecución normal
como con `python -O`, una pasada completa por modo y sin cambios de fuentes
entre ambas. Cubren los validadores, los límites de sesión y el rechazo de
falsas aprobaciones del agregador. Los datos sintéticos de esas pruebas no
representan una ejecución del juego. El
[registro de comprobaciones](castle-post-pipe-checks-2026-10-23.txt)
documenta las pruebas nuevas y sus límites.

## Procedencia y reproducción

El [manifiesto de compilación](castle-post-pipe-native-build.json) registra
la base `1a777c609ab2ede5655b66280c2d51416d7bdf2d`, los pines de motor y
desensamblado indicados arriba y las huellas de las fuentes de ambas revisiones.
El fixture del castillo tiene SHA-256
`02f131a013d8bc2c352b05502ce0f420136e6736c8b4864cbfb51c4307de11f2`.

Se usaron **CMake 4.4.4 recuperado, GCC 13.3, C++20 y Release**, con
`SMA3_WIDESCREEN_DIAGNOSTICS=ON`. El target comprobado es `sma3_runner`.
El primer intento de la base falló por falta del encabezado privado generado
`localization_data.h`. Se recuperó el archivo existente; la base y la revisión
corregida usan la misma huella y sus compilaciones posteriores terminaron
correctamente. No se publica el contenido de ese encabezado.

El archivo generado reutilizado tiene **66 miembros ELF, 49.507 funciones
declaradas y definidas y cero funciones ausentes**, según `verify_archive.py`.
Su SHA-256 comienza por `c9b218c57467` y **difiere** del archivo histórico
`8d3470c6b408` del 19 de octubre. Se volvió a calcular su huella y coincide
con el informe de verificación. No se afirma que se hayan regenerado todas
las fuentes desde la ROM y la BIOS durante esta reconstrucción.

La verificación del motor aplica sus cinco parches públicos a copias temporales
de los seis archivos afectados en el commit fijado. Sus huellas coinciden con
los archivos usados; la comprobación no modifica el motor. El manifiesto
incluye las huellas completas del archivo generado, la configuración, la ROM,
la BIOS, los fixtures y estos ejecutables:

| Ejecutor | Tamaño | SHA-256 |
| --- | ---: | --- |
| Base | 138.527.624 bytes | `4b4725f81166763d43f8a95c2484dcb56b85104a0fedea8c6f562520d2a7ec1f` |
| Corregido | 138.539.672 bytes | `45fa470e1145d780245e9b0ff85522ccad315a2acd488627be967d4cd315757b` |

Para repetir la calificación, compilar el target `sma3_runner` con
`SMA3_WIDESCREEN_DIAGNOSTICS=ON`, ejecutar ambas anchuras desde ese fixture
con `--castle-status`, que registra `capture_start=0`, y pasar el contrato final a
`compare_castle_post_pipe_progress.py`. Cada salida y cada informe usan una
ruta nueva para conservar los intentos anteriores.

Los agregados públicos contienen mediciones y huellas. ROM, BIOS, código
generado del juego, binarios, estados, capturas y trazas completas deben
permanecer fuera de la publicación. Los informes históricos
[de la ruta inicial](CASTLE_ROUTE_VALIDATION.md),
[del avance](CASTLE_PROGRESS_VALIDATION.md) y
[de la primera tubería](CASTLE_TRANSITION_VALIDATION.md) se conservan intactos.
