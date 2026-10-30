# Actualizar desde GitHub — 0.10.10 beta

**Atrás → Actualizar está disponible desde 0.10.5.** También puedes abrir
**Actualizar aplicación** desde la pantalla inicial, antes de cargar el juego.
El apartado muestra la versión instalada, la última comprobación y las
acciones disponibles.

La entrega actual es **0.10.10-beta**, código **21**. Amplía **Logros locales**
a 25 retos con progreso, fechas, filtros y avisos opcionales. Conserva
los diez logros previos y su progreso al actualizar sin borrar datos. También mantiene el color de
Yoshi y las funciones anteriores.
[Notas de 0.10.10](releases/v0.10.10-beta.md) ·
[Guía de logros](LOGROS_LOCALES.md).

## Primera instalación y uso

Las versiones 0.10.4 y anteriores no tienen este actualizador. En ellas,
instala **`sma3-recomp-0.10.10-beta-arm64.apk` manualmente sobre la aplicación
existente, sin desinstalarla**. El archivo conserva el paquete y la firma
y aumenta el código de versión a **21**. No borres los datos para actualizar.

Si ya usas 0.10.5 o posterior, puedes seguir los pasos de búsqueda, descarga e instalación
de este apartado cuando el mantenedor haya adjuntado el APK 0.10.10 a
**Descarga**. La instalación manual encima de la aplicación también es válida.

Después de instalarlo:

1. Deja activado **Buscar automáticamente** si quieres recibir un aviso
   de nuevas versiones. La app consulta GitHub al abrir o volver a primer
   plano, como máximo una vez cada seis horas tras una consulta correcta.
2. Abre **Atrás → Actualizar → Buscar ahora** para consultar manualmente.
   No hace falta esperar seis horas, salvo un bloqueo temporal de GitHub.
3. Cuando aparezca una versión, pulsa **Descargar actualización**. Se muestra
   su progreso y puedes cancelar; el archivo parcial se elimina. También
   puedes volver al juego y regresar a este apartado cuando termine.
4. Pulsa **Instalar actualización**. Se comprueba de nuevo el archivo antes
   de abrir el instalador de Android.
5. Si Android lo solicita, permite instalar aplicaciones desde SMA3.
   Confirma la actualización en el instalador del sistema.

La **detección es automática**; la descarga y la instalación requieren
las acciones anteriores. No hay un servicio de comprobación permanente
mientras la aplicación está cerrada. Si Android termina el proceso durante
una descarga, vuelve a abrir el apartado y descárgala de nuevo.

La app no instala una actualización en silencio ni se reinicia durante
la partida para hacerlo. Si cancelas el permiso o la instalación, puedes
seguir usando la versión actual. Salir de la pantalla de verificación
impide que su resultado abra el instalador sobre otra pantalla.

Si Android recrea la pantalla mientras concedes el permiso, al volver se
muestra el apartado y un aviso para pulsar **Instalar actualización** otra
vez. La intención anterior no se reanuda automáticamente desde una nueva
instancia de pantalla. En un retorno normal, se vuelve a verificar el APK
y se abre la confirmación del sistema cuando la app está en primer plano.

## Dónde busca

El origen es la publicación existente
[**Descarga**](https://github.com/Edwin2929/sma3-recomp-android./releases/tag/Descarga)
del repositorio **`Edwin2929/sma3-recomp-android.`**. El punto final forma
parte del nombre del repositorio.

La consulta utiliza:

```text
https://api.github.com/repos/Edwin2929/sma3-recomp-android./releases/tags/Descarga
```

Se examinan los APK adjuntos a esa publicación, aunque no se cree una
etiqueta nueva. No se consulta `releases/latest`. Subir una versión solo a
otra publicación no la hará aparecer en este actualizador.

El nombre admitido es:

```text
sma3-recomp-<versionName>-arm64.apk
```

Por ejemplo, **`sma3-recomp-0.10.10-beta-arm64.apk`** para esta entrega.
Se comparan los componentes numéricos de versión: 0.10.11 es posterior a
0.10.10. Se elige la mayor versión compatible, no simplemente el último
archivo de la lista.
Una versión estable es posterior a su prerelease del mismo número.
Los borradores, archivos incompletos, nombres genéricos y versiones que no
superan la instalada no se ofrecen.

En la consulta pública del **29 de octubre de 2026**, Descarga contiene
**0.10.8-beta**. El APK 0.10.10 se entrega al mantenedor para su publicación
manual. La pantalla no ofrece una actualización si solo encuentra una
versión igual o inferior a la instalada; la disponibilidad debe comprobarse
con **Buscar ahora** después de publicar el nuevo archivo.

## Publicar una futura actualización

Para publicar esta entrega, el mantenedor adjunta manualmente el APK a la
release existente **Descarga** con el nombre **`sma3-recomp-0.10.10-beta-arm64.apk`**, versión
interna **`0.10.10-beta`** y código **21**. Sus datos de integridad e instalación
están en las [notas de la entrega](releases/v0.10.10-beta.md).

Para preparar una actualización posterior que puedan detectar los usuarios
de 0.10.10:

1. Compila una versión posterior, por ejemplo `versionName = '0.10.11-beta'`
   y **`versionCode = 22`**, superior al 21 actual. Incrementa ambos respecto
   a la última entrega distribuida; no basta con renombrar el mismo APK.
2. Conserva **`org.sma3.recomp.experimental`**, la arquitectura ARM64 y
   **la misma clave de firma**. Una compilación firmada con otra clave
   no puede actualizar esta línea de APK.
3. Nombra el archivo exactamente **`sma3-recomp-0.10.11-beta-arm64.apk`**,
   usando el `versionName` real del APK.
4. Adjunta ese APK a la release existente **Descarga** y espera a que GitHub
   termine de procesarlo. Los APK anteriores pueden permanecer allí.
5. Desde 0.10.10, abre **Actualizar → Buscar ahora**, comprueba la versión
   ofrecida, descarga e instala en un dispositivo de prueba. Comprueba
   después partidas, preferencias, mods y controles.

El APK 0.10.10 se entrega por separado para esa publicación manual. Publicar
el código y estas notas en `main` no adjunta el APK a Releases.

## Validaciones y fallos recuperables

Las consultas y transferencias usan HTTPS y destinos de GitHub permitidos.
El archivo inicial debe proceder de Descarga en este repositorio. Se rechazan
redirecciones a HTTP u otros orígenes. No se utilizan credenciales del usuario
ni se envían partidas, ROM o BIOS.

La descarga se limita al tamaño anunciado y a **128 MiB**. Si GitHub publica
un digest SHA-256, debe coincidir. También se guarda un SHA-256 local del
archivo completo y se vuelve a calcular antes de instalarlo. Se comprueban:

- paquete de aplicación esperado;
- versión interna coincidente con el nombre publicado y código superior
  al instalado;
- requisito de Android compatible con el dispositivo;
- bibliotecas nativas ARM64 esperadas;
- certificados firmantes coincidentes con los de la app instalada.

Estas son comprobaciones previas. **Android realiza la validación final de
la firma y la instalación.** La app no sustituye al instalador ni intenta
eludir su confirmación. El archivo se guarda en la caché privada y se
comparte únicamente para lectura mediante un proveedor no exportado.

Los errores de red no borran partidas ni alteran el juego. Los archivos
parciales se limpian; las descargas dañadas o incompatibles se rechazan.
Las comprobaciones automáticas fallidas esperan al menos quince minutos
antes de volver a intentarlo. Se respeta el plazo adicional de espera si
GitHub limita las peticiones. La app utiliza ETag para evitar transferir
otra vez metadatos que no han cambiado.

Si se publica otra versión mientras queda una descarga anterior preparada,
la tarjeta **Instalar actualización** muestra la versión de ese archivo,
y la tarjeta de descarga muestra la versión recién detectada.

## Pruebas y límites

[UPDATES_0.10.5_VALIDATION.md](../validation/UPDATES_0.10.5_VALIDATION.md)
conserva la validación del actualizador introducido en 0.10.5: política de
versiones, respuestas de la release real, archivos futuros simulados,
redirecciones, tamaño, digest y limpieza.
Los casos de versiones futuras modifican metadatos locales de prueba; no
se ha publicado un APK ficticio en GitHub.

La compilación Java/ARM64 y el manifiesto del APK se verifican por separado.
No se ha ejecutado una actualización completa con el instalador de un
teléfono durante esta entrega. La prueba física de permisos, descarga,
instalación y conservación de datos sigue pendiente.

Referencias oficiales:

- [GitHub REST: obtener una release por etiqueta](https://docs.github.com/en/rest/releases/releases#get-a-release-by-tag-name).
- [Android: permiso para solicitar instalación de paquetes](https://developer.android.com/reference/android/content/pm/PackageManager#canRequestPackageInstalls()).
- [Android: permitir instalaciones por aplicación](https://developer.android.com/studio/publish#publishing-unknown).
