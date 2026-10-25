# Integración Android — 0.10.5 beta

## Resultado

**PASS** de compilación y verificación del APK `sma3-recomp-0.10.5-beta-arm64.apk`,
código 16, tamaño **27.899.160 bytes** y SHA-256:

```text
7e6bf645377be5c592c890defacd574d5fc09e7a162a1a444399621ca9ab9c87
```

Los [datos de construcción](android-0.10.5-build.json) registran el APK,
las fuentes exactas, los 12 parches, el certificado, las bibliotecas y las
entradas del contenedor. La producción se comprobó antes y después del
ensamblado. Los documentos e informes se generan después de esa congelación
y no se confunden con entradas compiladas.

## Contratos integrados

| Área | Comprobación |
| --- | --- |
| Superficie y frecuencia | Los callbacks reales de SDL se contrastaron con Activity: invalidación al recrear Surface, solicitud al reanudar, retirada al pausar y mezcla solo con tasa compatible. |
| Temporización JNI | `resetPresentationTiming()`, descriptor `()V`, incrementa la época nativa en reanudación y creación de superficie. No reinicia la medida desde cada muestra FPS. |
| Apariencia JNI | `setTouchAppearance(int,int)`, descriptor `(II)V`, transmite estilo y opacidad mediante una instantánea atómica; slider y editor comparten preferencias. |
| Controles | Las zonas táctiles, bits de entrada y diseños de 28 valores permanecen iguales. La opacidad se limita a 20–100 y se guarda al mover la barra. |
| Actualizaciones | Entrada en juego y setup, observadores retirados al destruirse, resultados de permiso diferenciados del selector de mods y de ROM/BIOS. |
| Instalación | Retorno de permisos diferido hasta primer plano y ligado a la página que recibió el toque. Una Activity recreada requiere otra acción del usuario. |
| Descarga | Tamaño/hash e identidad se validan antes de publicar el archivo preparado y se revalidan antes de instalar. Proveedor no exportado, URI exacta y permiso temporal de lectura. |
| Compatibilidad | Mismo paquete/certificado, versión 15→16, mínimo API28, destino35, ARM64; sin cambios al formato de partidas, mods o diseños. |

Todos los archivos Java de la aplicación se compilaron conjuntamente con
JDK17/API35. Gradle recompiló las clases finales, incluidas las correcciones
posteriores de permisos y finalización de operaciones. Se verificaron en
el DEX las clases, preferencias, etiquetas y firmas nuevas, y en `libmain.so`
los símbolos JNI correspondientes.

El manifiesto final incluye INTERNET y REQUEST_INSTALL_PACKAGES. Su proveedor
`org.sma3.recomp.experimental.updates` tiene `exported=false` y
`grantUriPermissions=true`. La firma APK v2 y la alineación pasan; las
bibliotecas empaquetadas coinciden con las salidas nativas de esta compilación.

## Preservación y límites

Los hashes de las fuentes de Fuzzy, reconstrucción panorámica, PPU y mezcla
temporal coinciden con 0.10.4 en las rutas enumeradas por el informe de
construcción. `runtime.cpp` sí cambia en la ruta de presentación, como
describe el parche 11. No se presenta como inalterado todo el motor.
El ejemplo de mod sigue coincidiendo con sus fuentes editables; el APK no
contiene archivos de ROM, BIOS, partidas ni clave de firma.

Las revisiones de integración y actualizador se realizaron por separado
del desarrollo de cada componente. Las pruebas nativas y de Java host
están descritas en los informes de
[rendimiento](PERFORMANCE_0.10.5_VALIDATION.md),
[controles](CONTROLS_0.10.5_VALIDATION.md) y
[actualizaciones](UPDATES_0.10.5_VALIDATION.md).

**No se ejecutó físicamente este APK en Android.** La compilación y las
pruebas con dobles controlados no verifican el aspecto final de Canvas,
el compositor/GPU del Pixel, la gestión térmica, las ventanas de permisos
reales ni la instalación de una nueva versión en el teléfono. Tampoco se
atribuye una nueva campaña de juego completo a esta entrega.

Para la comprobación física, sigue
[rendimiento](../docs/RENDIMIENTO_Y_FPS.md#comprobación-en-el-teléfono)
y [actualizaciones](../docs/ACTUALIZACIONES.md). Conserva el APK exacto,
la versión de Android y las condiciones de cada medición.
