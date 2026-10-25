# Ajustes y diseños — Android 0.10.1 experimental

0.10.1 conserva las opciones de 0.10 y corrige el titileo de la panorámica,
los cambios de tamaño y la restauración de la imagen. [Detalle](../validation/ANDROID_0.10.1_VISUAL_FIXES.md).

El botón Atrás abre cuatro apartados:

- **Rendimiento y FPS:** contador y presentación de 120 FPS por mezcla temporal.
- **Imagen:** Automática, 720p y 1080p; modos Original (3:2), Panorámica (experimental) y Estirar a pantalla completa.
- **Controles:** visibilidad de botones táctiles, mapeo de mando y diseños personalizados.
- **Idiomas:** idioma del menú e idioma del juego, independientes.

Volver o Atrás regresa al apartado anterior. Continuar cierra Ajustes.
Los textos del menú y del editor están en español, inglés y portugués.

## Imagen sin deformaciones

En **0.10**, abre **Atrás → Imagen → Original (3:2)**. Los gráficos originales
de GBA miden **240 × 160 píxeles**; la imagen se centra con bandas cuando la
superficie tiene otra proporción. La elección se aplica sin reiniciar y queda
guardada.

**Panorámica (experimental)** dibuja escenario adicional donde hay cobertura,
hasta 356 × 160, y adapta el ancho al área disponible. Vuelve a la vista
original cuando no reúne datos suficientes. Sigue teniendo límites de
aparición de objetos y efectos no cubiertos; no está validada para todo el juego.

**Estirar a pantalla completa** llena el área de juego con el mismo fotograma:
puede deformarlo y no amplía el campo de visión. Automática, 720p y 1080p
cambian el escalado de presentación; tampoco añaden escenario por sí solos.
El panel permite desplazarse para acceder a todas las opciones.

En **0.9 y 0.9.1** se conserva 3:2 desactivando la antigua casilla **Estirar a
pantalla completa**. Al actualizar, esa elección pasa a Original o Estirar;
la panorámica debe seleccionarse expresamente y no existe en esos APK anteriores.
Consulta la [guía de imagen](CALIDAD_IMAGEN.md) y el
[estado de la panorámica](PANORAMICA_EN_DESARROLLO.md).

## Diseños personalizados

Controles → Diseños personalizados → Diseño 1, 2 o 3.
Arrastra la cruceta, A, B, L, R, Select y Start. Selecciona un control con el
desplegable si está detrás de otro, y cambia su tamaño con el deslizador.
La vista previa mantiene las proporciones del área segura de la pantalla.
Los límites impiden guardar controles completamente fuera de esa área.
El dibujo y las zonas táctiles del juego usan la misma geometría.

- Guardar y usar conserva ese diseño y lo activa.
- Cancelar o Atrás descarta la edición sin cambiar el diseño activo.
- Restablecer recupera las posiciones originales dentro del editor; solo se
  guardan al pulsar Guardar y usar.
- Predeterminado vuelve al diseño automático y conserva las tres ranuras.

El juego se pausa durante el editor y se reanuda al cerrarlo. En los otros
menús se bloquean las entradas del juego, como en la versión anterior.
Los diseños personalizan posición y tamaño; no importan imágenes de botones.

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

El APK 0.10, la nueva selección de imagen y sus límites se documentan en
[ANDROID_0.10_VALIDATION.md](../validation/ANDROID_0.10_VALIDATION.md).
La orientación Android pasa a `fullUser`; las pruebas SDL de tamaños y las
pruebas anteriores del editor no equivalen a una prueba física de rotación,
controles o segundo plano en esta nueva versión.

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
