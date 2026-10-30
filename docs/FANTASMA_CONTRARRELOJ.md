# Fantasma de tu mejor recorrido — 0.10.17 beta

El fantasma permite correr contra una grabación de tu mejor intento de
contrarreloj. Aparece como un Yoshi semitransparente con los movimientos,
la animación y el color que tenía cuando se grabó. No necesita conexión a
Internet y no actúa sobre el juego: no recoge objetos ni golpea enemigos.

## Activarlo y crear la primera grabación

1. Abre **Atrás → Contrarreloj**.
2. Activa **Activar contrarreloj** y **Fantasma de mi récord**.
3. Vuelve al juego y entra a un nivel desde su inicio principal.
4. Completa el nivel con un tiempo que iguale o mejore tu récord personal.
5. Espera a que termine el guardado. La entrada del nivel mostrará
   **Fantasma disponible** junto al tiempo.
6. Vuelve a entrar por el principio de ese nivel para competir con él.

La opción viene desactivada. **Mostrar cronómetro** es independiente: puedes
ocultar el contador pequeño y mantener el contrarreloj y el fantasma activos.
La página usa el idioma de los menús del port: español, inglés o portugués.

Si ya tienes una marca de una versión anterior, esa marca solo contiene un
tiempo. No se puede reconstruir un recorrido a partir de ella. Debes igualarla
o mejorarla con el fantasma activado para asociarle una grabación. Una carrera
más lenta no reemplaza el fantasma del récord.

## Qué se muestra durante el intento

La grabación utiliza el mismo tiempo del juego que el contrarreloj. Elegir
presentación a 60 o 120 FPS no altera su velocidad. Start y el inventario
pausan el contador; al regresar se retoma la misma posición temporal.

El fantasma conserva su posición dentro del nivel. Tu cámara decide qué
parte se ve: si estás lejos de él, puede quedar fuera de la pantalla. Solo
se dibuja cuando ambos recorridos están en la misma habitación. Si siguen
rutas distintas, puede desaparecer hasta que vuelvan a coincidir en una.

Esta primera versión representa **la forma normal de Yoshi**. Oculta el
fantasma durante transformaciones, Fuzzy, pausas, menús, transiciones y
escenas donde no se puede reproducir su imagen correctamente. Esas partes
del recorrido conservan su tiempo; no se inventa una animación alternativa.
El fantasma vuelve a aparecer cuando la escena y su muestra son compatibles.

El dibujo respeta el área de la imagen, las franjas del filtro cinematográfico
y los controles táctiles. Usa los colores de la grabación, incluso si se
grabó con el mod de color de Yoshi. Una elección de color posterior no
repinta los fantasmas existentes.

## Cuándo se guarda y cuándo se cancela

Solo se conserva una carrera completa reconocida por el contrarreloj y
compatible con la mejor marca del nivel. Hay una entrada independiente para
cada uno de sus **60 niveles**: los 48 principales, seis Extra y seis Secretos.
La disponibilidad de esas entradas no implica que se hayan probado todos
los niveles o todas las formas de Yoshi.

| Situación | Resultado |
| --- | --- |
| Primera carrera válida sin récord previo | Guarda el tiempo y prepara su fantasma. |
| Carrera más rápida | Sustituye la referencia cuando se confirma el nuevo récord y termina el guardado. |
| Empate con un récord que no tiene grabación | Permite crear el fantasma de esa marca. |
| Carrera más lenta | Mantiene el mejor tiempo y su fantasma. |
| Activar a mitad del nivel | Espera a otra entrada completa desde el principio. |
| Morir, salir, cargar un estado o desactivar durante la carrera | Cancela la grabación parcial. Las carreras completas anteriores se conservan. |
| Reabrir la app y recuperar una sesión parcial | La sesión se recupera como antes; ese intento parcial no acredita una nueva grabación. |

Una grabación admite hasta **72.000 intervalos del juego**, unos veinte
minutos, y **512 imágenes diferentes** de Yoshi. Si alcanza uno de esos
límites, se descarta únicamente esa grabación parcial. El contrarreloj puede
seguir registrando el tiempo y guardar su marca aunque no tenga fantasma.

## Estados y conservación

La página distingue la preparación, la grabación, el guardado y la
competición contra una marca disponible. Si indica que hace falta activar
el contrarreloj, activa ambos interruptores y entra de nuevo desde el inicio.
Si se alcanza un límite o no se puede capturar la imagen, termina el intento
normalmente si quieres conservar su tiempo y vuelve a probar en otra entrada.

Los recorridos se guardan en los datos privados de la aplicación, separados
de la partida y del archivo de marcas. Desactivar el fantasma no los borra.
Instala las actualizaciones encima de la aplicación existente, conservando
sus datos. No hace falta borrar partidas para usar esta función.

Los archivos se verifican antes de reproducirse. Una grabación dañada, de
formato desconocido o cuyo tiempo no coincide con el récord no se usa como
referencia. Los archivos desconocidos se conservan; la función no los
reemplaza silenciosamente. Un problema de lectura o escritura se muestra
en el apartado. Los guardados pendientes se reintentan mientras la app
permanece abierta, pero no deben considerarse confirmados hasta que finalicen.

**Reportar un problema** incluye si has activado el fantasma. No adjunta
las grabaciones, las partidas ni los estados; puedes revisar el texto antes
de compartirlo.

## Alcance de esta entrega

0.10.17 incorpora el fantasma. **No cambia el arranque desde la intro ni la
recuperación automática al abrir el juego**: se mantiene el comportamiento
de 0.10.16.

Las pruebas de lógica, almacenamiento, interfaz Android en host y ejecución
nativa están descritas en el [informe de validación](../validation/GHOST_0.10.17_VALIDATION.md).
La instalación y el comportamiento sostenido en una GPU de teléfono Android
requieren comprobación física; no se prometen mejoras de FPS con esta función.

[Contrarreloj y reportes](CONTRARRELOJ_Y_REPORTES.md) ·
[Notas de la entrega](releases/v0.10.17-beta.md).
