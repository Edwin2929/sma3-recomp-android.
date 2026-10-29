# Validación de Color de Yoshi — Android 0.10.8 beta

Fecha: 29 de octubre de 2026. Resultado de las pruebas descritas: **PASS**.
APK: `sma3-recomp-0.10.8-beta-arm64.apk`, versión `0.10.8-beta`, código `19`.

Esta entrega incorpora el selector **Atrás → Mods → Color de Yoshi** y
corrige un fallo previo en la selección de efectos de brillo de los sprites.
Las comprobaciones cubren el código nativo, la interfaz Android en un entorno
de pruebas, partidas reproducidas en Linux y el APK firmado. **No se ha
probado esta versión en un teléfono ni se han medido sus FPS en Android.**

## Resultados

| Comprobación | Resultado | Alcance |
| --- | --- | --- |
| Color selectivo y compositor | 5 grupos, 932.845 comprobaciones internas | Selección de sprites, paletas, sombras, fundidos, memoria inmutable y cambio de preferencia |
| ASan y UBSan | PASS | Mismos grupos nativos; exclusiones explicadas debajo |
| Ventanas del renderizador | 488 escenarios, 8.102.978 aserciones internas | Compositor original y panorámico |
| Pila de parches | 3 pruebas | 16 parches, 17 estados reconocidos y 9 archivos protegidos |
| Modelo del selector | 5.014 comprobaciones | HEX, RGB, paleta, sombras y datos inválidos |
| Interfaz Android | 4 pruebas Robolectric | Aplicar, cancelar, volver, persistencia, errores y distribución |
| Partidas nativas | 6 configuraciones, 27 ejecuciones, 18.138 fotogramas ejecutados | Vista original, panorámica, castillo, transformación y Fuzzy |
| Rutinas ARM relacionadas con Fuzzy | 144 escenarios, 3.422 comprobaciones | Registros, flags, memoria y ciclos frente al intérprete |
| Presentación conservada de 0.10.7 | 4.992 + 71 + 8 comprobaciones | Cadencia, trabajo cooperativo y métricas; pruebas sintéticas, no FPS de un teléfono |
| APK | PASS | Compilación, firma, manifiesto, JNI, bibliotecas ARM64 y contenido |

Los conteos de aserciones internas no representan ese número de niveles,
dispositivos o partidas independientes. Los resultados estructurados están
en [android-0.10.8-yoshi-color.json](android-0.10.8-yoshi-color.json).

## Aislamiento del color

La prueba nativa usa las ocho paletas de referencia y comprueba los tres
tonos del cuerpo, los fundidos legítimos y las paletas desconocidas. Recorre
los 16 bancos y los 1.024 índices de tile para verificar el selector.
Baby Mario, los zapatos, el contorno, las partes blancas, los fondos y los
objetos no seleccionados mantienen sus entradas originales. Las paletas
de entrada permanecen intactas.

Las escenas sintéticas ejecutan el PPU real a **240 × 160** y **356 × 160**,
por fotograma y por scanline, incluyendo objetos afines. En la escena de
aislamiento cambian exactamente los 24 píxeles previstos del cuerpo y
ningún otro. Se comprueban 256 combinaciones de tamaño, activación del mod,
transparencia, objetivos, ventanas y modos de efectos, además de límites de
brillo y fondos desactivados.

ASan y UBSan pasan sobre esas mismas pruebas. UBSan excluye `vptr` porque
el ejecutable aislado sustituye `active_bus` y no enlaza la clase completa
`GbaBus`; LeakSanitizer está desactivado por las restricciones de inspección
de procesos del entorno. No se presenta esto como una prueba de fugas.

[Prueba reproducible, hashes y referencias](yoshi-color-test.txt) ·
[Código de la prueba](yoshi_color_test.cpp).

## Interfaz y guardado

Se compilaron las 29 fuentes Java de aplicación y SDL con API 35 y JDK 17.
Las pruebas de interfaz utilizan Robolectric 4.16.1, API 34 y gráficos
nativos. Se revisaron las vistas del selector en **360 × 760**, **600 × 328**,
**264 × 560** y **360 × 240 dp**, incluidas las pestañas de paleta y RGB.
Los botones de aplicación y cancelación permanecen fuera del área desplazable.

Se verifican los 48 colores distintos, los códigos cortos y completos,
las barras RGB, el borrador independiente de la preferencia guardada, la
cancelación con botón o Atrás, la restauración del color original y el
arranque con preferencias guardadas. Los errores de guardado o JNI no se
tratan como éxito. El arnés de interfaz sustituye JNI; la integración nativa
se prueba por separado y el APK comprueba la correspondencia de firmas.

[Informe y reproducción de la interfaz](YOSHI_COLOR_UI_VALIDATION.md).

## Reproducciones de partidas

Todas estas ejecuciones usan el candidato final con los **16 parches**.
Cada configuración ejecuta la versión anterior, el candidato con el mod
desactivado y el candidato con los colores indicados.

| Escena | Ancho | Fotogramas por ejecución | Colores ensayados | Resultado |
| --- | --- | --- | --- | --- |
| Nivel exterior | 240 | 120 | `FF304F`, `A855F7`, `00D4C8`, `000000`, `FFFFFF` | PASS |
| Nivel exterior | 356 | 120 | Los mismos cinco | PASS |
| Castillo | 356 | 120 | `00D4C8`, `A855F7` | PASS |
| Yoshi → coche → Yoshi | 356 | 1.526 | `A855F7` | PASS |
| Contacto y recuperación de Fuzzy | 240 | 1.900 | `A855F7` | PASS |
| Contacto y recuperación de Fuzzy | 356 | 1.900 | `A855F7` | PASS |

Las 21 comparaciones comprueban la traza del juego, 277 estados capturados
y 256 pares de imágenes. CPU, memoria del bus, registros de E/S, audio,
guardado y metadatos coinciden; solo se excluyen los ocho bytes del reloj
de pared del RTC inactivo. Los tiempos del PPU coinciden y los bytes de
imagen se comparan por separado. No aparecen faltas de despacho ni ejecución
de instrucciones interpretadas en estas reproducciones.

En la transformación se comprueban **25 capturas estables del coche sin
ninguna recoloración** y el regreso del color al recuperar Yoshi. En ambos
ensayos de Fuzzy se conserva el efecto: aparece en el fotograma transcurrido
1.018, se recupera naturalmente en el 1.800 y termina con **101 fotogramas
normales consecutivos**, sin reinicio ni muerte. La preparación de entrada
usa un estado de prueba; el contacto, el efecto y la recuperación se ejecutan
con las rutinas del juego.

El mod activo se compara siempre con **el mismo candidato desactivado**.
Así se separa su efecto de la corrección del compositor descrita a
continuación. Las diferencias de las capturas reales demuestran que el
color se dibuja; el aislamiento exacto de sprites se apoya además en la
prueba sintética del PPU.

### Corrección previa de brillo identificada durante las pruebas

Antes del parche 16, los objetos normales no respetaban el bit de primer
objetivo de BLDCNT. La prueba nueva reproduce el fallo con ese código
anterior: `FAIL: full hardware fade remains black`. El compositor corregido
respeta el brillo y oscurecimiento seleccionados y mantiene separado el
alpha forzado de los objetos semitransparentes.

Por eso, durante las secuencias de Fuzzy hay diferencias de imagen entre
0.10.7 y el candidato **incluso con el mod desactivado**: 25.957 píxeles
acumulados en 240 y 26.260 en 356, repartidos en 16 capturas por tamaño.
Cada píxel diferente es compatible exactamente con la fórmula de brillo
u oscurecimiento del compositor existente. No hay cambios en la ejecución
del juego ni en los tiempos del PPU. Este análisis atribuye las diferencias
al cambio de efectos; las pruebas de selección de objetivos y sus referencias
técnicas constituyen la comprobación independiente de la corrección.

### Reproducción y datos privados

El verificador [validate_yoshi_color_gameplay.py](validate_yoshi_color_gameplay.py)
recibe los ejecutables anterior y candidato, ROM, BIOS, estado inicial,
secuencia de entradas, ancho y directorio de salida. `--fuzzy` exige contacto
y recuperación natural; `--transform-cycle` exige el ciclo de transformación.
`--verify-obj-brightness-fix` permite únicamente diferencias atribuibles a la
fórmula existente al comparar la versión anterior con el mod desactivado.
No relaja la comparación de estados ni la comparación del mod con su propio
candidato desactivado.

La secuencia de Fuzzy está en
[yoshi-color-fuzzy-actions.json](yoshi-color-fuzzy-actions.json). El informe
estructurado registra los hashes de los ejecutables y las entradas usadas.
No se incluyen ROM, BIOS, estados de partida, claves ni capturas del juego
en el repositorio o en el APK.

## APK y procedencia

- Tamaño: **27.936.024 bytes**.
- SHA-256: `1cfe2f32ddd8f180df5ed1078afdabbcea6facb26b82cce178f0c9e18ed82a26`.
- Firma: mismo certificado que las entregas anteriores; firma v2 verificada.
- Arquitectura: ARM64. Android mínimo: API 28. SDK objetivo: 35.
- Las 19 firmas JNI concuerdan con DEX, incluido `setYoshiColor(ZI)V`.
- Las tres bibliotecas nativas coinciden con las compiladas y son AArch64.
- Los 1.801 archivos de entrada de compilación permanecieron idénticos
  antes y después del ensamblado.

[Manifiesto de compilación y verificación](android-0.10.8-build.json) ·
[Notas de entrega](../docs/releases/v0.10.8-beta.md).

## Límites

No se ha completado el juego entero ni probado todos los objetos, escenas,
transformaciones o teléfonos. El mod reconoce el Yoshi normal durante el
juego; vehículos, mapa y gráficos no identificados conservan el color
original. Los colores se aproximan a cinco bits por canal. Los filtros y
perfiles globales pueden alterar el resultado final.

Al cargar un estado instantáneo a mitad de un fotograma puede aparecer la
imagen guardada o un cuadro parcial antes de que la selección se actualice
al comenzar el siguiente fotograma. La panorámica conserva su etiqueta beta.
Las pruebas de presentación son sintéticas y no demuestran 120 FPS en un
Pixel 7 Pro. Queda la prueba física del selector, instalación, rotación,
segundo plano y rendimiento en Android.
