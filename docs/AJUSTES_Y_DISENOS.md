# Ajustes y diseños — 0.9 experimental

El botón Atrás abre cuatro apartados:

- **Rendimiento y FPS:** contador y presentación de 120 FPS por mezcla temporal.
- **Imagen:** Automática, 720p, 1080p y estirar a pantalla completa.
- **Controles:** visibilidad de botones táctiles, mapeo de mando y diseños personalizados.
- **Idiomas:** idioma del menú e idioma del juego, independientes.

Volver o Atrás regresa al apartado anterior. Continuar cierra Ajustes.
Los textos del menú y del editor están en español, inglés y portugués.

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

## Validación de 0.9

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
