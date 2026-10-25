# Android 0.10.2 beta — Fuzzy, panorámica e interfaz

## Resultado y alcance

Se investigó el reporte de que el titileo continuaba después de 0.10.1 y de
que tocar un Fuzzy reiniciaba el juego. Se reprodujeron causas diferentes:
rutinas ARM dinámicas sin cobertura nativa, cierre innecesario de todos los
márgenes por objetos ordinarios sin posición resuelta, y reconstrucción de
fondos animados a partir de datos que el juego ya había modificado.

La entrega incorpora esas correcciones, el nombre **Panorámica (beta)** con
advertencia de fase temprana, una interfaz de ajustes reorganizada y un
icono de huevo blanco con manchas verdes. Mantiene Original (3:2) y Estirar
como opciones independientes.

No se afirma que cualquier defecto visual de todos los niveles esté resuelto.
Se corrigen los casos reproducidos y se conservan restricciones explícitas
para los efectos que no tienen una ampliación verificada. Tampoco se ha
ejecutado esta entrega en un teléfono físico.

## Reinicio al tocar un Fuzzy

El binario intacto de 0.10.1 se detiene al intentar ejecutar una rutina ARM
copiada a RAM, también en Original. Faltaban dos rutinas del efecto de
desplazamiento del terreno. Ahora se generan como código nativo y se
comprueban la asignación activa, el callback, la alineación y todos sus
bytes antes de despacharlas. Se conserva la ejecución estática estricta.

| Prueba | Resultado |
| --- | --- |
| Contacto en la misma entrada de 1-7 que detenía 0.10.1 | El recorrido corregido completa 1.500 fotogramas, en 240 y 356 de ancho. |
| Efecto original | Efecto activo desde el fotograma 600; 782 fotogramas de efecto; recuperación natural en 1382. |
| Continuación normal | 119 fotogramas normales consecutivos al final, sin muerte ni reinicio. |
| Carga de un estado guardado durante el mareo | Otros 600 fotogramas en ambas anchuras; las 601 observaciones coinciden con el tramo continuo. |
| Rutinas ARM | 144 escenarios y 3.422 comprobaciones; registros, flags, memoria y ciclos iguales a la referencia ARM. |

El juego conserva el mareo y su recuperación. No se elimina el enemigo, no
se evita la colisión ni se ponen sus contadores a cero para simular una
solución. Las entradas iniciales de diagnóstico son sintéticas y privadas;
el contacto y la recuperación se producen mediante la lógica original.

[Causa, reproducción y límites de Fuzzy](FUZZY_VALIDATION.md) ·
[Registro de ejecución](android-0.10.2-fuzzy.json).

## Titileo y texturas en los márgenes

### Objetos ordinarios sin posición lateral

0.10.1 exigía resolver todos los objetos para mostrar cualquier parte del
escenario ampliado. Un objeto ordinario desconocido podía apagar ambos
márgenes durante uno o varios fotogramas. El recorrido del castillo usado
en aquella entrega no contenía ese caso.

Ahora el terreno y los objetos comprobados siguen visibles. Cada objeto
ordinario sin posición lateral resuelta se limita a los 240 píxeles
originales. Las ventanas de iluminación mantienen una política más estricta;
no se adivina la posición de una máscara desconocida.

### Fondos animados

El juego modifica algunos mapas BG3 completos después de cargarlos.
Reconstruirlos desde sus piezas originales descartaba esas modificaciones.
La corrección conserva el mapa nativo cuando se cumplen las condiciones
comprobadas: capa BG3, imagen de 32 filas de metatiles, mapa de 512 × 512
píxeles y screenblock 26. Los fondos mayores o cargados parcialmente
conservan su tratamiento específico.

### Efecto especial de Fuzzy

El desplazamiento de terreno de Fuzzy solo calcula las columnas del
encuadre original. Durante todo ese efecto se mantienen bandas negras
estables, con la imagen original intacta. Cuando el juego restaura sus
mapas, se recupera la panorámica automáticamente. La opción guardada no
cambia y el jugador no necesita volver a seleccionarla.

Las capturas reales de presentación SDL comparan cada imagen con Original:
tres recorridos de 900 fotogramas y el recorrido Fuzzy de 1.500. Los centros
coinciden en todos los pares. En Fuzzy se comprueba que los márgenes se
cierran una vez, permanecen cerrados durante el efecto y vuelven a abrirse
después de la restauración, sin cambiar la anchura de la superficie.

[Pruebas, oráculo de píxeles y límites de panorámica](PANORAMA_0.10.2_VALIDATION.md) ·
[Registro de presentación](android-0.10.2-panorama.json).

La calificación final también pasa **187 pruebas Python normales y 187 con
`-O`**, incluidas 18 pruebas adversarias del contrato de recuperación de
Fuzzy. Las regresiones de tiles, objetos, observadores de ventana y los
**488 escenarios** del compositor de ventanas pasan con ASan/UBSan. Los
72 archivos de entrada registrados permanecen idénticos durante esa ejecución.
[Resultados de regresión](android-0.10.2-regressions.json).

## Interfaz e icono

- Panel oscuro con acentos verdes, tarjetas por categoría y descripciones.
- Apartados Imagen, Rendimiento y FPS, Controles e Idiomas.
- Resolución en un submenú; botón de regreso separado del contenido desplazable.
- **Panorámica (beta)** y advertencia de errores de fase temprana en ES/EN/PT.
- Preferencias anteriores, mapeo de mando y diseños guardados conservados.
- Lanzador y editor con la misma paleta; icono vectorial de huevo adaptativo,
  versión convencional y versión monocroma para iconos temáticos.

La advertencia de Imagen dice:

> Es una fase temprana y todavía puede presentar errores visuales o de compatibilidad. Si encuentras un problema, usa Original (3:2).

Java y los recursos se compilaron con SDK 35. La revisión comprobó claves
de preferencias, navegación, contraste, unidades de texto escalables y
límites de tamaño. No se atribuyen pruebas de interacción a un emulador o
teléfono que no se haya utilizado.
[Informe de la interfaz](ANDROID_0.10.2_SETTINGS.md).

## Compilación, firma y evidencia

El APK es **`sma3-recomp-0.10.2-beta-arm64.apk`**, versión `0.10.2-beta`, código
**13**, paquete `org.sma3.recomp.experimental`, ARM64 y Android 9/API 28 mínimo.
Se conserva el certificado de las entregas anteriores para preparar una
actualización sobre ellas sin desinstalar.

El archivo entregado mide **27.690.180 bytes**, aproximadamente 27,7 MB.
SHA-256:

```text
06fe2ae0e041557ebaf1b12bcc4d6684a5ccea7bcd7d462fea0ee0404f5c829f
```

Se compilan las fuentes modificadas y las nuevas rutinas ARM de Fuzzy. El
archivo generado principal de 0.10 se reutiliza con su hash verificado; no
se afirma una nueva compilación completa de sus 64 fragmentos ni una nueva
generación completa desde ROM/BIOS. El motor mantiene los mismos ocho parches.

La comprobación del archivo entregado incluye firma v2, integridad ZIP,
alineación, arquitectura, coincidencia exacta de las bibliotecas actuales,
versión y paquete, enlaces JNI, textos beta, clase del panel de ajustes e
iconos. No se incluyen ROM, BIOS, partidas ni clave de firma.

El empaquetado incremental dejaba espacio ZIP sin utilizar. Se creó un
contenedor nuevo con los mismos archivos internos, se compactó, alineó y
firmó de nuevo con el certificado original. Se verificó que cada archivo
interno coincide byte por byte con la salida de la compilación y se repitió
la comprobación de firma e integridad sobre el APK entregado.

[Datos exactos y hashes del APK](android-0.10.2-build.json) ·
[Regresiones y fuentes verificadas](android-0.10.2-regressions.json) ·
[Instalación y publicación manual](../docs/releases/v0.10.2-beta.md).

## Límites pendientes

La panorámica no modifica los límites originales de aparición y dibujo de
objetos: algunos pueden aparecer tarde o faltar en los márgenes. Los efectos
sin una ampliación comprobada usan el encuadre original con bandas. Las
pruebas descritas no cubren todos los niveles, jefes ni posibles secuencias
de juego. La mezcla temporal puede producir estelas y no genera nuevos
estados independientes del juego.

La instalación, la GPU, la navegación táctil, la rotación y el rendimiento
de este APK deben contrastarse en el dispositivo. El reporte anterior del
Pixel 7 Pro no se traslada automáticamente a esta nueva entrega.
