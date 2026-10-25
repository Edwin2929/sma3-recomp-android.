# Selector de filtros de imagen de Android 0.10.3

## Navegación y opciones

**Atrás → Imagen → Filtros de imagen** abre una lista de selección única con
seis opciones. Se conserva el diseño de ajustes de 0.10.2: tarjetas legibles,
contenido desplazable y botón de navegación fijo. La página Imagen y su
resumen en Ajustes muestran el filtro seleccionado.

| ID guardado | Opción en español | Explicación presentada |
| --- | --- | --- |
| 0 | Equilibrado (recomendado) | Píxeles definidos con bordes suavizados al adaptar la imagen. Es el valor predeterminado. |
| 1 | Píxel nítido | Bordes duros y aspecto pixelado; los escalones quedan más visibles. |
| 2 | Suave bilineal | Suaviza la imagen, con posible pérdida de definición en textos pequeños. |
| 3 | Scale2x | Escalado de 2× para reducir contornos escalonados. |
| 4 | Scale3x | Escalado de 3×; puede modificar detalles pequeños y requiere más procesamiento que Scale2x. |
| 5 | CRT suave | Líneas horizontales fijas de estilo retro; oscurece ligeramente la imagen. |

Los nombres, las descripciones y la ayuda están traducidos al español, inglés
y portugués. La ayuda distingue estos filtros de la ampliación del campo de
visión y de la resolución de salida: cambian el aspecto de la imagen y no
añaden detalle original. Identifica CRT como un efecto estético y recomienda
volver a Equilibrado o Píxel nítido si se percibe menos fluidez.

## Persistencia e integración

- La nueva preferencia entera `image-filter` acepta IDs 0–5. Si falta o queda
  fuera de rango, se utiliza 0, Equilibrado.
- La selección se guarda con `SharedPreferences.apply()` y se transmite
  mediante `GbaNative.setVideoFilter(int)`, firma JNI `(I)V`.
- Se utiliza la ruta de aplicación de opciones existente: al seleccionar,
  iniciar la parte nativa, reanudar la actividad y recrear/cambiar la
  superficie. No requiere reiniciar el juego.
- La firma existente `setVideoOptions(II)V` y sus preferencias de modo de
  imagen y resolución se conservan. También permanecen las opciones Original
  (3:2), Panorámica (beta), Estirar y la advertencia de fase temprana.
- Las páginas emplean el mismo ciclo de navegación que los demás ajustes;
  cambiar de página no libera brevemente la entrada del juego.

## Comprobación de la interfaz

Se revisó la navegación, los seis IDs, sus seis descripciones en cada idioma
y los puntos de aplicación de la preferencia. Los dos archivos Java
modificados se compilaron de forma aislada con `javac` de JDK 17 y el SDK
Android 35, enlazando las clases existentes del proyecto: **compilación
correcta**. El compilador solo emitió el aviso de uso de APIs obsoletas ya
presentes en la actividad.

Esta comprobación de código no equivale a una prueba táctil ni de rendimiento
en un teléfono. La verificación del APK completo y de los filtros nativos
corresponde al informe de construcción de esta versión.

Archivos: [`GbaGameActivity.java`](../android/java/org/gbarecomp/GbaGameActivity.java)
y [`GbaNative.java`](../android/java/org/gbarecomp/GbaNative.java).
