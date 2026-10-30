# Idiomas — 0.10.15 beta

Abre **Atrás → Idiomas → Idioma del juego** y elige **English (original)**,
**Español latinoamericano** o **Português do Brasil**. El idioma de los menús
Android se elige por separado en **Idioma del menú**. Ambos ajustes se guardan.

Los mensajes nuevos usan el idioma seleccionado. Un mensaje que ya estaba
abierto conserva sus datos hasta terminar. Los rótulos gráficos cambian cuando
el juego vuelve a cargar la pantalla correspondiente; volver a abrir solo el
panel de ajustes de Android no vuelve a dibujar los recursos del juego.

## Ampliación de 0.10.15

Esta entrega añade **20 paquetes de créditos por idioma** y **seis rótulos**
de la interfaz del juego. Las traducciones son propias; no se instala un
parche de terceros ni se modifica el archivo ROM del jugador.

| Zona | Español | Portugués |
| --- | --- | --- |
| Cabecera del menú de archivos | ARCHIVOS | ARQUIVOS |
| Cabecera de copiar | COPIAR | COPIAR |
| Cabecera de borrar | BORRAR | APAGAR |
| Pausa/inventario: mundo | MUNDO | MUNDO |
| Pausa/inventario: mejor puntuación | MEJOR | RECORDE |
| Pausa/inventario: puntuación total | PUNTOS TOTALES | PONTOS TOTAIS |

En los créditos se traducen el encabezado, los cargos, los equipos, los
agradecimientos, el aviso de derechos y la narración final, incluido **FIN / FIM**.
Los nombres propios, SRD, Nintendo y el orden del equipo original se conservan.

La fuente de los créditos admite ahora siete caracteres acentuados necesarios
para esos textos. Se utiliza una copia privada de sus gráficos y anchos para
evitar alterar otros usos de la fuente original. Los textos se centran con
sus anchos reales y respetan la posición vertical y los comandos originales.

## Cobertura acumulada

Hay **207 paquetes de texto por idioma**: los **187 anteriores**, más los
**20 nuevos** de créditos. Los rótulos gráficos se cuentan por separado.
Los textos anteriores incluyen diálogos con jefes, tutoriales, reglas de
minijuegos, nombres de los **61 niveles**, historia inicial, narración final,
preguntas del menú de archivos y respuestas Sí/No.

La ampliación conserva exactamente los bytes y las direcciones virtuales de
los textos anteriores. Esto permite que un estado guardado con un mensaje
abierto siga encontrando el mismo paquete al cargarlo. Cada idioma conserva
su propio banco de datos; cambiar la selección no mezcla dos idiomas dentro
de un paquete ya resuelto.

## Lo que sigue pendiente

La traducción continúa siendo parcial. Permanecen en inglés los demás
rótulos gráficos del título y del juego, los botones ilustrados de los menús,
los textos gráficos de estrellas/monedas/flores, el menú de suspensión y
**Mario Bros.** Algunos comparten fragmentos de gráficos con números u otros
elementos y necesitan una adaptación específica.

Los nombres propios y marcas que se conservan en los créditos no son textos
pendientes de traducir. La revisión de esta entrega tampoco equivale a probar
cada diálogo, cada nivel o cada pantalla en un teléfono.

## Fuentes y generación

Los catálogos editables son:

- `localization/messages.tsv`, `levels.tsv` y `story.tsv`: textos anteriores.
- `localization/credits.tsv`: cargos y mensajes de los créditos.
- `localization/graphics.tsv`: seis rótulos de archivos y pausa/inventario.

`tools/generate_localization.py --rom ROM --disasm DISASM` verifica el SHA-1
de la ROM USA, contrasta los bloques y las instrucciones de carga con el
disassembly y genera `src/localization_data.h`. Ese recurso privado contiene
datos derivados del juego y no se publica en GitHub. Las ampliaciones se
construyen mediante `tools/localization_credits.py` y
`tools/localization_graphics.py`; no se edita el header a mano.

Se redirigen **287 posiciones de punteros de texto**, **cuatro de recursos
gráficos** y **dos de la fuente de créditos**. Las lecturas comprueban la
dirección, el ancho y el valor original. Los recursos resueltos siguen siendo
legibles al cambiar de idioma y en los espejos de memoria ROM.

El juego conserva su renderer y descompresor originales. Los comandos de
créditos, los espacios numéricos del inventario, las paletas, los bordes y
las zonas ajenas a las letras mantienen sus datos correspondientes.

## Validación

El [informe de 0.10.15](../validation/LOCALIZATION_0.10.15_VALIDATION.md)
separa comprobaciones de recursos, compatibilidad de textos anteriores,
lecturas del runtime, ejecución nativa y paquete Android. Los datos de
cobertura están en [localization/coverage.json](../localization/coverage.json).

Las comprobaciones históricas de 0.7 —187 bloques, 267 punteros y 7.200
fotogramas por idioma— se conservan en sus
[notas originales](releases/v0.7-experimental.md). Esos resultados no se
presentan como nuevas pruebas de las pantallas añadidas en 0.10.15.
