# Actualizador de Android — validación de 0.10.5 beta

## Comportamiento implementado

El actualizador consulta la release pública cuyo **tag es `Descarga`** en
`Edwin2929/sma3-recomp-android.`. El tag permanece fijo: la selección examina
los APK adjuntos y compara sus versiones numéricamente. No depende de que
el mantenedor cree una release nueva, cambie su título o use `releases/latest`.

La detección automática está activada inicialmente. Se consulta al abrir o
volver a la aplicación, con un intervalo de seis horas y caché ETag. El
apartado incluye **Buscar ahora** y un interruptor para desactivar la búsqueda
automática. Los fallos de red tienen una espera antes del siguiente intento
automático; los límites de GitHub se distinguen y se respeta su plazo.
No hay un servicio de vigilancia permanente mientras la aplicación está
cerrada ni una notificación push desde GitHub.

La descarga y la instalación requieren elegir sus botones. El gestor usa
una cola única con prioridad de fondo y no bloquea la lógica del juego.
Muestra progreso, permite cancelar y permite volver al juego mientras se
descarga. Un aviso breve anuncia una versión disponible una sola vez por
identificador de archivo; no abre un diálogo sobre la partida.

El archivo se descarga a una carpeta privada de caché. Se comprueban el
tamaño exacto, el SHA-256 publicado por GitHub cuando existe, el paquete,
la versión anunciada, un `versionCode` superior, la versión mínima de
Android, las bibliotecas ARM64 y los certificados que informa
`PackageManager`. Se exige la misma lista de firmantes actuales que la
instalación existente; esta entrega no implementa rotación de claves.

Tras la comprobación se publica un solo archivo privado verificado. Antes
de instalar se comprueba de nuevo su hash y la identidad del paquete. Un
proveedor de contenido no exportado comparte únicamente ese archivo,
mediante una URI exacta y una concesión temporal de lectura al instalador.
No expone directorios, partidas, ROM, BIOS ni escritura.

**Android conserva el control de la instalación y su confirmación final.**
Si falta el permiso para instalar desde SMA3, se abre la pantalla del sistema.
Al volver se consulta el permiso real; denegarlo no crea un bucle de solicitudes.
El instalador verifica finalmente la firma y la compatibilidad. La consulta
de certificados del actualizador es una comprobación previa, no una
implementación propia del verificador criptográfico de APK.

## Pruebas ejecutadas

### Política y transporte HTTPS

[`UpdatePolicyTest.java`](UpdatePolicyTest.java) ejecuta las clases reales
`UpdatePolicy` y `UpdateHttp`, con conexiones HTTP controladas para provocar
las respuestas. Pasa **105 comprobaciones**:

- Orden numérico de versiones, sufijos de prerelease, versiones antiguas
  de dos componentes y rechazo de versiones malformadas.
- Selección del APK ARM64 más reciente, independencia del orden de los
  adjuntos, reemplazos de un adjunto y ausencia de un bucle de actualización
  hacia la misma versión.
- Rechazo de paquetes, versiones, plataforma o firmantes incompatibles
  mediante la política compartida con Android.
- Cabeceras ELF64 de AArch64 válidas y rechazo de cabeceras truncadas,
  archivos que no son ELF, bibliotecas x86-64, ARM de 32 bits y big-endian
  aunque se hayan renombrado como bibliotecas ARM64.
- URLs limitadas a HTTPS, el repositorio y el tag establecidos. Rechazo de
  cambios de repositorio, credenciales incrustadas, trampas de sufijo de host,
  redirecciones a otros sitios y degradación a HTTP.
- Descarga exacta tras la redirección de GitHub, progreso creciente,
  ausencia de credenciales en el CDN y cierre de conexiones.
- Rechazo de descarga truncada, exceso de bytes, longitud declarada
  incorrecta, hash distinto y redirecciones repetidas; retirada de parciales.
- Cancelación durante la transferencia, límite de metadatos, ETag, respuesta
  304 y diferenciación de un límite de consultas de GitHub.

Los límites son **128 MiB para el APK**, **2 MiB para los metadatos**, cinco
redirecciones, 15 segundos para conectar/leer, un minuto total de lectura de
metadatos y diez minutos para la transferencia. Los límites temporales
totales se revisan entre lecturas; cada lectura tiene además su propio timeout.
Estas pruebas no convierten sus respuestas HTTP simuladas en una descarga
real del APK desde un teléfono.

### Release real y futuras versiones

[`UpdateReleaseTest.java`](UpdateReleaseTest.java) reproduce el JSON real
consultado el **25 de octubre de 2026**, con tag `Descarga` y el APK
`sma3-recomp-0.10.2-beta-arm64.apk`. Esa respuesta no ofrece una actualización
a una instalación `0.10.5-beta`.

Las **19 comprobaciones** del analizador pasan. Las pruebas añaden a una
copia del JSON adjuntos sintéticos `0.10.6-beta`, `0.10.10-beta` y otros
incompatibles; se selecciona la versión superior sin cambiar el tag ni la
fecha de creación de la release. También se comprueban el digest, la caché,
metadatos dañados, release borrador, tag incorrecto, listas excesivas y una
release sin adjuntos. **No se subió ninguno de esos archivos a GitHub.**

Se ejecuta el `UpdateRelease` de producción con la biblioteca de referencia
`org.json:json:20240303` únicamente en el entorno de pruebas. El APK utiliza
el `org.json` de Android y no incluye esa biblioteca adicional. El JAR de
prueba tiene SHA-256
`3cf6cd6892e32e2b4c1c39e0f52f5248a2f5b37646fdfbb79a66b46b618414ed`.

### Compilación y revisión de Android

Las seis clases del actualizador compilan con **Java 17 y Android API 35**,
junto al panel de ajustes actualizado. El coordinador integra la entrada
en Ajustes y en la pantalla inicial y registra la construcción completa del
APK por separado.

La revisión del ciclo de vida comprueba que:

- Una respuesta tardía no vuelve a abrir una página que se ha abandonado.
- Una actividad pausada no abre ventanas ni lanza el instalador.
- El resultado del permiso llega antes de `onResume`; tanto las ventanas
  como la instalación se difieren hasta que la actividad está en primer
  plano. La continuación exige el token de solicitud en memoria y la misma
  página que recibió el toque en Instalar.
- Si Android recrea la actividad durante la pantalla de permiso, se abre
  el apartado al volver y se solicita tocar Instalar de nuevo. No se
  reconstruye automáticamente una intención de instalar que ya no se conserva.
- Cancelar la confirmación del instalador deja disponible el apartado.
- Destruir la actividad retira su observador; la cola de descarga pertenece
  al contexto de aplicación y no mantiene una referencia permanente a ella.
- Tras terminar el proceso, se limpia el parcial anterior al abrir otra vez.
- Un APK descargado de una versión anterior conserva su etiqueta correcta
  aunque la consulta ya haya detectado un adjunto más reciente.
- La limpieza termina antes de liberar la operación. El estado final y la
  liberación se publican bajo el mismo bloqueo para evitar botones activos
  mientras sigue ocupada la cola y respuestas antiguas que sobrescriban
  el estado de una nueva operación.

La revisión independiente encontró y corrigió dos problemas: una devolución
del permiso podía abrir el diálogo antes de `onResume`, y el final de una
operación podía publicar el estado antes de liberar la cola o después de
iniciar otra. La continuación del permiso ahora conserva el vínculo con la
página original; la finalización de la cola publica y libera de forma atómica.

[`test_updates_ui_lifecycle.py`](test_updates_ui_lifecycle.py) ejecuta el
`UpdatesUi` real sin modificarlo, con sustitutos controlados de Activity,
diálogos y gestor asíncrono. Pasa **8 escenarios y 32 comprobaciones**:
retorno del permiso antes de reanudar, denegación y persistencia de su aviso,
recreación de la actividad, abandono de la página antes y después del
resultado, verificación durante una pausa, destrucción o abandono, fallo de
digest, instalador ausente y ausencia de relanzamiento tras cancelar.

El arnés de ciclo de vida es una **prueba de host**, no instrumentación de
Android. Las comprobaciones del proveedor y la cola real se revisaron en
el código y la compilación. No se ejecutó el instalador Android en un Pixel
7 Pro. Las pruebas de política reciben metadatos de identidad controlados;
no ejecutan `PackageManager` fuera de Android.

## Reproducción de las pruebas de host

Desde la raíz del repositorio, con JDK 17:

```sh
mkdir -p /tmp/sma3-update-tests
javac -encoding UTF-8 -source 17 -target 17 -d /tmp/sma3-update-tests \
  android/java/org/gbarecomp/UpdatePolicy.java \
  android/java/org/gbarecomp/UpdateHttp.java validation/UpdatePolicyTest.java
java -cp /tmp/sma3-update-tests org.gbarecomp.UpdatePolicyTest
python3 validation/test_updates_ui_lifecycle.py --java-home "$JAVA_HOME"
```

Para el analizador JSON, proporciona `SMA3_JSON_JAR` apuntando al JAR de
referencia y `SMA3_RELEASE_FIXTURE` al JSON obtenido del endpoint público:

```sh
javac -encoding UTF-8 -source 17 -target 17 \
  -cp "$SMA3_JSON_JAR:/tmp/sma3-update-tests" -d /tmp/sma3-update-tests \
  android/java/org/gbarecomp/UpdateRelease.java validation/UpdateReleaseTest.java
java -cp "$SMA3_JSON_JAR:/tmp/sma3-update-tests" \
  org.gbarecomp.UpdateReleaseTest "$SMA3_RELEASE_FIXTURE"
```

La prueba del fixture espera la respuesta histórica descrita; una descarga
actual posterior a la publicación de APK más nuevos puede requerir conservar
esa respuesta histórica para repetir exactamente el caso sin actualización.
Los hashes y resultados se registran en
[`android-0.10.5-updates.json`](android-0.10.5-updates.json).

## Referencias oficiales

- [GitHub: obtener una release por su tag](https://docs.github.com/en/rest/releases/releases#get-a-release-by-tag-name), incluido el listado de assets, `browser_download_url` y `digest`.
- [Android: canRequestPackageInstalls](https://developer.android.com/reference/android/content/pm/PackageManager#canRequestPackageInstalls()).
- [Android: ACTION_MANAGE_UNKNOWN_APP_SOURCES](https://developer.android.com/reference/android/provider/Settings#ACTION_MANAGE_UNKNOWN_APP_SOURCES).
- [Android: ciclo de vida de onActivityResult](https://developer.android.com/reference/android/app/Activity#onActivityResult(int,int,android.content.Intent)).
- [Android: compartir archivos mediante URI y permisos temporales](https://developer.android.com/training/secure-file-sharing/share-file).
