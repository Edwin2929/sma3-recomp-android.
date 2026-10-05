# Idiomas — 0.7 experimental 

Selector en Atrás → Idioma: English, Español latinoamericano y Português do
Brasil. Los ajustes se guardan. Se aplican al abrir el siguiente mensaje o
reabrir una pantalla; un mensaje abierto conserva su idioma hasta terminar.

## Cobertura actual

187 entradas por idioma: diálogos con jefes, tutoriales, reglas de minijuegos,
nombres de los 61 niveles, historia inicial, texto final y preguntas del menú
de archivos. Incluye respuestas Sí/No y cuatro glifos nuevos para ã õ Ã Õ.
Los créditos, rótulos gráficos del título/pausa y Mario Bros conservan inglés.
Es una traducción propia y experimental; no usa un parche de terceros.

Las traducciones están en localization/*.tsv. La herramienta
`tools/generate_localization.py --rom ROM --disasm DISASM` verifica el SHA1 USA,
comprueba cada bloque original y genera el recurso privado
`src/localization_data.h`. Disassembly: KarisaAdvynia/sma3-disasm.

No modifica código ejecutable ni archivos del jugador. Redirige únicamente
267 posiciones verificadas de punteros ROM a paquetes de texto de solo lectura.
Cada idioma tiene un banco separado para evitar mezclar idiomas dentro de un
mensaje. Refluye las líneas según el ancho real de la fuente.

## Validación

- Comprobación exacta de la ROM y los 187 bloques originales: aprobada.
- 267 punteros x 3 idiomas, memoria reflejada, límites, tamaños y selección:
  `validation/localization-test.txt`.
- Android ARM64 compilado y APK firmado; misma firma que 0.6.
- 7200 fotogramas por idioma: 0 faltas de despacho, 0 instrucciones interpretadas.
- Verificación visual: historia ES/PT y tutorial PT, sin desbordamiento observado.
- No se ha revisado visualmente cada diálogo ni probado cada opción de respuesta.
- No se ha probado en un teléfono físico.
