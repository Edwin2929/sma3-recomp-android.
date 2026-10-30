# Créditos: rutina Thumb dinámica — 0.10.11 beta

## Fallo reproducido

Al ejecutar una escena controlada posterior al jefe final, la versión
candidata sin esta corrección alcanzó `CreditsInit`, estado `18`, y terminó
con un fallo de despacho estático para **`03002B64` en modo Thumb**.
No faltaba el estado de juego ni un recurso de imagen: faltaba la selección
del código recompilado para una rutina copiada a una asignación dinámica.

La fuente primaria es
[`asm/Credits.asm`](https://github.com/KarisaAdvynia/sma3-disasm/blob/c8532ec9a8d0038c3bfeb003dd0c7ea89d7e1071/asm/Credits.asm)
del desensamblado fijado. `CreditsInit`, en `0803430E..08034426`, calcula el
tamaño entre `Sub08033A2C` y `Sub08033BA8`, reserva **`17C` bytes**, copia la
rutina y publica `base | 1` en **`03006390`**. La dirección observada en esta
ejecución fue `03002B64`; no se presupone que todas las asignaciones usen
esa misma dirección.

## Corrección

[`tools/generate_credits.py`](../tools/generate_credits.py) comprueba la ROM
USA fijada y genera dos fragmentos privados con reanudación estática. Reubica
las direcciones de instrucción y las lecturas relativas a PC, manteniendo
canónicas las etiquetas internas de reanudación.

[`src/credits_dispatch.cpp`](../src/credits_dispatch.cpp) exige modo Thumb,
PC alineado, puntero publicado válido, asignación dentro del área permitida
y coincidencia de **todos los `17C` bytes**, incluidos los literales.
Después busca la entrada estática para la dirección relativa observada.
Al regresar restaura el desplazamiento de la llamada exterior.

La ruta se integra en `ram_dispatch.cpp`. El flujo completo de generación
incluye la rutina y CMake informa explícitamente si falta. Las fuentes
generadas y los datos del juego permanecen fuera del repositorio.

## Comparación nativa con la referencia

[`credits_test.cpp`](credits_test.cpp) pasó **96 escenarios y 2.378
comprobaciones**. Para cada escenario se comparan los 16 registros, CPSR,
los 32 KiB de IWRAM, los 256 KiB de EWRAM y los ciclos con la ejecución
Thumb de referencia del arnés de prueba.

La matriz cubre:

- Tres direcciones de asignación: `03002B64`, `03004D50` y `03005400`.
- Cuatro paquetes: dibujo normal, recorte en el borde, escalado mayor y
  objeto completamente fuera de pantalla.
- Retorno a ARM y a Thumb.
- Ejecución continua y tres interrupciones/reanudaciones en instrucciones
  interiores (`A`, `2E` y `164` respecto de la base).

Se comprueba que los paquetes visibles escriben píxeles y el paquete fuera
de pantalla no lo hace. Los casos negativos rechazan modo ARM, PC impar,
PC fuera de la asignación, alteración del último literal, puntero sin bit
Thumb, puntero desactualizado, asignación desbordada o desalineada y una
asignación liberada.

El intérprete solo es la referencia del ejecutable de prueba. **El APK
continúa con ejecución estática estricta**, sin recurrir a un intérprete
para evitar el fallo.

Reproducción, después de generar los demás insumos privados y configurar
una compilación host:

```sh
python3 tools/generate_credits.py --rom /ruta/sma3-usa.gba \
  --recompiler ../gbarecomp/build/gba_recompile
cmake -S . -B build-host
cmake --build build-host --target sma3_credits_test -j2
build-host/sma3_credits_test /ruta/sma3-usa.gba
```

## Ejecución de la ruta final

El candidato host final tiene SHA-256
`8288c3080c6e9ec498cfa392d060642dd9dd2f618a77338cd7adcc3e0f88323d`.
La misma escena controlada ejecutó **3.730 observaciones nativas**:

| Momento | Fotograma del juego |
| --- | --- |
| Entrada a créditos, estado `18` | 17.972 |
| Estado `19` | 18.000 |
| Estado `1A` | 18.060 |
| Continuación desde el primer estado `19` | 200 fotogramas |

La ejecución terminó normalmente, con **cero fallos de despacho, cero
instrucciones interpretadas y cero reparaciones dinámicas**. La prueba
descubrió también la ruta `17 → 18`, que ahora reconoce el detector del
logro cuando existe evidencia de las 48 metas y de continuidad.

El estado de prueba parte de una victoria preparada. No acredita un combate
natural contra Bowser, una campaña completa ni todos los fotogramas de los
créditos. El APK incluye tanto el nuevo despachador como los dos fragmentos;
la verificación de empaquetado comprueba su presencia.

[Informe nativo y hashes](yo-shi-pude-native-report.json) ·
[Detección de «Yo shi pude»](YO_SHI_PUDE_NATIVE.md) ·
[Compilación y firma](android-0.10.11-build.json).
