# SMA3 Recomp para Android

Proyecto experimental de recompilación estática de **Yoshi’s Island: Super Mario Advance 3 (USA)** para Android ARM64, basado en [GBARecomp](https://github.com/mstan/gbarecomp) y en el análisis de [sma3-disasm](https://github.com/KarisaAdvynia/sma3-disasm).

## Versión actual: 0.9.1 experimental

- **0.9.1:** corrige el redondeo del escalador al conservar la proporción original **3:2** y aclara la ayuda de imagen en español, inglés y portugués. El APK está compilado y entregado; **su subida manual a GitHub está pendiente**. [Notas de versión](docs/releases/v0.9.1-experimental.md).
- **Reporte de Edwin, 18 de octubre de 2026:** probó este APK en un **Google Pixel 7 Pro** y confirmó que funciona. No especificó la versión de Android, la duración de la prueba, los FPS ni las funciones comprobadas. [Verificación del APK y alcance de la prueba](validation/ANDROID_0.9.1_VALIDATION.md).

- **0.9:** ajustes agrupados en Rendimiento, Imagen, Controles e Idiomas; salto de la animación de BIOS; editor de posición/tamaño de botones con tres diseños guardables. [Guía](docs/AJUSTES_Y_DISENOS.md).
- **Reporte de Edwin, 5 de octubre de 2026:** 0.8 alcanza 120 FPS en su Pixel 7 Pro. Es un reporte del usuario; no constituye una validación del editor y la navegación incorporados en 0.9.

- **0.8:** idioma independiente del menú (ES/EN/PT), solicitud del modo de pantalla de 120 Hz y recuperación de retrasos moderados del temporizador. El usuario confirmó 120 FPS; no se ha medido independientemente su estabilidad prolongada. [Cambios y prueba en el teléfono](docs/MENU_Y_120FPS.md).

- **0.5:** cobertura nativa de la rutina de transformación, mapeo de mando y opción para ocultar los controles táctiles. [Detalles](docs/CORRECCION_TRANSFORMACIONES.md).
- **0.6:** escalado de presentación Automático, 720p y 1080p, con opción para estirar la imagen. Los gráficos originales siguen siendo de 240 × 160. [Detalles](docs/CALIDAD_IMAGEN.md).
- **0.7:** selector de inglés, español latinoamericano y portugués de Brasil. Traducción parcial de 187 entradas por idioma; incluye 61 nombres de niveles. Créditos, rótulos gráficos y Mario Bros siguen en inglés. [Cobertura](docs/IDIOMAS.md).
- Se conservan el contador de FPS y la mezcla temporal opcional de 120 FPS de 0.4. No son 120 estados independientes del juego; puede producir estelas. [Límites](docs/INTERPOLACION_120.md).

Abre las opciones con **Atrás del teléfono**. Los ajustes se conservan entre sesiones.

### Imagen completa sin deformaciones

En **0.9 y 0.9.1**, abre **Atrás → Imagen → Estirar a pantalla
completa** y **desactiva** la casilla. La imagen original de GBA es de
**240 × 160 píxeles (3:2)**. El juego conserva esa proporción y se centra con
bandas si la pantalla tiene otra proporción. Estirar llena el área de juego,
pero puede deformar la imagen y no muestra más escenario.

La **panorámica real**, que dibuja escenario adicional, sigue siendo un
**prototipo de escritorio y no está integrada en Android 0.9 ni 0.9.1**.
El APK 0.9.1 ya incluye la ayuda del menú aclarada y la corrección de redondeo
integradas en la [PR #2](https://github.com/Edwin2929/sma3-recomp-android./pull/2).
El APK 0.9 publicado en GitHub conserva los textos y el escalador anteriores.
[Guía de imagen y pruebas del escalador](docs/CALIDAD_IMAGEN.md) ·
[Estado del prototipo panorámico](docs/PANORAMICA_EN_DESARROLLO.md).

## Descargar e instalar

### APK 0.9.1 entregado, pendiente de subida manual

Archivo: `sma3-recomp-0.9.1-aspect-arm64.apk` (31.759.347 bytes), versión
`0.9.1-aspect-experimental`, código de versión `10`, Android 9 o posterior, ARM64.
Se entregó a Edwin el 18 de octubre de 2026 y todavía **no está disponible en
GitHub Releases**.

SHA-256:

```text
3885b7447455fd16c4af8882de88175d5d1efc361b7e9b618708839772d97d18
```

El APK conserva el identificador de aplicación y la firma de 0.9, con un código
de versión superior, por lo que está preparado para instalarse encima como
actualización. El reporte del usuario confirma que funciona en su Pixel 7 Pro;
no detalla el procedimiento de instalación ni la conservación de partidas.

[Notas e instrucciones para publicar 0.9.1 manualmente](docs/releases/v0.9.1-experimental.md).

### APK 0.9 disponible en GitHub

**El APK 0.9 está publicado en la release «Descarga».** Consulta [Releases](https://github.com/Edwin2929/sma3-recomp-android./releases).

Archivo: `sma3-recomp-0.9-layouts-arm64.apk` (31.838.319 bytes), Android 9 o posterior, ARM64.

SHA-256:

```text
7110024294051368945fd4ceb127cfaf4edb3a09f0e138ae6b81ae350a3dee3d
```

El APK de desarrollo 0.9 conserva la firma de las versiones anteriores: instálalo encima para conservar partidas y archivos importados. Las compilaciones de otras personas tendrán otra firma.

[Notas e instrucciones para publicar el APK manualmente](docs/releases/v0.9-experimental.md).

## Estado de validación

**0.9.1:** la compilación ARM64 terminó correctamente y se verificaron la firma,
su coincidencia con 0.9, la integridad y alineación del APK, los textos de ayuda
en ES/EN/PT y la correspondencia de las bibliotecas empaquetadas con esta
compilación. El APK no incorpora ROM, BIOS ni clave de firma.
La confirmación de Edwin del **18 de octubre de 2026** aporta una prueba de
funcionamiento en **Google Pixel 7 Pro**. No equivale a una medición de rendimiento
ni a una validación específica de cada ajuste, orientación o transformación.
[Registro completo y comprobaciones pendientes](validation/ANDROID_0.9.1_VALIDATION.md).

Edwin reportó 60 FPS con una versión anterior en Android y posteriormente identificó su teléfono como Google Pixel 7 Pro. Ese reporte **no valida el rendimiento de 0.5–0.7 ni confirma 120 FPS**.

Las pruebas de desarrollo cubren la rutina de transformación, el escalador y los punteros de traducción. Se ejecutaron 7.200 fotogramas nativos por idioma sin faltas de despacho ni instrucciones interpretadas. Siguen pendientes las comprobaciones específicas de estas funciones en el teléfono, todas las transformaciones y el juego completo. [Resultados y límites](docs/VALIDACION.md).

### Prototipo panorámico de escritorio

Se ha verificado una transición natural por tubería del corredor inferior a la
zona superior del nivel 3/subnivel 3, comparando **240 × 160 y 356 × 160** desde
la misma entrada sintética. La ruta continua de **3.907 fotogramas** cubre ese
recorrido y una sola salida; no certifica el castillo completo ni el rendimiento.
La panorámica real sigue fuera de los APK Android **0.9 y 0.9.1**.
[Resultado y límites de la transición](validation/CASTLE_TRANSITION_VALIDATION.md).

**Reconstrucción del 23 de octubre de 2026 (UTC):** se recuperaron los
observadores de la nube puente, el objeto compuesto `0x064`, las piezas afines
`0x076/0x077` y las copias de objetos para ventanas. La nueva ruta continua de
**130 acciones y 6.801 fotogramas** conserva la primera tubería y atraviesa una
segunda, del subnivel 3 al 153. La pareja corregida supera el contrato de
recorrido, salud y cobertura: **250 centros RGB idénticos, 251 comparaciones de
estado equivalentes y cero huecos de posiciones**. La regresión del ciclo
normal → coche → normal también pasa con esta compilación. Las cinco pruebas
de C++ pasan con ASan/UBSan y las 166 pruebas Python pasan normal y con `-O`.

Esa reconstrucción calificó el recorrido y la procedencia de los objetos,
pero dejó la máscara OBJ-window limitada a los 240 píxeles originales.
[Resultados nuevos, procedencia y límites](validation/CASTLE_POST_PIPE_VALIDATION.md).

**Iluminación lateral, 23 de octubre de 2026 (UTC):** el prototipo amplía ahora
la máscara OBJ-window a los márgenes cuando todas las posiciones de esa fila
están verificadas y no intervienen WIN0/WIN1. La nueva pareja de recorridos
conserva **250 centros RGB idénticos, 251 estados equivalentes y cero huecos de
posiciones**. La comparación con la revisión anterior solo cambia los márgenes
de nueve capturas de la sala iluminada; desaparecen las franjas negras de
precaución y se conserva la forma de la luz. Pasan las pruebas del compositor
real con ASan/UBSan y la regresión del coche. Los tipos de ventana todavía no
admitidos mantienen la política anterior. Este avance continúa siendo de
escritorio; no certifica el castillo completo, un APK panorámico o los FPS.
[Pruebas de la máscara, reproducción y límites](validation/WINDOW_MASK_VALIDATION.md).

## Qué significa «nativo» aquí

El código de CPU del juego y de la BIOS se traduce a C++ y se compila para el dispositivo. El motor sigue reproduciendo el funcionamiento del hardware de GBA. No es una reescritura completa del juego independiente de ese hardware.

El inicio activa `GBARECOMP_STRICT_STATIC=1`, `GBARECOMP_FORCE_INTERP=0`, `GBARECOMP_BIOS_HLE=0` y `GBARECOMP_BIOS_SKIP_INTRO=1`. Una rutina no cubierta detiene la ejecución; no se utiliza el intérprete para ocultar fallos de cobertura.

## Contenido del repositorio

| Ruta | Contenido |
|---|---|
| `src/` | Inicio nativo y selección de rutinas copiadas a RAM |
| `android/` | Interfaz, opciones de FPS, navegación Atrás y configuración Gradle |
| `game.toml`, `ram.toml`, `bios.toml` | Configuración de recompilación |
| `runtime.toml` | Configuración de ejecución |
| `symbols.tsv` | Direcciones y nombres usados como semillas |
| `tools/` | Generación local, compilación y comprobaciones |
| `validation/*.csv` | Secuencias de entradas para las pruebas |
| `licenses/` | Licencias de las dependencias |

**No se incluyen ROM, BIOS, código generado a partir de esos archivos, APK, partidas guardadas, claves de firma ni cachés de compilación.** Cada persona genera los archivos necesarios localmente. El repositorio no requiere los ZIP privados usados durante el desarrollo.

## Requisitos

- Linux con Python 3, Git, curl, JDK 17, compilador C++20 y CMake 3.22 o posterior.
- Android de destino: **Android 9/API 28 o posterior, ARM64**.
- Herramientas fijadas: Gradle 8.9, Android SDK 35, NDK 27.1.12297006, CMake del SDK 3.22.1.
- La compilación completa necesita varios GB de espacio y memoria; tarda más que volver a empaquetar bibliotecas ya compiladas.

## Compilar desde cero

Mantener el motor y este repositorio como carpetas hermanas:

```text
carpeta-de-trabajo/
  gbarecomp/
  sma3-recomp/
```

Desde `carpeta-de-trabajo`:

```sh
git clone https://github.com/mstan/gbarecomp.git
git -C gbarecomp checkout e7728148c6829ba526f682876430a0c9022dc6c0
git -C gbarecomp submodule update --init external/arm-recomp-core external/rbengine external/recomp-net platform/android/third_party/SDL
cmake -S gbarecomp -B gbarecomp/build -DCMAKE_BUILD_TYPE=Release
cmake --build gbarecomp/build --target gba_recompile -j2
git clone https://github.com/KarisaAdvynia/sma3-disasm.git
git -C sma3-disasm checkout c8532ec9a8d0038c3bfeb003dd0c7ea89d7e1071
cd sma3-recomp
python3 tools/generate_sources.py --rom /ruta/sma3-usa.gba --bios /ruta/gba_bios.bin --disasm ../sma3-disasm
python3 tools/bootstrap_android_build.py
```

El último script descarga las herramientas oficiales en `../build-tools`, prepara una clave **de desarrollo local** si no existe y ejecuta Gradle. No subas esa clave a GitHub. La primera configuración acepta las licencias del SDK mediante `sdkmanager`; revisa esas licencias antes de ejecutar el script si corresponde a tu entorno.

APK resultante: `android/app/build/outputs/apk/debug/app-debug.apk`.

Para compilar únicamente las bibliotecas nativas: `python3 tools/bootstrap_android_build.py --native-only`.

Para el APK 0.9.1 se compilaron desde cero las bibliotecas nativas ARM64 y se
empaquetó la aplicación con las fuentes públicas actualizadas. Se reutilizaron
las fuentes generadas privadas del proyecto de desarrollo; **no se regeneró el
conjunto de fuentes a partir de la ROM y la BIOS en esta ejecución**. Los scripts
de regeneración se habían revisado estáticamente y ese flujo completo de
regeneración sigue sin repetirse como parte de esta compilación.
[Detalle de la compilación y verificación](validation/ANDROID_0.9.1_VALIDATION.md).

## Archivos compatibles

| Archivo local | Tamaño | SHA-1 |
|---|---:|---|
| ROM USA | 4.194.304 bytes | `7352d2bd064d9ebaec579e264228aa21c7345b80` |
| BIOS GBA | 16.384 bytes | `300c20df6731a33952ded8c436f7f186d25d3492` |

La app solicita ambos archivos con **CHOOSE ROM** y **CHOOSE BIOS**; después se pulsa **PLAY**.

Una compilación realizada por otra persona tendrá otra firma. No podrá actualizar directamente los APK del desarrollador sin disponer de su misma clave. Conserva tu clave local para tus propias actualizaciones.

## Diagnóstico en Linux

```sh
python3 tools/rebuild_host.py --jobs 2
python3 tools/run_verified.py --rom /ruta/sma3-usa.gba --bios /ruta/gba_bios.bin --frames 18000 --timeout 240 --input validation/extended-gameplay.csv --save /tmp/sma3-prueba-nueva.sav --output /tmp/sma3-prueba.png
```

Usa una ruta de guardado nueva para repetir las condiciones iniciales. SDL2 es necesario para ventana y audio en Linux; sin él se permite el diagnóstico sin ventana. No se deben convertir los fotogramas simulados por esta prueba en una afirmación de FPS en Android.

## Pendientes

- Registrar versión de Android, APK exacto y duración de la prueba anterior de 60 FPS.
- Completar pruebas específicas de 0.9.1 en Pixel 7 Pro: transformaciones, mando, escalado, idiomas, editor de controles y navegación del menú.
- Registrar versión de Android, duración, ajustes utilizados y mediciones de rendimiento de la prueba de 0.9.1.
- Completar la traducción de rótulos gráficos, créditos y Mario Bros.
- Ampliar cobertura de niveles y transiciones.
- Documentar sonido, guardado, segundo plano y mandos físicos.
- Medir estabilidad de FPS en sesiones largas y en más dispositivos.

## Créditos y condiciones

Proyecto preparado para Edwin Rodríguez. Las dependencias conservan sus autores y licencias. GBARecomp usa PolyForm Noncommercial 1.0.0; arm-recomp-core usa MIT. Consulta [THIRD_PARTY.md](THIRD_PARTY.md) y `licenses/`. Las licencias del motor no conceden derechos sobre el juego ni sobre la BIOS. No existe afiliación con Nintendo.

Para subir este contenido: [docs/SUBIR_A_GITHUB.md](docs/SUBIR_A_GITHUB.md).



