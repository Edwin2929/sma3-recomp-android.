# Ajustes y diseños — Android 0.10.5 beta

El botón **Atrás** abre un panel oscuro con acentos verdes, tarjetas por
categoría y una descripción de cada opción. El contenido se puede desplazar
y el botón de navegación permanece separado, debajo del contenido. El ancho
se adapta a la pantalla, con un máximo de 600 dp; la altura se limita al área
disponible para facilitar el uso horizontal y vertical.

Hay seis apartados:

- **Imagen:** Original (3:2), Panorámica (beta) y Estirar a pantalla completa; submenús independientes de filtros y de resolución Automática, 720p y 1080p.
- **Mods:** importar perfiles de color `.gbamod`, instalar el ejemplo incluido, consultar sus datos, activarlos, desactivarlos y eliminarlos.
- **Rendimiento y FPS:** contador, objetivos de presentación de 60 y 120 FPS, estado de la pantalla y explicación de sus límites.
- **Controles:** visibilidad de botones táctiles, símbolos o letras clásicas, opacidad, mapeo de mando y diseños personalizados.
- **Idiomas:** idioma del menú e idioma del juego, independientes.
- **Actualizar:** detección de nuevas versiones en GitHub, descarga del APK y acceso al instalador de Android.

**Volver** o **Atrás** regresa al apartado anterior. **Volver al juego** cierra
Ajustes. Las selecciones se guardan al cambiarlas; no hace falta reiniciar.
Los textos del menú y del editor están en español, inglés y portugués.
La actualización conserva las claves anteriores de preferencias, las
asignaciones del mando y las tres ranuras de diseños.

## Imagen sin deformaciones

Abre **Atrás → Imagen → Original (3:2)**. Los gráficos originales
de GBA miden **240 × 160 píxeles**; la imagen se centra con bandas cuando la
superficie tiene otra proporción. La elección se aplica sin reiniciar y queda
guardada.

**Panorámica (beta)** dibuja escenario adicional donde hay cobertura,
hasta 356 × 160, y adapta el ancho al área disponible. Vuelve a la vista
original con bandas cuando la escena no reúne datos suficientes. Sigue
teniendo límites de aparición de objetos y efectos no cubiertos; no está
validada para todo el juego. La advertencia permanece visible en el apartado
Imagen, y se anuncia al lector de pantalla al seleccionar Panorámica:

> Es una fase temprana y todavía puede presentar errores visuales o de compatibilidad. Si encuentras un problema, usa Original (3:2).

**Estirar a pantalla completa** llena el área de juego con el mismo fotograma:
puede deformarlo y no amplía el campo de visión. Automática, 720p y 1080p
cambian el escalado de presentación; tampoco añaden escenario por sí solos.
Se configuran en **Atrás → Imagen → Resolución de salida**.

En **Atrás → Imagen → Filtros de imagen** hay siete acabados:
**Equilibrado**, **Píxel nítido**, **Suave bilineal**, **Scale2x**, **Scale3x**
y **CRT suave**, además de **Cinematográfico**. Cada uno incluye una explicación. La selección se guarda
al tocarla y aparece en el resumen de Imagen. Equilibrado es el valor
predeterminado y permite volver al acabado de la versión anterior.

Scale2x y Scale3x trabajan los contornos; bilineal suaviza toda la imagen;
Píxel nítido conserva los bordes duros. CRT añade líneas horizontales fijas
como efecto retro. Ninguno amplía el campo de visión ni añade detalle
original. Cinematográfico añade dos franjas negras del 10 % arriba y abajo:
cubren parte de la imagen y del HUD, con los controles táctiles por encima.
La [guía de filtros](CALIDAD_IMAGEN.md#filtros-de-imagen-en-0104)
explica las diferencias y el coste de cada opción.

La mezcla temporal de **Rendimiento y FPS** puede producir estelas. Mantiene
el ritmo original del juego; 60 FPS corresponde a unas 59,73 imágenes por segundo
y 120 FPS a unas 119,45 imágenes presentadas por segundo mediante mezcla 2×.
El modo de 120 solicita una pantalla de 120 Hz a Android. Si Android no concede
esa frecuencia, el juego conserva la presentación de 60 y la preferencia de 120
queda guardada. El panel muestra el modo informado de pantalla y la tasa
concedida a la aplicación. La frecuencia real depende del dispositivo y de su
configuración de energía.

En **0.9 y 0.9.1** se conserva 3:2 desactivando la antigua casilla **Estirar a
pantalla completa**. Al actualizar, esa elección pasa a Original o Estirar;
la panorámica debe seleccionarse expresamente y no existe en esos APK anteriores.
Consulta la [guía de imagen](CALIDAD_IMAGEN.md) y el
[estado de la panorámica](PANORAMICA_EN_DESARROLLO.md).

## Mods de color

Entra en **Atrás → Mods → Instalar ejemplo** para añadir **Tono cálido**.
Después toca su nombre y usa **Activar mod**. También puedes elegir
**Importar mod** y seleccionar un paquete `.gbamod` compatible desde el
selector de archivos de Android. La importación copia el archivo a la
aplicación y lo instala desactivado; no cambia el documento original.

El catálogo muestra nombre, versión, autor y estado. Los detalles permiten
activar, desactivar o eliminar el paquete. Activar otro perfil sustituye al
anterior; la elección se conserva al cerrar el juego. Eliminar solicita
confirmación con el nombre del mod. La copia y la validación se hacen fuera
del hilo del menú, con mensajes de progreso y errores.

Esta versión admite perfiles de color para SMA3. Los paquetes no modifican
la ROM ni las partidas. Hay un ejemplo editable y una herramienta de
empaquetado en el repositorio; la [guía de mods](MODS.md) describe los límites
y cómo crear más perfiles.

## Símbolos y opacidad de los controles

Entra en **Atrás → Controles → Símbolos y opacidad**. Puedes elegir
**Símbolos de acciones**, el estilo inicial, o **Letras clásicas**, que muestra
A, B, L, R, SELECT y START. La barra de opacidad va del **20 % al 100 %** y
comienza en **75 %**. El cambio se aplica y se guarda al moverla; también se
refleja en el editor de diseños. Para ocultar completamente los botones, usa la
opción independiente **Controles táctiles**.

| Control | Símbolo | Acción principal |
| --- | --- | --- |
| Cruceta | Cuatro flechas | Mover y mirar; abajo permite agacharse, tragar o hacer un salto bomba según el contexto. |
| A | Flecha ascendente | Saltar; mantener para aletear. |
| B | Lengua | Comer o escupir según lo que tenga Yoshi en la boca. |
| L | Mira | Fijar o liberar el cursor al apuntar un huevo. |
| R | Huevo con manchas | Apuntar y lanzar huevos. |
| SELECT | Lista | Abrir la pantalla de estado. |
| START | Dos barras | Pausar. |

La leyenda corresponde a los controles principales de Yoshi's Island descritos
en el [manual oficial de Nintendo](https://www.nintendo.com/eu/media/downloads/games_8/emanuals/game_boy_advance_8/Manual_GameBoyAdvance_YoshisIslandSuperMarioAdvance3_EN_DE_FR_ES_IT.pdf),
páginas impresas 6–7 de la sección inglesa (página 4 del PDF). Las acciones
pueden variar durante las transformaciones y en Mario Bros. El lanzamiento
respeta el estilo de control elegido dentro del juego. Cambiar los símbolos no
reasigna los botones.

Ambos estilos se dibujan con trazos vectoriales, por lo que las letras y los
símbolos son visibles aunque el backend SDL no disponga de una biblioteca de
fuentes. Los botones pulsados se distinguen por su color.

## Diseños personalizados

Controles → Diseños personalizados → Diseño 1, 2 o 3.
Arrastra la cruceta, A, B, L, R, Select y Start. Selecciona un control con el
desplegable si está detrás de otro, y cambia su tamaño con el deslizador.
La vista previa mantiene las proporciones del área segura de la pantalla.
Los límites impiden guardar controles completamente fuera de esa área.
El dibujo y las zonas táctiles del juego usan la misma geometría.
La vista previa utiliza el estilo, los símbolos, los colores y la opacidad del
juego; el contorno del control seleccionado conserva su intensidad para facilitar
el arrastre. Canvas y SDL pueden suavizar de distinta forma los bordes y las
esquinas.

- Guardar y usar conserva ese diseño y lo activa.
- Cancelar o Atrás descarta la edición sin cambiar el diseño activo.
- Restablecer recupera las posiciones originales dentro del editor; solo se
  guardan al pulsar Guardar y usar.
- Predeterminado vuelve al diseño automático y conserva las tres ranuras.

El juego se pausa durante el editor y se reanuda al cerrarlo. En los otros
menús se bloquean las entradas del juego, como en la versión anterior.
Los diseños personalizan posición y tamaño; no importan imágenes de botones.
Se conserva el formato anterior de **28 valores**: centro x/y y ancho/alto para
los siete controles. Las preferencias de apariencia se guardan por separado y
los tres diseños existentes siguen siendo compatibles.

## Actualizaciones

Abre **Atrás → Actualizar** para entrar en **Actualizaciones**.
**Buscar automáticamente** viene activado y
consulta la release **Descarga** del repositorio de GitHub al abrir o volver al
juego, como máximo cada seis horas. **Buscar ahora** permite consultar en ese
momento. Cuando encuentra un APK compatible más reciente, el apartado muestra
su versión y permite elegir **Descargar actualización**.

La descarga muestra el progreso y se puede cancelar. Cuando termina y se verifica
el archivo, **Instalar actualización** abre el instalador del sistema. Android
puede pedir permitir instalaciones desde SMA3 y solicita confirmar la instalación.
La detección es automática; la descarga y la instalación se inician desde este
apartado. Actualizar con el mismo paquete y certificado conserva los datos de la
aplicación.

## Inicio

La animación y el sonido de arranque de Game Boy se omiten mediante el salto
al estado posterior al arranque que proporciona el motor. La BIOS aportada
por el usuario sigue siendo necesaria para IRQ y servicios SWI. No se activa
la interpretación del juego ni se elimina la BIOS del funcionamiento interno.

## Reporte del usuario

El 5 de octubre de 2026 Edwin confirmó que 0.8 alcanza 120 FPS en su Pixel 7 Pro.
Se registra como reporte del usuario, sin una medición independiente ni una
duración de prueba documentada. 0.9 conserva ese modo de presentación.

## Validación actual

Los controles de 0.10.5 se validaron con el renderizador SDL real en superficies
horizontales, verticales y de dimensiones impares: ambos estilos, opacidad del
20/75/100 %, controles ocultos, un diseño personalizado y estados pulsados. Se
comprobó que la apariencia no cambia las zonas de entrada, que los símbolos
funcionan sin fuentes y que el dibujo conserva el estado SDL. Las pruebas
optimizadas y con AddressSanitizer/UndefinedBehaviorSanitizer aprobaron
**66.685 comprobaciones cada una**, incluidos 100 cuadros durante cambios
concurrentes de estilo y opacidad. El helper y el editor Java compilaron con el
SDK de Android API 35. Alcance, fuente de la leyenda y límites en el
[informe de controles de 0.10.5](../validation/CONTROLS_0.10.5_VALIDATION.md).

El gestor de mods y el selector de siete filtros de 0.10.4 se documentan
en la [revisión del menú](../validation/ANDROID_0.10.4_MODS_SETTINGS.md),
las [pruebas del catálogo](../validation/MODS_0.10.4_VALIDATION.md) y el
[informe cinematográfico](../validation/CINEMATIC_0.10.4_VALIDATION.md).
La evidencia de los seis filtros anteriores se conserva en el
[informe de 0.10.3](../validation/ANDROID_0.10.3_FILTER_VALIDATION.md).
La compilación y firma del APK constan en el
[registro de construcción](../validation/android-0.10.4-build.json).

Las correcciones de Fuzzy de 0.10.2 y los límites de la panorámica se
documentan en [ANDROID_0.10.2_FIXES.md](../validation/ANDROID_0.10.2_FIXES.md).
La compilación y revisión de preferencias, navegación y diseño están en el
[informe de la interfaz](../validation/ANDROID_0.10.2_SETTINGS.md).

La orientación Android conserva `fullUser`. Las pruebas SDL de tamaños y
las comprobaciones anteriores del editor no equivalen a una prueba física
de rotación, controles o segundo plano de 0.10.5. La vista previa Java se revisó
en código y se compiló; las capturas inspeccionadas corresponden al renderizador
SDL de software, no a una pantalla Android. Queda por comprobar el manejo
multitáctil, la persistencia tras reiniciar y la legibilidad sobre el juego en
un teléfono. La evidencia inicial de
integración de 0.10 permanece como historial en
[ANDROID_0.10_VALIDATION.md](../validation/ANDROID_0.10_VALIDATION.md).

## Historial de validación de 0.9

APK ARM64 compilado, ZIP y firma v2 verificados; mantiene el certificado
anterior. Java compilado y enlaces JNI del editor presentes. Pruebas de
geometría: movimiento, tamaño, límites, orientación, datos inválidos y retorno
al diseño predeterminado. Aplicación de parches desde motor limpio, repetición
e integridad ante ediciones ajenas: aprobadas.

Arranque nativo Linux con BIOS LLE e intro omitida: 7.200 fotogramas, cero
faltas de despacho, cero instrucciones interpretadas, cero reparaciones
nativas. Captura del tutorial revisada visualmente. Resultados en
`validation/boot-skip-test.txt` y `validation/touch-design-test.txt`.
El editor y los submenús todavía no se han probado en un teléfono físico.
