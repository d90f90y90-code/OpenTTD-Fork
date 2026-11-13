# OpenTTD TrueType font

The OpenTTD TrueType font was created by Zephyris and is maintained on [Github](https://github.com/OpenTTD/OpenTTD-TTF).
It is licensed under GPL-2.0.

The currently included files correspond to release v0.9.1.

## Arabic glyph extension (fork modification)

The four font files in this directory were modified in this fork: Arabic glyphs (including shaping tables)
from Noto Kufi Arabic Regular (Copyright Google LLC, SIL Open Font License 1.1, see `NotoKufiArabic-OFL.txt`)
were merged into them. The Latin glyphs are unchanged.
To use another Arabic font (e.g. a custom one), merge it the same way or set `small_font`, `medium_font`,
`large_font` and `mono_font` in `openttd.cfg`.
