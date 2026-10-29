# Selector de color de Yoshi — validación de la interfaz 0.10.8

## Resultado

El modelo de edición y guardado pasó **5.014 comprobaciones**. La interfaz
Android real pasó **4 pruebas Robolectric** con Android API 34 y gráficos
nativos. Las 29 fuentes Java de la aplicación y SDL también compilaron con
el SDK Android 35 y Temurin JDK 17.0.21+9.

Estas son pruebas de host. No equivalen a una prueba táctil en un Pixel 7 Pro,
ni ejecutan el juego ARM64. Las capturas se generan con las vistas Android de
producción; no son un diseño web ni imágenes simuladas mediante un generador.

## Comportamiento comprobado

La implementación está en `YoshiColorUi.java`, `YoshiColorModel.java` y el
panel compartido `SettingsPanel.java`. Se abre desde la tarjeta incorporada
**Mods → Color de Yoshi**, separada del catálogo de perfiles `.gbamod`.

- 48 colores distintos, incluidos blanco y negro; el borrador inicial usa
  `#39E739` y el mod comienza desactivado.
- Códigos de seis o tres dígitos, con o sin `#`, mayúsculas o minúsculas.
  La forma corta repite cada dígito: `#A7F` produce `#AA77FF`.
- Rechazo de códigos incompletos, alfa de ocho dígitos, símbolos ajenos a HEX,
  dígitos Unicode parecidos a los ASCII y formatos `rgb(...)`/`0x...`.
- Edición RGB por canal sin alterar los otros componentes.
- Vista previa de sombra/base/luz con la misma conversión RGB555 y las mismas
  fórmulas enteras que el código nativo. El código guardado conserva sus
  canales originales de ocho bits.
- Ninguna edición o cambio de tamaño publica un color ni escribe preferencias.
  **Aplicar color** guarda y activa; **Cancelar** y Back descartan el borrador.
- **Usar color original** desactiva el mod y conserva el último color guardado,
  descartando el borrador que estuviera abierto.
- Reapertura y aplicación de preferencias al iniciar usan el valor guardado,
  incluso si otro editor mantiene un borrador distinto.
- Un código inválido deshabilita **Aplicar color** y **Copiar**.
- Un helper cerrado o una biblioteca nativa no disponible no producen una
  confirmación de guardado.
- Preferencias con un tipo o valor inválido vuelven a una selección desactivada
  segura, sin provocar una excepción de lectura.

El modelo de guardado se ejercita además con fallos controlados de escritura,
excepciones de almacenamiento y errores de restauración. Si `commit()` falla,
se intenta restaurar tanto el estado nativo como las preferencias: Android
puede haber actualizado estas últimas en memoria aunque falle la escritura
en disco. Un fallo de restauración muestra un diagnóstico distinto y no se
presenta como un guardado correcto.

## Disposición y capturas

`YoshiColorUiTest.java` mide y dibuja los mismos componentes de producción.
No usa sustitutos del selector ni del panel. El acceso JNI se sustituye por
un contador controlado para comprobar exactamente cuándo se publicaría un
color, sin cargar una biblioteca ARM64 en el host.

| Restricción ofrecida al panel | Comprobación |
| --- | --- |
| 360 × 760 dp | Una columna; 48 muestras con objetivos táctiles de al menos 48 dp. |
| 600 × 328 dp | Dos columnas: HEX/vista previa y paleta/RGB; botones fijos visibles. |
| 264 × 560 dp | La cuadrícula reduce columnas y conserva el tamaño mínimo de toque. |
| 360 × 240 dp | El panel respeta la altura disponible y mantiene Aplicar/Cancelar fuera del desplazamiento. |

Los cambios de anchura se aplican al mismo árbol de vistas para comprobar su
redistribución. La altura reducida representa el espacio que podría ofrecer
un teclado o una ventana dividida; la prueba confirma también
`SOFT_INPUT_ADJUST_RESIZE`. No se ejecuta un teclado físico ni se certifica el
tratamiento de insets de todos los fabricantes.

Se revisaron visualmente las cuatro capturas generadas: vertical, horizontal,
controles RGB y altura reducida. Los rótulos, códigos y botones permanecen
legibles. Los colores adicionales y las explicaciones se alcanzan desplazando
el cuerpo del cuadro. Los botones Aplicar/Cancelar permanecen fijos.

Las cadenas de usuario están en español, inglés y portugués. Las capturas y
las interacciones Robolectric de esta ejecución usan español.

## Reproducir

El modelo no requiere Android:

```sh
javac -d /tmp/sma3-yoshi-model android/java/org/gbarecomp/YoshiColorModel.java validation/YoshiColorModelTest.java
java -cp /tmp/sma3-yoshi-model org.gbarecomp.YoshiColorModelTest
```

Para las vistas reales, con JDK 17, Gradle 8.9 y el SDK 35 disponibles:

```sh
python3 validation/run_yoshi_color_ui_test.py \
  --java-home /ruta/al/jdk17 \
  --android-jar /ruta/al/android-sdk/platforms/android-35/android.jar \
  --gradle /ruta/a/gradle-8.9/bin/gradle \
  --work-dir /tmp/sma3-yoshi-ui
```

La primera ejecución descarga Robolectric 4.16.1, JUnit 4.13.2 y el framework
Android de prueba desde Maven Central/Google. Las dependencias, clases,
capturas y registros permanecen en `--work-dir`; no se incluyen en el APK.
La salida esperada del modelo es `PASS YoshiColorModel checks=5014` y la de
Robolectric, `OK (4 tests)`.

## Límites de esta evidencia

Esta validación cubre el editor, las preferencias y el límite Java/JNI. Las
pruebas nativas y de partida verifican por separado qué sprites cambian y
qué estados del juego permanecen intactos. Quedan para el teléfono real el
teclado concreto, TalkBack, insets del fabricante y la experiencia táctil en
ambas orientaciones.
