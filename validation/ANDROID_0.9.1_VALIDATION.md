# Android 0.9.1 — compilación y comprobaciones

Fecha: **18 de octubre de 2026**.

El APK actualizado se compiló, se verificó y se entregó al mantenedor.
Edwin confirmó su funcionamiento en un **Google Pixel 7 Pro**. La subida
manual a GitHub Releases sigue pendiente; al redactar este informe, la
[release pública Descarga](https://github.com/Edwin2929/sma3-recomp-android./releases/tag/Descarga)
conserva el APK 0.9.

## Identidad del APK

| Campo | Valor verificado |
| --- | --- |
| Archivo entregado | `sma3-recomp-0.9.1-aspect-arm64.apk` |
| Paquete | `org.sma3.recomp.experimental` |
| `versionName` | `0.9.1-aspect-experimental` |
| `versionCode` | `10` |
| Arquitectura | ARM64, `arm64-v8a` |
| Android mínimo | API 28, Android 9 |
| SDK objetivo | API 35 |
| Tamaño | 31.759.347 bytes |

SHA-256 del APK:

```text
3885b7447455fd16c4af8882de88175d5d1efc361b7e9b618708839772d97d18
```

## Procedencia y compilación

La base funcional es el commit
[`128dc91721f52c5ea46c99009876a09a6af97991`](https://github.com/Edwin2929/sma3-recomp-android./commit/128dc91721f52c5ea46c99009876a09a6af97991),
que integra la [PR #2](https://github.com/Edwin2929/sma3-recomp-android./pull/2).
Para generar este APK se cambiaron únicamente los metadatos de versión de
[`android/app/build.gradle`](../android/app/build.gradle): código `9 → 10`
y nombre `0.9-layouts-experimental → 0.9.1-aspect-experimental`.
Esos valores se ajustaron localmente antes de la compilación; el commit base
identifica el código funcional, y este informe registra esa diferencia de
metadatos.

Se realizó una compilación nativa completa para ARM64, sin reutilizar un
archivo de juego precompilado. El registro `sma3-android-build.log` termina
con `BUILD SUCCESSFUL in 7m 33s` y `37 actionable tasks: 37 executed`.
Contiene advertencias de variables sin uso en el C++ generado.

La información de dependencias de Ninja confirma que la compilación incorpora
el [`video_scaler.h`](../src/video_scaler.h) corregido. La configuración de
CMake mantiene `SMA3_WIDESCREEN_DIAGNOSTICS=OFF`.

## Comprobaciones del paquete

Se revisó el informe de esta compilación, `sma3-apk-verification.json`, y el
registro de Gradle. Esta tabla resume los resultados sin incluir rutas del
entorno de trabajo ni material privado.

| Comprobación | Resultado |
| --- | --- |
| Firma del APK | Válida; firma v2 presente. |
| Certificado | Coincide con el del APK 0.9 publicado. |
| Integridad ZIP | CRC válidos. |
| Alineación | Comprobación de `zipalign` aprobada. |
| Bibliotecas nativas | ARM64; `libSDL2.so`, `libc++_shared.so` y `libmain.so` coinciden con las salidas de esta compilación. |
| Textos DEX | Ayuda actualizada del ajuste presente en español, inglés y portugués. |
| Archivos privados | La ROM, el archivo de BIOS y el almacén de claves no están empaquetados. |

Huella SHA-256 del certificado público de firma:

```text
2390f57b35d436ff37b2a303189d18d463c8f5114a5ff809b2f2e6df60f58da3
```

El mismo identificador de aplicación, el certificado coincidente y el código
de versión superior permiten preparar la instalación como actualización de
0.9. No se ha confirmado si la prueba del mantenedor se hizo instalando sobre
esa versión ni si comprobó la conservación de los datos.

## Prueba comunicada por el mantenedor

El **18 de octubre de 2026**, Edwin informó:

> El APK lo probé en el Google Pixel 7 Pro y funciona

Se registra como **confirmación de funcionamiento comunicada por el usuario**.
No se proporcionaron la versión de Android, la duración de la prueba, las
escenas recorridas, mediciones de FPS ni capturas. Tampoco se comunicó una
comprobación específica de todas las calidades de imagen, las dos posiciones
horizontales, la persistencia del ajuste o las dimensiones de la imagen.

El campo `physical_device_tested: false` del informe de empaquetado describe
el estado anterior a esta confirmación: durante la compilación no se había
realizado una prueba física. La confirmación posterior del mantenedor se
añade aquí, con el alcance indicado. No acredita 120 FPS estables, una prueba
exhaustiva ni el funcionamiento en otros modelos, incluido el POCO X7 Pro
de la [incidencia #1](https://github.com/Edwin2929/sma3-recomp-android./issues/1).

## Alcance del cambio de imagen

El APK incluye la corrección de redondeo del escalador y la explicación
actualizada del menú. **Atrás → Imagen → desactivar «Estirar a pantalla
completa»** conserva el fotograma original de **240 × 160, proporción 3:2**,
centrado y con bandas cuando correspondan. Activar la casilla llena el área
de juego con el mismo fotograma y puede deformarlo; no muestra más escenario.

Las pruebas previas del escalador aprobaron **45 casos de geometría y
110 comprobaciones de renderizado SDL**. Su entorno, resultados y límites
se conservan en [VIDEO_ASPECT_RATIO.md](VIDEO_ASPECT_RATIO.md) y
[video-scaler-test.txt](video-scaler-test.txt). Son comprobaciones de escritorio;
la confirmación del Pixel 7 Pro no amplía automáticamente su cobertura a
todas las combinaciones de ajustes del teléfono.

**La panorámica real que amplía el campo de visión sigue siendo un prototipo
de escritorio y no está integrada en este APK.** Su estado se detalla en
[PANORAMICA_EN_DESARROLLO.md](../docs/PANORAMICA_EN_DESARROLLO.md).

## Estado de entrega

- APK compilado y entregado al mantenedor: completado.
- Funcionamiento en Pixel 7 Pro comunicado por Edwin: registrado con el
  alcance anterior.
- Publicación manual del APK 0.9.1 en GitHub Releases: pendiente.

Este informe no acredita una publicación del binario ni modifica el estado
histórico del APK 0.9 ya publicado.
