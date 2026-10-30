# «Yo shi pude»: detección nativa y alcance de la validación

## Regla implementada

El logro exige una campaña nueva observada desde su creación, la introducción,
las metas de los **48 niveles principales** y la entrada al final del juego,
sin que un golpe o enemigo le quite a Bebé Mario durante la campaña. Una pérdida
invalida el intento completo aunque se recupere a Mario después. Las visitas a
niveles Secret/Extra también se vigilan; no sustituyen ninguno de los 48 niveles
principales necesarios para terminar la campaña.

La condición solicitada es conservar a Mario. Morir sin que se separe de Yoshi
no añade una condición de «cero muertes». Las transformaciones, Super Star y
los intercambios de las escenas no se consideran golpes.

El detector está en `src/no_mario_loss_challenge.h`. Es un observador: no cambia
RAM, personajes, velocidad, estrellas, estados ni reglas del juego. Se ejecuta
una vez por fotograma de GBA; las imágenes de presentación adicionales del modo
120 no cuentan como fotogramas nuevos de la campaña. No realiza E/S ni asigna
memoria por fotograma.

## Fuentes primarias y direcciones verificadas

Se revisaron los archivos locales de
[sma3-disasm, revisión c8532ec9a8d0038c3bfeb003dd0c7ea89d7e1071](https://github.com/KarisaAdvynia/sma3-disasm/tree/c8532ec9a8d0038c3bfeb003dd0c7ea89d7e1071),
correspondientes a la ROM USA que valida el port. Las direcciones siguientes
son direcciones del juego original, no offsets obtenidos de imágenes.

| Evidencia | Ubicación | Uso |
| --- | --- | --- |
| Modo Yoshi's Island | `03006D64` | Debe ser cero. |
| Estado del juego | `03006B05` | `0D` es juego normal; `0E` victoria; `23` transición de jefe. |
| Archivo seleccionado | `03006C39` | Identidad de archivo 0–2. |
| Nivel / habitación | `03006288` / `03004CB8` | Distingue mapas y la introducción, cuyo ID es 11. |
| Propietarios de memoria normal | `03007010`, `03007240` | Deben contener `0200000C` y `0300220C` antes de leer Mario. |
| Banderas de Mario | `03006F2A` | Bit `8000`: montado; `4000`: capturado por enemigo; `2000`: se usa en la devolución programada de Super Star y en el minijuego de meta. |
| Actividad / estado de Mario | `03002484` / `030024CE` | Sprite 061, campos `+24` y `+6E`; el segundo no está en `030024CA`. |
| Transformación de Yoshi | `03006DB2` | Distingue vehículos y Super Star del golpe normal. |
| Subestado de victoria | `03004828` | Confirma la entrada de una meta; evita contar un resultado simplemente cargado. |

Las tablas de estados se encuentran en
[`Data.asm`, `YIGameStatePtrs`](https://github.com/KarisaAdvynia/sma3-disasm/blob/c8532ec9a8d0038c3bfeb003dd0c7ea89d7e1071/asm/Data.asm).
El detector no utiliza `03004BC4` como indicador de pérdida: ese campo participa
en la actualización y sonido de las estrellas, por lo que no prueba que Mario
haya sido separado.

### Inicio nuevo y continuidad

[`YITitle.asm`, `Sub08101E30`](https://github.com/KarisaAdvynia/sma3-disasm/blob/c8532ec9a8d0038c3bfeb003dd0c7ea89d7e1071/asm/YITitle.asm)
lanza el archivo nuevo hacia `3A`/`3B`, la introducción. Se exige observar
continuamente `08 → 3A/3B`, con la firma `AYE0`, archivo todavía inexistente,
cero muertes y tabla de cursos inicial: únicamente 1-1 desbloqueado (`8000`)
y los otros 71 registros a cero. Esa inicialización está en
[`GameMode03.asm`, `InitSaveFileWRAM`, `080FA098`](https://github.com/KarisaAdvynia/sma3-disasm/blob/c8532ec9a8d0038c3bfeb003dd0c7ea89d7e1071/asm/GameMode03.asm).
La tabla se inspecciona solo en la escena de inicio, no durante cada fotograma
normal.

Abrir un guardado antiguo, aparecer en 1-1 o cargar un estado con una tabla
vacía no arma el desafío. Las metas deben observarse durante ese mismo intento.
El contador de fotogramas y la época de restauración detectan saltos, retrocesos
y restauraciones no verificadas. Las lecturas duplicadas del mismo fotograma
no añaden progreso; aun así, se comprueba la pérdida actual de Mario.

### Golpe normal, captura y separaciones programadas

[`Sprites/BabyMario.asm`, `Sub080E1DC4`](https://github.com/KarisaAdvynia/sma3-disasm/blob/c8532ec9a8d0038c3bfeb003dd0c7ea89d7e1071/asm/Sprites/BabyMario.asm)
quita el bit `8000`, deja el sprite activo en 8 y estado 0, y establece el
retardo de separación. Es la ruta normal de golpe; el Bowser pequeño también
la llama en `08102426`. La captura de enemigos usa `Sub080DFF5C`, que establece
el bit `4000` en `080DFFDE`. Esa evidencia explícita de captura prevalece sobre
las exclusiones de transformación.

En juego normal y memoria válida, sin transformación ni Mario montado, los
estados 0, 3 y 9–14 del sprite activo 8 reconocen separación, burbuja, captura
y sus rutas posteriores. Recuperar a Mario no borra la pérdida registrada.

Dos excepciones precisas evitan fallos falsos:

- El Mario de los vehículos permanece en estado 6. Al terminar la
  transformación puede existir un fotograma con transformación cero y estado
  6; comprobar solo «no está montado» marcaría erróneamente una pérdida.
- `SuperBabyEgg_Main` termina Super Star, pone la transformación a cero y
  reconstruye Mario con actividad 1 y estado 0. La inicialización genérica
  cambia después la actividad a 8 antes de que vuelva a montarse. Mientras
  conserva la bandera `2000` y el estado 0, es una devolución programada.
  **Solo esa combinación queda exenta** de la regla de golpe. El bit `4000`
  y los otros estados de daño siguen fallando. Las dos rutinas normales de
  reenganche aplican `(& 0FFF) | 8000` (`080E049A–080E04A6` y
  `080E0814–080E0820`), de modo que un golpe posterior no conserva la
  excepción de Super Star.

La segunda excepción se confirmó ejecutando el código recompilado de
[`SuperBabyEgg.asm`](https://github.com/KarisaAdvynia/sma3-disasm/blob/c8532ec9a8d0038c3bfeb003dd0c7ea89d7e1071/asm/Sprites/SuperBabyEgg.asm),
no solo mediante valores inventados en una prueba unitaria. La preparación
controlada colocó el sprite 029 y su temporizador de retorno en una escena
válida. El juego produjo actividad 1/estado 0/bandera `2000`, después actividad
8/estado 0, luego estado 1 y finalmente actividad 5/bandera `8000`. No fue una
recolección natural de Super Star; comprueba específicamente su retorno.

### Meta final y créditos

El final de Bowser en
[`Sprites/Bosses/Bowser.asm`, `08107026`](https://github.com/KarisaAdvynia/sma3-disasm/blob/c8532ec9a8d0038c3bfeb003dd0c7ea89d7e1071/asm/Sprites/Bosses/Bowser.asm)
entra en `23`. Después,
[`LevelVictory.asm`, `BossVictoryTransition`, `080E5D86`](https://github.com/KarisaAdvynia/sma3-disasm/blob/c8532ec9a8d0038c3bfeb003dd0c7ea89d7e1071/asm/LevelVictory.asm)
entra en `0E` con subestado `19`. Por tanto, 6-8 aporta la meta número 48;
no se presupone que finalizar una partida equivalga a leer 48 bits antiguos.

La entrada a créditos admite las tres rutas verificadas: `17 → 18/19`,
`39 → 18/19` y `4B → 18/19`, siempre con curso 67 (6-8), el mismo archivo,
los 48 objetivos observados y ninguna discontinuidad ni pérdida previa.
`16` comparte el contenedor de título/escena del castillo; `17` es su fundido.
Las otras dos rutas están en
[`WorldLevelSelect.asm`, `0800C4BA`, `0800DDEE`, `0800CB94`](https://github.com/KarisaAdvynia/sma3-disasm/blob/c8532ec9a8d0038c3bfeb003dd0c7ea89d7e1071/asm/WorldLevelSelect.asm).
La memoria de juego/mapa puede estar liberada al entrar a créditos y no se
exige una asignación normal en ese punto.

Una ejecución controlada de cierre de curso 67 produjo realmente
`23 → 0E → 1C → 1D → 1E → 33 → 34 → 14 → 15 → 16 → 17 → 18`.
Permitió corregir la omisión de la ruta `17 → 18` y encontrar un fallo anterior
del port al iniciar los créditos: faltaba la copia Thumb que se ejecuta en
IWRAM. La corrección y su cualificación se documentan junto a la integración
de créditos. Este recorrido parte de una escena de victoria preparada; no
acredita haber vencido a Bowser ni haber jugado los 48 niveles.

## Persistencia y estados del intento

El estado del intento está fuera de la memoria del juego. Su resumen público
contiene `[esquema 1, estado, archivo + 1, niveles principales observados]`:

| Estado | Significado |
| --- | --- |
| 0 | No comenzó una campaña verificable. |
| 1 | Intento activo. |
| 2 | Mario fue separado/capturado; este intento falló. |
| 3 | Se perdió la continuidad y el intento ya no puede verificarse. |
| 4 | Campaña limpia completada; logro ganado. |

La pausa segura del motor crea un registro asociado al archivo de
autosuspensión mediante su huella y una integridad del registro. Se consume
antes de volver a ejecutar el juego; no es una prueba reutilizable para
retroceder tras un golpe. Un proceso cerrado mientras la aplicación está
correctamente suspendida puede continuar mediante ese par verificado.
Las restauraciones manuales y el cierre abrupto sin continuidad verificable
no conceden crédito retroactivo. Los archivos encargados de esa protección
son `src/challenge_checkpoint.*` y `src/challenge_lifecycle.*`, con pruebas
independientes de persistencia y fallos de E/S.

En el límite de suspensión se vuelve a leer la pérdida actual, evitando
omitir un golpe ocurrido después de la última observación PPU. La restauración
tampoco omite esa comprobación por tratarse de un fotograma duplicado.
El registro incluye el estado ganado y las métricas monotónicas; una pausa
inmediatamente después del final no pierde el logro si Android todavía no
había realizado su consulta periódica.

El resumen general de logros mantiene ocho enteros y pasa a **esquema 2**.
El bit 0 de eventos sigue siendo Fuzzy; el bit 1 es «Yo shi pude».
Su registro ganado se combina por OR en Android. Los 25 logros anteriores se
conservan; la migración no inventa evidencia para el nuevo desafío.

## Pruebas reproducibles

`validation/no_mario_loss_challenge_test.cpp`: **5 grupos, 1.473
comprobaciones**, tanto en compilación normal como con ASan/UBSan. Incluye
inicio auténtico frente a estados importados; los 48 niveles y cada nivel
omitido; pérdidas en los 60 mapas normales/Secret/Extra y la introducción;
metas normales y jefes; las tres rutas finales; transformaciones;
la devolución de Super Star y golpes posteriores; datos incompletos;
saltos/épocas; intentos fallidos persistentes y reanudación segura después
de 0–47 metas. También se rechazaron registros truncados o inválidos sin
mutación parcial del intento existente.

La regresión del colector anterior pasa **166.657 comprobaciones**, también
con ASan/UBSan. No se ampliaron las condiciones de los 25 logros previos.

```sh
g++ -std=c++17 -O2 -Wall -Wextra -Werror -I src \
  validation/no_mario_loss_challenge_test.cpp -o /tmp/yo-shi-pude-test
/tmp/yo-shi-pude-test

g++ -std=c++17 -O1 -g -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer -I src \
  validation/no_mario_loss_challenge_test.cpp -o /tmp/yo-shi-pude-sanitized
ASAN_OPTIONS=detect_leaks=0 /tmp/yo-shi-pude-sanitized

g++ -std=c++17 -O2 -Wall -Wextra -Werror -I src \
  validation/replay_no_mario_loss_trace.cpp -o /tmp/replay-yo-shi-pude
```

LeakSanitizer se desactiva por las restricciones del entorno; AddressSanitizer
y UndefinedBehaviorSanitizer permanecen activos.

El ejecutable host puede registrar las observaciones completas con
`SMA3_LOCAL_ACHIEVEMENTS_TRACE`. Ese código no se incluye en Android. El
reproductor independiente admite `fresh`, `controlled_ending`,
`controlled_star` y `controlled_loss`. Los modos `controlled_*` añaden un
prefijo sintético de prueba en el verificador, **nunca en el APK ni en el
registro de logros**. Sirven para comprobar cómo responde el detector a las
transiciones realmente ejecutadas sin presentar esos prefijos como una
campaña jugada.

### Cualificación del candidato final

El candidato host final tiene SHA-256
`8288c3080c6e9ec498cfa392d060642dd9dd2f618a77338cd7adcc3e0f88323d`.
Las huellas de las trazas y de los archivos de código están en
[`yo-shi-pude-native-report.json`](yo-shi-pude-native-report.json).

| Recorrido ejecutado | Observaciones nativas | Resultado del detector |
| --- | ---: | --- |
| Arranque real, archivo nuevo e introducción | 3.468 | Activo, 0 metas; armado en el fotograma 693. |
| Cierre controlado de curso 67 y créditos | 3.730 | Ganado en 17.972, con un prefijo de prueba de 47 metas y la meta final observada. |
| Devolución controlada de Super Star | 180 | Activo; 16 fotogramas de actividad 8/estado 0/bandera `2000` no fallan. |
| Golpe normal por la ruta recompilada | 120 | Pérdida persistente detectada en 6.601; el juego sigue ejecutándose. |

El arranque real confirmó `08 → 3A` con archivo inexistente. El Mario separado
de la escena `3B` no invalidó el intento, y la introducción pasó a juego normal
con Mario montado. La copia pública del estado del nativo terminó en
`[1, 1, 1, 0]`: esquema 1, activo, archivo 1 y cero metas.

La prueba de golpe cambió únicamente el temporizador de daño en una escena
normal válida. Las rutinas originales `Sub080E1F18 → Sub080E1EE4 →
Sub080E1DC4` produjeron la separación: actividad 5 a 8, bandera `8000` a cero,
estado 0 y después estados 10/11. No se escribieron directamente las banderas
de pérdida. El depurador TCP vio el golpe en 6.600; el observador PPU lo vio
en 6.601, pues son puntos distintos de la misma ejecución. El verificador
mantiene el intento fallido hasta el final de las 120 observaciones.

Tras corregir la copia de código de créditos, el recorrido de cierre llegó a
`18` en 17.972, `19` en 18.000 y `1A` en 18.060, y continuó **200 fotogramas
después de la primera entrada a 19**, con salida normal del proceso. El motor
informó cero destinos sin recompilar, cero instrucciones interpretadas y cero
recompilaciones dinámicas. El prefijo sintético del verificador no ejecuta
el combate de Bowser ni los 47 niveles anteriores; la entrada a créditos
y la estabilidad posterior sí proceden del juego recompilado.

## Límite de las conclusiones

No se ha acreditado aquí una partida humana completa de los 48 niveles sin
perder a Mario, ni todos los enemigos o rutas opcionales. Las unidades
verifican el contrato; las ejecuciones nativas comprueban los recorridos
concretos descritos. La compilación Android, el comportamiento de la
notificación y las pruebas de suspensión se informan por separado.
Los estados y capturas preparados incluyen datos del juego y permanecen
privados; este informe, el código del detector y los verificadores no
redistribuyen ROM, BIOS ni estados de partida.
