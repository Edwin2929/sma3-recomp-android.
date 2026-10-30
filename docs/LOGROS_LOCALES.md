# 26 logros locales — Android 0.10.11 beta

**Atrás → Logros** muestra 26 retos que se registran en este dispositivo.
No hace falta crear una cuenta, iniciar sesión ni conectarse a Internet para
obtenerlos. El registro existe desde 0.10.9 y se conserva al actualizar
la aplicación manteniendo sus datos. 0.10.11 conserva los 25 retos anteriores
y añade **Yo shi pude**, junto con una nueva animación al desbloquear logros.

## Los 26 logros

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
| **Trío de flores** | Reúne tres flores de un mismo nivel. |
| **Cinco monedas rojas** | Reúne cinco monedas rojas de un mismo nivel. |
| **Diez monedas rojas** | Reúne diez monedas rojas de un mismo nivel. |
| **Diez aventuras** | Completa diez niveles distintos. |
| **Veinte aventuras** | Completa veinte niveles distintos. |
| **Treinta aventuras** | Completa treinta niveles distintos. |
| **Cuarenta aventuras** | Completa cuarenta niveles distintos. |
| **Cinco niveles perfectos** | Completa cinco niveles distintos con cien puntos y confirma sus resultados. |
| **Diez niveles perfectos** | Completa diez niveles distintos con cien puntos y confirma sus resultados. |
| **Veinte niveles perfectos** | Completa veinte niveles distintos con cien puntos y confirma sus resultados. |
| **Coche** | Transforma a Yoshi en coche. |
| **Tanque topo** | Transforma a Yoshi en tanque topo. |
| **Helicóptero** | Transforma a Yoshi en helicóptero. |
| **Tren** | Transforma a Yoshi en tren. |
| **Submarino** | Transforma a Yoshi en submarino. |
| **Yo shi pude** | Completa una partida nueva y los 48 niveles principales hasta el final sin que un golpe o enemigo separe a Bebé Mario de Yoshi en ningún mapa. |

Los logros de flores y monedas rojas usan la mayor cantidad alcanzada en
un solo nivel; no suman objetos de diferentes niveles. El progreso de niveles
distintos reúne las metas reconocidas en las sesiones jugadas desde 0.10.9.
La introducción y los minijuegos no se incluyen como niveles completados.
Cada nivel cuenta una sola vez para su grupo: repetir un nivel con cien puntos
no aumenta el número de niveles perfectos. Cada vehículo tiene su propio logro;
usar uno no concede los otros cuatro.

## Consultar y recibir avisos

El panel muestra el total desbloqueado sobre 26, una barra general y tarjetas con
la descripción de cada reto. Cada tarjeta indica su progreso o su fecha
de obtención. La fecha usa el reloj del teléfono y se conserva desde el
primer desbloqueo.

Los filtros **Todos**, **Logrados** y **Pendientes** permiten cambiar la lista.
Desplázate para ver todos los retos y la opción de avisos. **Volver** permanece
fuera del contenido desplazable.

Al obtener un logro aparece una medalla con un huevo que se despliega en
una tarjeta verde con **Logro desbloqueado** y su nombre. La animación se
inspira en las celebraciones de consola y usa gráficos originales del port.
Se sitúa arriba, dentro del área segura, no pausa el juego y deja pasar los
toques hacia los controles. Si obtienes varios logros juntos, se muestran
uno después de otro.

Puedes desactivar **Animación al desbloquear** sin desactivar el registro.
El aviso aparece después de guardar el logro; no se repiten reconocimientos
al actualizar ni premios ya guardados. Al salir de la app se retiran los
avisos y la cola pendiente. Si Android tiene las animaciones desactivadas,
se muestra la tarjeta sin movimiento. El tiempo de lectura se adapta a la
preferencia de accesibilidad del sistema. La tarjeta no añade sonidos.

## Yo shi pude: una partida completa sin perder a Bebé Mario

El panel de estado del reto aparece debajo del resumen de logros. Indica
**Intento en curso**, el archivo de partida y cuántos de los **48 niveles
principales** se han completado en ese intento. La introducción no aumenta
el contador. Los niveles secretos y extra son opcionales para completar
el juego; si los visitas durante el intento, una pérdida de Bebé Mario
también lo invalida.

1. Crea una **partida nueva desde el menú del juego**. El port debe observar
   su creación y recorrido desde la introducción. Una partida anterior o
   importada no demuestra qué ocurrió antes de activar este registro.
2. Completa el juego sin que ningún golpe o enemigo separe a Bebé Mario de
   Yoshi. **Recuperarlo enseguida no restaura la validez del intento.**
3. El logro se concede al observar los 48 niveles principales y la llegada
   al final después de la victoria sobre Bebé Bowser. Las animaciones normales
   de entrega entre Yoshis, transformaciones y Superestrella no cuentan
   como pérdidas.

Si pierdes a Bebé Mario, puedes seguir jugando normalmente. El panel marca
**Intento terminado**; para intentar este logro otra vez, comienza otra
partida nueva. Los otros logros y sus fechas no se borran. El reto no añade
una condición independiente de «cero muertes»; comprueba las separaciones
de Bebé Mario y la continuidad del recorrido.

### Pausar y continuar el intento

Al enviar la app a segundo plano, el port guarda un estado automático y
un comprobante del intento asociado a ese estado exacto. Si Android cierra
la app mientras está suspendida, la siguiente apertura puede continuar
ese punto y el intento verificado. Al continuar, ese comprobante se consume;
un estado anterior no permite deshacer una pérdida.

**Cargar manualmente un estado, rebobinar o continuar desde una partida
antigua no acredita este reto.** Tampoco se conserva la validez si el proceso
se termina de forma abrupta en primer plano sin una suspensión comprobada.
En esos casos el juego puede seguir, pero hay que empezar una partida nueva
para registrar un recorrido completo. Para interrumpir una sesión, envía
la app a segundo plano y permite que termine su guardado automático.

Si no se puede consumir el comprobante por un error de almacenamiento, la
sesión se detiene antes de continuar la ejecución. Esto evita reutilizar
un comprobante anterior después de acciones que no pudieron conservarse.
El comprobante del reto es independiente del archivo de partida y del
registro de los otros logros.

## Actualizar desde 0.10.10

Al instalar 0.10.11 encima de 0.10.10 se conservan los **25 IDs, fechas,
progreso y preferencia de avisos** anteriores. **Yo shi pude** empieza
pendiente: los registros antiguos no contenían la historia completa de
pérdidas de Bebé Mario y no pueden concederlo retroactivamente.

## Actualizar desde los diez logros de 0.10.9

Al instalar 0.10.11 sobre 0.10.9 se conservan el progreso local, las fechas
de los diez logros anteriores y la preferencia de avisos. Los retos nuevos
que ya cumplan esa evidencia se reconocen sin repetir las acciones.

Por ejemplo, un registro que ya conserva diez monedas rojas de un nivel
reconoce los nuevos retos de cinco y diez rojas. Si ya observó el coche,
reconoce **Coche** sin conceder los demás vehículos.

Estos logros muestran **Reconocido al ampliar los logros** y la fecha en que
la actualización los reconoció. Los diez anteriores mantienen su fecha
original. La ampliación se guarda de forma silenciosa; los logros obtenidos
después siguen usando la preferencia de avisos elegida.

Si falla una escritura, el panel muestra el problema y se reintenta conservar
el registro ampliado. Si vienes de una versión anterior a 0.10.9, que todavía
no tenía este registro, los logros comienzan a reconocerse al jugar.

## Partidas anteriores y estados instantáneos

Los primeros 25 logros observan acciones del juego. **Importar o cargar una partida
antigua con récords ya obtenidos no los concede automáticamente.**
Puedes seguir usando tu partida: las acciones que realices durante el juego
se podrán reconocer normalmente. El reconocimiento al ampliar el catálogo
usa exclusivamente el registro local de logros descrito arriba.

Por ejemplo, cargar un estado que ya muestra cinco flores no concede por
sí solo el logro. Llegar a la meta durante la nueva sesión sí puede registrar
ese nivel. Un récord antiguo de cien puntos tampoco sustituye completar
y confirmar una nueva puntuación de cien.

Cargar un estado no borra los logros ya guardados. El intento en curso de
**Yo shi pude** sigue las reglas específicas de continuidad descritas arriba. La detección reinicia
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

[Pruebas y límites de 0.10.11](../validation/LOCAL_ACHIEVEMENTS_0.10.11_VALIDATION.md) ·
[Interfaz y migración](../validation/LOCAL_ACHIEVEMENTS_26_STATE_VALIDATION.md) ·
[Detección del recorrido](../validation/YO_SHI_PUDE_NATIVE.md) ·
[Notas de 0.10.11](releases/v0.10.11-beta.md).
