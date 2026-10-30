# Contrarreloj y reportes — validación de 0.10.16 beta

Base publicada: **0.10.15**, commit
`dc785e5983a121709ec749d5f6be62d11fb492fe`.
Motor GBARecomp: `e7728148c6829ba526f682876430a0c9022dc6c0`, con la misma
serie de **19 parches**. Disassembly:
`KarisaAdvynia/sma3-disasm`, commit `c8532ec9a8d0038c3bfeb003dd0c7ea89d7e1071`.
La identidad del juego se comprueba contra la ROM USA SHA-1
`7352d2bd064d9ebaec579e264228aa21c7345b80`.

Esta entrega implementa los puntos 14 y 16: contrarreloj por nivel y
preparación manual de reportes de errores. Las comprobaciones de lógica,
interfaz Android, ejecución del juego y APK se documentan por separado.
**No se ha instalado esta versión en un teléfono físico ni completado
los 60 niveles como parte de esta validación.**

## 1. Contrato y reloj del contrarreloj

El observador de producción lee el estado del juego una vez por fotograma
PPU. No escribe RAM del juego, no reemplaza rutinas originales y no realiza
operaciones de disco dentro de ese callback. La comunicación con Java usa
una instantánea coherente de **74 enteros de 64 bits** protegida por un
mutex breve. Las tres variantes del callback de fotograma llaman al mismo
observador.

El reloj cuenta intervalos de **280.896 ciclos a 16.777.216 Hz**, la misma
escala con presentación normal o mezcla 2×. No utiliza tiempo de pared ni
el número de imágenes que SDL presenta. La UI consulta el estado cada
100 ms mientras está activado; esa consulta no adelanta el contador.
Duplicar una observación PPU no añade tiempo.

El reconocimiento de entrada exige haber observado la transición hacia
`LevelIntro` (`0C`) y después a juego (`0D`). La entrada principal escribe
`0C` en `0802CC5A/5C`; los cambios de pantalla y los puntos intermedios usan
`0B` en `0802CE8E`. Activar a mitad del nivel, cargar un estado ya situado
en la introducción o reaparecer en un punto intermedio no acredita un
recorrido desde el principio.

La pausa utiliza los indicadores reales de inventario (`030048B5`) y Start
(`03004D46`, cuatro bits inferiores). Una pausa del proceso que no avance
el juego tampoco añade intervalos. Abrir un panel de Android no sustituye
por sí mismo a una pausa del juego: si el juego sigue avanzando, su tiempo
continúa contando. Los cambios de habitación y los minijuegos internos
forman parte del recorrido.

Se reconocen meta normal `0D → 0E` y victoria de jefe `0D → 23 → 0E`, con
el subestado de victoria verificado. Morir, abandonar el nivel, cambiar
de partida o fuente, restaurar un estado o perder continuidad del reloj
invalida el intento. Los mejores tiempos ya completados se conservan.

Los identificadores originales se convierten a **60 entradas compactas**:
48 principales, seis Extra y seis Secretos. Se excluyen bienvenida y
Mario Bros. Los mínimos de la sesión permiten recuperar una mejora aunque
Java no haya consultado justo después de terminar el nivel.

[API](../src/time_trial.h) · [Lógica](../src/time_trial_detail.h) ·
[Observador](../src/time_trial.cpp).

## 2. Lógica controlada y persistencia

| Capa | Resultado | Alcance |
| --- | --- | --- |
| Núcleo C++ | 2.176 comprobaciones, optimizado y ASan/UBSan | Entradas, pausas, metas, cancelaciones, mapeo y mínimos |
| Modelo Java | 115 comprobaciones | Contrato JNI, conversión exacta, formato, validación y mezcla de récords |
| UI y almacenamiento Android | 8 pruebas JUnit | Navegación, ciclo de vida, preferencias, comparaciones y fallos de disco |

La prueba C++ construye observaciones controladas. Sus casos de meta y
jefes acreditan la lógica del observador ante esos datos; **no son un
recorrido natural hasta cada meta del juego**. La compilación optimizada
usa `-O2 -Wall -Wextra -Werror`; la segunda ejecución utiliza
AddressSanitizer y UndefinedBehaviorSanitizer.
Esa segunda ejecución usa `ASAN_OPTIONS=detect_leaks=0`; no incluye un
análisis de fugas con LeakSanitizer.

Los récords se guardan en un archivo propio `time-trials/records-v1.json`,
mediante `AtomicFile`, sincronización y comprobación de lectura. No se
añaden a SRAM ni al estado del juego. La escritura ocurre en un trabajador
serial. Un fallo conserva la mejora pendiente en memoria y reintenta;
un archivo existente ilegible o de versión desconocida se conserva.

La instantánea Java valida tamaño, esquema, escala del reloj, estados,
identificadores y rangos antes de modificar registros. La conversión de
fotogramas a milisegundos usa aritmética entera con límite de desbordamiento.
Una marca más lenta no sustituye el mejor tiempo ni su fecha. Se guarda
también el último tiempo conocido por nivel. Una finalización repetida no
genera otra escritura ni otro récord al recrear el helper.

Si pasan varias finalizaciones entre consultas, se recuperan los mejores
tiempos, pero no se inventa cuál era el récord inmediatamente anterior al
último intento. En ese caso se compara el último tiempo con el mejor
conocido, sin atribuir un anuncio de récord nuevo a una secuencia desconocida.

Las ocho pruebas Android cubren los seis mundos y sus diez filas,
activación inicialmente deshabilitada, preferencia de cronómetro, pausa y
reanudar consultas, reloj independiente de la UI, resultado respecto al
récord anterior, retorno/recreación, guardado fallido y reintento,
finalizaciones omitidas, escritura real con `AtomicFile`, archivos
malformados/futuros y cancelación al restaurar. Las capturas en español,
inglés y portugués se generan con la UI de producción y datos controlados;
se revisaron las vistas vertical y horizontal.

[Prueba C++](time_trial_test.cpp) ·
[Resultados C++](time-trial-0.10.16-results.json) ·
[Modelo Java](TimeTrialModelTest.java) ·
[Prueba UI/guardado](TimeTrialUiTest.java) ·
[Resultados UI](time-trial-ui-0.10.16-results.json).

## 3. Ejecución nativa y comparación con 0.10.15

Se completaron **ocho sesiones nativas por TCP**, con **5.674 intervalos
de frame solicitados** y 39 checkpoints. Todas terminaron **FULLY_STATIC**:
cero misses, instrucciones interpretadas, bloques sanados y fallos.
La producción permaneció idéntica al congelado utilizado para el APK.

**Procedencia del punto de entrada:** se preparó un fixture de 1-1 con
`make_level_fixture.py`, cambiando únicamente selector de nivel, entrada
principal y estado de inicialización. Después, botones normales produjeron
daño, pérdida de Bebé Mario y muerte reales, hasta la pantalla **Try Again**.
Las sesiones de esta prueba cargan ese checkpoint y pulsan A para que el
juego ejecute su inicialización, introducción y reentrada. El driver de la
comparación no modifica RAM, posición, meta ni puntuación.

Esta es una **reentrada real desde una ruta diagnóstica preparada**, no un
recorrido completo desde arranque nuevo. Los tres valores iniciales editados,
la cinta de origen y los hashes de fixture/checkpoint están incluidos en el
reporte. La exploración que produjo el checkpoint también terminó
FULLY_STATIC con los cuatro contadores a cero.

| Comprobación nativa | Resultado |
| --- | --- |
| Reentrada por inicio principal | Un intento de 1-1 activo, sin récord parcial |
| Start sostenido 180 frames PPU más | Mismo tiempo final: 174 intervalos |
| Inventario sostenido 180 frames PPU más | Ambos siguen pausados, indicador 8 y 142 intervalos |
| Restaurar un estado durante el intento | Invalidado por restauración, epoch 2, sin récord |
| Cargar el juego a mitad de nivel | Cero tiempo y cero intentos iniciados; no acredita una marca |

El ensayo de inventario compara los endpoints **mientras sigue abierto**.
Al incluir el cierre, las dos rutas terminaron con 174 y 172 intervalos;
esos resultados también se conservan. No se supone que la animación de
cierre dure lo mismo en ambos recorridos. La afirmación comprobada es que
los 180 frames adicionales con la pausa activa no incrementan el cronómetro.

**Regresión contra 0.10.15:** seis pares de checkpoints tienen imágenes
completas de **240 × 160 idénticas** y las mismas secciones serializadas
`CPU0`, `BUS0`, `IO_0`, `AUD0`, `SAV0`, **`PPU0`** y `META`. La única
exclusión son ocho bytes del reloj de host para el RTC inactivo en `BUS0`.
No se excluye la PPU ni se compara solo una parte de la pantalla. El candidato
tiene el observador habilitado durante la misma secuencia de botones.

Las metas normales, jefes y actualización de mejores tiempos se verifican
con observaciones controladas de la sección anterior. **Estas ocho sesiones
no incluyen una finalización natural completa de nivel.**

[Driver y comprobaciones](time_trial_native_test.py) ·
[Resultados, cintas y procedencia](time-trial-native-0.10.16-results.json).

Para repetirlas, prepara ambos runners y el checkpoint privado con la
procedencia registrada. `entry_actions` y `entry_provenance` del reporte
contienen los documentos JSON que reciben estos argumentos:

```bash
python3 validation/time_trial_native_test.py \
  --reference /ruta/runner-0.10.15 \
  --candidate /ruta/runner-0.10.16 \
  --rom /ruta/sma3-usa.gba --bios /ruta/gba_bios.bin \
  --entry-state /ruta/try-again.state \
  --entry-actions /ruta/entry-actions.json \
  --entry-provenance /ruta/entry-provenance.json \
  --private-output /tmp/time-trial-native \
  --report /tmp/time-trial-native-results.json
```

El directorio privado debe estar vacío y fuera del repositorio. ROM, BIOS,
partidas, estados y capturas del juego no se publican con las fuentes.

## 4. Preparación de reportes de errores

El reporte se construye con una lista explícita de datos y preferencias.
No vuelca todas las preferencias ni adjunta archivos. Incluye versión,
código, payload, fabricante/modelo, Android/API/ABI, pantalla y ajustes
pertinentes. El catálogo de perfiles aporta solo cantidades de instalados
y activos, sin nombres ni rutas. Los FPS solicitados y la última muestra
observada se describen por separado; la muestra no se presenta como
promedio sostenido de toda la sesión.

La pantalla inicial utiliza la ruta sin JNI. Desde el juego se añaden los
datos nativos solo cuando están disponibles. Los campos de título,
descripción, pasos y resultado esperado generan una vista previa completa
y editable. Copiar y compartir utilizan exactamente el texto revisado.

El botón GitHub abre un borrador en el repositorio correcto, cuyo nombre
termina en punto. Si la URL supera 6.000 caracteres, se copia el informe
completo y se abre el formulario con el título; se pide pegar el cuerpo.
No se trunca silenciosamente el informe. Ninguna acción publica un issue
por sí sola y no existe envío automático del diagnóstico.

| Capa | Resultado | Alcance |
| --- | --- | --- |
| Compositor Java | 669 comprobaciones | Lista permitida, Unicode, límites, integridad del texto y enlaces |
| UI Android | 8 pruebas JUnit | Formulario, edición, copiar, intents, navegación y ciclo de vida |

Las pruebas interceptan `ACTION_SEND` y `ACTION_VIEW`: se verifica el texto,
tipo `text/plain`, ausencia de adjuntos y URL, sin abrir aplicaciones externas
ni publicar reportes. Comprueban el borrador al volver, preparación asíncrona
abandonada, finalización en segundo plano, falta de datos nativos y errores
del catálogo. Hay cobertura de inglés, español y portugués y capturas en
vertical/horizontal. La vista previa horizontal se revisó visualmente.

**Reportes enviados e issues creados durante estas pruebas: cero.**

[Modelo](../android/java/org/gbarecomp/BugReportModel.java) ·
[Recopilación](../android/java/org/gbarecomp/BugReportCollector.java) ·
[UI](../android/java/org/gbarecomp/BugReportUi.java) ·
[Pruebas](BugReportUiTest.java) ·
[Resultados](bug-report-0.10.16-results.json).

## 5. Cómo repetir las pruebas de componentes

Las pruebas Android usan **Robolectric 4.16.1, JUnit 4.13.2, API 34 y JDK 17**,
y compilan contra el `android.jar` de API 35. No son ejecuciones del juego
ARM64 en un teléfono. El aviso de Robolectric sobre API 36/JDK 21 no afecta
estos casos, que seleccionan explícitamente API 34.

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Werror -I src \
  validation/time_trial_test.cpp -o /tmp/time-trial-test
/tmp/time-trial-test

python3 validation/run_time_trial_ui_test.py \
  --java-home /ruta/jdk-17 \
  --android-jar /ruta/android-sdk/platforms/android-35/android.jar \
  --dependencies-from /ruta/dependencias-robolectric \
  --robolectric-cache /ruta/cache-robolectric \
  --work-dir /tmp/time-trial-ui

python3 validation/run_bug_report_ui_test.py \
  --java-home /ruta/jdk-17 \
  --android-jar /ruta/android-sdk/platforms/android-35/android.jar \
  --dependencies-from /ruta/dependencias-robolectric \
  --robolectric-cache /ruta/cache-robolectric \
  --work-dir /tmp/bug-report-ui
```

Las dependencias deben estar resueltas previamente y la caché debe contener
el runtime instrumentado de Android 14. Los resultados JSON registran hashes
de las fuentes comprobadas; las capturas son evidencia privada de las pruebas.

## 6. APK y comprobaciones pendientes

La compilación entrega **0.10.16-beta**, código **27**, payload **0.10.16**,
paquete `org.sma3.recomp.experimental`, ARM64 y Android 9 o posterior.
La variante Android conserva el modo de depuración de esta línea de betas;
el código nativo se compila en Release. Las instrumentaciones de escritorio
quedan excluidas del APK.

El archivo generado de juego ARM64 se reutiliza desde 0.10.15 tras verificar
su SHA-256 y sus **71 dependencias**, todas iguales. El observador y los
adaptadores del port se recompilan en `libmain`; no se editan funciones
generadas ni se añade un parche nuevo al motor.

Se verifican **1.789 entradas de producción** idénticas antes y después,
incluidas las **1.659 entradas nativas** alineadas con el runner de QA.
También se comprueban CRC, alineación, tres DEX, 334 clases, assets y
bibliotecas AArch64. Las **23 declaraciones JNI**, sus descriptores y
atributos en DEX y las exportaciones ELF coinciden. Las 21 anteriores se
conservan; se añaden `setTimeTrialEnabled(Z)V` y `getTimeTrial()[J`.

SDL y libc++ coinciden byte a byte con 0.10.15. El contenedor final conserva
los 17 contenidos del APK de Gradle; no empaqueta archivos de ROM, BIOS,
claves ni partidas.
La firma v2 se verifica con el certificado original.

| Dato | Valor |
| --- | --- |
| Archivo | `sma3-recomp-0.10.16-beta-arm64.apk` |
| Tamaño | 28.091.672 bytes |
| SHA-256 APK | `12c6cbc9f5aaa6aed8de52df2f6c4ec7909198e114b1f3ff6428f0699bd323bc` |
| SHA-256 certificado | `2390f57b35d436ff37b2a303189d18d463c8f5114a5ff809b2f2e6df60f58da3` |
| SHA-256 manifiesto de producción | `c5a3ad2fb47eaf78b7b6e8de1afad86c8190a9d2242eb4fafbf72e2d1d4b74cd` |

La preparación encontró un bloqueo del IPC local de Gradle y después
clases temporales duplicadas en intermediarios Java recuperados. Se permitió
el IPC y se regeneraron esas salidas Java/DEX; las fuentes permanecieron
idénticas. El informe conserva esos intentos y el ensamblado final correcto
en modo offline.

[Informe completo de compilación](android-0.10.16-build.json).

Quedan por comprobar en Android físico la instalación sobre la versión
anterior, conservación de datos, presentación y tacto del cronómetro,
rotación y segundo plano, permisos/portapapeles, destinos reales de compartir
y retorno desde el navegador. Tampoco se han cronometrado los 60 niveles
completos, todos los jefes ni todas las transiciones posibles.

Las comprobaciones aquí descritas no miden FPS sostenidos, batería,
temperatura o GPU en el Pixel 7 Pro. Se mantienen las funciones anteriores,
pero sus informes históricos conservan el alcance de sus propias pruebas.

[Guía de uso](../docs/CONTRARRELOJ_Y_REPORTES.md) ·
[Notas de entrega](../docs/releases/v0.10.16-beta.md).
