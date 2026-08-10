# Animation alpha workflow

Run every generated animation PNG through the shared cleanup after the sheet is
assembled:

```bat
tools\run_png_alpha_repair.bat --mode repair --include characterName
```

The command now performs both alpha-fringe cleanup and the deeper, selective
silhouette decontamination proven on Widowroot. The second stage works on each
128x128 or 256x256 frame independently, keeps RGBA and alpha intact, zeros RGB
under alpha zero, and does not propagate colors between disconnected spell
effects and the character body.

For a full non-destructive audit:

```bat
tools\run_animation_halo_cleanup.bat --mode scan
```

For the visually reviewed legacy repair set:

```bat
tools\run_animation_halo_cleanup.bat --mode repair
```

For validation of one character after a build:

```bat
tools\run_animation_halo_cleanup.bat --mode validate --include characterName
```

Repair mode creates timestamped originals under
`tools/animation-halo-backups/` before replacing any PNG.
