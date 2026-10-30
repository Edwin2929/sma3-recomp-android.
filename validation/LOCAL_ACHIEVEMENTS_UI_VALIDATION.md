# Logros locales — interfaz y persistencia de Android 0.10.9 beta

Resultado: **PASS**. Se ejecutaron **598 comprobaciones del modelo** y
**6 pruebas de interfaz/persistencia con Robolectric 4.16.1**, API 34 y
gráficos nativos. Las **32 fuentes Java de la aplicación y SDL** compilan
con JDK 17 y las firmas de Android API 35.

Esta validación usa evidencias nativas controladas. **No ejecuta el juego
ARM64, no comprueba la GPU de Android y no constituye una prueba física en
un teléfono.** La detección de acciones del juego tiene sus propias pruebas
nativas y de partidas.

## Interfaz incorporada

**Atrás → Logros** muestra los diez retos, sus criterios, progreso y fecha
de desbloqueo. Incluye un resumen global, filtros **Todos**, **Logrados** y
**Pendientes**, y una opción para silenciar los avisos. El menú y el catálogo
están traducidos al español, inglés y portugués.

Se inspeccionaron capturas de las vistas de producción en **360 × 760**,
**600 × 328**, **264 × 560** y **360 × 240 dp**. El resumen se compacta en
pantallas anchas; el contenido se desplaza y el botón **Volver** permanece
fuera de la zona desplazable. Las pestañas conservan objetivos táctiles de
al menos 48 dp y el texto del filtro cabe en el ancho reducido ensayado.
Las tarjetas distinguen visualmente los logros obtenidos y anuncian su
nombre, descripción, estado y fecha como una unidad accesible.

Las capturas contienen progreso de prueba y se conservan en el directorio
de salida del verificador; no representan una partida del usuario.

## Evidencia y reglas del modelo

El método JNI `getLocalAchievements()` devuelve un arreglo de ocho valores:

| Posición | Contenido | Combinación entre muestras |
| --- | --- | --- |
| 0 | Esquema `1` | Validación estricta |
| 1 | Máscara de hasta 60 niveles completados | Unión de bits |
| 2 | Máscara de niveles completados con 100 puntos | Unión de bits; subconjunto de los completados |
| 3 | Máscara de cinco tipos de transformación | Unión de bits |
| 4 | Mejor cantidad observada de monedas rojas, 0–20 | Máximo |
| 5 | Mejor cantidad observada de flores, 0–5 | Máximo |
| 6 | Mejor cantidad observada de estrellas, 0–30 | Máximo |
| 7 | Evento de recuperación natural de Fuzzy, bit 0 | Unión de bits |

No se acumulan cantidades de flores o monedas de distintos niveles para
obtener los retos de un solo nivel. Repetir un nivel no aumenta el número de
niveles distintos. Una muestra antigua o vacía no borra la evidencia
guardada. Los identificadores persistentes de los diez logros son estables;
las fechas se asignan una sola vez cuando se alcanza cada criterio.

Las 598 comprobaciones cubren los 60 bits de nivel, umbrales completos de
recolección, unión de transformaciones, preservación de fechas y preferencias,
inmutabilidad de la entrada, rechazos de esquemas y valores inválidos y
ausencia de avisos duplicados en el modelo.

## Persistencia y ciclo de vida

Los logros se almacenan en el espacio privado de la aplicación, en
`achievements/progress-v1.json`, separados de las partidas. Un único
trabajador serial, propiedad del proceso, lee y escribe el archivo; no se
realiza escritura de logros en el hilo de interfaz ni en el hilo del juego.
La instalación del payload no incluye este archivo y lo reconoce como un
archivo del jugador si ya existe.

Se muestrea el progreso aproximadamente una vez por segundo mientras la
actividad está en primer plano, y se encola una muestra adicional al
pausar. Los cambios se escriben mediante `AtomicFile`, sincronizan el
descriptor y comprueban el registro confirmado antes de anunciar un logro.
Solo se escribe cuando cambian los datos o la preferencia de avisos.

Un fallo de escritura conserva la evidencia y las fechas pendientes en
memoria y vuelve a intentar el guardado. El menú muestra que hay progreso
pendiente; no anuncia el logro como guardado hasta confirmar la escritura.
Un archivo corrupto o de esquema desconocido se conserva sin sobrescribirlo
con un catálogo vacío. La lectura vuelve a intentarse y puede recuperar el
funcionamiento cuando el archivo vuelve a estar disponible.

La recreación de la actividad y el cambio de idioma conservan el progreso.
Los avisos de logros antiguos no se repiten al iniciar o abrir el menú. Los
avisos pendientes de una actividad cerrada no reabren ventanas ni aparecen
después de pasar a segundo plano. Silenciar avisos no desactiva el registro.

## Casos de Robolectric

1. Catálogo, filtros, progreso, fechas, semántica accesible y distribución
   real en los cuatro tamaños descritos.
2. Fallos de escritura sucesivos, acumulación de evidencia pendiente,
   conservación de la fecha original y un único aviso después de guardar;
   la opción de silencio no impide desbloquear nuevos logros.
3. Progreso preexistente, recreación del ayudante, cambio a inglés y
   portugués, muestra final al pausar y ausencia de avisos o ventanas tardíos.
4. Archivo real de `AtomicFile`, datos corruptos, esquema futuro, preservación
   exacta de los archivos rechazados, recuperación posterior y respaldo `.bak`.
5. JNI no disponible, esquema nativo inválido, recuperación de una lectura
   fallida y ausencia de borrado o sobrescritura de progreso válido.
6. Dos solicitudes simultáneas se ejecutan por el mismo trabajador, fuera
   del hilo principal, y conservan la unión de sus resultados.

## Reproducción

```sh
python3 validation/run_achievements_ui_test.py \
  --java-home /ruta/jdk-17 \
  --android-jar /ruta/android-sdk/platforms/android-35/android.jar \
  --gradle /ruta/gradle-8.9/bin/gradle \
  --work-dir /tmp/sma3-achievements-ui-validation
```

El primer uso descarga Robolectric y JUnit desde sus repositorios Maven.
`--dependencies-from` permite reutilizar una carpeta de dependencias ya
resueltas de Robolectric 4.16.1; `--robolectric-cache` permite reutilizar su
framework Android instrumentado. Esas dependencias pertenecen a las pruebas
y no se incorporan al APK. Robolectric puede avisar que SDK 36 exige JDK 21;
la batería declara y ejecuta explícitamente SDK 34 con JDK 17.

El modelo también puede probarse sin Android:

```sh
javac -encoding UTF-8 -d /tmp/sma3-achievements-model \
  android/java/org/gbarecomp/AchievementsModel.java \
  validation/AchievementsModelTest.java
java -cp /tmp/sma3-achievements-model org.gbarecomp.AchievementsModelTest
```

## Límites

El archivo es local: no hay sincronización, cuentas, clasificación en línea
ni un sistema contra la manipulación del progreso. Borrar los datos de la
aplicación o desinstalarla puede eliminar los logros locales. Un cierre
abrupto antes de la siguiente muestra o de una escritura correcta puede
perder el progreso reciente que todavía no era durable; las pruebas no
certifican un corte de alimentación real. No se importan automáticamente
los logros de partidas antiguas. La fecha procede del reloj del teléfono.

[Modelo](../android/java/org/gbarecomp/AchievementsModel.java) ·
[Persistencia](../android/java/org/gbarecomp/AchievementsStore.java) ·
[Interfaz](../android/java/org/gbarecomp/AchievementsUi.java) ·
[Pruebas del modelo](AchievementsModelTest.java) ·
[Pruebas Android](AchievementsUiTest.java).
