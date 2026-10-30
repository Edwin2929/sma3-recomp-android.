# Validación de 25 logros locales — Android 0.10.10 beta

Fecha local: **29 de octubre de 2026, El Salvador**; compilación y pruebas
del **30 de octubre en UTC**. Resultado: **PASS** para el alcance descrito.

El catálogo contiene exactamente **25 logros**: conserva los diez de 0.10.9
y añade quince. El APK está compilado y firmado. **No se ha instalado ni
probado físicamente esta versión en un teléfono.**

## Resultados nuevos de esta entrega

| Área | Comprobación | Resultado |
| --- | --- | --- |
| Modelo de progreso | 2.030 comprobaciones, 25 IDs únicos y orden anterior conservado | PASS |
| Registro anterior auténtico | 1 prueba con el encoder de 0.10.9 obtenido de su commit | PASS |
| Interfaz y almacenamiento | 10 pruebas Robolectric 4.16.1, API 34 | PASS |
| Compilación Java | 32 fuentes de aplicación y SDL, API 35 y JDK 17 | PASS |
| Archivos inválidos | 16 variantes incompletas, incompatibles o con máscaras inválidas preservadas | PASS |
| Vistas | 6 capturas; cuatro tamaños, reconocimiento al actualizar y último logro | PASS |
| APK | Firma, manifiesto, 25 IDs en DEX y 20 firmas JNI | PASS |
| Identidad nativa | 1.762 insumos y tres bibliotecas iguales a 0.10.9 | PASS |

Los casos de archivos inválidos y las capturas forman parte de las diez
pruebas Robolectric; no son suites adicionales. Las comprobaciones internas
del modelo tampoco equivalen a partidas jugadas.

[Informe estructurado](android-0.10.10-achievements.json) ·
[Interfaz, migración y reproducción](LOCAL_ACHIEVEMENTS_25_UI_VALIDATION.md).

## Criterios y compatibilidad

Los quince criterios nuevos son tres umbrales de recolección, cuatro de
niveles distintos, tres de niveles distintos con cien puntos y cinco vehículos
individuales. El predicado de progreso se comparte entre desbloqueo,
presentación y migración. Los vehículos usan un bit concreto; una máscara
numéricamente mayor no concede vehículos ausentes.

Se comprueban las 32 combinaciones posibles de los cinco bits de vehículo,
los umbrales de colecciones, los 60 bits de niveles y sus uniones. Repetir un
nivel no aumenta el número de niveles distintos o perfectos. La máscara
completa de avisos de 25 logros, incluido el bit 24, se conserva y permite
agrupar desbloqueos en un único aviso.

El snapshot JNI mantiene esquema 1 y ocho enteros. El archivo local pasa a
esquema 2 y conserva su ruta protegida, `achievements/progress-v1.json`.
La lectura del formato anterior exige las diez identidades originales; el nuevo exige
las 25 y la procedencia de los reconocimientos al ampliar el catálogo.
Un archivo incompleto de esquema 2 no se interpreta como un archivo anterior.

## Actualización desde 0.10.9

La prueba no construye únicamente un JSON parecido al antiguo. Se compilaron
el modelo y el encoder reales del commit
`2c5dbc94a707460da3ae1aac1b93cb6d2fcdaa32` para generar un registro de prueba.
El texto exacto y su hash están incluidos en
[AchievementsUiTest.java](AchievementsUiTest.java).

El registro contiene ocho logros anteriores obtenidos, diez fechas originales
incluidos los ceros de pendientes, siete métricas y avisos silenciados. Su
evidencia satisface nueve de los quince criterios nuevos. La actualización
produce **17 de 25 logros**, conserva las diez fechas originales y las siete
métricas, mantiene los avisos silenciados y reconoce solo esos nueve retos.

Las fechas nuevas corresponden al momento del reconocimiento y se etiquetan
en la interfaz. La actualización se guarda incluso si la biblioteca nativa
no está disponible o no aporta evidencia nueva. Al recrear el almacenamiento,
las fechas y el contenido ya confirmado permanecen iguales; los avisos de la
ampliación no se repiten.

Se provocaron fallos de escritura, reintentos y recreación antes y después
de confirmar el archivo. Mientras falla la escritura, se conserva el registro
anterior y las fechas pendientes en memoria. Si el proceso se pierde antes
de confirmar, la siguiente instancia puede volver a reconocer los retos
desde el archivo anterior intacto. No se atribuye a una fecha no guardada
la misma durabilidad que a una fecha confirmada.

Los archivos corruptos, de formato futuro, con campos ausentes o con marcas
de reconocimiento inválidas se preservan. También se comprueban un registro
anterior vacío, preferencias de avisos activadas y desactivadas, cambios de
idioma, respaldo `AtomicFile`, actividad en segundo plano y escrituras
serializadas fuera del hilo principal.

## Interfaz revisada

Las vistas se comprueban en **360 × 760, 600 × 328, 264 × 560 y 360 × 240 dp**.
Otras dos capturas muestran los logros reconocidos al ampliar el catálogo y
la tarjeta final de **Submarino**. La lista permite llegar a los 25 registros,
los filtros reflejan sus cantidades y **Volver** permanece fuera de la zona
desplazable. Se comprueban las etiquetas de fecha en español, inglés y
portugués.

Las capturas proceden de Robolectric con evidencia controlada. No son
capturas de una partida del usuario ni del Pixel 7 Pro.

## Evidencia nativa reutilizada

El detector, los callbacks, JNI, el motor y el renderizado permanecen sin
cambios. Se verificaron los hashes de los **1.762 insumos nativos** y las
tres bibliotecas del APK son idénticas byte por byte a las de 0.10.9.
La ampliación utiliza evidencia ya observada y no añade lecturas de memoria
por fotograma.

Se conservan las pruebas históricas del detector y los cinco replays de
0.10.9 con sus límites documentados. **Esta entrega no añade nuevas ejecuciones
de juego**, recorridos de vehículos ni una partida completa. La identidad
de coche, tanque topo, helicóptero, tren y submarino se contrasta con el
desensamblado original en la
[auditoría de los nuevos criterios](LOCAL_ACHIEVEMENTS_25_NATIVE_AUDIT.md).

[Pruebas históricas de 0.10.9](LOCAL_ACHIEVEMENTS_0.10.9_VALIDATION.md).

## APK final

- Archivo: `sma3-recomp-0.10.10-beta-arm64.apk`.
- Versión `0.10.10-beta`, código `21`, payload `0.10.10`.
- Tamaño: **27.972.888 bytes**.
- SHA-256: `ac157e1ae2b31b32d09ec19dda87412c1285a9a62fc1f7c413100de7c15e7524`.
- Paquete `org.sma3.recomp.experimental`, ARM64, mínimo API 28 y objetivo 35.
- Certificado anterior, firma v2, CRC y alineación verificados.
- Los 25 IDs están en DEX; las 20 firmas JNI conservan sus tipos.
- Los 1.807 insumos de ensamblado coinciden antes y después de compilar.
- Los tres archivos Java de producción coinciden con los hashes congelados
  usados en la compilación y las pruebas.

El archivo grande de código de juego se reutilizó desde la entrega anterior,
con compilación completa original de 0.10.8 e insumos verificados. La
identidad binaria se comprueba explícitamente; no se presenta como una
recompilación de todo el juego.

[Informe de compilación y paquete](android-0.10.10-build.json) ·
[Notas de entrega](../docs/releases/v0.10.10-beta.md).

## Límites

Queda comprobar la actualización e interacción en un teléfono real. No se
ha completado el juego entero, probado cada vehículo en una sesión nueva ni
medido FPS Android. Esta entrega no añade sincronización o exportación de
logros. La panorámica conserva su etiqueta beta y la cobertura anterior.
