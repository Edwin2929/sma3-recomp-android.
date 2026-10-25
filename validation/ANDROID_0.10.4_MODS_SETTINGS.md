# Interfaz de mods — Android 0.10.4 beta

## Uso

En **Atrás → Mods**, la opción **Importar mod** abre el selector de documentos
de Android. Se admite un paquete `.gbamod` de hasta **2 MiB**. Esta versión
admite perfiles de color para SMA3; no admite parches de ROM, bibliotecas
descargadas ni mods de otros juegos.

**Instalar ejemplo** importa el perfil Tono cálido incluido en el APK. Tanto
el ejemplo como los paquetes elegidos se añaden **desactivados**. Para aplicar
uno, se abre su ficha y se pulsa **Activar mod**. El catálogo muestra nombre,
versión, autor y estado; la ficha añade la descripción. Se usa un perfil de
color a la vez. Activar otro sustituye al anterior, y la elección se guarda.

La ficha también permite eliminar el paquete. La pantalla de confirmación
muestra el nombre exacto del mod y permite cancelar antes de eliminarlo. Los
paquetes instalados que el servicio identifica como incompatibles siguen
visibles para consultar el diagnóstico o eliminarlos, sin ofrecer un
interruptor de activación.

El menú, la ayuda, los avisos de transferencia y las operaciones tienen
textos en español, inglés y portugués. Los metadatos de un paquete se
muestran en el idioma de su autor. Los diagnósticos detallados del servicio
nativo se muestran tal como se reciben; en esta versión pueden aparecer en
español o inglés aunque el menú tenga otro idioma.

## Integración revisada

La interfaz llama a `listMods`, `importMod`, `setModEnabled` y `removeMod`
mediante una única cola de trabajo. La copia del documento y las llamadas
nativas se realizan fuera del hilo de la interfaz. La respuesta nativa
contiene el catálogo autorizado tras cada operación, incluidos los fallos;
la interfaz actualiza el estado desde esa respuesta.

La copia temporal se crea en la caché privada de la aplicación. El límite
se calcula con los bytes realmente leídos, sin confiar en el tamaño o tipo
MIME comunicado por el proveedor. Java no extrae el ZIP: el servicio nativo
valida el contenido. La copia se elimina al terminar, también si la
validación falla.

La revisión de código comprobó los siguientes casos de navegación y
ciclo de vida:

- Cancelar el selector vuelve a cargar el catálogo sin importar ningún
  documento.
- Si Android recrea la actividad mientras está abierto el selector, se
  reconstruye el controlador antes de entregarle el resultado.
- Volver a Ajustes mientras hay una operación evita que una respuesta
  tardía reabra Mods sobre otra pantalla o sobre el juego.
- Destruir la actividad cancela la cola de trabajo e impide actualizar sus
  ventanas. La transferencia de datos comprueba la interrupción.
- Los cambios de página reutilizan el bloqueo de entrada de Ajustes para
  mantener los controles del juego bloqueados mientras el menú está abierto.
- Un aviso nativo de recuperación o incompatibilidad se muestra una vez
  por mensaje distinto durante la vida de la actividad.

Estas comprobaciones son una **revisión del código**, no una ejecución del
selector de documentos en un teléfono. Un proveedor bloqueado en una lectura
puede tardar en atender una interrupción; las respuestas tardías continúan
sin poder abrir ventanas de una actividad destruida.

## Pruebas ejecutadas

| Comprobación | Resultado |
| --- | --- |
| Compilación aislada de las cinco clases Java modificadas | **PASS**, Java 17 y Android API 35, usando las dependencias ya compiladas del proyecto. |
| Transferencia con límite exacto | **PASS**, incluidos archivos vacíos y tamaños antes, en y después del bloque de 64 KiB. |
| Transferencia de datos que superan el límite | **PASS**, se rechaza y nunca se escribe por encima del límite. |
| Proveedor que devuelve una lectura de longitud cero | **PASS**, la transferencia progresa y conserva los bytes. |
| Interrupción y fallo de lectura del proveedor | **PASS**, se propagan para cancelar o limpiar la importación. |

La prueba del ayudante de transferencia ejecutó **26 comprobaciones** sobre
la clase real `ModArchiveCopy`, sin reemplazar su implementación. Los hashes
de fuentes y el resultado están en
[`android-0.10.4-mods-java.json`](android-0.10.4-mods-java.json).

La compilación aislada no sustituye a la construcción completa del APK. La
validación nativa de archivos, la persistencia y la aplicación de colores se
documentan por separado. No se ha ejecutado esta interfaz en un dispositivo
Android físico durante estas pruebas.

Para repetir la prueba de transferencia desde la raíz del repositorio,
con un JDK 17 o posterior:

```sh
mkdir -p /tmp/sma3-mod-copy-test
javac -encoding UTF-8 -source 17 -target 17 \
  -d /tmp/sma3-mod-copy-test \
  android/java/org/gbarecomp/ModArchiveCopy.java \
  validation/ModArchiveCopyTest.java
java -cp /tmp/sma3-mod-copy-test org.gbarecomp.ModArchiveCopyTest
```

## Revisión del ejemplo y de la guía de autores

Una revisión independiente del empaquetador, del ejemplo editable y del
manifiesto mínimo de [`MODS.md`](../docs/MODS.md) completó **37 comprobaciones**:

- Dos ejecuciones produjeron exactamente el mismo ZIP, idéntico al ejemplo
  preparado para incluirse en el APK. Cambiar el orden de creación, las
  fechas o los permisos de los archivos de origen tampoco cambió los bytes.
- El ZIP contiene únicamente `manifest.toml` y `README.txt` en la raíz,
  idénticos a los archivos revisados de `mods/examples/tono-calido`, con
  fechas y permisos normalizados y sin comentarios ni metadatos adicionales.
- Se rechazaron archivos ajenos, subdirectorios, enlaces simbólicos, texto
  UTF-8 inválido, bytes NUL y tamaños excesivos. Una salida existente se
  conservó salvo al usar `--replace` expresamente.
- Los hashes del informe Java y de la prueba de transferencia siguieron
  coincidiendo con las fuentes actuales.

El SHA-256 del ejemplo revisado es
`3e54d5d43bf4848e03e1d19ef74c366e2e286364e977e568350db52618975783`.

También se extrajo **el bloque TOML exacto de la guía**, se empaquetó y se
importó junto con el ejemplo mediante el catálogo nativo real en el equipo
de desarrollo. Ambos se importaron desactivados; activarlos de forma
sucesiva mantuvo un único perfil activo. La transformación cálida y la
transformación azul mínima produjeron los valores RGB esperados, incluidas
las opciones de identidad omitidas. Desactivar recuperó los píxeles
originales y eliminar ambos dejó vacío el catálogo.

Esta prueba utilizó un catálogo temporal y un controlador nativo de pruebas
que proporciona la identificación conocida del juego, sin ROM. Comprueba
el formato y la aplicación de esos paquetes, no el selector Android, JNI
ni la recuperación de un estado persistente dañado. La inclusión final
del archivo en el APK se comprueba en el registro de construcción.

La revisión de Gradle detectó que escribir directamente en un directorio
de assets sin limpiarlo podía conservar archivos ajenos de una construcción
anterior. Se corrigió separando la salida del empaquetador del directorio
de assets y añadiendo `stageSma3ColorExampleAssets`: una tarea `Sync` cuyo
único origen es `tono-calido.gbamod`, colocado bajo `mod-examples`. Esta
tarea se ejecuta siempre y limpia los archivos sobrantes del destino.
Se revisaron esas dependencias y la configuración corregida; esta revisión
independiente no ejecutó Gradle.

## Filtro cinematográfico

La selección de filtros contiene siete entradas. **Cinematográfico** usa el
ID 6, conservando los IDs 0–5. Los textos de los tres idiomas explican que
las dos franjas negras cubren parte de la imagen y sus indicadores, mientras
los controles permanecen visibles. La geometría y las pruebas del filtro
se documentan en
[`CINEMATIC_0.10.4_VALIDATION.md`](CINEMATIC_0.10.4_VALIDATION.md).
