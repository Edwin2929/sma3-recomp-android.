# Android 0.10.2 — nueva interfaz de ajustes

## Cambios

Los ajustes usan un panel nativo oscuro con acentos verdes, tarjetas de
navegación, descripciones breves y un botón de regreso separado del contenido
desplazable. El ancho se limita a 600 dp y la altura disponible se recalcula
al rotar. Los textos usan unidades escalables; las descripciones acompañan a
la selección y no dependen únicamente del color.

| Sección | Opciones |
| --- | --- |
| Imagen | Original (3:2), Panorámica (beta), Estirar a pantalla completa; submenú de resolución Automática/720p/1080p. |
| Rendimiento y FPS | Contador FPS y mezcla temporal a 120 FPS, con explicación de sus posibles estelas y requisitos de pantalla. |
| Controles | Visibilidad táctil, tres diseños personalizados, distribución predeterminada y mapeo del mando. |
| Idiomas | Idioma del menú y traducción parcial del juego, configurados por separado. |

**Panorámica (beta)** sustituye el rótulo «experimental» en español, inglés y
portugués. El selector incluye un aviso visible de fase temprana y posibles
errores visuales o de compatibilidad. Al seleccionarla, el aviso también se
anuncia a los servicios de accesibilidad. No se añade una confirmación modal
repetida.

El lanzador adopta la misma paleta, muestra el icono de la aplicación y
traduce botones, importación y errores según el idioma guardado del menú.
Su aviso beta está localizado; no muestra directamente la nota de metadatos
en un idioma distinto. Los botones del pie se apilan en pantallas estrechas.
El editor táctil conserva su comportamiento y usa la misma paleta verde.

## Preferencias y comportamiento conservados

La comparación del código anterior y nuevo encuentra las mismas once
claves o prefijos de preferencias de ajustes:

```text
game-language, image-mode, interpolation-120, menu-language, pad-,
show-fps, stretch-screen, touch-design-, touch-design-active,
touch-visible, video-quality
```

Los métodos `imageMode`, `padBinding`, `applyInputOptions`, `storedDesign`
y `applyTouchDesign` permanecen sin cambios. Por tanto, se conserva la
migración desde Estirar, los valores JNI de los tres modos, los botones
asignados y los diseños guardados. La importación mantiene las comprobaciones
de tamaño y SHA-1 y los destinos de archivos originales.

Al navegar hacia un submenú, se establece primero el diálogo nuevo antes de
cerrar el anterior. Al cancelar una página, el padre se abre antes de su
callback de cierre, para conservar el bloqueo de entrada durante la
navegación. Cerrar el panel principal libera la entrada. Los ajustes no se
describen como una pausa completa del motor; el editor de controles sigue
usando su mecanismo específico de pausa.

## Verificación realizada

- `:app:compileDebugJavaWithJavac`, Gradle 8.9, SDK 35/JDK 17: **correcto**.
  La ejecución final terminó en siete segundos, con dos tareas ejecutadas
  y quince actualizadas. Se registraron las huellas de las cuatro fuentes
  Java antes y después de compilar, sin cambios durante la ejecución.
- Revisión de diferencias: sin errores de espacios en las fuentes Java.
- Comparación de preferencias: ninguna clave añadida ni eliminada.
- Contraste calculado de la paleta: texto principal/tarjeta **13,43:1**;
  texto secundario/tarjeta **8,29:1**; botón verde/texto **11,97:1**;
  aviso ámbar/fondo **9,90:1**.
- Revisión de rutas de regreso, selección inicial antes de registrar
  listeners, actualizaciones de preferencias y adaptación de tamaño.

La comprobación es de compilación y revisión del código. No se ejecutó
esta interfaz en un emulador ni en un teléfono físico durante esta revisión;
no se atribuyen capturas o resultados de interacción a un dispositivo.

Fuentes: [`SettingsPanel.java`](../android/java/org/gbarecomp/SettingsPanel.java),
[`GbaGameActivity.java`](../android/java/org/gbarecomp/GbaGameActivity.java),
[`GbaSetupActivity.java`](../android/java/org/gbarecomp/GbaSetupActivity.java) y
[`TouchDesignEditor.java`](../android/java/org/gbarecomp/TouchDesignEditor.java).
