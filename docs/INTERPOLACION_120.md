# Modo experimental 120 FPS (v0.4)

Actualización Android ARM64 para instalar sobre v0.3 con la misma firma de desarrollo. No se debe desinstalar la versión anterior para actualizar, pues eso puede borrar los datos de la aplicación.

## Activación en Pixel 7 Pro

1. Activar Pantalla fluida en los ajustes del teléfono; desactivar Ahorro de batería durante la prueba.
2. Abrir el juego, pulsar Atrás y activar «120 FPS experimentales (mezcla)» y «Mostrar FPS».
3. «Continuar» vuelve a la partida. Desmarcar la mezcla restaura la presentación original.

El modo está desactivado por defecto y la preferencia se conserva. La aplicación solicita 119,455 Hz; Android decide la frecuencia final de la pantalla. El Pixel 7 Pro admite hasta 120 Hz.

## Qué hace

El juego conserva su reloj de 59,7275 Hz. Se calcula una imagen intermedia como promedio por canal RGB entre el fotograma anterior y el actual, y se presenta en la mitad del periodo. La imagen original se presenta al final del periodo. No se ejecuta otro paso de CPU, PPU o audio para la imagen intermedia. El objetivo es aproximadamente 119,455 presentaciones por segundo, no 120 estados de juego independientes.

Es una mezcla temporal sencilla, sin compensación de movimiento. Puede generar imágenes dobles, estelas y un pequeño aumento de latencia; no garantiza una mejora visual. Se omite la mezcla si el plazo ya pasó, hay una discontinuidad de fotogramas, cambia el tamaño del framebuffer, se detecta un cambio grande de escena o el juego vuelve de una pausa larga. Fast-forward no usa interpolación.

El contador «Salida» procede de las llamadas reales a SDL_RenderPresent por tiempo transcurrido e incluye imágenes mezcladas; no cuenta las llamadas de refresco de Android. No prueba que el compositor muestre cada imagen. «Pantalla» muestra los Hz que informa Android. La frecuencia obtenida y la estabilidad deben medirse en el teléfono.

## Compilación y prueba

El parche patches/gbarecomp-interpolation.patch se aplica de forma idempotente al motor fijado en e7728148c6829ba526f682876430a0c9022dc6c0 al configurar Android con CMake. Incluye el punto de presentación y el plazo intermedio; las funciones generadas del juego permanecen intactas.

validation/interpolation_test.cpp comprueba mezcla por canales, cortes, reinicio, cambio de tamaño, salto/repetición del contador y temporización. Sesenta ciclos originales siguen durando aproximadamente un segundo aunque se usen sesenta plazos intermedios.

La compilación y verificación del APK no equivalen a una prueba en Pixel 7 Pro. Validar movimiento, audio, controles, guardado, segundo plano, navegación Atrás y estabilidad térmica; comparar con la opción desactivada.
