# Validación y límites

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
