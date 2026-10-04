# SMA3 Recomp para Android

Proyecto experimental de recompilación estática de **Yoshi’s Island: Super Mario Advance 3 (USA)** para Android ARM64, basado en [GBARecomp](https://github.com/mstan/gbarecomp) y en el análisis de [sma3-disasm](https://github.com/KarisaAdvynia/sma3-disasm).

## Estado

- **Prueba del usuario:** Edwin reportó el 3 de octubre de 2026 que el juego funciona en Android a **60 FPS**. Modelo del teléfono, versión de Android, versión exacta del APK y duración de la prueba: pendientes de registrar.
- Prueba automatizada en Linux: recorrido de **18.000 fotogramas** completado con recompilación estática, cero fallos de despacho y cero instrucciones ejecutadas mediante el intérprete.
- APK ARM64 compilado y firmado; versión actual del código: **0.3-back-menu**, `versionCode=3`.
- Contador de FPS opcional. Durante la partida, **Atrás del teléfono → Opciones → Mostrar FPS**. **Continuar** cierra el menú.
- La opción también aparece en la pantalla inicial y se conserva entre sesiones.

La prueba de 60 FPS corresponde al dispositivo del usuario; no constituye una medición en todos los teléfonos ni una validación de todos los niveles. Véase [docs/VALIDACION.md](docs/VALIDACION.md).

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
cd sma3-recomp
python3 tools/generate_sources.py --rom /ruta/sma3-usa.gba --bios /ruta/gba_bios.bin
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

- Registrar el teléfono, Android, APK y duración de la prueba de 60 FPS.
- Ampliar cobertura de niveles y transiciones.
- Documentar sonido, guardado, segundo plano y mandos físicos.
- Medir estabilidad de FPS en sesiones largas y en más dispositivos.

## Créditos y condiciones

Proyecto preparado para Edwin Rodríguez. Las dependencias conservan sus autores y licencias. GBARecomp usa PolyForm Noncommercial 1.0.0; arm-recomp-core usa MIT. Consulta [THIRD_PARTY.md](THIRD_PARTY.md) y `licenses/`. Las licencias del motor no conceden derechos sobre el juego ni sobre la BIOS. No existe afiliación con Nintendo.

Para subir este contenido: [docs/SUBIR_A_GITHUB.md](docs/SUBIR_A_GITHUB.md).
