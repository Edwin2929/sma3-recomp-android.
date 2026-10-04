# Validación y límites

## Reporte del usuario

El 3 de octubre de 2026, Edwin informó: «Ya se probó en android y funcionó a 60 fps».

Este dato se registra como una prueba reportada por el usuario. No se aportaron el modelo, la versión de Android, la versión concreta del APK, la duración ni un registro de rendimiento. No se han inventado esos datos.

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
