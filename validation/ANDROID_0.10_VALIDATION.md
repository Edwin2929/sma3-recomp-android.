# Android 0.10 — integración panorámica, compilación y validación

Validación realizada el **25 de octubre de 2026 (UTC)**, durante la noche
local del 24 de octubre en El Salvador. La entrega es **experimental** y
permite seleccionar la panorámica real desde Android. La cobertura de
escenas y las pruebas físicas tienen los límites indicados abajo.

## Resultado y artefacto

El APK **`sma3-recomp-0.10-panorama-arm64.apk`** corresponde al paquete
`org.sma3.recomp.experimental`, versión **`0.10-panorama-experimental`**, código
**11**, ARM64, Android 9/API 28 o posterior y SDK objetivo 35. La identidad,
la firma, el tamaño, el SHA-256 y las bibliotecas de la entrega están
registrados en [android-0.10-build.json](android-0.10-build.json) y en las
[notas de versión](../docs/releases/v0.10-experimental.md).

La base del cambio es
[`bf83a2685639fd8b6eca05a6de1d467e64220cef`](https://github.com/Edwin2929/sma3-recomp-android./commit/bf83a2685639fd8b6eca05a6de1d467e64220cef).
El motor está fijado a `e7728148c6829ba526f682876430a0c9022dc6c0` y recibe los
siete parches ordenados del repositorio. Las fuentes funcionales se congelaron
antes de las pruebas finales y del último ensamblado de Android.

## Comportamiento integrado

El menú **Atrás → Imagen** ofrece tres opciones excluyentes en ES/EN/PT:

- **Original (3:2):** fotograma completo de 240 × 160, centrado.
- **Panorámica (experimental):** altura lógica 160 y ancho adaptable de 240
  a 356 en escenas admitidas; retorno a Original si faltan datos coherentes.
- **Estirar a pantalla completa:** cambia las proporciones del fotograma
  original para llenar el destino; no muestra más escenario.

La calidad Automática/720p/1080p es independiente. La antigua preferencia
`stretch-screen` se migra a Original o Estirar únicamente cuando aún no
existe `image-mode`. Original es el valor inicial; los valores de modo no
admitidos vuelven a Original. La revisión de Java y JNI confirma dos enteros,
`setVideoOptions(II)V`, y una única selección atómica en el código nativo.

La orientación Android es `fullUser` y la nativa es `Any`. El ancho se calcula
con el área que recibe el renderizador. El límite de 356 y el redondeo pueden
dejar bandas incluso en horizontal. En vertical conserva 240 × 160.

Se requieren juego normal compatible, mapa y fondos habilitados válidos,
y una lista de objetos coherente con los atributos mostrados. La selección
de Original o Estirar retira el observador propio y sus proveedores.
La panorámica no modifica RAM, registros, temporización ni lógica de juego.
**Tampoco amplía los límites de aparición o dibujo originales**: los objetos
que el juego no llegó a emitir no se detectan necesariamente mediante OAM.

El compositor extiende OBJ-window solo en las filas con posiciones verificadas.
WIN0/WIN1 y otros casos no admitidos conservan márgenes negros por fila;
esa política es distinta del retorno completo a Original por falta de datos
de la escena. No se atribuye cobertura total a esas salvaguardas.

## Defecto comprobado y corregido

Una restauración de estado a mitad de fotograma podía conservar en la
superficie ampliada los márgenes de la escena anterior, aunque el centro ya
procediera del estado cargado. Se reprodujo con una restauración en la línea
80 y el caso falló antes del cambio.

El séptimo parche, `gbarecomp-panorama-android.patch`, limpia la superficie
de trabajo activa antes de restaurar el centro nativo. La misma regresión
pasa después del cambio. Además, la autorización de presentación comprueba
la época del estado y el bus activo; no reutiliza inmediatamente el historial
anterior a una carga. Hasta disponer de una observación coherente conserva
la vista original. Al cambiar de anchura, la mezcla temporal descarta el
fotograma de tamaño anterior.

## Pruebas ejecutadas

| Grupo | Resultado comprobado | Evidencia |
| --- | --- | --- |
| Ciclo de vida PPU y geometría | 40 escenarios, 2.082.122 comparaciones internas; ASan/UBSan sin errores. | [Informe](android-0.10-lifecycle-checks.json) |
| Compositor real de ventanas | 488 escenarios, 8.102.978 comparaciones internas; ASan/UBSan sin errores. | [Informe](android-0.10-lifecycle-checks.json) |
| Validadores Python y parches | 169 pruebas normales y 169 con `-O`; ocho etapas de los siete parches, repetición y rechazo de ediciones desconocidas. | [Informe](android-0.10-python-checks.json) |
| Castillo, pareja 240/356 | 6.801 fotogramas, 250 centros de imagen idénticos, 251 estados equivalentes y dos transiciones naturales por tubería. | [Contrato y resultado](widescreen-castle-android-0.10.json) |
| Coche, pareja 240/356 | 3.506 fotogramas, 61 centros idénticos, 62 estados equivalentes y ciclo normal → coche → normal completo. | [Contrato y resultado](widescreen-car-android-0.10.json) |
| Regresión frente a la revisión anterior | 622 imágenes completas y 626 estados conservados en las cuatro sesiones, incluidos márgenes y PPU. | [Informe](android-0.10-native-source-regression.json) |
| Presentación de producción | Siete casos SDL, cinco comparaciones centrales exactas y contadores finales iguales a las referencias. | [Informe](android-0.10-production-presentation.json) |

Las comparaciones internas de píxeles no son millones de pruebas o
fotogramas independientes. LeakSanitizer está desactivado por las limitaciones
del entorno de inspección de procesos; AddressSanitizer y
UndefinedBehaviorSanitizer permanecen activos.

### Presentación de producción

Estas ejecuciones utilizan los callbacks reales de `SMA3_PANORAMA`,
`SMA3_IMAGE_MODE` y `--resize-view`, sin `SMA3_WIDE_PROBE_DIR`. SDL emplea
un renderizador software y una ventana de host 720 × 480; las dimensiones
siguientes se proporcionan mediante `GBARECOMP_FORCE_DRAWABLE` para comprobar
el cálculo y el framebuffer, sin simular todo Android.

| Caso | Superficie simulada | Fotograma lógico final |
| --- | --- | --- |
| Original | 2400 × 1080 | 240 × 160 |
| Panorámica | 3120 × 1440 | 347 × 160 |
| Panorámica | 2400 × 1080 | 356 × 160 |
| Panorámica vertical | 1080 × 2400 | 240 × 160 |
| Estirar, fuente | 2400 × 1080 | 240 × 160 |
| Transición no admitida, Original | 2400 × 1080 | 240 × 160 |
| Transición no admitida, Panorámica | 2400 × 1080 | 240 × 160 |

Los cinco casos de juego ejecutan 120 presentaciones cada uno; los dos de
transición, 12. Los márgenes de las panorámicas contienen escenario y varios
colores, y se registra una sola transición de anchura original a amplia,
sin oscilación posterior. Tras cargar el estado se observan datos incompletos
antes de esa transición; no se deduce un número exacto de fotogramas visibles
a partir de esos mensajes.

El caso Estirar comprueba que la fuente sigue siendo nativa. El ajuste del
destino Android se respalda por la revisión del código y por las pruebas
previas del escalador: [45 casos de geometría y 110 comprobaciones SDL](VIDEO_ASPECT_RATIO.md).
No se presentan estas pruebas históricas como ejecuciones nuevas de Android.

### Recorridos y conservación del estado

El runner final de host tiene SHA-256
`f01eddcfa2875a9905c9f5403ddebbfdeac68effe3bb1daed339ff498abd8e16`.
Las cuatro sesiones usan las cintas, entradas sintéticas y contratos existentes,
sin restauraciones intermedias ni cambios de criterios para obtener el resultado.
Todas terminan con cobertura `FULLY_STATIC`, cero faltas de despacho, cero
instrucciones interpretadas y cero reparaciones nativas. Los recorridos
registran cero huecos de posiciones, objetos visibles sin resolver y envíos
incoherentes.

La comparación entre anchuras excluye el estado de presentación PPU y
únicamente los ocho bytes del reloj RTC inactivo ya documentados. La
comparación contra la revisión anterior a igual anchura incluye PPU y todas
las demás secciones, conservando solo esa excepción RTC. Los contratos no
han ampliado las excepciones. La regresión comprueba 228 de 250 capturas del
castillo y las 61 del coche con contenido lateral no negro; el resultado
no se explica por mantener todos los márgenes vacíos.

[Resumen nativo, comandos y hashes](android-0.10-native-checks.json).
Los informes publicados contienen resultados y huellas; los archivos privados
de ROM, BIOS, partidas, capturas y código generado no se publican.

## Compilación y actualización

Se compilan de nuevo las bibliotecas ARM64 y los 64 fragmentos generados del
juego. La variante Android es `debug`, con código nativo `Release`, como
corresponde a esta entrega experimental de desarrollo. No se reutiliza el
archivo ARM64 antiguo: no contenía todas las
referencias necesarias de la instrumentación actual. Las fuentes generadas
privadas sí se reutilizan; esta ejecución no repite su generación desde
ROM/BIOS. CMake tiene `SMA3_PANORAMA=ON`, diagnósticos panorámicos de escritorio
`OFF` y ninguna biblioteca de juego precompilada seleccionada.

La firma utiliza el certificado original:

```text
2390f57b35d436ff37b2a303189d18d463c8f5114a5ff809b2f2e6df60f58da3
```

El mismo paquete y certificado, con versionCode 11 superior al 10 de 0.9.1,
permiten preparar la actualización. La revisión de `installPayload` confirma
que solo reemplaza archivos presentes en el contenido distribuido y protege
los datos del jugador. El paquete no contiene ROM, BIOS ni clave de firma.
La instalación real encima de 0.9.1 y la conservación de partidas en el
Pixel aún no se han ejecutado en esta validación.

Los detalles del ensamblado, las herramientas, la firma, la alineación, el
DEX y la coincidencia de bibliotecas están en
[android-0.10-build.json](android-0.10-build.json).

## Reproducción

El repositorio conserva los scripts de construcción y requiere los archivos
compatibles aportados localmente. La matriz de producción puede repetirse con:

```sh
python3 validation/run_production_presentation.py \
  --runner /ruta/sma3_runner \
  --rom /ruta/sma3-usa.gba --bios /ruta/gba_bios.bin \
  --gameplay-state /ruta/escena-compatible.state \
  --fallback-state /ruta/transicion-no-compatible.state \
  --output /ruta/nueva-prueba-privada
```

El informe de esa matriz registra las huellas de las entradas utilizadas.
El resumen nativo incluye los comandos de las rutas y comparadores. El nuevo
argumento `--runner` de `run_panorama_session.py` permite seleccionar el binario
sin sustituir las pruebas históricas. La regresión de ciclo de vida está en
[panorama_lifecycle_test.cpp](panorama_lifecycle_test.cpp), con las opciones de
compilación y fuentes registradas en su informe JSON.

## Pendiente en el teléfono y cobertura abierta

La entrega completa la integración, la compilación, el firmado y las
comprobaciones descritas. Quedan las pruebas que requieren el dispositivo:
instalar encima de 0.9.1; alternar los tres modos; rotar; suspender y reanudar;
comprobar controles, preferencias y partidas; y medir rendimiento durante
sesiones largas. La confirmación anterior de Edwin en Pixel 7 Pro pertenece
a **0.9.1**, no a esta compilación.

Tampoco se ha completado la cobertura de todos los niveles, jefes, mensajes,
transformaciones, ventanas y objetos fuera del encuadre original. La
panorámica es opcional y experimental. Original (3:2) permanece disponible.
No se afirma una mejora de FPS, estabilidad a 120 FPS ni ausencia total de
defectos laterales.

El mantenedor realiza la publicación del APK en Releases manualmente.
Este informe acredita la construcción y verificación del archivo entregado,
no una publicación pública del binario ni una ejecución física de Android.
