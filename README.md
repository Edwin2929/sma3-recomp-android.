# SMA3 Recomp para Android

Proyecto experimental de recompilación estática de **Yoshi’s Island: Super Mario Advance 3 (USA)** para Android ARM64, basado en [GBARecomp](https://github.com/mstan/gbarecomp) y en el análisis de [sma3-disasm](https://github.com/KarisaAdvynia/sma3-disasm).

## Versión actual: 0.10.17 beta — fantasma de tu mejor recorrido

**Atrás → Contrarreloj → Fantasma de mi récord** graba un recorrido completo
y muestra un Yoshi semitransparente al volver a intentar ese nivel. Conserva
la animación y el color grabados, sigue el reloj del contrarreloj y permite
comparar tu avance con el mejor recorrido guardado. Está desactivado inicialmente.

Activa también **Activar contrarreloj** antes de entrar por el principio del
nivel. Completa una marca que iguale o mejore tu récord para guardar su
fantasma. Los tiempos anteriores no contienen movimientos: necesitan una
nueva grabación. Cada nivel indica si dispone de fantasma.

Esta primera versión dibuja a Yoshi en su forma normal y en la misma
habitación. Oculta el fantasma durante transformaciones, Fuzzy y escenas
sin cobertura. Es una referencia visual; no interactúa con enemigos ni
objetos. Desactivarlo conserva los recorridos guardados. **El arranque y
la recuperación automática de la sesión permanecen como en 0.10.16.**

[Notas de 0.10.17](docs/releases/v0.10.17-beta.md) ·
[Cómo usar el fantasma](docs/FANTASMA_CONTRARRELOJ.md) ·
[Pruebas y límites](validation/GHOST_0.10.17_VALIDATION.md).

### Contrarreloj y reportes conservados de 0.10.16

**Atrás → Contrarreloj** añade tiempo por nivel, comparación con la mejor
marca y un cronómetro pequeño opcional. Guarda récords locales separados
de la partida para **60 niveles**: los 48 principales, seis Extra y seis
Secretos. Se activa antes de entrar por el inicio principal. Cuenta tiempo
del juego con la misma escala a 60/120; Start e inventario lo pausan. Morir,
salir o cargar un estado cancela el intento. Está desactivado inicialmente.

**Reportar un problema** está en Ajustes y en la pantalla inicial. Prepara
un informe con la descripción, versión, modelo del teléfono y ajustes;
permite revisar y editar todo el texto antes de copiarlo, compartirlo o
abrir un borrador en GitHub. No envía nada automáticamente ni adjunta ROM,
BIOS, partidas o registros privados. Los dos apartados admiten español,
inglés y portugués.

[Notas de 0.10.16](docs/releases/v0.10.16-beta.md) ·
[Cómo usar ambas funciones](docs/CONTRARRELOJ_Y_REPORTES.md) ·
[Pruebas y límites](validation/FEATURES_0.10.16_VALIDATION.md).

### Traducción ampliada conservada de 0.10.15

La traducción al español latinoamericano y portugués de Brasil añade
**seis rótulos de archivos y pausa/inventario** y **veinte paquetes de créditos
por idioma**. Incluye las cabeceras de archivos, copiar y borrar; mundo,
mejor puntuación y puntos totales; cargos, agradecimientos y mensajes finales.
Los nombres del equipo original se conservan.

Los 187 textos anteriores mantienen exactamente sus bytes y direcciones.
El total pasa a **207 paquetes por idioma**, además de los rótulos gráficos.
Los acentos de los créditos se adaptan a su fuente y se comprueba el ancho
de las líneas. Actívalo en **Atrás → Idiomas → Idioma del juego**.

La traducción sigue siendo parcial: quedan otros rótulos, botones ilustrados,
estrellas/monedas/flores, el menú de suspensión y Mario Bros. Los gráficos
nuevos aparecen cuando el juego vuelve a cargar la pantalla correspondiente.

[Notas de 0.10.15](docs/releases/v0.10.15-beta.md) ·
[Cobertura de idiomas](docs/IDIOMAS.md) ·
[Pruebas y límites](validation/LOCALIZATION_0.10.15_VALIDATION.md).

### Optimizaciones conservadas de 0.10.14 — panorámica y presentación

La panorámica reutiliza la resolución del mismo bloque de terreno entre
píxeles contiguos, mediante una caché de 128 bytes que se renueva por fila y
fotograma. El servicio de 120 solo consulta el reloj cuando tiene una imagen
adicional pendiente y atiende su plazo sin la compuerta adicional de 1 ms.

Las optimizaciones se aplican automáticamente. Conservan la frecuencia del
juego, los filtros, la resolución, los límites panorámicos y el rechazo de
imágenes tardías. Las mediciones de componentes y SDL software se distinguen
de los FPS, la batería y la temperatura de un teléfono; estos últimos siguen
pendientes de prueba física.

[Notas de 0.10.14](docs/releases/v0.10.14-beta.md) ·
[Pruebas y alcance](validation/PERFORMANCE_0.10.14_VALIDATION.md) ·
[Guía de rendimiento](docs/RENDIMIENTO_Y_FPS.md).

### Mejora conservada de 0.10.13 — bordes panorámicos decorativos

**Panorámica (beta)** prolonga el terreno en los extremos exteriores del mapa
para evitar que el suelo o una pared de piedra terminen bruscamente junto a
un espacio vacío. Usa una franja de 32 píxeles del propio terreno, con sus
colores, transparencias y textura; el cielo y los fondos conservan su dibujo.
Se aplica automáticamente al seleccionar **Atrás → Imagen → Panorámica (beta)**.

El añadido es decorativo: **no amplía la zona jugable ni elimina las paredes
invisibles**. Se limita a los extremos comprobados del mapa; no rellena huecos
interiores ni deduce paredes a partir de un límite de cámara. Las bandas de
protección de Fuzzy, las transiciones y las escenas sin cobertura se mantienen.

[Notas de 0.10.13](docs/releases/v0.10.13-beta.md) ·
[Pruebas y límites](validation/PANORAMA_EDGES_0.10.13_VALIDATION.md) ·
[Guía de imagen](docs/CALIDAD_IMAGEN.md#usar-panorámica-beta).

### Optimizaciones conservadas de 0.10.12 — rendimiento del modo 120

Se reduce trabajo repetido del reloj de vídeo y de los filtros. El planificador
recupera antes las imágenes adicionales después de un tirón aislado; mantiene
el rechazo cuando la carga no cabe y conserva la velocidad original del juego.
El objetivo de mezcla 2× sigue siendo aproximadamente **119,45 presentaciones/s**.

[Notas de 0.10.12](docs/releases/v0.10.12-beta.md) ·
[Pruebas y límites](validation/PERFORMANCE_0.10.12_VALIDATION.md) ·
[Guía de 60/120 FPS](docs/RENDIMIENTO_Y_FPS.md).

### Funciones conservadas de 0.10.11 — animaciones y «Yo shi pude»

**Atrás → Logros** contiene **26 logros locales**. La nueva animación muestra
un huevo dentro de una medalla verde que se despliega con **Logro desbloqueado**
y el nombre del reto. Los premios se muestran uno a uno, sin pausar el juego
ni interceptar los controles. Se puede desactivar desde **Animación al desbloquear**.

**Yo shi pude** exige completar una partida nueva, sus 48 niveles principales
y el final sin que un golpe o enemigo separe a Bebé Mario de Yoshi en ningún
mapa. Recuperarlo no elimina la pérdida; las entregas entre Yoshis y las
transformaciones normales no cuentan como daño. Un panel bajo el resumen
muestra la validez del intento y su progreso.

La suspensión automática permite continuar un intento verificado después
de cerrar la app en segundo plano. Los estados manuales y partidas antiguas
no acreditan este recorrido. Los **25 logros anteriores, sus fechas y la
preferencia de avisos** se conservan al actualizar sin borrar datos.

También se corrige un cierre reproducido al iniciar los créditos: se añade
la rutina gráfica que el juego copia a RAM y se comprueba antes de ejecutarla
como código nativo. [Corrección y pruebas](validation/CREDITS_0.10.11_VALIDATION.md).

[Guía de los 26 logros](docs/LOGROS_LOCALES.md) ·
[Notas de 0.10.11](docs/releases/v0.10.11-beta.md) ·
[Pruebas y límites](validation/LOCAL_ACHIEVEMENTS_0.10.11_VALIDATION.md).

### Catálogo ampliado en 0.10.10

0.10.10 amplió los diez primeros logros a 25, con umbrales de colecciones,
niveles distintos, puntuaciones perfectas y cinco vehículos individuales.
Los retos reconocidos desde evidencia local anterior conservaron su fecha
de reconocimiento y los primeros diez mantuvieron sus fechas originales.
[Notas de 0.10.10](docs/releases/v0.10.10-beta.md).

### Logros locales incorporados en 0.10.9

**Atrás → Logros** añade diez retos con progreso, fecha de obtención y
avisos de desbloqueo opcionales. Incluye flores, monedas rojas, estrellas,
transformaciones, recuperación de Fuzzy, niveles distintos y cien puntos.
Los filtros **Todos / Logrados / Pendientes** ayudan a consultar el catálogo.

El registro es local, independiente de la partida y se conserva al actualizar
sin borrar datos. No requiere cuenta ni Internet. Cargar un estado o un récord
antiguo no concede premios automáticamente; se observan las acciones del juego.

[Guía de logros](docs/LOGROS_LOCALES.md) ·
[Notas de 0.10.9](docs/releases/v0.10.9-beta.md) ·
[Pruebas y límites](validation/LOCAL_ACHIEVEMENTS_0.10.9_VALIDATION.md).

### Color de Yoshi incorporado en 0.10.8

**Atrás → Mods → Color de Yoshi** abre un mod incorporado con **48 colores
predefinidos**, barras **RGB** y entrada de códigos **HEX** (`#RRGGBB` o
`#RGB`). Muestra el color y sus sombras antes de aplicarlo, guarda la elección
y permite volver al color original. Se puede combinar con un perfil global
de color y los filtros existentes.

El cambio se realiza sobre los tonos del cuerpo del personaje durante el
dibujo. No modifica la ROM ni los datos de la partida. El selector viene
incluido en el APK y está desactivado inicialmente.

[Cómo usar el mod](docs/MOD_COLOR_YOSHI.md) ·
[Notas de 0.10.8](docs/releases/v0.10.8-beta.md) ·
[Pruebas y límites](validation/YOSHI_COLOR_0.10.8_VALIDATION.md).

### Presentación de 120 conservada de 0.10.7

**0.10.7 corrige la distribución del trabajo que podía mantener la presentación
cerca de 60 FPS aunque Android informara 120 Hz.** El modo 120 presenta una
mezcla en el plazo del juego y la imagen completa medio periodo después,
aprovechando puntos de servicio durante el cálculo del siguiente fotograma.
Se conserva una sola espera del juego por periodo y el dibujo permanece en
el mismo hilo de SDL. La imagen completa sale unos 8,37 ms después que con la
ruta anterior; el menú explica esta compensación de latencia.

El presupuesto incluye el trabajo observado, preparación, presentación y
esperas. Si falta tiempo, se reducen las imágenes adicionales para proteger
la velocidad del juego. Pausas, cambios de tamaño y reinicios descartan las
imágenes pendientes. **Mostrar FPS** añade base/extra y tiempos de preparar
y presentar para comprobar el resultado real en el teléfono.

[Notas de 0.10.7](docs/releases/v0.10.7-beta.md) ·
[Pruebas y límites](validation/PERFORMANCE_0.10.7_VALIDATION.md) ·
[Uso del contador](docs/RENDIMIENTO_Y_FPS.md).

### Mejoras conservadas de 0.10.6

**0.10.6 comprueba si una imagen intermedia de 120 FPS puede llegar a tiempo
antes de prepararla.** Si no queda margen, evita la mezcla y el trabajo de
renderizado que se descartarían y da prioridad al siguiente fotograma
completo. También reduce el coste de Scale2x/Scale3x, de los perfiles de color
diagonales y de las pasadas de escalado entero exacto, conservando los
píxeles, el filtro y la calidad seleccionados.

Las pruebas comparan el resultado visual y el coste de componentes en host;
no garantizan FPS estables en el Pixel 7 Pro ni sustituyen una prueba física.
[Validación de rendimiento](validation/PERFORMANCE_0.10.6_VALIDATION.md) ·
[Equivalencia y coste del renderizado](validation/RENDER_0.10.6_PERFORMANCE.md) ·
[Notas de 0.10.6](docs/releases/v0.10.6-beta.md).

### Presentación, controles y actualizaciones incorporados en 0.10.5

**Atrás → Rendimiento y FPS** permite elegir **60 FPS** o **120 FPS ·
mezcla 2×**. La presentación prepara la imagen antes de esperar su momento
de salida, descarta los puntos intermedios que llegan tarde y restablece el
ritmo al reanudar o cambiar de modo. Solicita el refresco correspondiente a Android;
si este no concede 120 Hz, utiliza la presentación normal y conserva tu
preferencia. El contador distingue objetivo, presentación y Hz de pantalla.
La simulación conserva sus aproximadamente 59,73 Hz: no se puede garantizar
60/120 estables contra los límites del dispositivo o del sistema.

**Atrás → Controles → Símbolos y opacidad** añade iconos de acciones y una
alternativa de letras clásicas, más una barra de **20 a 100 %** de opacidad
(75 % inicialmente). Se corrige la ausencia de etiquetas del renderizador
Android: los símbolos y letras se dibujan sin depender de fuentes. El editor
de diseños usa la misma apariencia y conserva posiciones, tamaños y tres
diseños guardados.

**Atrás → Actualizar**, también accesible desde la pantalla inicial,
detecta nuevas versiones en la release **Descarga** de este repositorio.
La consulta automática se realiza al abrir o regresar a la app, con un
intervalo de seis horas. La descarga se inicia desde el apartado y Android
pide confirmar la instalación. Las versiones anteriores a 0.10.5 requieren
una primera actualización manual para obtener este apartado. Desde 0.10.5,
0.10.17 se detectará cuando el mantenedor adjunte su APK con el nombre indicado a Descarga.

[Rendimiento y límites](docs/RENDIMIENTO_Y_FPS.md) ·
[Controles](docs/AJUSTES_Y_DISENOS.md) ·
[Uso y publicación de actualizaciones](docs/ACTUALIZACIONES.md) ·
[Notas de 0.10.5](docs/releases/v0.10.5-beta.md).

### Mods y cinematográfico incorporados en 0.10.4

**0.10.4 añade Atrás → Mods**, con importación de paquetes `.gbamod`, lista
de instalados, activación, desactivación y eliminación. Incluye **Instalar
ejemplo → Tono cálido** para probar la función desde el propio APK. Los
paquetes se instalan desactivados; solo se aplica un perfil de color a la vez
y la selección se conserva entre sesiones.

Esta primera integración admite **perfiles de color compatibles con SMA3**.
Los parches de ROM, niveles, personajes y mods de otros juegos no son formatos
admitidos por este apartado. La [guía de mods](docs/MODS.md) incluye el formato
exacto y una herramienta para crear paquetes a partir de texto editable.

En **Atrás → Imagen → Filtros de imagen** se añade **Cinematográfico**:
dos franjas negras fijas, arriba y abajo, sobre el escalado Equilibrado. Cada
franja cubre el 10 % de la altura del juego, redondeado hacia abajo. **Oculta
parte de la imagen y puede cubrir indicadores del juego**; conserva la escala
del centro y deja visibles los controles táctiles. Es independiente del modo
Original, Panorámica o Estirar y no amplía el campo de visión.

[Guía de filtros](docs/CALIDAD_IMAGEN.md#filtros-de-imagen-en-0104) ·
[Guía de mods](docs/MODS.md) · [Notas de 0.10.4](docs/releases/v0.10.4-beta.md) ·
[Pruebas de mods](validation/MODS_0.10.4_VALIDATION.md) ·
[Pruebas del filtro](validation/CINEMATIC_0.10.4_VALIDATION.md).

### Filtros incorporados en 0.10.3

**0.10.3 añade seis filtros seleccionables en Atrás → Imagen → Filtros de
imagen:** Equilibrado, Píxel nítido, Suave bilineal, Scale2x, Scale3x y CRT
suave. Cada opción explica su aspecto y se guarda al elegirla. Equilibrado
es el valor inicial y conserva la presentación de la versión anterior.

Los filtros trabajan sobre la imagen terminada; no cambian la lógica del
juego, el encuadre elegido ni los controles. Scale2x y Scale3x suavizan las
escaleras de algunos contornos usando los colores vecinos. CRT suave añade
líneas estáticas de estilo retro, sin un efecto temporal de parpadeo.

[Guía de filtros](docs/CALIDAD_IMAGEN.md#filtros-de-imagen-en-0103) ·
[Notas de 0.10.3](docs/releases/v0.10.3-beta.md) ·
[Pruebas y límites](validation/ANDROID_0.10.3_FILTER_VALIDATION.md).

### Correcciones e interfaz incorporadas en 0.10.2

**0.10.2 incorpora las rutinas nativas que faltaban al tocar un Fuzzy, corrige
otros casos de parpadeo lateral y renueva la interfaz de ajustes.** La opción
se llama ahora **Panorámica (beta)** y muestra una advertencia de fase temprana:
todavía puede contener errores visuales o de compatibilidad.

- El efecto original del Fuzzy se conserva. Sus dos rutinas ARM se ejecutan
  como código recompilado después de verificar los bytes y la asignación
  activa; no se activa el intérprete para ocultar el fallo.
- Un objeto ordinario sin posición lateral resuelta ya no apaga todo el
  escenario ampliado. Ese objeto queda limitado al encuadre original.
- Los fondos BG3 completos compatibles usan el mapa actualizado por el
  juego para conservar las animaciones y las modificaciones de texturas.
- Durante el efecto de desplazamiento de terreno del Fuzzy, se muestra
  temporalmente el centro original con bandas. La panorámica vuelve cuando
  el juego restaura sus mapas; el efecto y la lógica del juego se conservan.
- Los ajustes usan tarjetas, descripciones, submenús y navegación clara.
  El icono de la aplicación es ahora un huevo blanco con manchas verdes.

[Notas de 0.10.2](docs/releases/v0.10.2-beta.md) ·
[Correcciones y validación](validation/ANDROID_0.10.2_FIXES.md) ·
[Nueva interfaz](validation/ANDROID_0.10.2_SETTINGS.md).

La panorámica sigue en desarrollo y conserva los límites originales de
aparición de objetos. Las pruebas documentadas no equivalen a recorrer todo
el juego ni a ejecutar este APK físicamente en el Pixel 7 Pro.

### Corrección anterior de presentación en 0.10.1

**0.10.1 corrige el titileo y la corrupción de texturas reproducidos en la
presentación panorámica de 0.10.** Mantiene estable la anchura durante cada
escena y conserva la imagen terminada al cambiar de tamaño o cargar una
partida. La prueba que mostraba seis centros corruptos en 600 fotogramas
pasa ahora con los 600 centros idénticos a Original, sin cambios de anchura.
También se verifica la ruta de mezcla temporal y los cambios reales de tamaño.
[Notas de 0.10.1](docs/releases/v0.10.1-experimental.md) ·
[Diagnóstico y pruebas](validation/ANDROID_0.10.1_VISUAL_FIXES.md).

La entrega continúa siendo experimental: conserva los límites de aparición
de objetos de 0.10 y todavía requiere comprobación física en el teléfono.

### Integración inicial de la panorámica en 0.10

- **0.10:** incorpora en Android el modo **Panorámica (experimental)**, que dibuja escenario adicional sin deformar la imagen en las escenas compatibles. El ancho se adapta a la superficie, hasta **356 × 160**, y conserva la vista original cuando faltan datos suficientes. Se activa en **Atrás → Imagen**; no se activa automáticamente al actualizar. [Notas de versión](docs/releases/v0.10-experimental.md) · [Compilación y pruebas](validation/ANDROID_0.10_VALIDATION.md).
- El APK está compilado y firmado con el mismo certificado que 0.9/0.9.1, preparado para actualizar la aplicación. Su subida a GitHub Releases corresponde al mantenedor. **La prueba física de 0.10 en el Pixel 7 Pro y la cobertura del juego completo siguen pendientes.**

### Versiones anteriores

- **0.9.1:** corrige el redondeo del escalador al conservar la proporción original **3:2** y aclara la ayuda de imagen en español, inglés y portugués. El APK está compilado y entregado; su entrega original se documenta en las notas históricas. [Notas de versión](docs/releases/v0.9.1-experimental.md).
- **Reporte de Edwin, 18 de octubre de 2026:** probó este APK en un **Google Pixel 7 Pro** y confirmó que funciona. No especificó la versión de Android, la duración de la prueba, los FPS ni las funciones comprobadas. [Verificación del APK y alcance de la prueba](validation/ANDROID_0.9.1_VALIDATION.md).

- **0.9:** ajustes agrupados en Rendimiento, Imagen, Controles e Idiomas; salto de la animación de BIOS; editor de posición/tamaño de botones con tres diseños guardables. [Guía](docs/AJUSTES_Y_DISENOS.md).
- **Reporte de Edwin, 5 de octubre de 2026:** 0.8 alcanza 120 FPS en su Pixel 7 Pro. Es un reporte del usuario; no constituye una validación del editor y la navegación incorporados en 0.9.

- **0.8:** idioma independiente del menú (ES/EN/PT), solicitud del modo de pantalla de 120 Hz y recuperación de retrasos moderados del temporizador. El usuario confirmó 120 FPS; no se ha medido independientemente su estabilidad prolongada. [Cambios y prueba en el teléfono](docs/MENU_Y_120FPS.md).

- **0.5:** cobertura nativa de la rutina de transformación, mapeo de mando y opción para ocultar los controles táctiles. [Detalles](docs/CORRECCION_TRANSFORMACIONES.md).
- **0.6:** escalado de presentación Automático, 720p y 1080p, con opción para estirar la imagen. Los gráficos originales siguen siendo de 240 × 160. [Detalles](docs/CALIDAD_IMAGEN.md).
- **0.7:** incorporó inglés, español latinoamericano y portugués de Brasil, con 187 entradas por idioma y 61 nombres de niveles. En esa versión los créditos y rótulos gráficos seguían en inglés; 0.10.15 amplía esa cobertura. [Cobertura actual](docs/IDIOMAS.md).
- Se conservan el contador de FPS y la mezcla temporal opcional de 120 FPS de 0.4. No son 120 estados independientes del juego; puede producir estelas. [Límites](docs/INTERPOLACION_120.md).

Abre las opciones con **Atrás del teléfono**. Los ajustes se conservan entre sesiones.

### Imagen completa sin deformaciones

En **0.10.16**, abre **Atrás → Imagen → Modo de imagen**:

| Opción | Resultado |
| --- | --- |
| **Original (3:2)** | Conserva los **240 × 160** originales, centrados y con bandas cuando hacen falta. Predeterminada en una instalación nueva. |
| **Panorámica (beta)** | Muestra más escenario y remata los extremos exteriores del mapa con terreno decorativo. Conserva las proporciones y los límites jugables; usa bandas durante efectos o escenas sin cobertura. Incluye un aviso de fase temprana. |
| **Estirar a pantalla completa** | Llena el área disponible deformando el mismo fotograma original; no añade escenario. |

La proporción original es **3:2**, porque 240 ÷ 160 = 1,5. En **0.9 y 0.9.1**,
conservarla se elige desactivando la casilla **Estirar a pantalla completa**;
esas dos versiones **no incluyen panorámica real**. Al actualizar a 0.10 se
respeta la elección anterior entre Original y Estirar.

[Guía de imagen y pruebas del escalador](docs/CALIDAD_IMAGEN.md) ·
[Cobertura y límites de la panorámica](docs/PANORAMICA_EN_DESARROLLO.md).

## Descargar e instalar

### APK 0.10.16 beta — nueva entrega

Archivo: **`sma3-recomp-0.10.16-beta-arm64.apk`**, versión `0.10.16-beta`,
código **27**, ARM64 y Android 9 o posterior. Añade contrarreloj por nivel,
récords personales y preparación de reportes de errores.

Instálalo encima de la versión anterior, sin desinstalarla ni borrar datos.
Conserva el paquete y el certificado y aumenta el código de versión. Desde
0.10.5 puedes usar **Actualizar → Buscar ahora** cuando el mantenedor haya
publicado el archivo con el nombre admitido.
[Instalación y datos del APK](docs/releases/v0.10.16-beta.md).

### Publicación verificada: APK 0.10.13 beta

En la consulta de preparación de 0.10.16, la release pública
**Descarga** contiene
[**sma3-recomp-0.10.13-beta-arm64.apk**](https://github.com/Edwin2929/sma3-recomp-android./releases/download/Descarga/sma3-recomp-0.10.13-beta-arm64.apk).
Su tamaño es **28.017.944 bytes** y su SHA-256 coincide con el APK entregado:
`37f46ea4b1826639dec53081b9dd198cc057539501f18ed2b0bc43ef7c51e6da`.
El nombre coincide con el patrón del actualizador. Para esta nueva entrega
conserva exactamente **`sma3-recomp-0.10.16-beta-arm64.apk`**.

El APK 0.10.16 aparecerá en
[Descarga](https://github.com/Edwin2929/sma3-recomp-android./releases/tag/Descarga)
cuando el mantenedor lo adjunte manualmente. Publicar el código en `main`
no publica el APK. Las indicaciones de disponibilidad en notas anteriores
corresponden al momento de cada entrega.

## Estado de validación

**0.10.16:** pruebas del reloj y sus estados, récords y guardado resistente
a fallos; interfaz Android en tres idiomas; edición, copia y preparación
manual de reportes. El informe distingue lógica controlada, ejecución nativa,
paquete Android y prueba física pendiente. Disponer de 60 entradas no implica
haber completado los 60 niveles en esta validación.
[Validación y alcance](validation/FEATURES_0.10.16_VALIDATION.md) ·
[APK](validation/android-0.10.16-build.json).

**0.10.15:** recursos contrastados con ROM/disassembly, conservación de textos
anteriores, anchos y acentos de créditos, rótulos y campos numéricos, lectura
de bancos y selección de idioma. El informe distingue pruebas de componentes,
ejecución nativa, paquete Android y comprobación física pendiente.
[Validación y alcance](validation/LOCALIZATION_0.10.15_VALIDATION.md) ·
[APK](validation/android-0.10.15-build.json).

**0.10.14:** comparación diferencial del proveedor de terreno, sondeo con
reloj controlado y cancelación, replays nativos contra 0.10.13 y medición de
intervalos entre llamadas SDL completadas. El informe separa los resultados
de componente, juego nativo, paquete Android y pruebas físicas pendientes.
[Validación y alcance](validation/PERFORMANCE_0.10.14_VALIDATION.md) ·
[APK](validation/android-0.10.14-build.json).

**0.10.13:** pruebas de coordenadas, caché y volteos del relleno, comparaciones
del centro original y de los estados del juego en ambos extremos del mapa,
movimiento y transiciones. Se conserva la política de protección de Fuzzy y
las ventanas. No equivale a cubrir todas las paredes interiores ni a una
medición física en Pixel 7 Pro.
[Validación y alcance](validation/PANORAMA_EDGES_0.10.13_VALIDATION.md) ·
[APK](validation/android-0.10.13-build.json).

**0.10.12:** comparación diferencial del reloj PPU, recuperación del presupuesto
con reloj controlado, equivalencia de píxeles de los filtros y replays del juego
nativo. Las mediciones de SDL software en Linux se distinguen de la simulación
determinista y de las comprobaciones del APK. No se ha medido esta entrega en
un Pixel 7 Pro ni se certifican 120 FPS sostenidos en Android.
[Validación y alcance](validation/PERFORMANCE_0.10.12_VALIDATION.md) ·
[APK](validation/android-0.10.12-build.json).

**0.10.11:** pruebas del catálogo de 26 logros, migración de los registros de
0.10.9 y 0.10.10, animación, entrada táctil y ciclo de vida Android en
Robolectric. El detector nuevo observa el recorrido y conserva un comprobante
vinculado a la suspensión automática. Sus pruebas distinguen escenas reales,
fixtures preparados y casos unitarios; no equivalen a haber completado una
campaña entera sin daño. No se ha instalado esta entrega en un teléfono ni
medido sus FPS en Android.
[Validación y alcance](validation/LOCAL_ACHIEVEMENTS_0.10.11_VALIDATION.md) ·
[APK](validation/android-0.10.11-build.json).

**0.10.10:** se valida el catálogo de 25 logros, sus umbrales y máscaras,
la actualización del registro de diez logros, conservación de fechas,
errores recuperables y la interfaz. El detector, el motor y el renderizado
nativos conservan los archivos de 0.10.9; sus replays mantienen su alcance
histórico y no se contabilizan como ejecuciones nuevas. No se ha probado
esta entrega en un teléfono ni medido sus FPS en Android.
[Validación y alcance](validation/LOCAL_ACHIEVEMENTS_0.10.10_VALIDATION.md) ·
[APK](validation/android-0.10.10-build.json).

**0.10.9:** pasan las pruebas del detector de logros, persistencia e interfaz
Android en Robolectric. Las reproducciones nativas confirman transformación,
recuperación de Fuzzy y confirmación de cien puntos, conservando los estados
y las imágenes de la versión anterior. El APK usa el mismo certificado.
No se ha probado esta entrega en un teléfono ni medido FPS en Android.
[Validación y alcance](validation/LOCAL_ACHIEVEMENTS_0.10.9_VALIDATION.md) ·
[APK](validation/android-0.10.9-build.json).

**0.10.8:** color selectivo comprobado en los compositores original y
panorámico, interfaz probada con Robolectric y partidas nativas con cinco
colores, transformación en coche y recuperación natural del efecto de
Fuzzy. Se corrige además un fallo previo en la selección del brillo de los
sprites. El APK ARM64 está compilado y firmado con el certificado anterior.
No se ha probado esta versión en un teléfono ni se han medido FPS en Android.
[Validación y alcance](validation/YOSHI_COLOR_0.10.8_VALIDATION.md) ·
[APK](validation/android-0.10.8-build.json).

**0.10.7:** reproducción determinista de la limitación, pruebas de cadencia,
imágenes pendientes, pausas/reinicios, geometría, cambios de escena y cargas
que exceden el presupuesto. La comparación con SDL software usa trabajo de
juego simulado y muestra los resultados de todos los tamaños ensayados;
no son FPS del Pixel. El APK incluye un contador ampliado para comprobar
presentaciones base/extra y costes de preparación e intercambio en el teléfono.
[Validación](validation/PERFORMANCE_0.10.7_VALIDATION.md) ·
[APK](validation/android-0.10.7-build.json).


**0.10.6:** se revisa la admisión de imágenes intermedias antes del trabajo
de mezcla y renderizado. Los kernels Scale2x/Scale3x y los perfiles de color
pasan oráculos de equivalencia y ASan/UBSan; el escalador conserva todos los
bytes de salida en **714 presentaciones pareadas** contra 0.10.5. Las medidas
de coste separan kernels y SDL software y no se convierten en afirmaciones
de FPS Android. La construcción y la prueba física del APK tienen su propio
alcance en las notas de la entrega.
[Rendimiento](validation/PERFORMANCE_0.10.6_VALIDATION.md) ·
[Renderizado](validation/RENDER_0.10.6_PERFORMANCE.md) ·
[Notas de la entrega](docs/releases/v0.10.6-beta.md).

**0.10.5:** pruebas de temporización y política de pantalla, renderizado
real de controles con SDL software, opacidad, geometría de entrada y cambios
concurrentes; comprobaciones del actualizador con respuestas reales y
metadatos futuros simulados. La pila de doce parches se aplica desde sus
trece estados reconocidos, de forma idempotente, y protege ocho archivos
del motor frente a cambios ajenos. La compilación final verifica Java,
ARM64, enlaces JNI, manifiesto, firma y paquete.

Durante la entrega 0.10.5 no se midió la estabilidad de FPS ni se ejecutó el
instalador de actualizaciones en un teléfono. La evidencia de Fuzzy,
panorámica, filtros y mods de versiones previas mantiene su alcance histórico.
[Rendimiento](validation/PERFORMANCE_0.10.5_VALIDATION.md) ·
[Controles](validation/CONTROLS_0.10.5_VALIDATION.md) ·
[Actualizador](validation/UPDATES_0.10.5_VALIDATION.md).

**0.10.4:** se comprueban el catálogo de mods, los archivos incompatibles y
dañados, la persistencia, la recuperación tras errores y la aplicación y
restauración de colores. El filtro Cinematográfico pasa **108 casos** y los
seis filtros previos conservan **648 presentaciones exactas** respecto a
0.10.3. El selector de siete filtros pasa **2.976 presentaciones SDL**. La
compilación Java y los enlaces JNI se verifican junto con el APK final.
No se atribuye una prueba física en el Pixel ni nuevos recorridos completos
del juego a esta entrega.
[Mods y alcance](validation/MODS_0.10.4_VALIDATION.md) ·
[Cinematográfico](validation/CINEMATIC_0.10.4_VALIDATION.md) ·
[Interfaz](validation/ANDROID_0.10.4_MODS_SETTINGS.md).

**0.10.3:** las pruebas se centran en los filtros y su presentación, el
comportamiento predeterminado, los cambios de tamaño y selección y el APK
resultante. La evidencia histórica de Fuzzy y panorámica se conserva abajo;
no se atribuyen nuevos recorridos completos de juego a esta entrega.
[Informe de filtros y alcance](validation/ANDROID_0.10.3_FILTER_VALIDATION.md).

**0.10.2:** el fallo de Fuzzy se reproduce en el binario anterior y el candidato
completa contacto, mareo, recuperación natural y continuación normal. También
se comprueba cargar una partida durante el efecto. Las dos rutinas nuevas
pasan **144 escenarios de equivalencia ARM**. La presentación SDL conserva
**4.200 centros exactos** en tres recorridos de 900 fotogramas y uno de 1.500;
el oráculo de mapas corrige **7.300 píxeles laterales incorrectos** del caso
reproducido de BG3. Se mantienen las restricciones de iluminación y las
bandas temporales durante Fuzzy. Java, recursos y APK final están verificados.
[Informe de 0.10.2 y alcance](validation/ANDROID_0.10.2_FIXES.md).

**0.10.1:** el recorrido de producción compara cada fotograma: **600 centros
idénticos**, frente a seis corruptos en 0.10, y ninguna oscilación de anchura.
Una segunda secuencia cambia 356 → 240 → 347 → 356 sin alterar ninguno de los
600 centros. La ruta real de mezcla temporal, compilada para host, conserva
600 presentaciones completas y 600 intermedias correctas. Pasan **87 casos
de redimensionado/restauración**, **40 de ciclo de vida**, **488 de ventanas**
y **169 pruebas Python** normales y con `-O`. La compilación Android y la
firma se verifican por separado; estas pruebas no miden FPS del Pixel.
[Informe de la corrección](validation/ANDROID_0.10.1_VISUAL_FIXES.md).

**0.10:** compilación ARM64 completa, firma, identidad del paquete, contenido
y enlaces JNI comprobados. Las ejecuciones nativas emparejadas recorrieron
**6.801 fotogramas del castillo** y **3.506 del ciclo normal → coche → normal**.
En los puntos de captura coinciden **311 centros de imagen y 313 estados**,
con las excepciones de comparación documentadas. Frente a la revisión anterior
se conservan **622 imágenes completas y 626 estados** en esos recorridos.

La ruta de presentación de producción pasa siete casos SDL: Original,
panorámica de 347 y 356 píxeles de ancho, vertical, fuente de Estirar y una
transición no admitida en ambos modos. También pasan **40 casos de ciclo de
vida**, **488 casos del compositor de ventanas** y **169 pruebas Python**
tanto normales como con `-O`. Son comprobaciones de desarrollo; **no acreditan
FPS ni funcionamiento físico de 0.10 en el teléfono**.
[Informe, reproducción y límites](validation/ANDROID_0.10_VALIDATION.md).

**0.9.1:** la compilación ARM64 terminó correctamente y se verificaron la firma,
su coincidencia con 0.9, la integridad y alineación del APK, los textos de ayuda
en ES/EN/PT y la correspondencia de las bibliotecas empaquetadas con esta
compilación. El APK no incorpora ROM, BIOS ni clave de firma.
La confirmación de Edwin del **18 de octubre de 2026** aporta una prueba de
funcionamiento en **Google Pixel 7 Pro**. No equivale a una medición de rendimiento
ni a una validación específica de cada ajuste, orientación o transformación.
[Registro completo y comprobaciones pendientes](validation/ANDROID_0.9.1_VALIDATION.md).

Edwin reportó 60 FPS con una versión anterior en Android y posteriormente identificó su teléfono como Google Pixel 7 Pro. Ese reporte **no valida el rendimiento de 0.5–0.7 ni confirma 120 FPS**.

Las pruebas de desarrollo cubren la rutina de transformación, el escalador y los punteros de traducción. Se ejecutaron 7.200 fotogramas nativos por idioma sin faltas de despacho ni instrucciones interpretadas. Siguen pendientes las comprobaciones específicas de estas funciones en el teléfono, todas las transformaciones y el juego completo. [Resultados y límites](docs/VALIDACION.md).

### Historial de validación panorámica anterior a 0.10

Los hitos de esta sección preceden a la integración Android de 0.10. Conservan
su alcance original; el estado actual se documenta en el informe de arriba.

Se ha verificado una transición natural por tubería del corredor inferior a la
zona superior del nivel 3/subnivel 3, comparando **240 × 160 y 356 × 160** desde
la misma entrada sintética. La ruta continua de **3.907 fotogramas** cubre ese
recorrido y una sola salida; no certifica el castillo completo ni el rendimiento.
La panorámica real sigue fuera de los APK Android **0.9 y 0.9.1**.
[Resultado y límites de la transición](validation/CASTLE_TRANSITION_VALIDATION.md).

**Reconstrucción del 23 de octubre de 2026 (UTC):** se recuperaron los
observadores de la nube puente, el objeto compuesto `0x064`, las piezas afines
`0x076/0x077` y las copias de objetos para ventanas. La nueva ruta continua de
**130 acciones y 6.801 fotogramas** conserva la primera tubería y atraviesa una
segunda, del subnivel 3 al 153. La pareja corregida supera el contrato de
recorrido, salud y cobertura: **250 centros RGB idénticos, 251 comparaciones de
estado equivalentes y cero huecos de posiciones**. La regresión del ciclo
normal → coche → normal también pasa con esta compilación. Las cinco pruebas
de C++ pasan con ASan/UBSan y las 166 pruebas Python pasan normal y con `-O`.

Esa reconstrucción calificó el recorrido y la procedencia de los objetos,
pero dejó la máscara OBJ-window limitada a los 240 píxeles originales.
[Resultados nuevos, procedencia y límites](validation/CASTLE_POST_PIPE_VALIDATION.md).

**Iluminación lateral, 23 de octubre de 2026 (UTC):** el prototipo amplía ahora
la máscara OBJ-window a los márgenes cuando todas las posiciones de esa fila
están verificadas y no intervienen WIN0/WIN1. La nueva pareja de recorridos
conserva **250 centros RGB idénticos, 251 estados equivalentes y cero huecos de
posiciones**. La comparación con la revisión anterior solo cambia los márgenes
de nueve capturas de la sala iluminada; desaparecen las franjas negras de
precaución y se conserva la forma de la luz. Pasan las pruebas del compositor
real con ASan/UBSan y la regresión del coche. Los tipos de ventana todavía no
admitidos mantienen la política anterior. Este avance continúa siendo de
escritorio; no certifica el castillo completo, un APK panorámico o los FPS.
[Pruebas de la máscara, reproducción y límites](validation/WINDOW_MASK_VALIDATION.md).

## Qué significa «nativo» aquí

El código de CPU del juego y de la BIOS se traduce a C++ y se compila para el dispositivo. El motor sigue reproduciendo el funcionamiento del hardware de GBA. No es una reescritura completa del juego independiente de ese hardware.

El inicio activa `GBARECOMP_STRICT_STATIC=1`, `GBARECOMP_FORCE_INTERP=0`, `GBARECOMP_BIOS_HLE=0` y `GBARECOMP_BIOS_SKIP_INTRO=1`. Una rutina no cubierta detiene la ejecución; no se utiliza el intérprete para ocultar fallos de cobertura.

## Contenido del repositorio

| Ruta | Contenido |
|---|---|
| `src/` | Inicio nativo y selección de rutinas copiadas a RAM |
| `android/` | Interfaz, opciones de FPS, navegación Atrás y configuración Gradle |
| `game.toml`, `ram.toml`, `bios.toml` | Configuración de recompilación |
| `runtime.toml` | Configuración de ejecución |
| `symbols.tsv` | Direcciones y nombres usados como semillas |
| `tools/` | Generación local, compilación y comprobaciones |
| `mods/examples/` | Perfiles de color de ejemplo, editables y empaquetables como `.gbamod` |
| `validation/*.csv` | Secuencias de entradas para las pruebas |
| `licenses/` | Licencias de las dependencias |

**No se incluyen ROM, BIOS, código generado a partir de esos archivos, APK, partidas guardadas, claves de firma ni cachés de compilación.** Cada persona genera los archivos necesarios localmente. El repositorio no requiere los ZIP privados usados durante el desarrollo.

## Requisitos

- Linux con Python 3, Git, curl, JDK 17, compilador C++20 y CMake 3.22 o posterior.
- Android de destino: **Android 9/API 28 o posterior, ARM64**.
- Herramientas fijadas: Gradle 8.9, Android SDK 35, NDK 27.1.12297006, CMake del SDK 3.22.1.
- La compilación completa necesita varios GB de espacio y memoria; tarda más que volver a empaquetar bibliotecas ya compiladas.

## Compilar desde cero

Mantener el motor y este repositorio como carpetas hermanas:

```text
carpeta-de-trabajo/
  gbarecomp/
  sma3-recomp/
```

Desde `carpeta-de-trabajo`:

```sh
git clone https://github.com/mstan/gbarecomp.git
git -C gbarecomp checkout e7728148c6829ba526f682876430a0c9022dc6c0
git -C gbarecomp submodule update --init external/arm-recomp-core external/rbengine external/recomp-net platform/android/third_party/SDL
cmake -S gbarecomp -B gbarecomp/build -DCMAKE_BUILD_TYPE=Release
cmake --build gbarecomp/build --target gba_recompile -j2
git clone https://github.com/KarisaAdvynia/sma3-disasm.git
git -C sma3-disasm checkout c8532ec9a8d0038c3bfeb003dd0c7ea89d7e1071
cd sma3-recomp
python3 tools/generate_sources.py --rom /ruta/sma3-usa.gba --bios /ruta/gba_bios.bin --disasm ../sma3-disasm
python3 tools/bootstrap_android_build.py
```

El último script descarga las herramientas oficiales en `../build-tools`, prepara una clave **de desarrollo local** si no existe y ejecuta Gradle. No subas esa clave a GitHub. La primera configuración acepta las licencias del SDK mediante `sdkmanager`; revisa esas licencias antes de ejecutar el script si corresponde a tu entorno.

APK resultante: `android/app/build/outputs/apk/debug/app-debug.apk`.

Para compilar únicamente las bibliotecas nativas: `python3 tools/bootstrap_android_build.py --native-only`.

0.10.11 añade la rutina Thumb que el juego copia a una dirección dinámica
de RAM al iniciar los créditos. `generate_sources.py` también la genera.
Para actualizar un entorno con las demás fuentes ya preparadas:

```sh
python3 tools/generate_credits.py --rom /ruta/sma3-usa.gba --recompiler ../gbarecomp/build/gba_recompile
```

La compilación comprueba que estas fuentes existen. En ejecución, se verifica
el puntero publicado por el juego y cada byte de la copia antes de seleccionar
el código nativo. La corrección mantiene la ejecución estática y evita el
cierre observado al entrar en los créditos; no introduce un intérprete en el APK.

0.10.4 habilita el catálogo nativo de perfiles de color con
`GBARECOMP_ENABLE_MODS=ON` y añade un décimo parche al motor fijado. Gradle
genera el paquete de ejemplo desde `mods/examples/tono-calido` usando
`tools/package_color_mod.py` y lo incluye como un recurso separado de los
archivos de instalación del juego. El ejemplo solo se instala cuando el
usuario lo elige en Mods. [Formato y uso](docs/MODS.md).

0.10.2 añade las fuentes privadas de los dos efectos ARM de Fuzzy en
`generated-effects/`. `generate_sources.py` las genera como parte del flujo
completo. Al actualizar un entorno que ya tiene las demás fuentes generadas,
puedes añadirlas sin regenerar el juego completo:

```sh
python3 tools/generate_effects.py --rom /ruta/sma3-usa.gba --recompiler ../gbarecomp/build/gba_recompile
```

La compilación se detiene con una indicación explícita si faltan esas fuentes.
El APK 0.10.2 recompila las fuentes modificadas y las dos rutinas nuevas;
reutiliza el archivo ARM64 del juego de 0.10 con su hash comprobado.
[Procedencia de 0.10.2](validation/android-0.10.2-build.json).

Para 0.10.1 se recompilan las fuentes nativas modificadas y se reutiliza,
con hash verificado, el archivo del juego generado por la compilación ARM64
completa de 0.10. No se atribuye otra compilación de los 64 fragmentos a este
ensamblado incremental. [Procedencia de 0.10.1](validation/android-0.10.1-build.json).

Para el APK 0.10 se compilaron desde cero las bibliotecas nativas ARM64 y se
empaquetó la aplicación con las fuentes públicas actualizadas. Se reutilizaron
las fuentes generadas privadas del proyecto de desarrollo; **no se regeneró el
conjunto de fuentes a partir de la ROM y la BIOS en esta ejecución**. Los scripts
de regeneración se habían revisado estáticamente y ese flujo completo de
regeneración sigue sin repetirse como parte de esta compilación.
`SMA3_PANORAMA=ON` incluye la función opcional; `SMA3_WIDESCREEN_DIAGNOSTICS=OFF`
mantiene fuera del APK las capturas de diagnóstico exclusivas de escritorio.
[Detalle de la compilación y verificación](validation/ANDROID_0.10_VALIDATION.md).

## Archivos compatibles

| Archivo local | Tamaño | SHA-1 |
|---|---:|---|
| ROM USA | 4.194.304 bytes | `7352d2bd064d9ebaec579e264228aa21c7345b80` |
| BIOS GBA | 16.384 bytes | `300c20df6731a33952ded8c436f7f186d25d3492` |

En español, la app solicita ambos archivos con **Elegir ROM** y **Elegir BIOS**;
después se pulsa **Jugar**. El lanzador utiliza el idioma guardado del menú.

Una compilación realizada por otra persona tendrá otra firma. No podrá actualizar directamente los APK del desarrollador sin disponer de su misma clave. Conserva tu clave local para tus propias actualizaciones.

## Diagnóstico en Linux

```sh
python3 tools/rebuild_host.py --jobs 2
python3 tools/run_verified.py --rom /ruta/sma3-usa.gba --bios /ruta/gba_bios.bin --frames 18000 --timeout 240 --input validation/extended-gameplay.csv --save /tmp/sma3-prueba-nueva.sav --output /tmp/sma3-prueba.png
```

Usa una ruta de guardado nueva para repetir las condiciones iniciales. SDL2 es necesario para ventana y audio en Linux; sin él se permite el diagnóstico sin ventana. No se deben convertir los fotogramas simulados por esta prueba en una afirmación de FPS en Android.

## Pendientes

- Registrar versión de Android, APK exacto y duración de la prueba anterior de 60 FPS.
- Probar 0.10.16 en Pixel 7 Pro: cronómetro, récords, pausas, cancelación al cargar estados, formulario de errores, destinos reales de compartir y navegador; además de rótulos/créditos, panorámica, logros, suspensión, color de Yoshi, 60/120, actualizaciones, mods, filtros, Fuzzy, rotación, segundo plano, mandos y controles.
- Registrar versión de Android, duración, ajustes utilizados y mediciones de rendimiento de 0.10.14, comparando la zona de la captura de 65,2 FPS y sesiones largas con 0.10.13.
- Ampliar y validar los límites originales de aparición y dibujo de objetos; la panorámica 0.10 no los modifica.
- Completar los demás rótulos gráficos, botones ilustrados, estrellas/monedas/flores, el menú de suspensión y Mario Bros.
- Ampliar cobertura de niveles y transiciones.
- Documentar sonido, guardado, segundo plano y mandos físicos.
- Medir estabilidad de FPS en sesiones largas y en más dispositivos.

## Créditos y condiciones

Proyecto preparado para Edwin Rodríguez. Las dependencias conservan sus autores y licencias. GBARecomp usa PolyForm Noncommercial 1.0.0; arm-recomp-core usa MIT. Consulta [THIRD_PARTY.md](THIRD_PARTY.md) y `licenses/`. Las licencias del motor no conceden derechos sobre el juego ni sobre la BIOS. No existe afiliación con Nintendo.

Para subir este contenido: [docs/SUBIR_A_GITHUB.md](docs/SUBIR_A_GITHUB.md).



