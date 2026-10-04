# Subir a tu GitHub

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

Este paquete se preparó localmente: no se creó ni publicó un repositorio en tu cuenta.
