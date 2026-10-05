# Transformaciones, mando y controles táctiles — 0.5 experimental

El código de la burbuja (ROM 080DA1A2–080DA1CE) copia 0x4B4 bytes de
08040CE0 a una asignación dinámica y publica su dirección Thumb en 03006394.
La versión 0.4 no tenía cobertura nativa para esta rutina. En modo estricto,
entrar a esa dirección produce una falta de despacho.

La versión 0.5 añade una compilación nativa reubicable de esa rutina. Verifica
la dirección, alineación, longitud y todos sus bytes frente a la ROM antes de
entrar. Mantiene las direcciones reales de instrucciones, accesos al pool de
literales y reanudaciones. No activa interpretación como solución alternativa.

La herramienta `tools/generate_morph.py` genera únicamente esta rutina auditada;
no es un reubicador genérico. `validation/morph_test.cpp` compara los registros
y la memoria resultantes con el intérprete de referencia en varias direcciones
RAM y entradas. Esto no sustituye una prueba completa de cada transformación
en el teléfono.

## Opciones con el botón Atrás

- **Mostrar controles táctiles**: ocultar o mostrar la cruceta y los botones;
  al ocultarlos se liberan los contactos activos. Se conserva al reiniciar.
- **Mapear mando**: elegir una función GBA y asignarle un botón del vocabulario
  estándar SDL (Xbox/PlayStation). Permite dejarla sin asignar y restablecer
  el mapa predeterminado. Se conserva al reiniciar y reconectar el mando.
- El stick izquierdo también mueve a Yoshi, con zona muerta. Los gatillos
  analógicos conservan las funciones auxiliares existentes del motor.
- La navegación del menú no envía pulsaciones al juego; el juego sigue
  ejecutándose. El mando debe estar emparejado con Android.

La mezcla experimental de 120 FPS permanece opcional y desactivada por defecto.
La versión usa el mismo identificador y firma que 0.4 para instalarse como una
actualización sin borrar las partidas ni los archivos importados.
