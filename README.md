# PaperSpecimen

**Font specimen viewer for M5Paper e-ink display**

Visualize TrueType glyphs in high-quality anti-aliased bitmap or detailed vector outline mode with Bézier construction lines. Toggle between modes, explore Unicode ranges, auto-scale glyphs, and enjoy weeks of battery life with deep sleep.

## Features

### Dual Rendering Modes

**Bitmap Mode** (default)
- Custom FreeType rendering with 16-level grayscale anti-aliasing
- Smooth, high-quality glyph visualization
- Per-glyph scaling to 400px target size

**Outline Mode** (long press center button)
- Vector outline visualization with control points
- On-curve points: filled black circles
- Off-curve points: hollow circles (Bézier control points)
- Construction lines: dashed black lines showing curve structure
- Gray outline showing the final glyph shape

### Smart Display Management

- **Per-glyph scaling**: Each glyph scales independently to fill screen optimally (400px max)
- **Smart refresh**: Partial refresh for speed, automatic full refresh every 5 updates or 10 seconds
- **Ghosting prevention**: Initial full refresh on boot to clear screen artifacts
- **Unicode exploration**: Random glyph selection from 14 common Unicode ranges

### Power Management

- **Deep sleep**: Automatic sleep after 10 seconds of inactivity
- **Auto-wake**: Wake every 15 minutes, display random font + glyph
- **Manual wake**: Press center button to wake and restore previous state
- **Ultra-low power**: WiFi and Bluetooth disabled, weeks of battery life
- **Low battery alert**: Warning at 5% battery, automatic shutdown

## Hardware Requirements

- **Device**: M5Paper (not M5PaperS3)
- **MicroSD**: FAT32 formatted with `/fonts` directory
- **Fonts**: TrueType (.ttf) files only

## Setup

### 1. Prepare MicroSD Card

1. Format microSD card as **FAT32**
2. Create `/fonts` directory in root
3. Copy your `.ttf` font files into `/fonts`
4. **Important**: If you add/remove/change fonts while the device is on, you must restart by pressing the reset button on the back, then the center side button to wake

Example structure:
```
/
└── fonts/
    ├── Helvetica-Regular.ttf
    ├── Helvetica-Bold.ttf
    ├── Arial.ttf
    └── YourFont.ttf
```

### 2. Flash Firmware

**Using PlatformIO:**
```bash
pio run --target upload
```

**Using pre-compiled binary:**
```bash
esptool.py --chip esp32 --port /dev/ttyUSB0 write_flash 0x10000 firmware.bin
```

### 3. Insert SD Card and Power On

The device will:
1. Show boot screen
2. Scan for fonts in `/fonts`
3. Display first random glyph in bitmap mode
4. Perform full refresh to clear ghosting

## Controls

| Button | Action |
|--------|--------|
| **Wheel UP** (BtnR) | Next font (keeps same glyph) |
| **Wheel DOWN** (BtnL) | Previous font (keeps same glyph) |
| **Wheel SHORT PRESS** (BtnP) | Random glyph (keeps same font) |
| **Wheel LONG PRESS** (BtnP, ≥800ms) | Toggle bitmap ↔ outline mode |

## Display Information

**On-screen labels:**
- **Top**: Font name (e.g., "Helvetica-Bold")
- **Bottom**: Unicode codepoint (e.g., "U+0041")
- **Bottom-right**: Font position (e.g., "2/5")

**Serial output** (115200 baud):
- Font list on startup
- Current font and glyph
- Glyph changes with Unicode range info
- Refresh type (partial/full) per update
- Outline parsing details (segments, points, curves)

## Unicode Ranges

Random glyph selection includes these ranges:

- Basic Latin (ASCII)
- Latin-1 Supplement (accented characters)
- Latin Extended A/B
- General Punctuation
- Currency Symbols
- Letterlike Symbols
- Number Forms
- Arrows
- Mathematical Operators
- Miscellaneous Technical
- Box Drawing
- Block Elements
- Geometric Shapes

## Technical Specifications

### Memory Usage
- **Flash**: 85.2% (1,116,661 / 1,310,720 bytes)
- **RAM**: 1.3% (57,820 / 4,521,984 bytes)
- **PSRAM**: Used for large glyph rendering

### Display Configuration
- **Orientation**: Vertical (540×960)
- **Rendering**: 16 grayscale levels (4-bit per pixel)
- **Target glyph size**: 400px (auto-scaled per glyph)
- **Refresh modes**:
  - `UPDATE_MODE_GL16`: Fast partial refresh
  - `UPDATE_MODE_GC16`: Full refresh (ghosting removal)

### Power Optimization
- WiFi disabled
- Bluetooth disabled
- Touchscreen not initialized
- External sensors (SHT30) powered down
- Only physical buttons active
- Deep sleep between interactions

## Building from Source

### Requirements
- [PlatformIO](https://platformio.org/)
- ESP32 toolchain (installed automatically by PlatformIO)

### Dependencies
- M5EPD @ 0.1.5 (includes FreeType for TTF rendering)
- SD @ 2.0.0
- WiFi @ 2.0.0

### Build Commands

**Compile:**
```bash
pio run
```

**Upload:**
```bash
pio run --target upload
```

**Serial monitor:**
```bash
pio device monitor -b 115200
```

## Troubleshooting

### "SD CARD ERROR"
- Verify microSD is inserted correctly
- Ensure card is formatted as FAT32
- Try reformatting the card

### "NO FONTS FOUND"
- Check fonts are in `/fonts` directory
- Verify files have `.ttf` extension (only TrueType supported)
- File names are case-insensitive (TTF/ttf both work)
- If you added fonts while device was on, press reset button on back then center button to restart

### "FONT LOAD ERROR"
- Font file may be corrupted
- Try a different font to verify
- Some complex fonts may fail to load

### Display Ghosting
- Firmware automatically does full refresh every 10 seconds
- If excessive ghosting occurs, wait for next automatic refresh
- Some ghosting is normal for e-ink after many partial updates

### Deep Sleep Not Working
- Check serial output for sleep messages
- Ensure no button is stuck pressed
- Verify 10 second timeout has elapsed since last full refresh

## Architecture

### Custom Bitmap Rendering
Instead of using M5EPD's built-in text rendering, PaperSpecimen implements custom FreeType bitmap rendering:

1. Load glyph with `FT_Load_Glyph()`
2. Calculate bounding box from outline
3. Compute pixel size for 400px target: `(target_size × units_per_EM) / max_glyph_dimension`
4. Render with `FT_LOAD_RENDER` (8-bit grayscale)
5. Convert 8bpp → 4bpp for M5EPD
6. Draw pixel-by-pixel on canvas

**Benefits:**
- Per-glyph scaling (small glyphs become larger)
- Identical dimensions between bitmap and outline modes
- Full control over rendering pipeline

### Outline Mode Implementation

**Step 1**: Access protected `FT_Face` via external symbol linkage
```cpp
extern font_face_t _ZN8TFT_eSPI10_font_faceE;
FT_Face face = _ZN8TFT_eSPI10_font_faceE.ft_face;
```

**Step 2**: Decompose outline with FreeType callbacks
- `outlineMoveTo`: Start new contour
- `outlineLineTo`: Straight line segment
- `outlineConicTo`: Quadratic Bézier (TrueType)
- `outlineCubicTo`: Cubic Bézier (PostScript/OpenType)

**Step 3**: Render outline with line approximation
- Lines: Direct `drawLine()`
- Quadratic Bézier: 10-step approximation
- Cubic Bézier: 15-step approximation

**Step 4**: Draw control points
- On-curve: Filled circles (4px radius)
- Off-curve: Hollow circles (4px outer, 3px inner)

**Step 7**: Construction lines (dashed)
- Connect control points to anchor points
- Pattern: 10px on, 5px off
- Color: Black (15) for visibility against gray outline (12)

### State Persistence (RTC Memory)

State survives deep sleep via ESP32 RTC memory:
```cpp
RTC_DATA_ATTR struct {
    bool isValid;
    int currentFontIndex;
    uint32_t currentGlyphCodepoint;
    ViewMode viewMode;  // BITMAP or OUTLINE
} rtcState;
```

## Development Timeline

- **Step 1**: FT_Face access via external symbol hack
- **Step 2**: Outline decomposition with FreeType callbacks
- **Step 3**: Basic outline rendering with centering
- **Step 4**: Control point visualization (on-curve/off-curve)
- **Step 5**: Bitmap/outline toggle with long press + RTC persistence
- **Step 5.1**: Per-glyph scaling for both modes
- **Step 7**: Construction lines with dashed pattern

## Credits

- **Hardware**: M5Stack M5Paper
- **Libraries**: M5EPD (includes FreeType for TTF rendering)
- **Framework**: Arduino ESP32 via PlatformIO
- **Development**: Built with [Claude Code](https://claude.com/claude-code)

## License

MIT License - use as you prefer

## Contributing

Issues and pull requests welcome! Especially interested in:
- Additional Unicode range support
- Performance optimizations
- Alternative rendering modes
- Font metadata display
- Multi-glyph comparison view

---

**Enjoy exploring typography on e-ink!** 📖✨
