# Procedencia y licencias

- **GBARecomp** — https://github.com/mstan/gbarecomp, commit `e7728148c6829ba526f682876430a0c9022dc6c0`. Motor externo y base de la interfaz Java incluida en `android/java`. Licencia conservada en `licenses/GBARecomp-LICENSE.txt` (PolyForm Noncommercial 1.0.0).
- **arm-recomp-core** — dependencia fijada por GBARecomp, commit `c626f4e53fcdca0c72d2a7d34d41663d87c7e175`. Licencia MIT conservada en `licenses/arm-recomp-core-LICENSE.txt`.
- **sma3-disasm** — https://github.com/KarisaAdvynia/sma3-disasm, commit `c8532ec9a8d0038c3bfeb003dd0c7ea89d7e1071`. Referencia de direcciones, etiquetas y copias a RAM; `symbols.tsv` deriva de sus anotaciones. El desensamblado no se incluye.
- **SDL y demás submódulos** — se descargan mediante el checkout fijado del motor y conservan sus propias licencias.
- **Gradle Wrapper** — archivos del sistema de compilación, con los avisos de licencia de sus scripts.

No se añade una licencia permisiva global que sustituya o elimine las condiciones de estos componentes. No se incluyen archivos de juego ni BIOS, y este proyecto no concede derechos sobre ellos. Las modificaciones de integración se publican aquí sin una concesión adicional de licencia general.
