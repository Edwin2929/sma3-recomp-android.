# SMA3 Recomp para Android

Proyecto experimental de recompilación estática de **Yoshi’s Island: Super Mario Advance 3 (USA)** para Android ARM64, basado en [GBARecomp](https://github.com/mstan/gbarecomp) y en el análisis de [sma3-disasm](https://github.com/KarisaAdvynia/sma3-disasm).

## Versión actual: 0.7 experimental

- **0.5:** cobertura nativa de la rutina de transformación, mapeo de mando y opción para ocultar los controles táctiles. [Detalles](docs/CORRECCION_TRANSFORMACIONES.md).
- **0.6:** escalado de presentación Automático, 720p y 1080p, con opción para estirar la imagen. Los gráficos originales siguen siendo de 240 × 160. [Detalles](docs/CALIDAD_IMAGEN.md).
- **0.7:** selector de inglés, español latinoamericano y portugués de Brasil. Traducción parcial de 187 entradas por idioma; incluye 61 nombres de niveles. Créditos, rótulos gráficos y Mario Bros siguen en inglés. [Cobertura](docs/IDIOMAS.md).
- Se conservan el contador de FPS y la mezcla temporal opcional de 120 FPS de 0.4. No son 120 estados independientes del juego; puede producir estelas. [Límites](docs/INTERPOLACION_120.md).

Abre las opciones con **Atrás del teléfono**. Los ajustes se conservan entre sesiones.

## Descargar e instalar

**La carga del APK 0.7 en GitHub queda pendiente: Edwin lo subirá manualmente.** Consulta [Releases](https://github.com/Edwin2929/sma3-recomp-android./releases).

Archivo previsto: `sma3-recomp-0.7-es-pt-arm64.apk` (31.731.619 bytes), Android 9 o posterior, ARM64.

SHA-256:
```text
4a31372f8fda47cb56195d0cf8490637246f547634c5ea70feed0c4dbae5cd2d
```

El APK de desarrollo 0.7 conserva la firma de las versiones anteriores: instálalo encima para conservar partidas y archivos importados. Las compilaciones de otras personas tendrán otra firma.

[Notas e instrucciones para publicar el APK manualmente](docs/releases/v0.7-experimental.md).

## Estado de validación

Edwin reportó 60 FPS con una versión anterior en Android y posteriormente identificó su teléfono como Google Pixel 7 Pro. Ese reporte **no valida el rendimiento de 0.5–0.7 ni confirma 120 FPS**.

Las pruebas de desarrollo cubren la rutina de transformación, el escalador y los punteros de traducción. Se ejecutaron 7.200 fotogramas nativos por idioma sin faltas de despacho ni instrucciones interpretadas. Falta comprobar estas novedades en el teléfono, todas las transformaciones y el juego completo. [Resultados y límites](docs/VALIDACION.md).

## Qué significa «nativo» aquí

El código de CPU del juego y de la BIOS se traduce a C++ y se compila para el dispositivo. El motor sigue reproduciendo el funcionamiento del hardware de GBA. No es una reescritura completa del juego independiente de ese hardware.

El inicio activa `GBARECOMP_STRICT_STATIC=1`, `GBARECOMP_FORCE_INTERP=0`, `GBARECOMP_BIOS_HLE=0` y `GBARECOMP_BIOS_SKIP_INTRO=0`. Una rutina no cubierta detiene la ejecución; no se utiliza el intérprete para ocultar fallos de cobertura.

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

Los scripts de regeneración y esta organización pública se revisaron estáticamente; **no se repitió una compilación completa desde este ZIP público**. La compilación y pruebas anteriores corresponden al proyecto de desarrollo del que se extrajeron estos archivos.

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
- Probar 0.7 en Pixel 7 Pro: transformaciones, mando, escalado y ambos idiomas.
- Completar la traducción de rótulos gráficos, créditos y Mario Bros.
- Ampliar cobertura de niveles y transiciones.
- Documentar sonido, guardado, segundo plano y mandos físicos.
- Medir estabilidad de FPS en sesiones largas y en más dispositivos.

## Créditos y condiciones

Proyecto preparado para Edwin Rodríguez. Las dependencias conservan sus autores y licencias. GBARecomp usa PolyForm Noncommercial 1.0.0; arm-recomp-core usa MIT. Consulta [THIRD_PARTY.md](THIRD_PARTY.md) y `licenses/`. Las licencias del motor no conceden derechos sobre el juego ni sobre la BIOS. No existe afiliación con Nintendo.

Para subir este contenido: [docs/SUBIR_A_GITHUB.md](docs/SUBIR_A_GITHUB.md).

