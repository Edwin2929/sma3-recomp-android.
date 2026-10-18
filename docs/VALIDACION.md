# Validación y límites

## 0.9.1: escalado y prueba reportada en Pixel 7 Pro

El 18 de octubre de 2026 se compiló y verificó
`sma3-recomp-0.9.1-aspect-arm64.apk`, versión `0.9.1-aspect-experimental`,
código 10. Incluye la corrección de redondeo del escalador y la ayuda de
imagen en ES/EN/PT de la PR #2. Las bibliotecas nativas se compilaron para
ARM64 desde las fuentes generadas privadas ya disponibles, sin reutilizar
un archivo nativo precompilado. No se regeneró el corpus desde ROM/BIOS
durante esta compilación.

La firma v2 coincide con la del APK 0.9 publicado. Se verificaron la integridad
y alineación del ZIP, la arquitectura ELF64 AArch64, las bibliotecas
empaquetadas y las tres traducciones de la ayuda. El paquete contiene la
configuración de ejecución y no incorpora ROM, BIOS ni almacenes de claves
de firma.

Ese mismo día, Edwin informó que probó el APK en un **Google Pixel 7 Pro**
y que **funciona**. Se registra como confirmación del usuario. No se
documentaron la versión de Android, la duración, las escenas, el tipo de
instalación ni mediciones de FPS. Esta confirmación no acredita por sí sola
todos los ajustes, rotaciones, estabilidad prolongada o compatibilidad con
otros dispositivos. La panorámica real sigue desactivada en Android.

Los 45 casos de geometría y 110 renderizados del escalador corresponden a
las pruebas de escritorio registradas durante la revisión de la incidencia
#1. No son mediciones del teléfono.

[Verificación y huellas del APK 0.9.1](../validation/ANDROID_0.9.1_VALIDATION.md) ·
[Pruebas del escalador](../validation/VIDEO_ASPECT_RATIO.md) ·
[Notas de versión](releases/v0.9.1-experimental.md).

Los apartados siguientes conservan los resultados y límites de las
versiones anteriores.

## Reporte del usuario

El 3 de octubre de 2026, Edwin informó: «Ya se probó en android y funcionó a 60 fps».

Este dato se registra como una prueba reportada por el usuario. El usuario identificó después su teléfono como Google Pixel 7 Pro. No se registraron la versión de Android, el APK concreto, la duración ni una captura de rendimiento de esa prueba.

Antes de este reporte, el usuario también observó que el botón flotante de opciones se superponía al botón R. La versión 0.3 elimina ese botón y usa Atrás para abrir las opciones.

## Pruebas del desarrollo

- Recorrido nativo de Linux de 18.000 fotogramas, con cero dispatch misses y cero instrucciones interpretadas.
- Se corrigió una rutina copiada de ROM 0x08033224 a RAM 0x03004054, de 0x3A4 bytes; una escena distinta la coloca en 0x03004110.
- Se admiten nueve variantes de rutinas HBlank en RAM 0x03007040. La selección verifica los bytes antes de ejecutar su versión compilada.
- Compilación Android: 64 bloques del juego, dos tablas, BIOS y motor enlazados para ARM64.
- APK 0.3: firma v2 verificada, versión 3, estructura ZIP y manejadores de Atrás comprobados.
- El registro extendido de Linux contó 448 accesos de memoria sin mapear; su causa no se evaluó en esa corrección.

## Contador

Cuenta presentaciones del motor SDL por tiempo transcurrido; se actualiza aproximadamente cada 500 ms. No cuenta llamadas de refresco de la interfaz Android ni fija artificialmente el resultado en 60. La cifra puede variar entre muestras.

## Por confirmar

Cobertura del juego completo, persistencia de partidas, audio, pausa/reanudación, estabilidad térmica y rendimiento en otros teléfonos. La organización pública de este repositorio no recibió una compilación completa desde cero durante su preparación.


## Actualización 0.5–0.7

- Transformaciones: 18 casos diferenciales, tres direcciones RAM, distintas entradas y reanudación; comparación de registros y memoria con el intérprete de referencia. El juego sigue ejecutándose en modo nativo estricto. Resultado: `validation/morph-test.txt`.
- Vídeo: 10 escenarios con renderizador SDL software; tamaños de buffers, proporción, estiramiento, colores interiores, restauración de destino y cambios de calidad. Resultado: `validation/video-scaler-test.txt`.
- Idiomas: 267 posiciones de punteros por tres idiomas, bancos inmutables, espejos de ROM y comprobaciones de valor/ancho. Resultado: `validation/localization-test.txt`.
- Ejecución nativa: 7.200 fotogramas por idioma ES/PT, sin faltas de despacho ni instrucciones interpretadas. Inspección visual de historia ES/PT y tutorial PT, sin desbordamiento observado en esas escenas.
- APK 0.7 ARM64 compilado y firmado con el mismo certificado del desarrollo anterior; código de versión 7, nombre `0.7-languages-experimental`.

Estas pruebas no sustituyen jugar todas las transformaciones, revisar cada diálogo o probar un mando físico. No se midieron 0.5–0.7 en Android ni 120 FPS en Pixel 7 Pro. No se repitió una compilación limpia completa de esta distribución pública.


## 0.9: arranque y editor

Edwin confirmó 120 FPS con 0.8 en su Pixel 7 Pro el 5 de octubre de 2026.
Para 0.9: arranque sin animación de BIOS en Linux, 7200 fotogramas nativos con
cero faltas de despacho e instrucciones interpretadas; tutorial revisado en
captura. Pruebas de geometría táctil, datos inválidos, límites y aplicación de
parches aprobadas. APK ARM64 firmado con el certificado anterior. Los submenús,
perfiles y gestos del editor aún necesitan prueba en teléfono físico.
