# PaperSpecimen

**Font specimen viewer for M5Paper e-ink display**

Visualize TrueType glyphs in high-quality anti-aliased bitmap or detailed vector outline mode with Bézier construction lines. Configure fonts, refresh intervals, and standby behavior with an intuitive UI. Enjoy weeks of battery life with intelligent deep sleep.

**Current version: v2.1.3**

## Features

### Dual Rendering Modes

**Bitmap Mode** (default)
- Custom FreeType rendering with 16-level grayscale anti-aliasing
- Smooth, high-quality glyph visualization
- Per-glyph auto-scaling to 375px target size

**Outline Mode** (toggle via setup)
- Vector outline visualization with control points
- On-curve points: filled black circles
- Off-curve points: hollow circles (Bézier control points)
- Construction lines: dashed black lines showing curve structure
- Gray outline showing the final glyph shape

### Configuration UI (v2.1+)

**First Boot Setup:**
On first boot (or after reset), interactive setup screen allows you to configure:

1. **Refresh Timer** - How often to wake from sleep and show new random glyph:
   - 15 minutes (default)
   - 10 minutes
   - 5 minutes

2. **When in Standby** - Behavior when auto-waking from sleep:
   - ✓ Allow different font - Randomly select from enabled fonts (default: ON)
   - ✓ Allow different mode - Randomly switch between bitmap/outline (default: ON)
   - Both OFF: Maintains current font and display mode

3. **Font Selection** - Enable/disable individual fonts:
   - Select/Deselect all - Quick toggle for all fonts
   - Individual font checkboxes
   - Multi-page navigation for many fonts (5+ fonts)
   - Only enabled fonts will appear in rotation

**Configuration Persistence:**
- Settings saved to `/paperspecimen.cfg` on SD card
- Survives power cycles and deep sleep
- To reconfigure: press reset button on the back and restart (config will be recreated automatically)

**Navigation:**
- `<<<` / `>>>` - Previous/next page (when 6+ fonts)
- Wheel UP/DOWN - Move cursor
- Wheel PRESS - Toggle selection / Confirm

### Boot & Shutdown Screens (v2.1.1+)

**Boot Splash** (shown for 5 seconds):
- QR code linking to GitHub repository
- "PaperSpecimen" header
- Version number footer

**Shutdown Screen**:
- Long press center button (5+ seconds) triggers graceful shutdown
- Same QR code and labels as boot
- Enters deep sleep until manual wake (center button press)

### Smart Display Management

- **Per-glyph scaling**: Each glyph scales independently to fill screen optimally (375px max)
- **Smart refresh**: Partial refresh for speed, automatic full refresh every 5 updates or 10 seconds
- **Ghosting prevention**: Full refresh on boot and periodically during use
- **Unicode exploration**: Random glyph selection from 9 common Unicode ranges
- **Long name truncation**: Font names truncated with "..." if exceeding screen margins

### Power Management

- **Deep sleep**: Automatic sleep after configurable interval (5/10/15 min)
- **Auto-wake**: Wake every N minutes, display random glyph based on standby settings
- **Manual wake**: Press center button to wake and restore previous state
- **Graceful shutdown**: Long press center button (5s) for shutdown with visual feedback
- **Ultra-low power**: WiFi and Bluetooth disabled, weeks of battery life
- **Low battery alert**: Warning at 5% battery, automatic shutdown

## Hardware Requirements

- **Device**: M5Paper (not M5PaperS3)
- **MicroSD**: FAT32 formatted with `/fonts` directory
- **Fonts**: TrueType (.ttf) files

## Setup

### 1. Prepare MicroSD Card

1. Format microSD card as **FAT32**
2. Create `/fonts` directory in root
3. Copy your `.ttf` font files into `/fonts`
4. **Important**: If you add/remove/change fonts while the device is on, you must restart by pressing the reset button on the back, then the center side button to wake

Example structure:
```
/
├── fonts/
│   ├── Helvetica-Regular.ttf
│   ├── Helvetica-Bold.ttf
│   ├── NotoSans-Regular.ttf
│   └── YourFont.ttf
└── paperspecimen.cfg  (auto-generated on first boot)
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
1. Show boot splash for 5 seconds (QR code + version)
2. Scan for fonts in `/fonts`
3. Launch setup UI (only after reset)
4. Save configuration to `/paperspecimen.cfg`
5. Display first random glyph in bitmap mode
6. Enter deep sleep after inactivity

## Controls

### Setup Screen
| Button | Action |
|--------|--------|
| **Wheel UP** (BtnL) | Move cursor up |
| **Wheel DOWN** (BtnR) | Move cursor down |
| **Wheel PRESS** (BtnP) | Select/Toggle/Confirm |

### Glyph Display
| Button | Action |
|--------|--------|
| **Wheel UP** (BtnR) | Next font (keeps same glyph) |
| **Wheel DOWN** (BtnL) | Previous font (keeps same glyph) |
| **Wheel SHORT PRESS** (BtnP) | Random glyph (keeps same font) |
| **Wheel LONG PRESS** (BtnP, ≥5s) | Graceful shutdown with visual feedback |

## Display Information

**Glyph Screen:**
- **Top**: Font name (truncated if long, e.g., "NotoSans...Regular")
- **Bottom**: Unicode codepoint (e.g., "U+0041")

**Serial output** (115200 baud):
- Font list on startup
- Configuration loading/saving
- Current font and glyph
- Glyph changes with Unicode range info
- Refresh type (partial/full) per update
- Outline parsing details (segments, points, curves)
- Wake reason (timer, button, cold boot)
- Battery level on wake from sleep

## Unicode Ranges

Random glyph selection currently includes these ranges (190 total characters):

**Basic Latin:**
- Uppercase letters (A-Z)
- Lowercase letters (a-z)
- Digits (0-9)
- Basic punctuation (!"#$%&'()*+,-./:;<=>?@[\]^_`{|}~)

**Latin-1 Supplement:**
- Extended punctuation (¡-¿)
- Accented letters (À-ÿ)

**Future expansion** (planned for v2.2):
- User-configurable Unicode block selection
- Greek, Cyrillic, Arabic, Hebrew
- Currency symbols, Math operators, Arrows
- Emoji, Box drawing, Geometric shapes

## Technical Specifications

### Memory Usage (v2.1.3)
- **Flash**: 86.4% (1,132,153 / 1,310,720 bytes) - ~174 KB free
- **RAM**: 1.3% (57,868 / 4,521,984 bytes)
- **PSRAM**: Used for large glyph rendering

### Display Configuration
- **Resolution**: 540×960 (vertical orientation)
- **Rendering**: 16 grayscale levels (4-bit per pixel)
- **Target glyph size**: 400px (auto-scaled per glyph)
- **QR code**: 29×29 modules (Version 3), 6×6 pixels per module
- **Refresh modes**:
  - `UPDATE_MODE_GL16`: Fast partial refresh
  - `UPDATE_MODE_GC16`: Full refresh (ghosting removal)
  - `UPDATE_MODE_A2`: Anti-aliased partial (setup UI)

### Power Optimization
- WiFi disabled
- Bluetooth disabled
- Touchscreen not initialized
- External sensors (SHT30) powered down
- Only physical buttons active
- Deep sleep with timer wake (5/10/15 min configurable)
- RTC GPIO hold to maintain power state


**Format** (plain text):
```
15                    # Wake interval (5, 10, or 15 minutes)
1                     # Allow different font (1=yes, 0=no)
1                     # Allow different mode (1=yes, 0=no)
1                     # Font 1 enabled (1=yes, 0=no)
1                     # Font 2 enabled
0                     # Font 3 disabled
...                   # One line per font
```

**To reset:** Press reset button on the back and restart device (config will be recreated automatically)

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
screen /dev/cu.usbserial-* 115200
# or
pio device monitor -b 115200
```

**Exit screen:** Ctrl+A, K, Y

## Troubleshooting

### "SD CARD ERROR"
- Verify microSD is inserted correctly
- Ensure card is formatted as FAT32
- Try reformatting the card

### "NO FONTS FOUND"
- Check fonts are in `/fonts` directory
- Verify files have `.ttf` or `.otf` extension
- File names are case-insensitive (TTF/ttf both work)
- If you added fonts while device was on, press reset button on back then center button to restart

### "NO FONTS ENABLED"
- At least one font must be enabled in configuration
- Press reset button on the back and restart to reconfigure (config will be recreated automatically)
- Setup screen prevents saving with zero fonts enabled (auto-enables all as fallback)

### "FONT LOAD ERROR"
- Font file may be corrupted
- Try a different font to verify
- Some complex fonts may fail to load
- Check serial output for specific error codes

### Display Ghosting
- Firmware automatically does full refresh every 5 updates or 10 seconds
- Setup UI uses anti-aliased partial refresh (A2 mode)
- Some ghosting is normal for e-ink after many partial updates

### Configuration Not Saving
- Verify SD card is writable
- Check for filesystem errors
- Serial output shows save success/failure
- Config file must be in root directory

### Random Mode Not Working
- Check "Allow different mode" is enabled in config
- Works only on auto-wake (timer), not manual wake (button)
- Serial output shows: "Random mode selected: BITMAP/OUTLINE"

## Architecture

### v2.1 Configuration System

**Setup Flow:**
1. **Cold boot** → Check for `/paperspecimen.cfg`
2. If missing → Launch unified setup screen (interval + fonts + standby)
3. User configures → Save to SD card
4. **Wake from sleep** → Load config from SD

**Config Structure:**
```cpp
struct AppConfig {
    uint8_t wakeIntervalMinutes;      // 5, 10, or 15
    bool allowDifferentFont;          // Random font on wake
    bool allowDifferentMode;          // Random mode on wake
    std::vector<bool> fontEnabled;    // Per-font enable flags
};
```

**Menu System:**
- Item types: CONFIRM, LABEL, RADIO, CHECKBOX, SEPARATOR, PAGE_NAV
- Smart pagination: ≤5 fonts = 1 page, 6+ fonts = multi-page
- Cursor navigation with wrap-around
- Visual indicators: `(*)` selected, `( )` unselected
- Text truncation for long font names (380px max width)

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

**Step 5**: Construction lines (dashed)
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

**Wake behavior:**
- **Auto-wake (timer)**: Apply random font/mode based on config
- **Button wake**: Restore exact previous state (font, glyph, mode)
- **Cold boot**: Show setup if no config, else load first font

## Version History

### v2.1.3 (Current)
- **Standby behavior configuration**: "When in standby" options
  - Allow different font (random font selection on wake)
  - Allow different mode (random bitmap/outline toggle on wake)
- **UI improvements**:
  - Font name truncation with balanced "..." for long names
  - Page navigation symbols: `<<<` previous, `>>>` next
  - Optimized pagination: ≤5 fonts in single page
  - Reduced right margin for font list (60px)
- **Bug fixes**:
  - Fixed text truncation measurement (bitmap font textSize=3)
  - Corrected cursor position after page navigation

### v2.1.2
- **QR code fixes**: Corrected 29×29 QR bitmap data
- **Display refinements**: 6×6 pixel modules for better scannability
- **Code cleanup**: Removed debug rectangles and test code

### v2.1.1
- **Boot & shutdown screens**: QR code linking to GitHub
- **Graceful shutdown**: Long press (5s) triggers visual shutdown sequence
- **Text rendering fix**: Bitmap font labels on shutdown screen
- **Version display**: Show version number on all screens

### v2.1.0
- **Unified setup UI**: Single-screen configuration for all settings
- **Font selection**: Per-font enable/disable with checkboxes
- **Refresh timer**: Configurable wake interval (5/10/15 minutes)
- **Multi-page support**: Pagination for 6+ fonts
- **Config persistence**: Settings saved to SD card
- **Smart cursor navigation**: Wrap-around, skip non-selectable items

### v2.0
- **Deep sleep integration**: Auto-wake with configurable interval
- **RTC state persistence**: Restore font/glyph/mode after wake
- **Power optimization**: WiFi/BT disabled, minimal power draw
- **Low battery handling**: Alert and shutdown at 5%

### v1.0
- Initial release
- Dual rendering modes (bitmap/outline)
- Per-glyph scaling
- Random glyph generation
- Smart refresh management

## Roadmap

### v2.2 (Planned)
- **Unicode block configuration**: User-selectable character ranges
  - Greek, Cyrillic, Arabic, Hebrew
  - Currency, Math, Arrows, Emoji
  - Geometric shapes, Box drawing
- **Enhanced UI**: Second configuration page for Unicode blocks
- **Smart fallback**: Skip unavailable ranges per font

## Credits

- **Hardware**: M5Stack M5Paper
- **Libraries**: M5EPD (includes FreeType for TTF rendering)
- **Framework**: Arduino ESP32 via PlatformIO
- **Development**: Built with [Claude Code](https://claude.com/claude-code)

## License

MIT License - use as you prefer

## Contributing

Issues and pull requests welcome! Especially interested in:
- Unicode range expansion
- Performance optimizations
- Alternative rendering modes
- Font metadata extraction
- UI/UX improvements
- Power consumption optimization
- Additional file format support

---

**Enjoy exploring typography on e-ink!** 📖✨
