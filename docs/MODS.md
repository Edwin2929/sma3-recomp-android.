# Mods de color — Android 0.10.4 beta

**Atrás → Mods** permite instalar paquetes `.gbamod`, ver sus datos,
activarlos, desactivarlos y eliminarlos. Los cambios se aplican durante la
partida y la selección se conserva entre sesiones.

La integración actual admite **perfiles de color para SMA3 USA**, mediante
el plugin `sma3.color-profile` incluido en el APK. Un perfil transforma los
colores de la imagen terminada y puede combinarse con cualquier filtro de
imagen. No modifica el archivo de ROM, las partidas ni la lógica del juego.

## Probar el ejemplo incluido

1. Instala el APK **0.10.4-beta** encima de la versión anterior.
2. Abre el juego y entra en **Atrás → Mods**.
3. Toca **Instalar ejemplo**. Aparece **Tono cálido**, inicialmente desactivado.
4. Abre **Tono cálido** y activa **Activar mod**.
5. Vuelve al juego para comparar. Desactívalo para recuperar los colores
   anteriores con el mismo filtro y modo de imagen.

El ejemplo aumenta suavemente el canal rojo y reduce el azul. Se incluye
como datos dentro del APK y no necesita una descarga adicional.

## Importar otros perfiles

Elige **Importar mod** y selecciona el archivo `.gbamod` en el selector de
Android. El archivo se copia a la aplicación, se valida y se instala
**desactivado**. La aplicación no modifica ni borra el documento original.
El tamaño máximo del archivo es **2 MiB**; se comprueba lo que realmente se
lee, aunque el proveedor de archivos anuncie otro tamaño.

El catálogo muestra nombre, versión, autor y estado. Al tocar un mod se abre
su descripción, el interruptor de activación y **Eliminar mod**. Eliminar
pide confirmar el nombre y desactiva el perfil si estaba en uso.

Solo se usa **un perfil a la vez**. Activar otro desactiva el anterior en la
misma operación. Importar un paquete no sustituye el perfil activo. Para
instalar otra versión del mismo identificador, elimina antes la anterior;
no se sobrescriben silenciosamente los paquetes instalados.

## Compatibilidad de esta versión

| Contenido | Compatibilidad |
| --- | --- |
| Perfil `.gbamod` para `sma3-usa` y `sma3.color-profile` | Admitido si pasa la validación del manifiesto y del archivo. |
| Ejemplo **Tono cálido** incluido | Admitido; se instala desde el propio menú. |
| Filtros de imagen, incluido Cinematográfico | Se pueden usar junto con el perfil activo. |
| Nuevos niveles, sprites, personajes, música o reglas de juego | Sin un plugin compatible en este APK; no se importan mediante este gestor. |
| Parches `.ips`, `.bps`, `.ups` o ROMs modificadas | No son paquetes de perfil de color compatibles. Cambiar la ROM invalida la identificación de esta recompilación estática. |
| Bibliotecas `.so`, scripts y plugins externos ejecutables | No se cargan desde un paquete. |
| Paquetes de otros juegos o plugins no incluidos | Se rechazan con un diagnóstico. |

El perfil actúa sobre la imagen presentada, también cuando está activada la
mezcla temporal. Mantiene negros los píxeles negros; las bandas de seguridad
panorámicas y las franjas cinematográficas siguen negras. Los controles y
el contador se dibujan después. La panorámica conserva su etiqueta **(beta)**,
su aviso y su cobertura previa; instalar un perfil no corrige escenas sin
cobertura ni amplía el campo de visión.

## Crear un perfil compatible

El ejemplo editable está en
[`mods/examples/tono-calido`](../mods/examples/tono-calido).
Copia esa carpeta para crear un perfil propio. Cambia `id`, `name`, `author`,
`description` y los coeficientes; usa un identificador propio para que los
dos perfiles puedan coexistir. Los identificadores y la versión deben ser
componentes simples, sin separadores de ruta; por ejemplo,
`mi.perfil.azul` y `1.0.0`.

La raíz del ZIP debe contener `manifest.toml`, sin una carpeta exterior.
El empaquetador incluido acepta además `README.txt` y `LICENSE.txt`. Todos
son archivos de texto UTF-8. No añadas la ROM, la BIOS ni recursos del juego.

Este es un manifiesto mínimo que aumenta ligeramente el azul:

```toml
format_version = 1
id = "mi.perfil.azul"
version = "1.0.0"
name = "Azul suave"
author = "Tu nombre"
description = "Aumenta suavemente el azul de la imagen."
license = "Tu licencia"
resolver = "declarative"
save_compatibility = "shared"

[[target]]
game_id = "sma3-usa"
rom_sha1 = "7352d2bd064d9ebaec579e264228aa21c7345b80"

[[feature]]
id = "colors"
name = "Color"
default_enabled = false

[[plugin]]
feature = "colors"
id = "sma3.color-profile"

[[option]]
feature = "colors"
id = "bb"
label = "Azul desde azul"
type = "integer"
default = 110
min = -200
max = 200
step = 1
```

Debe existir un solo destino, una sola función `colors` y un solo plugin
`sma3.color-profile`. Se admiten hasta nueve opciones de tipo `integer`,
con `min=-200`, `max=200`, `step=1` y el valor `default` dentro de ese rango.
El gestor usa esos valores declarados; no incorpora un editor de coeficientes
en Android.

| Canal de salida | Coeficientes aplicados a rojo, verde y azul de entrada |
| --- | --- |
| Rojo | `rr`, `rg`, `rb` |
| Verde | `gr`, `gg`, `gb` |
| Azul | `br`, `bg`, `bb` |

Los coeficientes tienen denominador **100**. Para el rojo, por ejemplo,
se calcula `(rr × rojo + rg × verde + rb × azul) / 100`, se redondea al
entero más cercano y se limita al intervalo 0–255. Los valores omitidos
son **100** en `rr`, `gg`, `bb` y **0** en los otros seis. No hay sumas
constantes: un píxel negro permanece negro. No se reduce la profundidad
de color de las imágenes intermedias de mezcla temporal.

### Empaquetar

Desde la raíz del repositorio, con Python 3:

```sh
python3 tools/package_color_mod.py mods/examples/tono-calido /tmp/tono-calido.gbamod
```

Sustituye la carpeta de origen por la de tu perfil y el nombre de salida por
otro archivo que termine en `.gbamod`. Si ya existe la salida, se requiere
`--replace` para reemplazarla expresamente. El resultado es un ZIP estándar
determinista: los mismos archivos de entrada producen los mismos bytes.
Transfiérelo al teléfono y usa **Atrás → Mods → Importar mod**.

La herramienta limita los archivos empaquetados. La validación semántica
definitiva del juego, el plugin y la matriz la hace el servicio nativo al
importar. Renombrar cualquier ZIP a `.gbamod` no lo vuelve compatible.

## Límites, errores y recuperación

El importador admite ZIP almacenado o deflate, hasta **2 MiB comprimidos**,
**1 MiB descomprimido**, **32 entradas** y **64 KiB de manifiesto**. Solo
admite el manifiesto y archivos README/LICENSE en la raíz. Comprueba CRC,
rutas, estructura, destino, plugin y coeficientes. El catálogo admite hasta
**64 perfiles**.

Las operaciones se serializan. La configuración se guarda antes de publicar
la matriz que usará el siguiente fotograma; un error de validación o guardado
conserva la selección anterior. Si no se puede activar un archivo, el menú
muestra el diagnóstico. Un paquete instalado dañado se identifica como
incompatible y puede eliminarse. Una selección de inicio ilegible vuelve a
colores normales con un aviso, permitiendo continuar con el juego base.

Los títulos, controles y mensajes habituales están en español, inglés y
portugués. Algunos diagnósticos técnicos del servicio nativo se muestran en
español o inglés aunque se haya elegido otro idioma para el menú.

## Comprobaciones y alcance

El [informe de mods](../validation/MODS_0.10.4_VALIDATION.md) registra las
pruebas del importador, persistencia, recuperación, concurrencia y píxeles.
La [revisión de Android](../validation/ANDROID_0.10.4_MODS_SETTINGS.md)
documenta la compilación Java, la transferencia acotada y el ciclo de vida.
El [registro del APK](../validation/android-0.10.4-build.json) comprueba
el ejemplo incluido, los enlaces JNI, la versión y la firma.

Estas pruebas de desarrollo no equivalen a ejecutar el APK físicamente en
el Pixel 7 Pro, ni acreditan rendimiento o cobertura completa del juego.
