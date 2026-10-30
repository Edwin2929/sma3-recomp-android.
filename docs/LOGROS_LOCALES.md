# Logros locales — Android 0.10.9 beta

**Atrás → Logros** muestra diez retos que se registran en este dispositivo.
No hace falta crear una cuenta, iniciar sesión ni conectarse a Internet para
obtenerlos. El registro comienza con esta versión y se conserva al actualizar
la aplicación manteniendo sus datos.

## Los diez logros

| Logro | Cómo obtenerlo |
| --- | --- |
| **Primera flor** | Recoge una flor durante el juego. |
| **Ramo completo** | Reúne las cinco flores de un mismo nivel. |
| **Rojo brillante** | Recoge una moneda roja durante el juego. |
| **Veinte tesoros** | Reúne las veinte monedas rojas de un mismo nivel. |
| **Treinta estrellas** | Alcanza treinta estrellas durante una partida. |
| **Cambio de forma** | Transfórmate mediante una burbuja de vehículo, como el coche. |
| **Cabeza despejada** | Recupérate naturalmente del efecto de un Fuzzy, sin interrumpir la secuencia. |
| **Primera meta** | Cruza una meta o completa la transición de victoria de un jefe. |
| **Cinco aventuras** | Completa cinco niveles distintos. Repetir el mismo no aumenta este contador. |
| **Puntuación perfecta** | Completa un nivel con cien puntos y confirma el resultado en su pantalla de puntuación. |

Los logros de flores y monedas rojas usan la mayor cantidad alcanzada en
un solo nivel; no suman objetos de diferentes niveles. El progreso de niveles
distintos reúne las metas reconocidas en las sesiones jugadas con esta versión.
La introducción y los minijuegos no se incluyen como niveles completados.

## Consultar y recibir avisos

El panel muestra el total desbloqueado, una barra general y tarjetas con
la descripción de cada reto. Cada tarjeta indica su progreso o su fecha
de obtención. La fecha usa el reloj del teléfono y se conserva desde el
primer desbloqueo.

Los filtros **Todos**, **Logrados** y **Pendientes** permiten cambiar la lista.
Desplázate para ver todos los retos y la opción de avisos. **Volver** permanece
fuera del contenido desplazable.

Al obtener un logro aparece un aviso breve. Puedes silenciar los avisos
desde este mismo apartado; el registro de progreso sigue funcionando.
El aviso solo se emite después de confirmar que el logro quedó guardado.
No vuelve a anunciarse un logro ya registrado al abrir el menú o reiniciar.

## Partidas anteriores y estados instantáneos

Los logros observan acciones del juego. **No se conceden por abrir Ajustes
ni por importar o cargar una partida antigua con récords ya obtenidos.**
Puedes seguir usando tu partida: las acciones que realices a partir de
esta versión se podrán reconocer normalmente.

Por ejemplo, cargar un estado que ya muestra cinco flores no concede por
sí solo el logro. Llegar a la meta durante la nueva sesión sí puede registrar
ese nivel. Un récord antiguo de cien puntos tampoco sustituye completar
y confirmar una nueva puntuación de cien.

Cargar un estado no borra los logros ya guardados. La detección reinicia
las secuencias incompletas al cargar, cambiar de nivel o perder continuidad.
En particular, cargar directamente el final del efecto de Fuzzy o una pantalla
de puntuación no sustituye observar la secuencia válida.

## Dónde se conserva el progreso

El registro de logros es independiente de la ROM y del archivo de partida.
Se conserva al instalar una actualización sobre la aplicación existente,
con el mismo paquete y certificado, sin borrar datos.

**Desinstalar la aplicación o borrar sus datos elimina este registro local.**
Esta versión no añade sincronización entre teléfonos ni exportación de logros.
Los logros son compartidos por las partidas usadas en la misma instalación.

Si falla una escritura, los cambios pendientes se conservan en memoria y
se reintenta guardarlos. El panel muestra el problema; no se anuncia un
desbloqueo como guardado hasta confirmarlo. Si el archivo no puede leerse
o pertenece a un formato futuro, se conserva y no se reemplaza por uno vacío.

## Compatibilidad y alcance

Los logros son independientes de los filtros, del color de Yoshi y de los
modos de imagen. La detección sigue la ejecución del juego; seleccionar
presentación de 120 no duplica los eventos ni los premios.

Los retos se apoyan en las escenas y transiciones identificadas de SMA3 USA.
No se ha completado todo el juego ni probado esta versión en todos los
dispositivos. La panorámica mantiene su etiqueta beta y sus límites anteriores.

[Pruebas y límites](../validation/LOCAL_ACHIEVEMENTS_0.10.9_VALIDATION.md) ·
[Interfaz](../validation/LOCAL_ACHIEVEMENTS_UI_VALIDATION.md) ·
[Detección nativa](../validation/LOCAL_ACHIEVEMENTS_NATIVE.md) ·
[Notas de 0.10.9](releases/v0.10.9-beta.md).
