# DEFRecolor

DEFRecolor is a Windows tool designed for editing palette-based DEF files used in **Heroes of Might and Magic 3**. It provides an easy way to load, modify, and save DEF files with simple graphical interface.

This tool is specifically tailored for Heroes of Might and Magic 3 modding needs, making palette management for DEF files simple and efficient.

## Features

- Load and visualize DEF palettes from Heroes 3.
- Modify individual colors with a color picker.
- Export and import palettes in JSON format.
- Preview the first useful DEF frame and see palette changes immediately.
- Browse every frame and group in the loaded DEF.
- Select one or multiple palette colors and adjust hue, saturation, and lightness with live preview.
- Build color sets automatically with Magic Recolor, similarity tolerance, target color, strength, and optional shade preservation.
- Switch English/Czech UI language at runtime from external JSON language packs.
- Use contextual tooltips and the built-in Help guide for editing and reset behavior.

## Usage
1. Run `DEFRecolor.exe`.
2. Use the menu to:
   - **Load DEF**: Open a DEF file to edit its palette.
   - **Save DEF**: Save the modified DEF file.
   - **Export Palette**: Save the palette as a JSON file.
   - **Import Palette**: Load a palette from a JSON file.
3. Click on any color to modify it using the color picker.

The left palette is the original reference; edit colors in the right palette.

### Command line

Paths are Unicode-aware, including folders with accented or Cyrillic names.

```text
DEFRecolor /load input.def /import palette.json /save output.def
DEFRecolor /load input.def /export palette.json
```

### Live preview

Live preview uses [DefThumbnailProvider](https://github.com/GeorgeK1ng/DefThumbnailProvider),
so it renders DEF frames the same way as Windows Explorer. Install the thumbnail
provider for the same architecture as DEFRecolor, or place `DefThumbnailProvider.dll`
next to `DEFRecolor.exe`.

The frame browser and editor controls require a recent provider build exposing
the `DefEditor*` API. Older provider DLLs continue to support the first-frame preview.

Initial view, left side is loaded static and right side is editable

![image](https://github.com/user-attachments/assets/dc932ac8-5333-4fd2-a4fb-0b2b000f1d31)

Imported DEF

![image](https://github.com/user-attachments/assets/a676a27a-df26-479e-9533-cd80c80ee1cf)

Color picker in right editable side

![image](https://github.com/user-attachments/assets/2d009997-f1ef-4afc-ac00-798577f2fb91)

Recolored DEF using imported JSON palette

![image](https://github.com/user-attachments/assets/f37f8819-8802-4b09-9fd4-2757065ad76d)

Palette in human readable form

![image](https://github.com/user-attachments/assets/aeb98c0d-edc5-4095-b3e7-26177a114c55)

Example of recolored Hero movement arrow

![image](https://github.com/user-attachments/assets/4e18de62-8f97-4e1d-ab3a-60fc887808c7)


## Contributing
Feel free to open an issue or submit a pull request to contribute to the project.
