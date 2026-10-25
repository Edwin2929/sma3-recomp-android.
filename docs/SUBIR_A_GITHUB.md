# Subir a tu GitHub

## Repositorio actual y APK nuevos

El proyecto ya está publicado en
[Edwin2929/sma3-recomp-android.](https://github.com/Edwin2929/sma3-recomp-android.).
El código y los documentos se actualizan en `main`; el mantenedor adjunta
los APK manualmente a la release **Descarga**. Desde 0.10.5, el actualizador
de la aplicación examina los archivos de esa misma release.

Para publicar otra versión compatible, consulta
[Actualizaciones: publicar una futura actualización](ACTUALIZACIONES.md#publicar-una-futura-actualización).
Conserva el nombre versionado, aumenta el código de versión y usa la misma
clave de firma. Subir código al repositorio no publica automáticamente el APK.

## Guía histórica para crear una copia propia

Nombre sugerido: **sma3-recomp-android**.

Descripción sugerida:

> Recompilación estática experimental de SMA3 USA para Android ARM64, con contador de FPS y opciones mediante Atrás.

1. Descomprime el ZIP.
2. Crea un repositorio vacío en tu cuenta de GitHub.
3. Abre una terminal dentro de la carpeta `sma3-recomp` y ejecuta lo siguiente, sustituyendo `TU_USUARIO`:

```sh
git init -b main
git add .
git status
git commit -m "Proyecto inicial de SMA3 Recomp para Android"
git remote add origin https://github.com/TU_USUARIO/sma3-recomp-android.git
git push -u origin main
```

Git puede solicitar tu identidad de autor y la autenticación de tu cuenta. Usa tus propios datos. No pongas contraseñas ni tokens en archivos del proyecto.

También puedes subir los archivos descomprimidos desde GitHub. **No subas solamente el ZIP**, porque quedaría como un archivo descargable en vez de mostrar el código y el README. Incluye `.gitignore`.

Antes de confirmar el primer commit, comprueba con `git status` que no aparezcan ROM, BIOS, claves, APK ni carpetas generadas. `.gitignore` ya contempla esos archivos.

Estas instrucciones describen la importación inicial de una copia local.
No son necesarias para actualizar el repositorio existente indicado arriba.
