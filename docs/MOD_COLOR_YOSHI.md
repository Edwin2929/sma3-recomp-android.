# Color de Yoshi — mod incorporado en 0.10.8 beta

**Atrás → Mods → Color de Yoshi** abre un selector para personalizar el
cuerpo de Yoshi. Está incluido en el APK; no requiere importar un paquete
ni cambiar la ROM.

## Elegir un color

1. Abre el selector durante una partida.
2. Elige uno de los **48 colores predefinidos**, ajusta las barras **Rojo,
   Verde y Azul** de 0 a 255, o escribe un código **HEX**.
3. Comprueba la muestra del color y sus tonos de sombra.
4. Pulsa **Aplicar color** y vuelve al juego. La elección se conserva al cerrar
   y abrir la aplicación.

Los códigos admitidos son `#RRGGBB` y su forma corta `#RGB`, con o sin el
signo `#`, usando mayúsculas o minúsculas. Por ejemplo:

| Color | Código |
| --- | --- |
| Rojo | `#FF304F` |
| Azul | `#3478F6` |
| Morado | `#A855F7` |
| Turquesa | `#00D4C8` |
| Rosa | `#FF69B4` |
| Blanco | `#FFFFFF` |
| Negro | `#000000` |

La forma corta `#F0A` equivale a `#FF00AA`. Un código incompleto o inválido
no se aplica. **Copiar** permite reutilizar el color elegido.

Los cambios del selector son un borrador hasta pulsar **Aplicar color**.
**Cancelar** o volver atrás descarta ese borrador. **Usar color original** desactiva el mod y
recupera el color que corresponde al juego, conservando el último color
personalizado para poder volver a usarlo.

## Qué cambia

El mod sustituye los tonos del **cuerpo de Yoshi** al dibujar sus sprites.
Conserva los contornos, las partes blancas y los zapatos. No cambia las
paletas de los fondos, enemigos, huevos o Baby Mario.

Se aplica al Yoshi normal durante el juego. Los vehículos de transformación,
el mapa y las escenas con gráficos no identificados conservan su color
original. La selección sigue guardada y vuelve a usarse al regresar al
Yoshi normal. Este alcance evita recolorear otros elementos que comparten
una paleta con el personaje.

El código HEX se guarda con sus canales de 8 bits. El juego utiliza colores
RGB de 5 bits por canal, por lo que la elección se aproxima al color
representable más cercano. Dos códigos muy próximos pueden producir el
mismo color dentro del juego. La muestra indica el color de la paleta; los
efectos de iluminación y los filtros pueden cambiar su apariencia final.

## Combinación con otros ajustes

**Color de Yoshi** tiene su propia activación y puede combinarse con un
perfil `.gbamod`, cualquiera de los siete filtros, los modos Original,
Panorámica (beta) y Estirar, y la presentación de 60/120. El perfil global
actúa después sobre la imagen completa, por lo que también puede modificar
el color personalizado. Desactívalo para comparar directamente los colores.

No se alteran los archivos de ROM, las entradas del jugador, la física, el
audio ni los datos de la partida. La preferencia pertenece a la aplicación:
cargar una partida no sustituye el color que elegiste en Ajustes.

La selección se incorpora al dibujo al comenzar un fotograma. No se realiza
una búsqueda de colores sobre todos los píxeles de la pantalla ni se añade
una pasada de escalado. Al cargar un estado instantáneo a mitad de un
fotograma puede aparecer primero la imagen guardada o un cuadro parcial;
la selección se actualiza al iniciar el siguiente fotograma. La panorámica
conserva su aviso de fase temprana y su cobertura existente.

## Instalación

Instala `sma3-recomp-0.10.8-beta-arm64.apk` sobre la versión anterior, sin
desinstalarla ni borrar sus datos. El mod está desactivado inicialmente.
Los APK 0.10.7 y anteriores no incluyen este selector; importar un perfil
global `.gbamod` en ellos no añade esta función.

[Gestor de mods](MODS.md) · [Notas de 0.10.8](releases/v0.10.8-beta.md).
