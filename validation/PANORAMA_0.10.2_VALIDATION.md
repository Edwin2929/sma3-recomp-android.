# Panorámica 0.10.2: márgenes estables, fondos animados y Fuzzy

## Resultado y alcance

La revisión de 0.10.1 encontró **dos causas adicionales** de los defectos
visuales: el cierre de todos los márgenes cuando aparecía un único sprite sin
posición ampliada conocida, y la reconstrucción de ciertos fondos a partir de
un mapa que omitía modificaciones animadas realizadas directamente en VRAM.
La prueba anterior del castillo no ejercitaba estos casos.

0.10.2 corrige ambos casos comprobados. También mantiene el encuadre original
mientras está activo el efecto de deformación de Fuzzy y restaura después la
panorámica. No elimina el efecto del juego ni modifica su duración. La
corrección del despacho nativo que permite ejecutar ese efecto se documenta
por separado en la validación de Fuzzy.

Las sesiones aquí descritas son diagnósticos de host con las rutas de
presentación de producción. **No son una prueba física del Pixel 7 Pro, de
la GPU de Android ni de todos los niveles.** Los estados de entrada son
fixtures privados preparados para reproducir estos escenarios; no acreditan
un recorrido natural desde el inicio del juego.

## 1. Un sprite desconocido ya no hace desaparecer todo el escenario lateral

En 0.10.1, `ready` exigía que todos los objetos de la entrega OAM tuvieran una
posición panorámica resuelta. Una sola animación no cubierta cerraba los dos
márgenes, incluso cuando el terreno y los demás objetos seguían siendo
válidos. Al resolverse el objeto, los márgenes reaparecían.

La política nueva conserva la escena coherente y el terreno comprobado.
Cada sprite ordinario sin posición ampliada probada se dibuja dentro de los
240 píxeles originales mediante `object_clip`; los objetos conocidos conservan
su posición lateral. No se adivinan coordenadas ni se reutilizan objetos de
un fotograma anterior. Los límites originales de aparición de objetos siguen
siendo una limitación de la beta.

### La iluminación conserva una excepción estricta

Un objeto que define una **máscara de ventana** no equivale a un sprite de
color. Su posición decide qué otras capas pueden verse. Una posición de
ventana desconocida sigue cerrando los márgenes de ese fotograma.

Se añadió un contraejemplo al ensayo de la PPU: una ventana con X de hardware
488 queda fuera de los 240 píxeles originales, por lo que todas las columnas
nativas seleccionan el mismo control. Sin la protección global, el antiguo
fallback de control uniforme podría extender ese control al margen, aunque
la ventana real pudiera estar en X = −24. `window_positions_resolved`
rechaza esa extensión no probada. Las ventanas conocidas mantienen sus
límites exactos; los sprites ordinarios desconocidos no bloquean una ventana
ya comprobada. Las ventanas desactivadas no cierran la escena.

### Presentaciones reales, con el código final integrado

Se capturaron **900 presentaciones consecutivas** por recorrido, en Original
y Panorámica, con idéntica entrada y estado inicial. Se compararon los
240 × 160 píxeles centrales completos y la presencia de contenido en ambos
márgenes. Se repitió la captura panorámica con el último guard de ventanas.

| Nivel interno | Caso | Márgenes negros indebidos en 0.10.1 | Corregidos en 0.10.2 | Centros exactos |
| --- | --- | ---: | ---: | ---: |
| 0 | Inicio de 1-1 con movimiento | 312 presentaciones | 312 | 900/900 |
| 1 | 1-2 con movimiento | 26 presentaciones, en dos intervalos | 26 | 900/900 |
| 7 | Inicio de 1-8 | 447 presentaciones | 447 | 900/900 |

En 1-2 se conserva el cierre posterior asociado al cambio de escena; no se
fuerza la panorámica durante la transición. Las tres superficies se mantienen
en 356 × 160: no hay cambios espontáneos de anchura.

## 2. Los fondos completos conservan las modificaciones de VRAM

El código original de SMA3 USA `Sub080148B4` rellena los 64 × 64 tiles de BG3
a partir del bloque de pantalla 26. `Sub08079DF8` puede modificar parches de
ese mapa directamente durante una animación. El lector genérico de
metatiles omitía esas modificaciones y podía mostrar otros gráficos en los
márgenes.

El nuevo guard usa la VRAM nativa **solo** para BG3, con el bloque 26,
geometría de 64 × 64 tiles y una imagen fuente de exactamente 32 filas de
metatiles, es decir, 512 × 512 píxeles. Sigue requiriendo la escena normal
reconocida, la entrega OAM coherente y una cabecera de imagen válida.
No se generaliza la regla a imágenes mayores que utilizan actualizaciones
parciales. BG0/BG1 conservan el lector de escenario completo; BG2 y los
BG3 que no cumplen el guard conservan el lector previo.

El ensayo [`widescreen_background_map_test.cpp`](widescreen_background_map_test.cpp)
utiliza **13 mapas privados capturados**. Renderiza cada margen con la PPU
ampliada y lo contrasta con la PPU original de 240 píxeles desplazando el
scroll. Una paleta sintética única permite distinguir índices, flips y
bancos, sin confundir un color igual con un mapa correcto.

- En el estado capturado de 1-8, el lector antiguo produce **7.300 píxeles
  laterales distintos** del mapa nativo correcto.
- El lector corregido produce **cero diferencias** en los dos márgenes de
  los 13 estados, y conserva exactamente todos sus centros.
- Junto con el fallback individual de sprites, el contraejemplo de ventanas
  y los límites del guard, pasan **2.507.920 comprobaciones internas** con
  AddressSanitizer y UndefinedBehaviorSanitizer. LeakSanitizer permanece
  desactivado por las restricciones del entorno.

Son comprobaciones del compositor y de una capa aislada con paleta de prueba;
no se presentan como 13 recorridos completos ni como 2,5 millones de escenas.

## 3. Fuzzy conserva el efecto original y vuelve a la panorámica

El efecto de Fuzzy no consiste solo en cambiar el scroll: su rutina ARM
`Sub0800091C` modifica una franja nativa de 31 columnas con desplazamientos
verticales distintos. El lector del mapa completo no dispone de esas
columnas adicionales deformadas. En una captura del efecto activo, comparar
ese lector con la VRAM original detecta 245 tiles diferentes en BG0 y 435 en
BG1 entre las muestras del centro. No es válido extrapolar el mapa sin
aplicar el mismo efecto a las columnas adicionales.

Mientras el indicador nativo de ese efecto, `0x03007294`, vale 1, se conserva
el centro original con márgenes negros dentro de la misma superficie. Al
restaurar el juego sus mapas, vuelven los márgenes panorámicos. La selección
«Panorámica (beta)» permanece guardada.

La cinta [`fuzzy-contact-recovery-actions.json`](fuzzy-contact-recovery-actions.json)
produce **1.500 presentaciones** en cada modo, incluyendo contacto, efecto y
recuperación. Con el código final:

- Los **1.500 centros** coinciden exactamente con Original.
- La anchura permanece en 356 píxeles.
- Los márgenes se abren en la subida 93, se cierran una sola vez en la 601
  y vuelven a abrirse en la 1.383. Permanecen negros durante las 782
  presentaciones intermedias; no alternan entre negro y escenario.
- La ejecución finaliza con cobertura `FULLY_STATIC`, cero faltas de
  despacho y cero instrucciones interpretadas.

Estos contadores se refieren a presentaciones completas de host, no a una
medición de 120 FPS ni al ejecutable Android en un teléfono.

## 4. Una diferencia adicional se investigó hasta el resultado visible

La exploración inicial, sobre el runner anterior instrumentado y 16 entradas
de nivel de 1.200 fotogramas cada una, encontró también una entrada de BG1
`0x4065` frente a `0x4163` en 1-2, fotograma 7.190. No se descartó por tratarse
de una sola entrada.

Se congelaron los datos originales de RAM, VRAM, OAM, registros y paleta.
Reemplazar esa entrada en una copia de VRAM y renderizar de nuevo toda la
escena con la PPU original produce **cero píxeles visibles diferentes**.
Al retirar únicamente OBJ en las copias del ensayo aparecen 64 diferencias:
el tile completo de 8 × 8 estaba oculto por el Chomp opaco. Por tanto, esa
observación no acredita otro defecto visible de textura. No se modificó el
juego para ocultarla.

## Reproducción y evidencia

El [registro de resultados](android-0.10.2-panorama.json) identifica los
runners, entradas, fuentes y capturas por SHA-256. Los RGB, estados y datos
originales del juego permanecen privados.

Para los recorridos de márgenes:

1. Crear los estados privados de los niveles 0, 1 y 7 con
   [`make_level_fixture.py`](make_level_fixture.py), usando el mismo estado
   normal de partida.
2. Capturar Original y Panorámica con
   [`capture_production_frames.py`](capture_production_frames.py), 900
   presentaciones, estado de origen en el fotograma 6.599 y la cinta
   [`panorama-margin-stability-actions.json`](panorama-margin-stability-actions.json).
3. Comparar con [`compare_production_frames.py`](compare_production_frames.py).
   Revisar también `margin_visibility_changes`; un centro exacto por sí
   solo no demuestra que los márgenes permanezcan estables.

Para el oráculo de BG3, los volcados de `run_widescreen_probe.py` se convierten
con [`prepare_background_map_fixtures.py`](prepare_background_map_fixtures.py).
Este helper rechaza imágenes que no encajan en el guard. El ensayo C++ se
compila contra la PPU real y recibe los `.bgf` privados como argumentos.
El código del ensayo no contiene ROM, texturas originales ni estados.

## Límites que permanecen explícitos

La panorámica sigue siendo una beta temprana. No se amplían los límites de
aparición de todos los objetos, y las posiciones no verificadas permanecen
recortadas al encuadre original. Las escenas no cubiertas, las máscaras de
ventana sin posiciones comprobadas y el efecto de Fuzzy utilizan márgenes
negros. No se garantiza ausencia de defectos en cualquier nivel o combinación
de ajustes. La validación física en el Pixel 7 Pro sigue pendiente.
