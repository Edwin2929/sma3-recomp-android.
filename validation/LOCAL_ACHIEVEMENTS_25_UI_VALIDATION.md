# Validación Android: ampliación de 10 a 25 logros locales

Resultado: **PASS** para el modelo, la migración del registro local y la interfaz de **0.10.10-beta**. Se conserva el contrato nativo de 0.10.9. Las pruebas Android de este informe se ejecutaron en el host con Robolectric; no representan una prueba física del APK.

## Alcance y resultados

| Comprobación | Resultado |
| --- | --- |
| Modelo Java independiente | **2.030 comprobaciones**, 25 definiciones |
| Pruebas Android con Robolectric 4.16.1 / API 34 | **10 tests**, todos aprobados |
| Encoder real de 0.10.9 recompilado para producir el fixture | **1 test**, aprobado |
| Compilación de todas las fuentes Java Android y SDL contra API 35 | **32 fuentes**, aprobada con JDK 17.0.21+9 |
| Registros incompletos, futuros o con metadatos inválidos | **16 variantes** rechazadas sin sobrescribir sus bytes |
| Capturas de la interfaz de producción | **6 renders** revisados, con desplazamiento y botón Volver accesible |

Las pruebas reproducibles están en [AchievementsModelTest.java](AchievementsModelTest.java), [AchievementsUiTest.java](AchievementsUiTest.java) y [run_achievements_ui_test.py](run_achievements_ui_test.py). El contrato y las correspondencias del motor se documentan por separado en [LOCAL_ACHIEVEMENTS_25_NATIVE_AUDIT.md](LOCAL_ACHIEVEMENTS_25_NATIVE_AUDIT.md). No se repitieron partidas de validación nativa para esta ampliación de catálogo.

## Catálogo y criterios

Los diez primeros IDs, su orden y sus nombres en español, inglés y portugués se conservan. Se añaden exactamente estos quince al final:

| Índice interno | ID estable | Nombre en español | Criterio |
| --- | --- | --- | --- |
| 10 | `three_flowers` | Trío de flores | 3 flores en un mismo nivel |
| 11 | `five_red_coins` | Cinco monedas rojas | 5 monedas rojas en un mismo nivel |
| 12 | `ten_red_coins` | Diez monedas rojas | 10 monedas rojas en un mismo nivel |
| 13 | `ten_clears` | Diez aventuras | 10 niveles distintos |
| 14 | `twenty_clears` | Veinte aventuras | 20 niveles distintos |
| 15 | `thirty_clears` | Treinta aventuras | 30 niveles distintos |
| 16 | `forty_clears` | Cuarenta aventuras | 40 niveles distintos |
| 17 | `five_perfect_clears` | Cinco niveles perfectos | 5 niveles distintos con 100 puntos confirmados |
| 18 | `ten_perfect_clears` | Diez niveles perfectos | 10 niveles distintos con 100 puntos confirmados |
| 19 | `twenty_perfect_clears` | Veinte niveles perfectos | 20 niveles distintos con 100 puntos confirmados |
| 20 | `car_transformation` | Coche | Máscara de transformación `1` |
| 21 | `mole_transformation` | Tanque topo | Máscara de transformación `2` |
| 22 | `helicopter_transformation` | Helicóptero | Máscara de transformación `4` |
| 23 | `train_transformation` | Tren | Máscara de transformación `8` |
| 24 | `submarine_transformation` | Submarino | Máscara de transformación `16` |

El modelo usa el mismo cálculo de progreso para mostrar cada tarjeta, conceder el logro durante el juego y reconocerlo al migrar. Los cinco vehículos comprueban su bit concreto. Las pruebas recorren las **32 combinaciones** de vehículos y los **60 bits de niveles**; repetir un nivel no incrementa el número de niveles distintos. Las máscaras de avisos admiten los **25 bits** (`0x1ffffff`), incluido Submarino en el bit 24.

## Migración del registro anterior

El snapshot JNI sigue siendo de **esquema 1 y ocho valores `long`**. El archivo local cambia a **esquema 2** y mantiene su ubicación `achievements/progress-v1.json`; el nombre se conserva para respetar la protección de datos de la instalación existente.

El lector de esquema 1 exige los diez IDs originales y las siete métricas originales. El lector de esquema 2 exige los 25 IDs y `recognized_on_upgrade`. Un archivo de 25 IDs incompleto no se interpreta como un registro antiguo. Un esquema futuro, una preferencia de avisos de tipo incorrecto, fechas incoherentes o una máscara fuera de rango impiden la escritura del archivo. La máscara de reconocimiento solo admite los índices 10–24 y solo puede marcar logros respaldados por evidencia con fecha positiva.

Durante la actualización se conservan las siete métricas, las diez fechas anteriores y la preferencia de avisos. Los nuevos retos que ya estén respaldados por ese registro local se reconocen con la fecha de la migración. Su tarjeta indica **«Reconocido al ampliar los logros»**; no se presenta esa fecha como la fecha histórica de la acción. Cargar una partida avanzada no importa sus récords al registro de logros.

La migración queda marcada como pendiente de escritura y se guarda aunque no haya evidencia nueva o la biblioteca nativa todavía no esté disponible. Se utiliza la cola de trabajo existente, `AtomicFile`, sincronización explícita y lectura de confirmación. Los reconocimientos derivados del registro anterior son la base de comparación de avisos, por lo que no generan una ráfaga de notificaciones al actualizar, tanto con los avisos habilitados como silenciados.

Si falla la escritura, el estado y las fechas pendientes se mantienen en memoria para el reintento. Si el proceso termina antes de una escritura correcta, el registro anterior permanece intacto y el siguiente proceso realiza la migración; la fecha de reconocimiento todavía no persistida puede ser posterior. Después de guardar correctamente, reiniciar o recrear la interfaz conserva todas las fechas y no vuelve a anunciar logros antiguos.

### Fixture producido por el encoder real de 0.10.9

Se extrajeron y compilaron `AchievementsModel.java` y `AchievementsStore.java` del commit **`2c5dbc94a707460da3ae1aac1b93cb6d2fcdaa32`**. Un test en el mismo paquete llamó a su `Disk.encode()` real con estos datos:

```java
long[] metrics = {16777215, 511, 21, 10, 3, 30, 1};
long[] dates = {
    1700000000100L, 0, 1700000000200L, 0, 1700000000300L,
    1700000000400L, 1700000000500L, 1700000000600L,
    1700000000700L, 1700000000800L
};
String fixture = AchievementsStore.Disk.encode(
    new AchievementsModel.State(metrics, dates, false));
```

El resultado tiene **464 bytes** y SHA-256:

```text
e66c5b7e85bf7102fbe35b7918912cf160e9c68d92ef9a880c47f9337f104cc8
```

Los bytes exactos, incluido el salto de línea final, están incorporados en `AchievementsUiTest.java`. El test verifica ese hash antes de escribir el archivo y probar el Store real. Esto evita depender de un fixture privado y verifica el formato emitido por la versión anterior.

Este registro tiene ocho logros originales, 24 niveles distintos, nueve perfectos y las transformaciones coche/helicóptero/submarino. La actualización reconoce nueve logros nuevos y obtiene **17/25**: conserva las diez fechas originales exactamente, mantiene los avisos silenciados, añade las nuevas fechas sin alterar las siete métricas y persiste el archivo incluso con JNI ausente. Se comprueba también que cambiar el orden de las claves JSON no altera el resultado.

## Casos cubiertos

- Catálogo completo en los tres idiomas y filtros **Todos / Logrados / Pendientes** con estados parciales, migrados y completos.
- Progreso y fechas para todos los umbrales de monedas, flores, niveles y puntuaciones perfectas; unión de evidencia entre sesiones.
- Archivo real anterior, registro antiguo vacío y registro con todos los criterios satisfechos; persistencia y recuperación de todos los bits de reconocimiento.
- Reintentos de migración con escritura fallida, biblioteca nativa ausente y snapshot inválido; reinicio antes y después de la escritura correcta.
- Conservación de un registro futuro o incompleto; recuperación tras volver a disponer de un registro válido; recuperación desde copia de seguridad de `AtomicFile`.
- Un único aviso agrupado cuando una muestra controlada satisface los 25 criterios desde un registro vacío, sin repetirlo al recrear la interfaz; silencio cuando el usuario desactiva avisos.
- Cambio de idioma y ciclo de vida de la actividad sin repetir avisos antiguos; guardado al pausar y cancelación del muestreo al cerrar.
- Lecturas/escrituras serializadas fuera del hilo principal.

## Interfaz y capturas

Se generaron y revisaron estos renders Android a densidad `xhdpi`:

| Captura | Tamaño lógico | Comprobación |
| --- | --- | --- |
| `achievements-360x760.png` | 360 × 760 dp | Vertical, resumen 5/25 y tarjetas |
| `achievements-600x328.png` | 600 × 328 dp | Horizontal, resumen compacto y cuerpo desplazable |
| `achievements-264x560.png` | 264 × 560 dp | Ancho reducido, texto repartido en varias líneas |
| `achievements-360x240.png` | 360 × 240 dp | Altura extrema, título y botón Volver conservados |
| `achievements-upgrade-360x760.png` | 360 × 760 dp | Nuevas tarjetas con fecha de reconocimiento claramente identificada |
| `achievements-all25-600x328.png` | 600 × 328 dp | Desplazamiento hasta Submarino y fecha obtenida durante el juego |

El título y el botón Volver permanecen fuera del cuerpo desplazable. Los filtros y Volver tienen al menos 48 dp de altura. En el caso extremo de 240 dp de alto hace falta desplazar el cuerpo para acceder a filtros y tarjetas. No se oculta información del catálogo: las 25 tarjetas permanecen en la lista y los filtros cuentan sus criterios correctamente. Las descripciones de accesibilidad incluyen nombre, criterio, progreso y fecha correspondiente.

## Reproducción

Con JDK 17, Android 35 y Gradle 8.9 disponibles:

```sh
python3 validation/run_achievements_ui_test.py \
  --java-home /ruta/jdk-17 \
  --android-jar /ruta/android-sdk/platforms/android-35/android.jar \
  --gradle /ruta/gradle-8.9/bin/gradle \
  --work-dir /ruta/validacion-logros-25
```

El runner compila el modelo y las clases de producción, ejecuta las pruebas y escribe sus logs, `report.json` y capturas. Se pueden reutilizar dependencias con `--dependencies-from` y el framework instrumentado con `--robolectric-cache`. El aviso de que API 36 necesita Java 21 no afecta a estos tests, que solicitan explícitamente API 34.

## Límites

La compilación Java y los renders host no sustituyen una prueba de instalación o juego en un teléfono. El enlace JNI y la detección nativa conservan el contrato anterior; las pruebas de este informe inyectan snapshots controlados. Los fallos de persistencia probados son fallos controlados y recuperación de archivos; no equivalen a una campaña de cortes de alimentación o fallos físicos del almacenamiento. Los logros siguen siendo locales, sin cuenta, conexión ni sincronización; borrar los datos o desinstalar elimina el registro.
