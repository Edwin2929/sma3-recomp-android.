# Validación — Android 0.10.11 beta

Fecha local: **29 de octubre de 2026, El Salvador**; pruebas y compilación
del **30 de octubre en UTC**. Resultado: **PASS** para los alcances descritos.

Se entregan la animación de desbloqueo, el logro número 26 **Yo shi pude**,
su continuidad mediante suspensión automática y una corrección comprobada
del cierre al iniciar los créditos. El APK está compilado y firmado con la
identidad anterior. No se ha probado esta versión en un teléfono físico.

## Resultados

| Área | Comprobación | Resultado |
| --- | --- | --- |
| Modelo Java | 2.055 comprobaciones; 26 IDs; criterios y máscaras | PASS |
| Interfaz y almacenamiento | 22 pruebas Robolectric API 34: 13 de modelo/registro/estado y 9 de animación | PASS |
| Migración auténtica | Registro emitido por el encoder real de 0.10.10; 25 identidades y fechas conservadas | PASS |
| Reto nativo | 1.473 comprobaciones del recorrido, incluidas pérdidas, 48 metas y rutas finales | PASS |
| Colector anterior | 166.657 comprobaciones de regresión | PASS |
| Comprobante de suspensión | 510 comprobaciones de formato, asociación al estado, consumo y fallos | PASS |
| Integración de persistencia | 18 comprobaciones, con fallos de `unlink` y `fsync` inyectados | PASS |
| Ciclo de vida real del motor | Tres casos: proceso terminado en segundo plano, reproducción de comprobante consumido y retorno en el mismo proceso | PASS |
| Escenas nativas del reto | Cuatro recorridos observados: inicio nuevo, final controlado, Superestrella y golpe | PASS |
| Fuzzy y coche | Dos replays nuevos frente a referencias anteriores verificadas; 73 imágenes idénticas | PASS |
| Rutina gráfica de créditos | 96 escenarios; 2.378 comparaciones y comprobaciones con referencia Thumb | PASS |
| APK | Firma v2, alineación, CRC, 26 IDs, 21 métodos JNI y fuentes congeladas | PASS |

Las cantidades de comprobaciones internas no equivalen a partidas jugadas.
Las capturas de interfaz y los casos de migración forman parte de las 22
pruebas de framework; no se cuentan como suites independientes.

[Informe estructurado](android-0.10.11-achievements.json) ·
[Interfaz y migración](LOCAL_ACHIEVEMENTS_26_STATE_VALIDATION.md) ·
[Auditoría del detector](YO_SHI_PUDE_NATIVE.md).

## Animación y conservación del registro

La interfaz de producción se compiló contra API 35 y se ejecutó bajo
Robolectric 4.16.1 con API 34. Se comprobaron entrada, expansión, lectura,
contracción y salida, los 26 avisos individuales, deduplicación, toque y
multitoque, límites de pantalla, giro, texto al 200 %, accesibilidad y
animaciones del sistema desactivadas. Durante la lectura estable no quedan
animadores solicitando nuevos dibujos. Una pausa o preferencia desactivada
retira los avisos pendientes; los callbacks antiguos no los repiten.

Se produjeron 18 capturas de prueba y se inspeccionaron las fases y las vistas
de ancho reducido. Son renders host de Android, no capturas de un Pixel.

El catálogo mantiene exactamente los 25 IDs, títulos y descripciones
anteriores. La migración conserva las fechas y las preferencias desde los
formatos 1 y 2, escribe formato 3 y añade el reto nuevo pendiente. El fixture
de 0.10.10 tiene **944 bytes** y SHA-256
`fdbb7044d0e6241a36bd460a32c0a372b500ec349d6db607453e21262537a790`.
Fue producido por el encoder anterior, no por una imitación del formato.

El snapshot nativo conserva ocho enteros y usa esquema 2. Los eventos Fuzzy
(`0x1`) y «Yo shi pude» (`0x2`) son independientes; uno no puede conceder el
otro. El estado del intento se consulta mediante un método separado y no
basta por sí solo para conceder un logro.

## Detección y escenas ejecutadas

El detector exige observar una creación auténtica de archivo desde el menú,
la introducción, las 48 metas principales y la llegada al final con continuidad.
Una partida cargada no acredita lo ocurrido antes. La pérdida por golpe y la
captura de enemigos son permanentes para ese intento, aunque Mario regrese.
Los vehículos, las entregas y el retorno programado de Superestrella quedan
diferenciados de las pérdidas reales.

Las unidades se ejecutaron normalmente y con AddressSanitizer y
UndefinedBehaviorSanitizer. LeakSanitizer se desactiva por las restricciones
del entorno. Se prueban los 48 niveles, cada nivel omitido, pérdidas en los
mapas normales/Secret/Extra y la introducción, secuencias de jefes, rutas
finales, discontinuidades y restauraciones parciales o inválidas.

El candidato host final tiene SHA-256
`8288c3080c6e9ec498cfa392d060642dd9dd2f618a77338cd7adcc3e0f88323d`.

| Recorrido ejecutado | Observaciones | Evidencia y límite |
| --- | ---: | --- |
| Arranque con archivo inexistente | 3.468 | Entradas normales; el intento se arma en el fotograma 693 y continúa activo durante la introducción |
| Final desde una victoria preparada | 3.730 | El juego ejecuta la meta final y entra a créditos; el verificador usa 47 metas anteriores sintéticas para aislar el criterio final |
| Regreso de Superestrella preparado | 180 | El juego produce 16 fotogramas de retorno que antes podían confundirse con daño; el intento sigue activo |
| Golpe normal provocado mediante su temporizador | 120 | El juego ejecuta la rutina real de separación y sigue funcionando; el intento queda perdido |

Los tres últimos parten de estados privados preparados. Sus prefijos de
historia existen **solo en el verificador independiente**, nunca en el APK
ni en un registro del usuario. Demuestran cómo responde el detector a esas
transiciones reales, sin atribuirles una campaña natural completa.
Los replays mantienen ejecución estática estricta y no usan reparación
dinámica para cubrir instrucciones ausentes.

El recorrido final permitió corregir la ruta `17 → 18` y la rutina Thumb
dinámica omitida por el port. La ejecución corregida continúa 200 fotogramas
después de entrar al estado `19` de créditos, pasando por `1A`.
[Validación específica de créditos](CREDITS_0.10.11_VALIDATION.md).

### Regresiones de Fuzzy y del coche

Se repitieron dos recorridos con el candidato final y se compararon con las
ejecuciones archivadas de 0.10.9, después de verificar las identidades del
estado inicial, entradas, ROM, BIOS, configuración y binario de referencia.
La referencia histórica se reutiliza y no se presenta como una ejecución
nueva de la versión anterior.

Fuzzy a ancho 240 coincide en 1.901 filas de traza, 41 estados guardados y
40 imágenes. El efecto permanece activo 782 fotogramas y termina con 101
fotogramas normales, sin reinicio ni muerte; se obtiene únicamente el evento
Fuzzy `0x1`. El coche a ancho 356 coincide en 1.527 filas, 34 estados y 33
imágenes, con transformación y regreso a Yoshi completos. **Las 73 imágenes
tienen cero píxeles diferentes**. Ambos recorridos tienen cero instrucciones
interpretadas, destinos ausentes o reparaciones dinámicas.

La comparación de estados excluye únicamente el campo de reloj de pared del
RTC inactivo ya identificado en la validación anterior. CPU, memoria, E/S,
audio, partida, metadatos y tiempos PPU deben coincidir.
[Informe de las dos regresiones](achievement-0.10.11-focused-native-regressions.json).

## Persistencia y ciclo de vida

`challenge_checkpoint_test.cpp` comprueba el registro acotado, la asociación
con el estado automático exacto, alteraciones de cada byte, truncamientos,
contenido adicional, estados diferentes, enlaces simbólicos, escrituras
fallidas y consumo sin reutilización. `challenge_lifecycle_test.cpp` inyecta
fallos de borrado y sincronización, verifica que el progreso no se importa
parcialmente y que no se ejecuta una acción nueva sin consumir el comprobante.
Ambas suites pasan con ASan/UBSan: **510 y 18 comprobaciones**, respectivamente.

También se ejecutaron tres casos con el motor real, SDL y código de juego
recompilado del candidato final. Un auxiliar privado entrega eventos
ordinarios de segundo plano y primer plano a SDL; no escribe RAM del juego.

1. Se inicia un archivo nuevo, se entra en la espera real de segundo plano,
   se termina el proceso y se abre otro. El estado automático de 553.424
   bytes y el comprobante de 296 bytes recuperan un intento activo.
2. Se intenta repetir ese mismo estado automático sin su comprobante ya
   consumido. El juego no obtiene un intento elegible ni el evento del logro.
3. Se vuelve a primer plano dentro del mismo proceso. El comprobante se
   consume antes de continuar y el intento permanece activo.

El borrado se sincroniza antes de reanudar. Si no puede confirmarse por un
error de E/S, la sesión se detiene antes de ejecutar nuevas acciones. Al
guardar la suspensión se revisa otra vez la separación actual de Mario;
un golpe posterior a la última observación de fotograma no se omite.
La lectura y escritura del comprobante ocurre en esos límites de suspensión,
no en cada fotograma.

[Informe de suspensión y reanudación](yo-shi-pude-lifecycle-report.json).

## APK y procedencia

- Archivo: `sma3-recomp-0.10.11-beta-arm64.apk`, **28.017.944 bytes**.
- SHA-256: `b596fbf6fba4a8176dcb4a4d292ecacb733f2fac7afa852b2eb129c3ff1ccaa5`.
- Versión `0.10.11-beta`, código `22`, payload `0.10.11`.
- Paquete `org.sma3.recomp.experimental`, ARM64, mínimo API 28, objetivo 35.
- Certificado anterior y firma v2 verificados; APK de desarrollo como la
  línea anterior, con `debuggable=true`.
- Los 26 IDs y la nueva clase de animación están presentes en DEX; las 21
  firmas JNI corresponden con sus exportaciones nativas.
- Los 1.822 insumos de ensamblado coinciden antes y después de compilar;
  los 1.776 insumos compartidos con el candidato host final coinciden.
- `libmain.so` se recompila con el detector, la persistencia, el enlace JNI
  y los créditos. SDL y la biblioteca de C++ mantienen sus bytes anteriores.

Se conserva el archivo grande de código generado de juego, cuya compilación
completa original procede de 0.10.8; se verificaron su hash y sus 102 insumos.
No se presenta ese archivo reutilizado como una compilación completa nueva.
Los fragmentos nuevos de créditos se compilan por separado en esta entrega.

[Informe de compilación y firma](android-0.10.11-build.json) ·
[Notas de versión](../docs/releases/v0.10.11-beta.md).

## Límites

No se ha jugado una campaña natural completa sin perder a Mario, probado
todos los enemigos ni completado todos los créditos. Tampoco se ha instalado
este APK en un teléfono, medido FPS Android o probado el instalador del
sistema en el Pixel 7 Pro. Las unidades, los recorridos controlados y el
framework host tienen los alcances indicados; no sustituyen esas pruebas.
La panorámica conserva su etiqueta beta y su cobertura anterior.
