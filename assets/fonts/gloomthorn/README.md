# Gloomthorn Display

`GloomthornDisplay-Regular.ttf` is a display serif intended to match the
Gloomthorn title treatment. It contains uppercase and lowercase letters,
numerals, punctuation, ligatures, and broad Latin-script coverage.

The font is a modified static instance of Cormorant Garamond at weight 500,
with display spacing and independent family metadata. Cormorant is Copyright
2015 The Cormorant Project Authors and is distributed under the SIL Open Font
License 1.1. The same license applies to this derivative; see `OFL.txt`.
The source variable font is available from:
https://github.com/google/fonts/tree/main/ofl/cormorantgaramond

The TTF stores monochrome glyph outlines. Apply the gold gradient, bevel,
inner shadow, distressing, and rim lighting in the renderer or graphics tool.

Build:

```powershell
python tools/build_gloomthorn_font.py `
  CormorantGaramond-wght.ttf `
  assets/fonts/gloomthorn/GloomthornDisplay-Regular.ttf
```

Render the in-game metallic title texture:

```powershell
python tools/render_gloomthorn_title.py `
  assets/fonts/gloomthorn/GloomthornDisplay-Regular.ttf `
  assets/ui/gloomthorn-title.png
```
