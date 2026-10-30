# Contrarreloj y reportes de errores

Desde 0.10.16 hay un cronómetro por nivel con récords personales y una
herramienta para preparar reportes de errores. Ambos apartados siguen el
idioma elegido para los menús del port: español, inglés o portugués.

**0.10.17** añade **Fantasma de mi récord** dentro de Contrarreloj. Permite
grabar un intento completo que iguale o mejore tu marca y competir contra
su imagen semitransparente. Los récords antiguos necesitan una nueva
grabación. Consulta la [guía del fantasma](FANTASMA_CONTRARRELOJ.md).

## Activar el contrarreloj

1. Abre **Atrás → Contrarreloj**.
2. Activa **Activar contrarreloj** antes de entrar al nivel.
3. Regresa al juego y entra por el inicio principal del nivel. El contador
   comienza al terminar la entrada y empezar el recorrido.
4. Completa el nivel. Cuando el juego confirma la meta, el tiempo termina
   y se compara con tu mejor marca de ese nivel.

Si lo activas cuando ya estás dentro de un nivel, espera a una nueva entrada
desde el principio. Un intento empezado a mitad del recorrido o desde un
punto intermedio no puede establecer un récord.

**Mostrar cronómetro** controla el contador pequeño que aparece durante el
juego. Puedes ocultarlo y mantener activado el registro de tiempos. El
apartado Contrarreloj permite consultar el tiempo actual, la marca personal
y el último intento completado aunque el contador esté oculto.

### Niveles incluidos

El registro dispone de **60 niveles**, organizados en los seis mundos:

| Tipo | Cantidad | Identificadores en el menú |
| --- | ---: | --- |
| Principales | 48 | `1-1` a `1-8`, hasta `6-1` a `6-8` |
| Extra | 6 | `1-Extra` a `6-Extra` |
| Secretos | 6 | `1-Secreto` a `6-Secreto` |

Selecciona **Mundo 1** a **Mundo 6** para ver sus diez entradas. En inglés,
los secretos aparecen como `Secret`. El nivel de bienvenida y Mario Bros.
quedan fuera del contrarreloj.

### Cómo se mide el tiempo

Se mide **tiempo del juego**, con la misma escala al elegir presentación de
60 o 120 FPS. La presentación adicional de 120 no añade tiempo al intento.
El cronómetro muestra minutos, segundos y centésimas; para recorridos de
una hora o más también muestra las horas.

La pausa con **Start** y el inventario del propio juego detienen el contador.
Al volver al recorrido, continúa. Los cambios de habitación y la transición
de victoria de los jefes forman parte del tiempo hasta que se confirma la
meta.

El intento se cancela al perder una vida, salir del nivel o cargar un estado
guardado. También se cancela si el port detecta una discontinuidad que
impide verificar el recorrido. Para intentarlo de nuevo, entra desde el
inicio principal; reaparecer en un punto intermedio no inicia una marca válida.

La carga manual de un estado no restaura el cronómetro parcial como un intento
válido. Lo mismo se aplica si el port recupera una sesión desde un estado
después de cerrarse el proceso de la aplicación. Las marcas completas ya
guardadas se conservan aparte.

### Consultar tus marcas

La tabla muestra la mejor marca registrada de cada nivel, su fecha y el
último tiempo registrado cuando está disponible. Una entrada con **—**
todavía no tiene récord.

Tras una finalización, **Primera marca** identifica el primer tiempo del
nivel. **Nuevo récord** indica que has mejorado el anterior. La diferencia
usa un signo negativo cuando has tardado menos y positivo cuando has tardado
más; por ejemplo, `−0:01.23` significa un segundo y 23 centésimas menos.

Los récords se guardan localmente en este dispositivo, como datos del port
separados de la partida y de los estados del juego. Solo una marca menor
reemplaza el mejor tiempo. Cargar una partida o cancelar otro intento no
convierte ese recorrido parcial en récord ni empeora la mejor marca existente.

Si aparece **Guardado pendiente**, el port reintentará escribir los récords.
Si no puede leer el archivo existente, lo conserva y muestra el problema.
Estos avisos distinguen el tiempo observado de una escritura confirmada.

## Preparar un reporte de errores

Durante el juego, abre **Atrás → Reportar un problema**. La pantalla inicial
también incluye **Reportar un problema**, útil cuando todavía no puedes
entrar al juego.

1. Escribe un título breve, **Qué pasó**, los **Pasos para reproducirlo** y
   **Qué esperabas**.
2. Pulsa **Vista previa**. Se añaden los datos del port y del dispositivo.
3. Revisa el **Informe completo**. Puedes editar todo el texto y quitar los
   datos que no quieras compartir.
4. Elige una de las acciones disponibles.

| Acción | Resultado |
| --- | --- |
| **Copiar informe** | Copia exactamente el texto revisado al portapapeles. |
| **Compartir…** | Abre el selector de Android para elegir una aplicación y compartir el texto. No adjunta archivos. |
| **Abrir reporte en GitHub** | Abre un borrador de issue en el repositorio. La revisión y publicación final se realizan en GitHub. |

Si el informe es demasiado largo para incluirlo en el enlace de GitHub,
el port lo copia completo y abre el issue con el título. Pega el texto en
el cuerpo del issue; el aviso de la aplicación te indica cuándo hacerlo.

**Nada se envía automáticamente.** El borrador se conserva al volver a este
apartado desde Ajustes, el selector de compartir o el navegador, durante
la misma sesión de la aplicación. **Editar descripción y actualizar datos**
permite volver al formulario y generar una nueva vista previa. Esa acción
reemplaza las ediciones de la vista previa anterior con el informe regenerado.

### Datos que se añaden

El informe recoge la versión y el código del port, el paquete y el payload;
fabricante y modelo del teléfono; Android y su API; arquitecturas; dimensiones
y refresco de pantalla; cantidad de mandos detectados; y una lista concreta
de ajustes del port.

Los ajustes incluyen FPS solicitados, estado de la mezcla a 120, modo de
imagen, resolución, filtro, idiomas, controles táctiles, indicadores de
personalización del mando y de Yoshi, contrarreloj, cronómetro visible y
fantasma del récord. Las grabaciones del fantasma no se adjuntan.
Los perfiles gráficos se describen mediante cantidades de instalados y
activos, sin sus nombres, autores ni rutas.

Los **FPS solicitados** y la **última muestra observada** aparecen por
separado. Esa muestra no es una media de toda la sesión ni acredita un
rendimiento sostenido. Si el juego todavía no ha proporcionado una medida,
el informe indica que no está disponible.

Los datos automáticos no incluyen ROM, BIOS, partidas, archivos de estado,
nombres de archivos, identificadores únicos del dispositivo ni registros
privados. Los apartados de descripción contienen lo que tú escribas y se
pueden corregir antes de compartir.

## Alcance de las comprobaciones

La disponibilidad de 60 entradas no significa que se haya recorrido y
cronometrado cada nivel completo en esta entrega. Las pruebas de lógica,
las ejecuciones nativas acotadas y las pruebas de interfaz se documentan
con sus alcances por separado.

La instalación, el uso continuado del cronómetro, los destinos reales de
compartir y el navegador siguen requiriendo comprobación en un teléfono
Android físico. Consulta el [informe de validación de 0.10.16](../validation/FEATURES_0.10.16_VALIDATION.md)
para conocer las pruebas realizadas y sus límites.
