# Detección nativa de logros locales — 0.10.9

## Contrato y alcance

`local_achievements_frame()` observa la memoria del juego en el hilo de
ejecución, una vez por fotograma de GBA. Las imágenes adicionales del modo
120 no generan acciones. No escribe RAM, paletas, guardados ni estados del
juego. No realiza E/S ni asignaciones dinámicas por fotograma.
El ejecutable host admite una traza diagnóstica opcional mediante
`SMA3_LOCAL_ACHIEVEMENTS_TRACE`; ese código se excluye por completo de Android.

`local_achievements_snapshot()` devuelve ocho enteros de 64 bits:

| Índice | Dato | Valores admitidos |
| --- | --- | --- |
| 0 | Versión del esquema | 1 |
| 1 | Niveles completados | Máscara de 60 bits |
| 2 | Niveles completados con 100 puntos | Subconjunto de la máscara anterior |
| 3 | Transformaciones en vehículo observadas | Bits 0–4 |
| 4 | Máximo de monedas rojas observado tras un incremento | 0–20 |
| 5 | Máximo de flores observado tras un incremento | 0–5 |
| 6 | Máximo de estrellas enteras observado tras un incremento | 0–30 |
| 7 | Eventos observados | Bit 0: recuperación natural de Fuzzy |

Android combina las máscaras mediante OR y los máximos mediante `max`, y
guarda el resultado aparte de la partida. El nativo no importa récords de
guardados anteriores ni necesita recibir el progreso Android. Un mutex
protege la copia completa publicada; solo se toma desde el hilo del juego
cuando cambia el progreso, evitando lecturas incoherentes de varias máscaras.

Los IDs originales de niveles usan `mundo * 12 + posición`. Se aceptan
los seis mundos y las posiciones 0–9, y se compactan en
`mundo * 10 + posición`: 60 bits distintos. Se excluyen introducción, bonus
y huecos. Repetir un nivel no aumenta el número de niveles distintos.
Las transformaciones 2, 4, 6, 8 y 12 usan los bits 0–4; se excluyen esquís
y otras formas especiales de esta primera entrega.

## Evidencia de los eventos

Referencia: [sma3-disasm, revisión c8532ec](https://github.com/KarisaAdvynia/sma3-disasm/tree/c8532ec9a8d0038c3bfeb003dd0c7ea89d7e1071).
Se revisaron los archivos de esa revisión localmente. El motor verifica la
ROM USA antes de ejecutar el juego.

| Dato | Dirección IWRAM | Evidencia de lectura/escritura del juego |
| --- | --- | --- |
| Estado del juego | `03006B05` | `GameInitMain.asm`: selección del código de estado; `LevelCode.asm`: 0D es juego normal |
| Nivel / subnivel | `03006288` / `03004CB8` | `LevelCode.asm`, carga de entrada y tablas `MainEntrancePtrs` en `Data2.asm` |
| Archivo de partida | `03006C39` | `GameMode03.asm`, selección de archivo; se aceptan 0–2 |
| Monedas rojas | `03006AD6` | `SpriteShared08049E80.asm`, incremento en `0804B70E`; reinicio en `0802C64C` |
| Flores | `03006A9A` | `Sprites/SpecialFlower.asm`, incremento en `08052B48` y límite cinco en `08052B54` |
| Estrellas | `03006ACE` | `080E2278.asm`, suma y límite en `080E2330`/`080E2342` |
| Transformación | `03006DB2` | `080352BC.asm`, despacho por ID en `0803DF7E`; tabla `CodePtrs0816DDDC` |
| Subestado de victoria | `03004828` | `LevelVictory.asm`, estructura de juego +2618, campo +4 |
| Puntuación actual | `03006AA4` | `LevelVictory.asm`, `Sub080E4D38` confirma el resultado antes de escribir el récord |
| Temporizador / amplitud Fuzzy | `03004BD6` / `03004AD6` | Mismos campos comprobados en `fuzzy_status.py`: base normal +29CA / +28CA |
| Efecto Fuzzy activo | `03007294` | Protocolo de recuperación de `fuzzy_status.py` y rutinas originales de Fuzzy |

Las estrellas usan décimas, no RGB ni punto fijo 8.8: 100 significa 10.0.
`Data08193264` contiene el límite `012C` (300), aplicado en `080E2342`.
La recuperación automática puede llegar a 109 (10.9), por las instrucciones
`080E23FC`/`080E2420`; eso no cambia el límite de 30. Los máximos publicados
usan división entera entre diez y 29.9 no desbloquea el logro de 30.

El anillo de meta (sprite 00D) cambia de 0D a 0E en `08054EFA`.
Los jefes pasan por `BossVictoryTransition`, estado 23, que cambia a 0E
en `080E5D86`. Se exige observar esas transiciones desde juego normal y
la entrada inicial de victoria; cargar directamente un resultado no cuenta.
El logro de 100 puntos exige además confirmar el resultado actual en la
transición de subestado almacenado 3D/3E a 3F/40. No se consulta un récord
antiguo para concederlo.

La recuperación de Fuzzy exige observar contacto desde el estado normal,
crecimiento de amplitud, al menos dos pasos de cuenta regresiva, el
desvanecimiento gradual de 0100 por paso y el retorno final correcto a cero.
Una cancelación abrupta, muerte, cambio de habitación o carga durante el
efecto invalida esa secuencia. No se modifica ni se desactiva el efecto.

La cualificación nativa encontró una diferencia entre las muestras del inicio
del fotograma PPU y las respuestas del depurador TCP: la secuencia observada
del temporizador fue 652, 652, 650, con incremento de amplitud de 0100 en el
último paso. Los fotogramas PPU siguieron siendo consecutivos; una actualización
del juego había quedado a un lado distinto del punto de observación.
El detector admite ese avance de dos actualizaciones solo si la observación
anterior mantuvo el temporizador. No acumula créditos ilimitados, no admite
un salto a cero y mantiene la comprobación independiente del desvanecimiento
y del cierre. La resta de uno y el aumento de amplitud de 0080 por actualización
están en `0810AB6C`/`0810AB74`. Una pausa de presentación o 120 imágenes por
segundo no genera actualizaciones adicionales del temporizador.

## Protección frente a observaciones incompletas

Se comprueban modo del juego, modo gráfico 0, punteros propietarios de las
asignaciones normales, ID de nivel, subnivel y archivo. Solo se observan
juego normal, victoria y transición de victoria de jefe.

`ExtendedViewFrameInfo.state_epoch` cambia al restaurar un estado. Ese cambio,
un salto o retroceso del contador de fotogramas, otra partida o una escena
no reconocida descartan la secuencia pendiente, conservando lo ya ganado.
El primer fotograma sirve de referencia y no concede nada. Leer un valor
máximo sin observar un incremento posterior tampoco concede un logro.
Una pausa sin nuevos fotogramas no inventa progreso.

El callback proviene de cada evento `frame_completed` de la PPU, incluso
cuando una operación larga hace que una respuesta TCP abarque más de un
fotograma. Los saltos de las respuestas TCP durante una carga de pantalla no
justifican relajar la continuidad del callback nativo.

Se puede continuar jugando desde una partida o estado cargado y ganar un
logro mediante una acción nueva. Estos logros son locales; no constituyen
un sistema competitivo o una verificación de partidas sin estados.

## Pruebas realizadas

`validation/local_achievements_test.cpp`: **7 grupos y 166.657 comprobaciones**.
Incluye el decodificador inmutable, buffers truncados, selección de escenas,
todos los IDs de nivel y transformación de 16 bits, límites, incrementos
reales frente a valores iniciales, duplicados, épocas de restauración,
cambios de habitación y archivo, metas, jefes, resultados imperfectos,
confirmación de 100 puntos, recuperaciones falsas de Fuzzy y la regresión del
temporizador detenido seguida por dos actualizaciones, con sus negativos.

```sh
g++ -std=c++17 -O2 -Wall -Wextra -Werror -I src \
  validation/local_achievements_test.cpp -o /tmp/sma3-local-achievements-test
/tmp/sma3-local-achievements-test

g++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra -Werror -I src validation/local_achievements_test.cpp \
  -o /tmp/sma3-local-achievements-test-sanitized
ASAN_OPTIONS=detect_leaks=0 /tmp/sma3-local-achievements-test-sanitized
```

Ambas ejecuciones pasan. ASan y UBSan cubren la prueba completa;
LeakSanitizer está desactivado por las restricciones del entorno. La unidad
de integración nativa también compila con `-Wall -Wextra -Werror`.

Como comprobación adicional, el `Tracker` actual procesó las trazas
registradas de 0.10.8: 1.901 observaciones de Fuzzy por cada ancho y 1.527
del ciclo de coche. Solo reconoció la recuperación de Fuzzy en el
fotograma de juego 15.200 y la transformación en coche en el 7.535,
coincidiendo con los eventos originales. Esa comprobación reutiliza
observaciones anteriores; no se presenta como una ejecución Android nueva.

Además, 1.901 observaciones del callback del candidato host reprodujeron el
problema de fase y comprobaron la corrección: únicamente se reconoció la
recuperación natural en el fotograma 15.200. Reprocesar esa traza comprueba el
detector; las ejecuciones posteriores del binario corregido quedan registradas
en la cualificación integrada.

La cualificación integrada del candidato y del APK se registra por separado.
Estas pruebas aisladas no acreditan todos los niveles, todas las rutas de
victoria, el teléfono ni la entrega física de notificaciones Android.
