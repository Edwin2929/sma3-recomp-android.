# Auditoría nativa del catálogo de 25 logros — 0.10.10

Esta auditoría revisa el detector y el contrato publicados en
`2c5dbc94a707460da3ae1aac1b93cb6d2fcdaa32` (0.10.9). La ampliación añade
quince criterios Android derivados de la evidencia existente. No requiere
cambiar direcciones de memoria, lógica nativa, JNI ni formato del snapshot.
La auditoría es de código y fuentes: **no constituye una nueva ejecución del
juego ni una prueba física del APK 0.10.10**.

## Identidad de las cinco transformaciones

Fuente primaria revisada localmente: [sma3-disasm, revisión
c8532ec9a8d0038c3bfeb003dd0c7ea89d7e1071](https://github.com/KarisaAdvynia/sma3-disasm/tree/c8532ec9a8d0038c3bfeb003dd0c7ea89d7e1071).

En `asm/080352BC.asm`, `0803DF7E` lee la transformación en
`03006D80 + 0x32 = 03006DB2`. `0803DF80` divide el ID entre dos; las
instrucciones siguientes seleccionan la función en `CodePtrs0816DDDC`,
declarada en `asm/Data.asm`. Los nombres se corroboran con las llamadas
de sonido identificadas en las rutinas correspondientes, no con el orden
visual de las burbujas.

| Nombre | ID del juego | Bit / máscara del snapshot | Función de la tabla | Evidencia nominal en el desensamblado |
| --- | --- | --- | --- | --- |
| Coche | 2 (`0x02`) | 0 / `0x01` | `Sub0803B424` | `0803B46C`: sonido 06, «Car morph» |
| Tanque topo | 4 (`0x04`) | 1 / `0x02` | `Sub0803B998` | Llama `Sub0803B934` en `0803BBE6`; `0803B97C`: sonido 04, «Mole tank morph» |
| Helicóptero | 6 (`0x06`) | 2 / `0x04` | `Sub0803BD50` | Llama `Sub0803BCEC` en `0803C01E`; `0803BD34`: sonido 03, «Helicopter morph» |
| Tren | 8 (`0x08`) | 3 / `0x08` | `Sub0803C3F8` | `0803C4F2`/`0803C4F6`: sonido 05, «Train morph» |
| Submarino | 12 (`0x0C`) | 4 / `0x10` | `Sub0803CD08` | `0803CD28`/`0803CD2C`: sonido 07, «Submarine morph» |

Los valores de sonido son otro identificador: no deben usarse como ID de
transformación ni como posición del bit. `vehicle_bit()` en
`src/local_achievements_detail.h` publica exactamente esas cinco máscaras,
con máscara permitida `0x1F`. Otros IDs no conceden un vehículo.
El detector exige una transición observada desde Yoshi normal, con continuidad
de escena y ejecución; cargar un estado ya transformado no basta.

Cada logro individual debe comprobar `(transformations & mask) != 0`.
Comparar numéricamente la máscara completa con un umbral podría conceder
vehículos que nunca se observaron.

## Quince criterios nuevos con el mismo snapshot

Los índices siguientes pertenecen al snapshot nativo de ocho enteros;
el modelo Android omite la posición 0 del esquema al almacenar sus métricas.

| Criterio | Nuevos logros | Evidencia | Cálculo |
| --- | --- | --- | --- |
| Tres flores en un nivel | 1 | Posición 5, máximo de flores | Máximo ≥ 3 |
| Cinco y diez monedas rojas en un nivel | 2 | Posición 4, máximo de rojas | Máximo ≥ 5 / 10 |
| Diez, veinte, treinta y cuarenta niveles distintos | 4 | Posición 1, niveles completados | Número de bits ≥ 10 / 20 / 30 / 40 |
| Cinco, diez y veinte niveles distintos con 100 puntos | 3 | Posición 2, niveles perfectos | Número de bits ≥ 5 / 10 / 20 |
| Coche, tanque topo, helicóptero, tren y submarino | 5 | Posición 3, transformaciones | Un bit concreto por vehículo |

Se conservan los diez criterios anteriores: **10 + 15 = 25**.
Flores y rojas son máximos alcanzados dentro de un nivel, no sumas de objetos
de varias partidas. Los niveles usan 60 bits: para ID original
`mundo * 12 + posición`, con mundo 0–5 y posición 0–9, el bit publicado es
`mundo * 10 + posición`. Repetir un nivel no añade otro bit. La máscara de
niveles perfectos es un subconjunto de la de niveles completados.

El catálogo no incorpora «mundo completado», «juego completado» ni un premio
por derrotar específicamente al jefe final. La representación de los IDs por
sí sola no acredita todas las rutas de victoria ni una partida completa.

## Evidencia reutilizada y límites

La cualificación de 0.10.9 registró 7 grupos y 166.657 comprobaciones del
detector, ASan/UBSan y cinco configuraciones de juego del candidato: Fuzzy
a 240 y 356, coche a 356, meta con confirmación de 100 puntos a 240 y
recogida de una flor auténtica a 240. Sus resultados y límites permanecen
en [LOCAL_ACHIEVEMENTS_0.10.9_VALIDATION.md](LOCAL_ACHIEVEMENTS_0.10.9_VALIDATION.md)
y [LOCAL_ACHIEVEMENTS_NATIVE.md](LOCAL_ACHIEVEMENTS_NATIVE.md).

El mapeo de los otros vehículos está sustentado por el código citado.
Esta auditoría no añade replays de helicóptero, topo, tren o submarino, ni
presenta la evidencia histórica como sesiones nuevas. Los nuevos umbrales y
la migración del catálogo deben verificarse en el modelo y el almacenamiento
Android.

## Compatibilidad de progreso y máscaras

El snapshot conserva esquema 1, ocho posiciones y las mismas operaciones
OR/max. La evidencia de 0.10.9 ya contiene los bits y máximos necesarios
para reconocer nuevos criterios cumplidos. Esto reutiliza el registro local
de logros; no importa récords desde archivos de partida del juego.

La migración debe conservar las diez identidades y fechas anteriores,
reconocer únicamente las nuevas identidades ausentes y guardar de forma
atómica el catálogo ampliado. Una fecha nueva derivada de evidencia anterior
indica cuándo la actualización la reconoció; no reconstruye una fecha
histórica que nunca fue registrada. Si la migración cambia fechas sin cambiar
métricas, sigue siendo una escritura pendiente y debe persistirse incluso
cuando no llegue evidencia nativa adicional.

La máscara Java de avisos es independiente de las máscaras nativas de niveles.
Con 25 definiciones usa índices 0–24: el bit mayor es `1 << 24` y la máscara
completa es `0x01FFFFFF` (33.554.431), dentro del rango positivo de `int`.
`Integer.bitCount`, `Integer.numberOfTrailingZeros` y `1 << i` son válidos
para este catálogo. Las máscaras de 60 niveles deben seguir siendo `long`.
