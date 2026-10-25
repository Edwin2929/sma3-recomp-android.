# Fuzzy — reinicio reproducido y corregido para 0.10.2

## Resultado

El APK 0.10.1 podía cerrar o reiniciar el juego al tocar un Fuzzy. Se reprodujo
el fallo en el ejecutable intacto de esa versión, también con la vista original
de **240 × 160**, y se identificó una rutina ARM que el juego copiaba a RAM y
que todavía no tenía una variante nativa registrada.

La corrección conserva el efecto original. En las dos ejecuciones corregidas,
de **1.500 fotogramas** cada una, el contacto real inicia el mareo, el contador
disminuye, la deformación aumenta y luego desaparece por la lógica original
del juego. Ambas terminan con **119 fotogramas consecutivos normales**, sin
muerte, cambio de nivel ni reinicio. No se desactivó Fuzzy, su colisión, el mareo
ni el modo estricto de recompilación.

Esta prueba cubre el caso reproducido en 1-7 y las rutinas descritas. No equivale
a haber recorrido todos los niveles ni a haber ejecutado el APK en un teléfono.

## Causa y corrección

El ejecutable anterior aborta al intentar despachar **ARM `03004E2C`**. Los
`0x78` bytes de esa asignación coinciden con **ROM `08000A08`**. La ejecución
estática rechaza correctamente una dirección para la que no existe código
nativo; el reinicio percibido no proviene de una muerte normal de Yoshi.

El efecto de desplazamiento vertical utiliza dos rutinas, asignadas por el
propio juego durante `Sub0810A608`:

| Función | Fuente ROM | Tamaño | Puntero de asignación | Callback publicado |
| --- | --- | ---: | --- | --- |
| Reordenar columnas de piezas | `0800091C` | `0xD8` | `03007264` | `0300736C` |
| Construir desplazamientos | `08000A08` | `0x78` | `03007260` | `03007370` |

[`tools/generate_effects.py`](../tools/generate_effects.py) genera por separado
las dos variantes ARM con el recompilador del motor fijado. Reubica las
direcciones de instrucción, retornos internos y lecturas relativas al PC; los
destinos de reanudación siguen siendo canónicos. Los archivos derivados de la
ROM permanecen excluidos de Git.

[`src/effects_dispatch.cpp`](../src/effects_dispatch.cpp) exige, en cada entrada:

- Modo ARM y dirección alineada dentro del área dinámica permitida.
- Efecto activo (`03007294 == 1`).
- Asignación actual válida y callback publicado que apunta a esa asignación.
- Coincidencia exacta de **todos** los bytes de la rutina con la ROM verificada
  al iniciar el juego.
- Entrada o punto de reanudación presente en la tabla nativa generada.

La comprobación no memoriza asignaciones antiguas. El juego deja punteros y
bytes detrás al liberar el efecto; la bandera activa y los callbacks evitan
aceptar esa memoria como una rutina todavía activa. Una dirección desconocida
continúa siendo rechazada por el modo estricto.

El bucle principal comprueba la bandera en `08002B32..08002B3A` antes de llamar
a `Sub0810A528`, que invoca estos dos callbacks. El callback de VBlank de
`03007308` copia o restaura los buffers; no invoca estas rutinas ARM. Por eso
la comprobación de la bandera no corta una última llamada ARM de VBlank
durante la liberación.

Referencias primarias del desensamblado fijado:
[rutinas ARM](https://github.com/KarisaAdvynia/sma3-disasm/blob/c8532ec9a8d0038c3bfeb003dd0c7ea89d7e1071/asm/CodeStart.asm),
[asignación y recuperación del efecto](https://github.com/KarisaAdvynia/sma3-disasm/blob/c8532ec9a8d0038c3bfeb003dd0c7ea89d7e1071/asm/GameOver.asm),
[llamada del bucle principal](https://github.com/KarisaAdvynia/sma3-disasm/blob/c8532ec9a8d0038c3bfeb003dd0c7ea89d7e1071/asm/GameInitMain.asm).

## Contacto y recuperación dentro del juego

La entrada inicial es **sintética y privada**: se pide al inicializador original
que cargue el subnivel 6 cerca del generador original de Fuzzy. No se alteran
la ROM, los sprites, las rutinas del juego ni los contadores del efecto. Tras
esa entrada, solo se aplican pulsaciones y se lee memoria para observar el
resultado.

| Evento | Fotograma del recorrido | Fotograma del juego |
| --- | ---: | ---: |
| Primer contador de contacto distinto de cero | **599** | **7198** |
| Primera imagen con el efecto activo | **600** | **7199** |
| Recuperación completa por la lógica original | **1382** | **7981** |
| Fin de la prueba | **1500** | **8099** |

Ambas anchuras, **240 y 356**, producen la misma secuencia de **1.501
observaciones** incluyendo la inicial. Se observan **782 fotogramas de efecto
activo**, amplitud máxima `24576` y **119 fotogramas normales al final**. El
contrato exige al menos 100 fotogramas normales finales y rechaza reinicios,
huecos del contador, muerte, ausencia del efecto o una recuperación forzada.
La revisión posterior de las mismas observaciones confirma **686 decrementos
del contador**, **191 aumentos de amplitud** y **96 pasos de disminución gradual**
antes de volver a la normalidad. Esta revisión solo lee las capturas ya
registradas; no se cuenta como una ejecución adicional del juego.

Los **33 pares de capturas de diagnóstico** conservan exactamente el centro
de 240 × 160. Estas capturas se obtienen con el servidor de diagnóstico; las
pruebas de presentación SDL de la entrega se documentan por separado.

Ambas ejecuciones finalizan como `FULLY_STATIC`, con **cero fallos de despacho,
cero instrucciones interpretadas y cero reparaciones dinámicas**. El
intérprete se utiliza únicamente como referencia en la prueba unitaria, no
como solución dentro del juego ni en el APK.

### Comprobar el validador de recuperación

[`test_fuzzy_status.py`](test_fuzzy_status.py) añade **18 pruebas** que pasan
tanto con Python normal como con `python -O`. Usan una secuencia sintética
breve para comprobar el contrato de evidencia; no simulan el juego. Rechazan
una secuencia completamente normal, un contador sin efecto visible, la falta
de observaciones anteriores al contacto, saltos o reinicios de fotogramas,
terminación prematura, muerte y cambio de escena o subnivel.

La revisión encontró y corrigió una debilidad concreta del validador anterior:
un efecto seguido de contadores puestos a cero y un tramo normal podía parecer
una recuperación válida. Ahora se exige la cuenta atrás y después la reducción
gradual original de la amplitud, incluidos los pasos de `0x100` de la fase final.
Las pruebas negativas cubren tanto borrar el efecto antes de completar la
cuenta atrás como saltarse esa disminución gradual. Este cambio afecta solo a
la validación y no modifica el comportamiento del juego.

### Cargar una partida durante el efecto

También se carga el estado guardado en el fotograma **7499**, con el mareo
activo, contador **387** y amplitud **24576**. Se ejecutan otros **600
fotogramas**, tanto a 240 como a 356 de ancho. En ambos casos, las **601
observaciones** coinciden con el tramo correspondiente de la ejecución sin
interrupción: la recuperación ocurre 482 fotogramas después de cargar y quedan
119 fotogramas normales al final. No hay fallos de despacho ni reinicio.

Esto cubre la restauración de los punteros dinámicos y del efecto, incluyendo
un estado de 240 de ancho restaurado en 356. Es una prueba del sistema de
estados en el host; no se presenta como una prueba física del ciclo de vida
de una actividad Android.

## Prueba de equivalencia ARM

[`effects_test.cpp`](effects_test.cpp) ejecuta **144 escenarios** y
**3.422 comprobaciones** sobre las dos rutinas:

- Tres asignaciones de RAM distintas y desplazamientos que cruzan ambos bancos
  de columnas.
- Retorno a ARM y a Thumb.
- Reanudación tras interrupciones de diagnóstico en cuatro posiciones,
  incluyendo la entrada normal sin pausa.
- Comparación exacta de los registros, CPSR, IWRAM, EWRAM y coste de ciclos
  frente al intérprete ARM del motor.
- Rechazo de modo incorrecto, PC desalineado, fin de asignación, código
  alterado, efecto inactivo, callback antiguo, asignación fuera del área,
  puntero impar y asignación liberada.

El coste de ciclos de esta prueba se cuenta con el reloj de validación del
runtime, sin PPU ni entrega de IRQ físicas. Las pausas comprueban reanudación
de instrucciones; no se presentan como una simulación de todas las posibles
interrupciones de hardware. El recorrido del juego sí utiliza el scheduler
y la PPU normales.

Los mismos 144 escenarios también pasan con **AddressSanitizer y
UndefinedBehaviorSanitizer** sobre el dispatcher, las nuevas variantes ARM y
la prueba. Se reutilizan las bibliotecas del motor sin instrumentarlas de nuevo;
no se afirma una ejecución completamente instrumentada del juego. LeakSanitizer
permanece desactivado por las limitaciones del entorno.

## Reproducir con archivos propios

Se requieren la ROM y BIOS compatibles aportadas localmente y un estado de
juego normal con la memoria inicializada. No se publican esos archivos.

```sh
python tools/generate_effects.py --rom /ruta/juego.gba --recompiler /ruta/gba_recompile
cmake --build /ruta/build --target sma3_runner sma3_effects_test
/ruta/build/sma3_effects_test /ruta/juego.gba

python validation/make_fuzzy_fixture.py /ruta/normal.state /ruta/fuzzy-entry.state
python validation/run_panorama_session.py \
  --rom /ruta/juego.gba --bios /ruta/bios.bin \
  --state /ruta/fuzzy-entry.state \
  --actions validation/fuzzy-contact-recovery-actions.json \
  --runner /ruta/build/sma3_runner --width 240 \
  --fuzzy-status --expect-fuzzy-recovery --output /ruta/fuzzy-240
```

El runner debe compilarse con `SMA3_WIDESCREEN_DIAGNOSTICS=ON` para comparar
las dos anchuras. Repite la última orden con `--width 356` y otro directorio
de salida nuevo. Cada salida conserva `run-status.json`, observaciones por
fotograma, cobertura nativa, capturas y huellas de los archivos utilizados.

La prueba de regresión anterior se ejecuta con el binario intacto de 0.10.1:
falla al alcanzar el primer contacto. El candidato corregido supera el
recorrido completo con los mismos archivos e instrucciones.

[Resultados y procedencia de esta ejecución](android-0.10.2-fuzzy.json).
