# Validación de logros locales — Android 0.10.9 beta

Fecha local: **29 de octubre de 2026, El Salvador**. Las últimas compilaciones
y comprobaciones corresponden al 30 de octubre en UTC. Resultado de las
pruebas descritas: **PASS**.

La entrega incorpora diez logros locales, interfaz, avisos opcionales y
persistencia independiente de la partida. El APK está compilado y firmado.
**No se ha instalado ni probado físicamente esta versión en un teléfono.**

## Resumen de resultados

| Área | Comprobación | Resultado |
| --- | --- | --- |
| Detector nativo | 7 grupos, 166.657 comprobaciones internas | PASS |
| Sanitizadores | ASan y UBSan sobre la prueba nativa | PASS |
| Modelo Android | 598 comprobaciones | PASS |
| Interfaz y persistencia | 6 pruebas Robolectric 4.16.1, API 34 | PASS |
| Compilación Java | 32 fuentes de aplicación y SDL, API 35/JDK 17 | PASS |
| Partidas reproducidas | 5 configuraciones, 7.258 fotogramas del candidato | PASS |
| Comparación con 0.10.8 | 256 estados y 251 pares de imágenes | PASS |
| Ejecutor histórico | 6 pruebas de límites, con su contrato predeterminado intacto | PASS |
| APK | Firma, manifiesto, 20 JNI, bibliotecas y contenido | PASS |

Las aserciones internas no equivalen a ese número de partidas o niveles.
El informe estructurado está en
[android-0.10.9-achievements.json](android-0.10.9-achievements.json).

## Detector y protección frente a falsos positivos

La observación se ejecuta en el hilo del juego y solo lee memoria. Publica
un snapshot coherente de ocho enteros: esquema, máscaras de niveles,
transformaciones, máximos de objetos y eventos. Android no inspecciona RAM
desde su hilo de interfaz. Los fotogramas sin cambios no asignan memoria,
no toman el mutex de publicación y no escriben archivos.

Las pruebas recorren los identificadores de niveles y transformaciones,
límites de contadores, escenas admitidas y entradas inválidas. Verifican que
un estado cargado, un fotograma repetido, un cambio de época de restauración,
un salto de observación o un récord antiguo no concedan logros automáticamente.
Los niveles distintos se combinan mediante bits; repetir uno no suma otro.
Los máximos de flores y rojas corresponden a cantidades observadas dentro de
un nivel, no a una suma de recogidas de varios niveles.

La puntuación perfecta requiere una victoria observada y la confirmación
del resultado actual de cien puntos. No se usa la tabla de récords anterior.
La recuperación de Fuzzy exige contacto, desarrollo del efecto y su final
natural; una cancelación brusca o una carga directa del final no son suficientes.

ASan y UBSan pasan. LeakSanitizer se desactiva por las restricciones de
inspección de procesos de este entorno; no se presenta como una prueba de fugas.

[Detalle técnico, direcciones, fuentes y reproducción](LOCAL_ACHIEVEMENTS_NATIVE.md) ·
[Prueba nativa](local_achievements_test.cpp).

### Corrección comprobada durante la integración de Fuzzy

El primer detector pasaba la prueba con observaciones obtenidas al terminar
cada orden TCP, pero no concedía Fuzzy en la ejecución real. La traza del
callback de la PPU mostró la causa: en un punto observaba el temporizador
**652 → 652 → 650**, mientras la otra fase de muestreo veía pasos individuales.

Se añadió una tolerancia acotada: dos actualizaciones se admiten únicamente
tras un callback consecutivo sin avance del temporizador. Se mantienen los
rechazos de saltos mayores, borrados forzados, cambios de escena, cargas y
pérdida de continuidad. La secuencia exacta y sus casos negativos tienen
regresión, y los replays finales a ambos anchos conceden el logro en la
recuperación real. No se modificó el efecto del juego para conceder el logro.

## Partidas reproducidas con el candidato final

Se compara el ejecutable final con el de 0.10.8 usando ROM, BIOS, estado,
entradas y ancho idénticos. Cada captura compara todos sus píxeles; también
coinciden CPU, bus, E/S, audio, guardado y metadatos. Solo se excluyen los
ocho bytes ya documentados del reloj de pared del RTC inactivo. Los tiempos
del PPU coinciden por separado. No hay faltas de despacho ni instrucciones
interpretadas en estas ejecuciones.

| Recorrido | Ancho | Fotogramas del candidato | Evidencia esperada y obtenida |
| --- | --- | --- | --- |
| Contacto y recuperación de Fuzzy | 240 | 1.900 | Solo el evento de recuperación |
| Contacto y recuperación de Fuzzy | 356 | 1.900 | Solo el evento de recuperación |
| Yoshi → coche → Yoshi | 356 | 1.526 | Primera transformación, sin premios adicionales |
| Meta, recuento y confirmación de cien puntos | 240 | 1.812 | Nivel completado y nivel con cien puntos |
| Recogida por contacto de una flor auténtica | 240 | 120 | Máximo de flores pasa de cero a uno |

Se ejecutaron cinco sesiones finales del candidato y dos referencias nuevas
para la puntuación y la flor: **7 ejecuciones nuevas y 9.190 fotogramas**.
Las otras tres referencias se reutilizaron después de comprobar hashes del
binario, ROM, BIOS, estado, configuración, entradas, ancho y finalización.
No se contabilizan como ejecuciones nuevas.

Fuzzy termina con 101 fotogramas normales consecutivos, sin muerte ni reinicio.
El logro se concede al recuperarse en el fotograma de juego 15.200. El ciclo
del coche termina en Yoshi normal. En la prueba de puntuación se observa
el paso de subestado `0x3D` a `0x3F`, con cien puntos, al confirmar el resultado.

La prueba de la flor usa el sprite auténtico `0FA` del nivel 1-1. Su contador
sube de cero a uno mediante la colisión original. Las cinco monedas rojas
y las 20,9 estrellas ya presentes al cargar no se importan como progreso.

### Procedencia y límites de los fixtures

Las entradas de Fuzzy y de la meta se preparan mediante estados privados
con una posición de entrada elegida. Para acercar a Yoshi a la flor se
modifican solamente su posición y velocidad; no se cambian la flor ni sus
contadores. El contacto y la recogida los ejecuta el juego.

El caso de cien puntos **prepara de forma sintética los contadores de
colección antes de la meta**. La meta, el recuento y la confirmación se
ejecutan con las rutinas reales. Esta prueba verifica la detección y el
aislamiento del observador; no demuestra una partida natural completa
recogiendo todos los objetos del nivel. Los umbrales de cinco flores,
veinte rojas, treinta estrellas y cinco niveles distintos también tienen
pruebas específicas del detector y del modelo.

Las imágenes, ROM, BIOS y estados privados no se publican ni se empaquetan
en el APK. El JSON registra sus hashes y los resultados.

### Particularidad del ejecutor durante el recuento

Una rutina nativa larga de carga de la pantalla de puntuación puede cruzar
dos VBlanks antes de regresar al servidor de pruebas. La siguiente orden
consume el VBlank pendiente sin avanzar el contador. En el recorrido
observado aparece exactamente un par de incrementos **+2, 0**.

El nuevo perfil explícito `--score-status` registra esos contadores brutos,
prohíbe retrocesos y permite como máximo un fotograma de adelanto. El
comparador exige que al terminar no quede un VBlank pendiente y que ambos
ejecutables produzcan la misma traza. Los perfiles habituales conservan
la comprobación estricta de un fotograma por orden. No se relaja la
continuidad de los callbacks del detector nativo.

[Comparador de logros](validate_local_achievements_gameplay.py) ·
[Ejecutor de sesiones](run_panorama_session.py).

## Interfaz, avisos y persistencia

El modelo combina evidencia sin retroceder, conserva la fecha inicial y
rechaza esquemas, máscaras o magnitudes inválidos. El almacenamiento usa
un trabajador serial por proceso, `AtomicFile`, sincronización y lectura
de confirmación. Un fallo de escritura conserva cambios pendientes en
memoria y los reintenta. Un archivo ilegible o de esquema futuro se conserva
y no se reemplaza con progreso vacío.

Las pruebas cubren guardado, reintentos, respaldo anterior, errores, JNI no
disponible, recreación de actividad, pausas, cambios de idioma y ausencia
de avisos duplicados. Los avisos solo se emiten tras confirmar el guardado.
El registro se protege durante la actualización del payload de la aplicación.

Se revisaron las vistas en **360 × 760, 600 × 328, 264 × 560 y 360 × 240 dp**.
Los gráficos de las pruebas son de Robolectric; sus snapshots nativos están
controlados por el arnés y no sustituyen una prueba en un dispositivo.

[Informe de interfaz y reproducción](LOCAL_ACHIEVEMENTS_UI_VALIDATION.md).

## APK final

- Archivo: `sma3-recomp-0.10.9-beta-arm64.apk`.
- Versión `0.10.9-beta`, código `20`, payload `0.10.9`.
- Tamaño: **27.968.792 bytes**.
- SHA-256: `9c2871ff541f9495660cb92128a7004ef9fe9dd49fcc298a388ced4a45a265e6`.
- Firma v2 y certificado anterior verificados; ARM64, API 28 mínimo y objetivo 35.
- Las veinte firmas JNI coinciden con DEX y conservan las diecinueve anteriores.
- Las tres bibliotecas nativas coinciden con la salida de Gradle.
- Los 1.807 insumos permanecieron idénticos antes y después del ensamblado.

El archivo grande de código de juego ARM64 se reutilizó desde 0.10.8 tras
comprobar su hash y los 102 insumos originales. El informe distingue esa
reutilización de una recompilación de todos los fragmentos.

[Compilación y verificación del APK](android-0.10.9-build.json) ·
[Notas de entrega](../docs/releases/v0.10.9-beta.md).

## Verificación pendiente

Queda probar instalación, logros, avisos, reanudación, rotación y rendimiento
en un teléfono real. No se ha completado el juego entero ni comprobado cada
jefe o transformación en una partida real. Esta entrega no incorpora
sincronización de logros ni exportación, y no demuestra 120 FPS en Android.
