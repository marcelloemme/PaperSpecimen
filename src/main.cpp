#include <M5EPD.h>
// #include <WiFi.h>  // Removed: WiFi not used, saves ~60-100KB Flash
#include <SD.h>
#include <vector>
#include <esp_sleep.h>

// FreeType headers for outline access and bitmap rendering
#include "freetype/freetype.h"
#include "freetype/ftoutln.h"
#include "freetype/ftglyph.h"
#include "freetype/ftbitmap.h"
#include <esp_heap_caps.h>

// Canvas for rendering
M5EPD_Canvas canvas(&M5.EPD);  // Single full screen canvas for everything

// QR Code bitmap for GitHub repository
// URL: https://github.com/marcelloemme/PaperSpecimen
// Size: 29x29 modules (Version 3 QR Code)
// Generated from qr-code.png - VERIFIED CORRECT
const uint8_t QR_SIZE = 29;
const uint8_t qrcode_data[] PROGMEM = {
    0b11111110, 0b01000010, 0b10111011, 0b11111000,  // Row 0
    0b10000010, 0b11011101, 0b11100010, 0b00001000,  // Row 1
    0b10111010, 0b01110001, 0b10010010, 0b11101000,  // Row 2
    0b10111010, 0b11010011, 0b01001010, 0b11101000,  // Row 3
    0b10111010, 0b00101010, 0b10111010, 0b11101000,  // Row 4
    0b10000010, 0b10100101, 0b00001010, 0b00001000,  // Row 5
    0b11111110, 0b10101010, 0b10101011, 0b11111000,  // Row 6
    0b00000000, 0b00001011, 0b00000000, 0b00000000,  // Row 7
    0b11111011, 0b11101111, 0b11000101, 0b01010000,  // Row 8
    0b11100001, 0b11000010, 0b11111011, 0b10001000,  // Row 9
    0b11110110, 0b11011001, 0b10001000, 0b10000000,  // Row 10
    0b10011100, 0b11110001, 0b00101000, 0b01010000,  // Row 11
    0b10110110, 0b11010111, 0b01010000, 0b01100000,  // Row 12
    0b00110100, 0b00101000, 0b11111111, 0b10001000,  // Row 13
    0b10100011, 0b11100011, 0b00001100, 0b11100000,  // Row 14
    0b00111001, 0b10001011, 0b00111111, 0b10010000,  // Row 15
    0b01000111, 0b00101111, 0b11010101, 0b01100000,  // Row 16
    0b11010101, 0b11100001, 0b11110111, 0b10101000,  // Row 17
    0b10100110, 0b11111101, 0b11001011, 0b10100000,  // Row 18
    0b10110101, 0b00010010, 0b10001110, 0b00010000,  // Row 19
    0b10001110, 0b01111110, 0b01011111, 0b10111000,  // Row 20
    0b00000000, 0b10101100, 0b00101000, 0b11111000,  // Row 21
    0b11111110, 0b11001001, 0b10011010, 0b11100000,  // Row 22
    0b10000010, 0b00010011, 0b10011000, 0b10000000,  // Row 23
    0b10111010, 0b10111111, 0b01001111, 0b10101000,  // Row 24
    0b10111010, 0b11010100, 0b11111000, 0b01111000,  // Row 25
    0b10111010, 0b11100011, 0b10011111, 0b11110000,  // Row 26
    0b10000010, 0b11000011, 0b10001101, 0b01010000,  // Row 27
    0b11111110, 0b11101110, 0b01010011, 0b10100000   // Row 28
};

// ========================================
// v2.2: Unicode Ranges
// ========================================
// Unicode ranges for random glyph selection
struct UnicodeRange {
    uint32_t start;
    uint32_t end;
    const char* name;
};

// v2.2: Expanded Unicode ranges (28 total, user-customizable)
const UnicodeRange glyphRanges[] = {
    // Original 6 ranges (enabled by default)
    {0x0041, 0x005A, "Latin Uppercase (U+0041-005A)"},
    {0x0061, 0x007A, "Latin Lowercase (U+0061-007A)"},
    {0x0030, 0x0039, "Digits (U+0030-0039)"},
    {0x0021, 0x002F, "Basic Punctuation (U+0021-002F)"},
    {0x00A1, 0x00BF, "Latin-1 Punctuation (U+00A1-00BF)"},
    {0x00C0, 0x00FF, "Latin-1 Letters (U+00C0-00FF)"},

    // New ranges (22 additional - disabled by default)
    {0x0100, 0x017F, "Latin Extended-A (U+0100-017F)"},
    {0x0180, 0x024F, "Latin Extended-B (U+0180-024F)"},
    {0x0370, 0x03FF, "Greek and Coptic (U+0370-03FF)"},
    {0x0400, 0x04FF, "Cyrillic (U+0400-04FF)"},
    {0x0590, 0x05FF, "Hebrew (U+0590-05FF)"},
    {0x0600, 0x06FF, "Arabic (U+0600-06FF)"},
    {0x0900, 0x097F, "Devanagari (U+0900-097F)"},
    {0x0E00, 0x0E7F, "Thai (U+0E00-0E7F)"},
    {0x10A0, 0x10FF, "Georgian (U+10A0-10FF)"},
    {0x3040, 0x309F, "Hiragana (U+3040-309F)"},
    {0x30A0, 0x30FF, "Katakana (U+30A0-30FF)"},
    {0x4E00, 0x9FFF, "CJK Ideographs (U+4E00-9FFF)"},
    {0xAC00, 0xD7AF, "Hangul (U+AC00-D7AF)"},
    {0x2000, 0x206F, "General Punctuation (U+2000-206F)"},
    {0x20A0, 0x20CF, "Currency Symbols (U+20A0-20CF)"},
    {0x2100, 0x214F, "Letterlike Symbols (U+2100-214F)"},
    {0x2190, 0x21FF, "Arrows (U+2190-21FF)"},
    {0x2200, 0x22FF, "Mathematical Operators (U+2200-22FF)"},
    {0x2500, 0x257F, "Box Drawing (U+2500-257F)"},
    {0x2580, 0x259F, "Block Elements (U+2580-259F)"},
    {0x25A0, 0x25FF, "Geometric Shapes (U+25A0-25FF)"},
    {0x2600, 0x26FF, "Miscellaneous Symbols (U+2600-26FF)"},
};
const int numGlyphRanges = sizeof(glyphRanges) / sizeof(glyphRanges[0]);

// ========================================
// v2.1: Configuration Structure
// ========================================
struct AppConfig {
    uint8_t wakeIntervalMinutes;  // 5, 10, or 15
    std::vector<bool> fontEnabled; // Per-font enable/disable flags
    bool allowDifferentFont;       // Allow random font on wake (default: true)
    bool allowDifferentMode;       // Allow random mode (outline/bitmap) on wake (default: true)
    std::vector<bool> rangeEnabled; // v2.2: Per-range enable flags (28 total)
};

// Global config instance
AppConfig config;
const char* CONFIG_FILE = "/paperspecimen.cfg";

// ========================================
// Font File Cache (for battery optimization)
// ========================================
#define MAX_FONT_CACHE_SIZE (1500000)  // 1.5MB total cache

struct FontCacheEntry {
    int fontIndex;
    uint8_t* data;
    size_t size;
};

std::vector<FontCacheEntry> fontCache;
size_t totalCacheSize = 0;

// ========================================
// View Mode (for display glyph rendering)
// ========================================
enum ViewMode {
    BITMAP,    // Bitmap rendering (filled glyph)
    OUTLINE    // Vector outline with control points and construction lines
};

// RTC memory structure to persist state across deep sleep
RTC_DATA_ATTR struct {
    bool isValid;
    int currentFontIndex;
    uint32_t currentGlyphCodepoint;
    ViewMode viewMode;  // STEP 5: Persist view mode across sleep
    uint64_t totalMillis; // Total uptime in milliseconds since last reset (accumulated across sleep cycles)
    bool debugMode;  // Debug mode: enables Serial output and battery logging (persists across wake, resets on cold boot)
} rtcState = {false, 0, 0x0041, BITMAP, 0, false};  // Default: BITMAP mode, 0 uptime, normal mode (debugMode=false)

// ========================================
// v2.2.1: Config File I/O Functions (Flash + SD)
// ========================================

// Forward declaration
bool loadConfigFromFile(File& file);

// Load config from SD
bool loadConfig() {
    Serial.println("\n=== Loading Config from SD ===");

    if (!SD.exists(CONFIG_FILE)) {
        Serial.println("Config file not found - will run first-time setup");
        return false;
    }

    File file = SD.open(CONFIG_FILE, FILE_READ);
    if (!file) {
        Serial.println("ERROR: Cannot open config file for reading");
        return false;
    }

    bool success = loadConfigFromFile(file);
    file.close();

    if (success) {
        Serial.println("=== Config Loaded from SD Successfully ===\n");
    }

    return success;
}

// Helper function to load config from any File object (Flash or SD)
bool loadConfigFromFile(File& file) {

    // Read wake interval (first line)
    String line = file.readStringUntil('\n');
    line.trim();
    config.wakeIntervalMinutes = line.toInt();

    // v2.2.1: Allow 1/2/5/10/15 minutes (1/2 for debug mode)
    if (config.wakeIntervalMinutes != 1 &&
        config.wakeIntervalMinutes != 2 &&
        config.wakeIntervalMinutes != 5 &&
        config.wakeIntervalMinutes != 10 &&
        config.wakeIntervalMinutes != 15) {
        Serial.printf("ERROR: Invalid wake interval %d, expected 1/2/5/10/15\n", config.wakeIntervalMinutes);
        file.close();
        return false;
    }

    Serial.printf("Wake interval: %d minutes\n", config.wakeIntervalMinutes);

    // Read allowDifferentFont (second line)
    line = file.readStringUntil('\n');
    line.trim();
    config.allowDifferentFont = (line == "1" || line == "true");
    Serial.printf("Allow different font: %s\n", config.allowDifferentFont ? "yes" : "no");

    // Read allowDifferentMode (third line)
    line = file.readStringUntil('\n');
    line.trim();
    config.allowDifferentMode = (line == "1" || line == "true");
    Serial.printf("Allow different mode: %s\n", config.allowDifferentMode ? "yes" : "no");

    // Read font enable flags (next N lines, where N = number of fonts)
    config.fontEnabled.clear();
    int fontIndex = 0;
    while (file.available()) {
        line = file.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            // Check if this is a separator line (range flags follow)
            if (line == "---") {
                break; // Stop reading font flags, move to range flags
            }
            bool enabled = (line == "1" || line == "true");
            config.fontEnabled.push_back(enabled);
            fontIndex++;
        }
    }

    // v2.2: Read Unicode range enable flags (remaining lines, or use defaults if not present)
    config.rangeEnabled.clear();
    int rangeIndex = 0;
    bool foundRangeFlags = false;
    while (file.available()) {
        line = file.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            bool enabled = (line == "1" || line == "true");
            config.rangeEnabled.push_back(enabled);
            rangeIndex++;
            foundRangeFlags = true;
        }
    }

    // If no range flags found in file (old config format), use defaults
    if (!foundRangeFlags) {
        Serial.println("No range flags in config - using defaults (first 6 enabled)");
        for (int i = 0; i < numGlyphRanges; i++) {
            config.rangeEnabled.push_back(i < 6);
        }
    }

    Serial.printf("Loaded %d font enable flags, %d range enable flags\n",
                  config.fontEnabled.size(), config.rangeEnabled.size());
    return true;
}

// Save config to SD card
bool saveConfig() {
    Serial.println("\n=== Saving Config ===");

    File file = SD.open(CONFIG_FILE, FILE_WRITE);
    if (!file) {
        Serial.println("ERROR: Cannot open config file for writing");
        return false;
    }

    // Write wake interval
    file.println(config.wakeIntervalMinutes);
    Serial.printf("Saved wake interval: %d minutes\n", config.wakeIntervalMinutes);

    // Write allowDifferentFont
    file.println(config.allowDifferentFont ? "1" : "0");
    Serial.printf("Saved allow different font: %s\n", config.allowDifferentFont ? "yes" : "no");

    // Write allowDifferentMode
    file.println(config.allowDifferentMode ? "1" : "0");
    Serial.printf("Saved allow different mode: %s\n", config.allowDifferentMode ? "yes" : "no");

    // Write font enable flags
    for (size_t i = 0; i < config.fontEnabled.size(); i++) {
        file.println(config.fontEnabled[i] ? "1" : "0");
    }
    Serial.printf("Saved %d font enable flags\n", config.fontEnabled.size());

    // v2.2: Write separator and Unicode range enable flags
    file.println("---");
    for (size_t i = 0; i < config.rangeEnabled.size(); i++) {
        file.println(config.rangeEnabled[i] ? "1" : "0");
    }
    Serial.printf("Saved %d range enable flags\n", config.rangeEnabled.size());

    file.close();
    Serial.println("=== Config Saved Successfully ===\n");
    return true;
}

// Initialize default config
void initDefaultConfig(int numFonts) {
    config.wakeIntervalMinutes = 15; // Default: 15 minutes
    config.allowDifferentFont = true; // Default: allow random font
    config.allowDifferentMode = true; // Default: allow random mode
    config.fontEnabled.clear();
    for (int i = 0; i < numFonts; i++) {
        config.fontEnabled.push_back(true); // All fonts enabled by default
    }

    // v2.2: Initialize Unicode range flags (first 6 enabled by default)
    config.rangeEnabled.clear();
    for (int i = 0; i < numGlyphRanges; i++) {
        config.rangeEnabled.push_back(i < 6); // First 6 ranges enabled, rest disabled
    }

    Serial.printf("Default config initialized: %d min, allow font=%s, allow mode=%s, %d fonts (all enabled), %d ranges (first 6 enabled)\n",
                  config.wakeIntervalMinutes,
                  config.allowDifferentFont ? "yes" : "no",
                  config.allowDifferentMode ? "yes" : "no",
                  numFonts, numGlyphRanges);
}

// ========================================
// v2.1: UI Framework for Setup Screens
// ========================================

// UI Constants
const int UI_TEXT_SIZE = 2;            // Font size for UI (bitmap font, 2 = 16px height)
const int UI_LINE_HEIGHT = 40;         // Spacing between lines
const int UI_TOP_MARGIN = 80;          // Top margin for first item
const int UI_CHECKBOX_SIZE = 20;       // Size of checkbox/radio button
const int UI_INDENT = 60;              // Indent for checkboxes
const int UI_MAX_TEXT_WIDTH = 380;     // Max text width (540 - 100 left - 60 right = 380px)

// Menu item types
enum MenuItemType {
    MENU_CONFIRM,      // Confirm button
    MENU_LABEL,        // Non-selectable label (e.g., "Refresh timer", "Select/Deselect all")
    MENU_RADIO,        // Radio button (interval selection)
    MENU_CHECKBOX,     // Checkbox (font selection)
    MENU_SEPARATOR,    // 30px empty space
    MENU_PAGE_NAV      // "…" for page navigation
};

// Menu item structure
struct MenuItem {
    MenuItemType type;
    String label;
    String displayLabel;  // Pre-computed truncated label (empty = use label)
    bool selected;     // For radio/checkbox
    int value;         // For radio (5, 10, 15) or page navigation
    int fontIndex;     // For font checkboxes (-1 for non-font items)
};

// Pagination state
struct PaginationState {
    int currentPage;
    int totalPages;
    int fontsPerPage;
};

// UI Helper: Shorten text if it exceeds max width, keeping equal chars before/after "..."
// NOTE: This function uses bitmap font for measurement (FreeType fonts return textWidth=0)
String shortenTextIfNeeded(const String& text, int maxWidth, int textSize = UI_TEXT_SIZE) {
    // IMPORTANT: Unload FreeType font and use bitmap font for measurement
    // FreeType canvas.textWidth() returns 0 because it's configured for glyph rendering, not labels
    canvas.unloadFont();
    canvas.setFreeFont(NULL);
    canvas.setTextFont(1); // Font 1 = bitmap font
    canvas.setTextSize(textSize);

    int textWidth = canvas.textWidth(text);

    Serial.printf("shortenTextIfNeeded: text='%s' textWidth=%d maxWidth=%d textSize=%d\n",
                  text.c_str(), textWidth, maxWidth, textSize);

    // If text fits, return as-is (textSize already set correctly)
    if (textWidth <= maxWidth) {
        Serial.println("  -> Text fits, no truncation needed");
        return text;
    }

    Serial.println("  -> Text too long, truncating...");

    // Calculate how many characters to keep on each side
    int textLen = text.length();
    int ellipsisWidth = canvas.textWidth("...");

    // Binary search to find optimal number of chars to keep on each side
    int charsPerSide = 1;
    int maxCharsPerSide = textLen / 2;

    for (int chars = maxCharsPerSide; chars >= 1; chars--) {
        // Build shortened string: first N chars + "..." + last N chars
        String shortened = text.substring(0, chars) + "..." + text.substring(textLen - chars);
        int shortenedWidth = canvas.textWidth(shortened);

        if (shortenedWidth <= maxWidth) {
            return shortened; // textSize already set correctly
        }
    }

    // Fallback: just show "..." if even 1 char per side is too long
    return "..."; // textSize already set correctly
}

// UI Helper: Draw checkbox as ASCII ( ) or (*)
void drawCheckbox(int x, int y, bool checked) {
    canvas.setTextDatum(CL_DATUM);
    if (checked) {
        canvas.drawString("(*)", x, y + UI_LINE_HEIGHT/2);
    } else {
        canvas.drawString("( )", x, y + UI_LINE_HEIGHT/2);
    }
}

// UI Helper: Draw radio button as ASCII ( ) or (*)
void drawRadioButton(int x, int y, bool selected) {
    canvas.setTextDatum(CL_DATUM);
    if (selected) {
        canvas.drawString("(*)", x, y + UI_LINE_HEIGHT/2);
    } else {
        canvas.drawString("( )", x, y + UI_LINE_HEIGHT/2);
    }
}

// UI Helper: Draw menu item
void drawMenuItem(int y, MenuItem& item, bool isCursor) {
    int textX = 270; // Center X for text
    int checkboxX = UI_INDENT; // Left margin for checkboxes

    // Draw cursor indicator ">" for selected item
    if (isCursor) {
        canvas.setTextDatum(CL_DATUM);
        canvas.drawString(">", 20, y + UI_LINE_HEIGHT/2);
    }

    // Draw based on item type
    switch (item.type) {
        case MENU_CONFIRM:
            // Draw invisible (*) for alignment, then "Confirm" button
            canvas.setTextColor(0); // White (invisible)
            canvas.setTextDatum(CL_DATUM);
            canvas.drawString("(*)", checkboxX, y + UI_LINE_HEIGHT/2);
            canvas.setTextColor(15); // Back to black
            canvas.drawString(item.label, checkboxX + 40, y + UI_LINE_HEIGHT/2); // +40 for "(*) " + space
            break;

        case MENU_LABEL:
            // Draw invisible (*) for alignment, then label text
            canvas.setTextColor(0); // White (invisible)
            canvas.setTextDatum(CL_DATUM);
            canvas.drawString("(*)", checkboxX, y + UI_LINE_HEIGHT/2);
            canvas.setTextColor(15); // Back to black
            canvas.drawString(item.label, checkboxX + 40, y + UI_LINE_HEIGHT/2); // +40 for "(*) " + space
            break;

        case MENU_RADIO:
            // Draw radio button + label with space
            drawRadioButton(checkboxX, y, item.selected);
            canvas.setTextDatum(CL_DATUM);
            canvas.drawString(item.label, checkboxX + 40, y + UI_LINE_HEIGHT/2); // +40 for "(*) " + space
            break;

        case MENU_CHECKBOX: {
            // Draw checkbox + label with space
            drawCheckbox(checkboxX, y, item.selected);
            canvas.setTextDatum(CL_DATUM);
            // Use pre-computed displayLabel if available, otherwise use label
            String textToDisplay = item.displayLabel.isEmpty() ? item.label : item.displayLabel;
            canvas.drawString(textToDisplay, checkboxX + 40, y + UI_LINE_HEIGHT/2); // +40 for "(*) " + space
            break;
        }

        case MENU_SEPARATOR:
            // Empty space (30px height) - nothing to draw
            break;

        case MENU_PAGE_NAV:
            // Draw "<<<" or ">>>" aligned left like fonts (with invisible marker for alignment)
            canvas.setTextColor(0); // White (invisible)
            canvas.setTextDatum(CL_DATUM);
            canvas.drawString("(*)", checkboxX, y + UI_LINE_HEIGHT/2);
            canvas.setTextColor(15); // Back to black
            // item.value: -1 = previous page (<<<), 1 = next page (>>>)
            String navSymbol = (item.value == -1) ? "<<<" : ">>>";
            canvas.drawString(navSymbol, checkboxX + 40, y + UI_LINE_HEIGHT/2); // +40 for "(*) " + space
            break;
    }
}

// UI Helper: Reset menu refresh counters (call before first menu screen)
void resetMenuRefresh() {
    // Force a full refresh on next render by triggering the condition
    // This is done by making static variables accessible via a reset function
    // We'll handle this inline in the render function with a static firstRun flag
}

// UI Helper: Render full menu screen with smart refresh
void renderMenuScreen(const String& title, std::vector<MenuItem>& items, int cursorIndex, int scrollOffset, bool forceFullRefresh = false) {
    // Static variables for smart refresh tracking (menu-specific)
    static uint8_t menuPartialCount = 0;
    static unsigned long menuFirstPartialTime = 0;
    static bool menuHasPartial = false;

    canvas.fillCanvas(0); // White background
    canvas.setTextSize(UI_TEXT_SIZE);
    canvas.setTextColor(15); // Black text

    // Draw title at top (if provided)
    if (title.length() > 0) {
        canvas.setTextDatum(TC_DATUM);
        canvas.drawString(title, 270, 30);
    }

    // Calculate visible items (max items that fit on screen)
    int maxVisibleItems = 8; // Conservative estimate for 540x960 screen
    int startIdx = scrollOffset;
    int endIdx = min((int)items.size(), scrollOffset + maxVisibleItems);

    // Draw visible items
    for (int i = startIdx; i < endIdx; i++) {
        int relativeIdx = i - scrollOffset;
        int y = UI_TOP_MARGIN + relativeIdx * UI_LINE_HEIGHT;
        bool isCursor = (i == cursorIndex);
        drawMenuItem(y, items[i], isCursor);
    }

    // Smart refresh logic (same as main program)
    if (forceFullRefresh) {
        // Force full refresh (e.g., first screen after boot)
        canvas.pushCanvas(0, 0, UPDATE_MODE_GC16);
        menuPartialCount = 0;
        menuHasPartial = false;
        Serial.println("Menu forced full refresh");
    } else {
        // Normal partial refresh
        canvas.pushCanvas(0, 0, UPDATE_MODE_GL16);

        // Track first partial after full
        if (!menuHasPartial) {
            menuFirstPartialTime = millis();
            menuHasPartial = true;
        }

        menuPartialCount++;

        // Trigger full refresh if:
        // A) 5 partials reached, OR
        // B) 10 seconds passed since FIRST partial
        unsigned long timeSinceFirstPartial = millis() - menuFirstPartialTime;
        if (menuPartialCount >= 5 ||
            (menuHasPartial && timeSinceFirstPartial >= 10000)) {

            Serial.printf("Menu full refresh (count=%d, time=%lums)\n", menuPartialCount, timeSinceFirstPartial);
            M5.EPD.UpdateFull(UPDATE_MODE_GC16);

            // Reset counters
            menuPartialCount = 0;
            menuHasPartial = false;
        }
    }
}

// ========================================
// v2.2: Unicode Range Customization
// ========================================

// Forward declarations
extern std::vector<String> fontPaths;
String getFontName(const String& path); // Already defined earlier in the file

// Build menu items for current page
void buildUnifiedMenu(std::vector<MenuItem>& items, PaginationState& pagination,
                      int selectedInterval, const std::vector<bool>& fontEnabled,
                      bool allowDifferentFont, bool allowDifferentMode) {
    items.clear();

    // 1. Confirm button (always first)
    items.push_back({MENU_CONFIRM, "Confirm", "", false, 0, -1});

    // v2.2: Customize Unicode ranges (navigable button)
    items.push_back({MENU_LABEL, "Customize Unicode ranges", "", false, 0, -5}); // fontIndex=-5 for Unicode ranges page

    // 2. Separator (30px)
    items.push_back({MENU_SEPARATOR, "", "", false, 0, -1});

    // 3. "Refresh timer" label (non-selectable)
    items.push_back({MENU_LABEL, "Refresh timer", "", false, 0, -1});

    // 4. Radio buttons for intervals (different in debug mode)
    if (rtcState.debugMode) {
        // Debug mode: shorter intervals for testing (1, 2, 5 min)
        items.push_back({MENU_RADIO, "5 min", "", selectedInterval == 5, 5, -1});
        items.push_back({MENU_RADIO, "2 min", "", selectedInterval == 2, 2, -1});
        items.push_back({MENU_RADIO, "1 min", "", selectedInterval == 1, 1, -1});
    } else {
        // Normal mode: standard intervals (5, 10, 15 min)
        items.push_back({MENU_RADIO, "15 min", "", selectedInterval == 15, 15, -1});
        items.push_back({MENU_RADIO, "10 min", "", selectedInterval == 10, 10, -1});
        items.push_back({MENU_RADIO, "5 min", "", selectedInterval == 5, 5, -1});
    }

    // 5. Separator (30px)
    items.push_back({MENU_SEPARATOR, "", "", false, 0, -1});

    // 6. "When in standby" label (non-selectable)
    items.push_back({MENU_LABEL, "When in standby", "", false, 0, -1});

    // 7. Checkboxes for standby behavior
    items.push_back({MENU_CHECKBOX, "Allow different font", "", allowDifferentFont, 0, -3}); // fontIndex=-3 for allowDifferentFont
    items.push_back({MENU_CHECKBOX, "Allow different mode", "", allowDifferentMode, 0, -4}); // fontIndex=-4 for allowDifferentMode

    // 8. Separator (30px)
    items.push_back({MENU_SEPARATOR, "", "", false, 0, -1});

    // 9. Font selection with pagination
    // Strategy: Always show "Select/Deselect all" on every page
    // Max 5 total items in font section: Select/Deselect all + up to 5 slots for fonts/dots

    int totalFonts = fontPaths.size();

    // Calculate fonts per page based on position
    // Page 0: Select/Deselect all + up to 5 fonts (if ≤5 total, show all; otherwise show 4 + ">>>")
    // Page 1+: Select/Deselect all + "<<<" + 3 fonts (+ ">>>" if more pages)

    int fontsThisPage;
    int startFont;

    if (pagination.currentPage == 0) {
        // First page: can show up to 5 fonts if ≤5 total, otherwise 4 + ">>>"
        if (totalFonts <= 5) {
            // Show all fonts in one page
            fontsThisPage = totalFonts;
            startFont = 0;
            pagination.totalPages = 1;
        } else {
            // Show 4 fonts, reserve space for ">>>"
            fontsThisPage = 4;
            startFont = 0;
            // Calculate total pages: first page 4 fonts, subsequent pages 3 fonts each
            int remainingFonts = totalFonts - 4;
            pagination.totalPages = 1 + (remainingFonts + 2) / 3; // Ceiling division
        }
    } else {
        // Subsequent pages: "<<<" at top, then up to 3 fonts
        startFont = 4 + (pagination.currentPage - 1) * 3;
        int remainingFonts = totalFonts - startFont;
        fontsThisPage = min(3, remainingFonts);
    }

    int endFont = startFont + fontsThisPage;

    // Select/Deselect all (ALWAYS first in font section, on every page)
    bool allSelected = true;
    for (bool enabled : fontEnabled) {
        if (!enabled) {
            allSelected = false;
            break;
        }
    }
    items.push_back({MENU_LABEL, "Select/Deselect all", "", allSelected, 0, -2}); // fontIndex=-2 special, clickable but no marker

    // "..." prev page navigation (if not on first page) - AFTER Select/Deselect all
    if (pagination.currentPage > 0) {
        items.push_back({MENU_PAGE_NAV, "...", "", false, -1, -1}); // value=-1 means prev
    }

    // Font checkboxes for current page - PRE-COMPUTE displayLabel to avoid recalculation on every cursor move
    for (int i = startFont; i < endFont; i++) {
        String fontName = getFontName(fontPaths[i]);
        String displayName = shortenTextIfNeeded(fontName, UI_MAX_TEXT_WIDTH);
        items.push_back({MENU_CHECKBOX, fontName, displayName, fontEnabled[i], 0, i});
    }

    // "..." next page navigation (if more pages available)
    if (pagination.currentPage < pagination.totalPages - 1) {
        items.push_back({MENU_PAGE_NAV, "...", "", false, 1, -1}); // value=1 means next
    }
}

// Render unified setup screen with fixed headers/footers
void renderUnifiedSetupScreen(std::vector<MenuItem>& items, int cursorIndex, bool forceFullRefresh = false) {
    static uint8_t menuPartialCount = 0;
    static unsigned long menuFirstPartialTime = 0;
    static bool menuHasPartial = false;

    canvas.fillCanvas(0);
    canvas.setTextSize(UI_TEXT_SIZE);
    canvas.setTextColor(15);

    // Fixed header: "PaperSpecimen" at Y=30
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString("PaperSpecimen", 270, 30);

    // Fixed footer: "v2.2.3" (or "v2.2.3*" in debug mode) at Y=930
    canvas.setTextDatum(BC_DATUM);
    canvas.drawString(rtcState.debugMode ? "v2.2.3*" : "v2.2.3", 270, 930);

    // Calculate available space for menu items
    const int headerBottom = 30 + 24; // Y=30 + text height
    const int footerTop = 930 - 24;   // Y=930 - text height
    const int availableHeight = footerTop - headerBottom;

    // Calculate flex space (divide remaining space into 2 equal parts)
    int totalItemHeight = items.size() * UI_LINE_HEIGHT;
    int flexSpace = max(0, (availableHeight - totalItemHeight) / 2);

    // Draw menu items starting after header + flex space
    int startY = headerBottom + flexSpace;

    for (int i = 0; i < items.size(); i++) {
        int y = startY + i * UI_LINE_HEIGHT;
        bool isCursor = (i == cursorIndex);
        drawMenuItem(y, items[i], isCursor);
    }

    // Smart refresh
    if (forceFullRefresh) {
        canvas.pushCanvas(0, 0, UPDATE_MODE_GC16);
        menuPartialCount = 0;
        menuHasPartial = false;
    } else {
        canvas.pushCanvas(0, 0, UPDATE_MODE_A2); // Anti-aliased partial refresh
        if (!menuHasPartial) {
            menuFirstPartialTime = millis();
            menuHasPartial = true;
        }
        menuPartialCount++;
        unsigned long timeSinceFirstPartial = millis() - menuFirstPartialTime;
        if (menuPartialCount >= 5 || (menuHasPartial && timeSinceFirstPartial >= 10000)) {
            M5.EPD.UpdateFull(UPDATE_MODE_GC16);
            menuPartialCount = 0;
            menuHasPartial = false;
        }
    }
}

// v2.2: Unicode Ranges Configuration Screen
void setupUnicodeRanges() {
    Serial.println("\n=== Unicode Ranges Configuration ===");

    // Local copy of range flags
    std::vector<bool> rangeEnabledLocal = config.rangeEnabled;

    // Ensure rangeEnabledLocal has correct size
    if (rangeEnabledLocal.size() != numGlyphRanges) {
        rangeEnabledLocal.clear();
        for (int i = 0; i < numGlyphRanges; i++) {
            rangeEnabledLocal.push_back(i < 6); // First 6 enabled by default
        }
    }

    int cursorIndex = 0;
    int currentPage = 0;
    const int rangesPerPage = 14; // 14 ranges per page
    const int totalPages = (numGlyphRanges + rangesPerPage - 1) / rangesPerPage;

    // Build initial menu
    std::vector<MenuItem> items;

    auto rebuildMenu = [&]() {
        items.clear();
        items.push_back({MENU_CONFIRM, "Confirm", "", false, 0, -1});

        // Select/Deselect all (clickable label with fontIndex=-2, like in main config)
        bool allSelected = true;
        for (bool enabled : rangeEnabledLocal) {
            if (!enabled) {
                allSelected = false;
                break;
            }
        }
        items.push_back({MENU_LABEL, "Select/Deselect all", "", allSelected, 0, -2});

        items.push_back({MENU_SEPARATOR, "", "", false, 0, -1});

        int startRange = currentPage * rangesPerPage;
        int endRange = min(startRange + rangesPerPage, numGlyphRanges);

        if (currentPage > 0) {
            items.push_back({MENU_PAGE_NAV, "<<<", "", false, -1, -1});
        }

        for (int i = startRange; i < endRange; i++) {
            items.push_back({MENU_CHECKBOX, glyphRanges[i].name, "", rangeEnabledLocal[i], 0, i});
        }

        if (currentPage < totalPages - 1) {
            items.push_back({MENU_PAGE_NAV, ">>>", "", false, 1, -1});
        }
    };

    rebuildMenu();

    // Initial render with full refresh
    renderUnifiedSetupScreen(items, cursorIndex, true);

    bool exitRangesScreen = false;

    while (!exitRangesScreen) {
        // Wait for button press
        M5.update();
        delay(50);

        if (M5.BtnL.wasPressed()) {
            // Move cursor up
            do {
                cursorIndex--;
                if (cursorIndex < 0) cursorIndex = items.size() - 1;
                // Skip separators and non-clickable labels (except fontIndex=-2 for Select/Deselect all)
            } while (items[cursorIndex].type == MENU_SEPARATOR ||
                     (items[cursorIndex].type == MENU_LABEL && items[cursorIndex].fontIndex != -2));
            renderUnifiedSetupScreen(items, cursorIndex, false);
        }

        if (M5.BtnR.wasPressed()) {
            // Move cursor down
            do {
                cursorIndex++;
                if (cursorIndex >= items.size()) cursorIndex = 0;
                // Skip separators and non-clickable labels (except fontIndex=-2 for Select/Deselect all)
            } while (items[cursorIndex].type == MENU_SEPARATOR ||
                     (items[cursorIndex].type == MENU_LABEL && items[cursorIndex].fontIndex != -2));
            renderUnifiedSetupScreen(items, cursorIndex, false);
        }

        if (M5.BtnP.wasPressed()) {
            MenuItem& currentItem = items[cursorIndex];

            if (currentItem.type == MENU_CONFIRM) {
                // Save and return to main config
                config.rangeEnabled = rangeEnabledLocal;
                Serial.println("Unicode ranges configured");
                exitRangesScreen = true;

            } else if (currentItem.type == MENU_LABEL && currentItem.fontIndex == -2) {
                // "Select/Deselect all" - update all range checkboxes without rebuilding menu
                bool newState = !currentItem.selected;
                for (size_t i = 0; i < rangeEnabledLocal.size(); i++) {
                    rangeEnabledLocal[i] = newState;
                }
                // Update selected state of all range checkboxes AND the Select/Deselect all item itself
                for (auto& item : items) {
                    if (item.type == MENU_CHECKBOX) {
                        item.selected = newState;
                    } else if (item.type == MENU_LABEL && item.fontIndex == -2) {
                        item.selected = newState;
                    }
                }
                Serial.printf("Select/Deselect all ranges: %s\n", newState ? "all selected" : "all deselected");
                renderUnifiedSetupScreen(items, cursorIndex, false);

            } else if (currentItem.type == MENU_CHECKBOX) {
                // Toggle range - update without rebuilding menu
                int rangeIndex = currentItem.fontIndex;
                rangeEnabledLocal[rangeIndex] = !rangeEnabledLocal[rangeIndex];
                currentItem.selected = rangeEnabledLocal[rangeIndex];
                Serial.printf("Toggled range %d (%s): %s\n",
                             rangeIndex, glyphRanges[rangeIndex].name,
                             rangeEnabledLocal[rangeIndex] ? "enabled" : "disabled");

                // Update "Select/Deselect all" state based on whether all ranges are now selected
                bool allSelected = true;
                for (bool enabled : rangeEnabledLocal) {
                    if (!enabled) {
                        allSelected = false;
                        break;
                    }
                }
                for (auto& item : items) {
                    if (item.type == MENU_LABEL && item.fontIndex == -2) {
                        item.selected = allSelected;
                        break;
                    }
                }
                renderUnifiedSetupScreen(items, cursorIndex, false);

            } else if (currentItem.type == MENU_PAGE_NAV) {
                // Page navigation
                if (currentItem.value == -1) {
                    // Previous page
                    currentPage--;
                } else {
                    // Next page
                    currentPage++;
                }
                rebuildMenu();

                // Position cursor on first range checkbox
                // Structure: Confirm(0), Select/Deselect(1), Sep(2), [<<<(3)], ranges...
                cursorIndex = 3; // Start at separator + 1
                if (currentPage > 0) {
                    cursorIndex = 4; // Skip "<<<" navigation item
                }

                renderUnifiedSetupScreen(items, cursorIndex, false);
            }

            delay(200);
        }
    }

    Serial.println("=== Unicode Ranges Configuration Complete ===\n");
}

// Unified setup screen - combines interval and font selection
void setupScreenUnified() {
    Serial.println("\n=== Unified Setup Screen ===");

    // Initialize state
    // Default interval depends on debug mode
    int selectedInterval = rtcState.debugMode ? 1 : 15; // Debug: 1 min, Normal: 15 min
    std::vector<bool> fontEnabledLocal = config.fontEnabled; // All fonts enabled by default
    bool allowDifferentFontLocal = config.allowDifferentFont;
    bool allowDifferentModeLocal = config.allowDifferentMode;

    PaginationState pagination = {0, 1, 4}; // page 0, will calculate totalPages
    std::vector<MenuItem> items;

    // Build initial menu
    buildUnifiedMenu(items, pagination, selectedInterval, fontEnabledLocal, allowDifferentFontLocal, allowDifferentModeLocal);

    int cursorIndex = 0; // Start at "Confirm"

    // Initial render with full refresh
    renderUnifiedSetupScreen(items, cursorIndex, true);

    // Auto-confirm timeout: 60 seconds of inactivity
    unsigned long lastActivityTime = millis();
    const unsigned long AUTO_CONFIRM_TIMEOUT = 60000; // 60 seconds

    // Navigation loop
    bool confirmed = false;
    while (!confirmed) {
        M5.update();

        // Check for auto-confirm timeout (60 seconds without button press)
        if (millis() - lastActivityTime >= AUTO_CONFIRM_TIMEOUT) {
            Serial.println("Auto-confirm: 60 seconds timeout - proceeding with current settings");
            confirmed = true;

            // Apply fallback if no fonts selected
            bool anyFontSelected = false;
            for (bool enabled : fontEnabledLocal) {
                if (enabled) {
                    anyFontSelected = true;
                    break;
                }
            }
            if (!anyFontSelected) {
                Serial.println("No fonts selected - enabling all fonts as fallback");
                for (size_t i = 0; i < fontEnabledLocal.size(); i++) {
                    fontEnabledLocal[i] = true;
                }
            }

            config.wakeIntervalMinutes = selectedInterval;
            config.fontEnabled = fontEnabledLocal;
            break; // Exit loop
        }

        // Button UP (BtnL): Move cursor up
        if (M5.BtnL.wasPressed()) {
            lastActivityTime = millis(); // Reset timeout
            Serial.println("BtnL pressed - moving up");
            do {
                cursorIndex--;
                if (cursorIndex < 0) {
                    cursorIndex = items.size() - 1; // Wrap to last item
                }
                // Skip separators and non-clickable labels (but not "Select/Deselect all" fontIndex=-2 or "Customize Unicode ranges" fontIndex=-5)
            } while (items[cursorIndex].type == MENU_SEPARATOR ||
                     (items[cursorIndex].type == MENU_LABEL && items[cursorIndex].fontIndex != -2 && items[cursorIndex].fontIndex != -5));
            renderUnifiedSetupScreen(items, cursorIndex);
            delay(200);
        }

        // Button DOWN (BtnR): Move cursor down
        if (M5.BtnR.wasPressed()) {
            lastActivityTime = millis(); // Reset timeout
            Serial.println("BtnR pressed - moving down");
            do {
                cursorIndex++;
                if (cursorIndex >= items.size()) {
                    cursorIndex = 0; // Wrap to Confirm
                }
                // Skip separators and non-clickable labels (but not "Select/Deselect all" fontIndex=-2 or "Customize Unicode ranges" fontIndex=-5)
            } while (items[cursorIndex].type == MENU_SEPARATOR ||
                     (items[cursorIndex].type == MENU_LABEL && items[cursorIndex].fontIndex != -2 && items[cursorIndex].fontIndex != -5));
            renderUnifiedSetupScreen(items, cursorIndex);
            delay(200);
        }

        // Button CENTER (BtnP): Select/Confirm
        if (M5.BtnP.wasPressed()) {
            lastActivityTime = millis(); // Reset timeout
            MenuItem& currentItem = items[cursorIndex];

            if (currentItem.type == MENU_CONFIRM) {
                // Confirm pressed - check if at least one font is selected
                bool anyFontSelected = false;
                for (bool enabled : fontEnabledLocal) {
                    if (enabled) {
                        anyFontSelected = true;
                        break;
                    }
                }

                // Fallback: if no fonts selected, select all
                if (!anyFontSelected) {
                    Serial.println("No fonts selected - enabling all fonts as fallback");
                    for (size_t i = 0; i < fontEnabledLocal.size(); i++) {
                        fontEnabledLocal[i] = true;
                    }
                }

                // Save and exit
                confirmed = true;
                config.wakeIntervalMinutes = selectedInterval;
                config.fontEnabled = fontEnabledLocal;
                config.allowDifferentFont = allowDifferentFontLocal;
                config.allowDifferentMode = allowDifferentModeLocal;
                Serial.printf("Setup confirmed: %d min, allow font=%s, allow mode=%s, fonts configured\n",
                             selectedInterval,
                             config.allowDifferentFont ? "yes" : "no",
                             config.allowDifferentMode ? "yes" : "no");

            } else if (currentItem.type == MENU_RADIO) {
                // Radio button: update selected state without rebuilding menu
                selectedInterval = currentItem.value;
                // Update only the selected field of radio buttons
                for (auto& item : items) {
                    if (item.type == MENU_RADIO) {
                        item.selected = (item.value == selectedInterval);
                    }
                }
                Serial.printf("Selected interval: %d min\n", selectedInterval);
                renderUnifiedSetupScreen(items, cursorIndex);

            } else if (currentItem.type == MENU_LABEL && currentItem.fontIndex == -5) {
                // v2.2: "Customize Unicode ranges" - open Unicode ranges configuration
                Serial.println("Opening Unicode ranges configuration...");
                setupUnicodeRanges();
                // Return to main config, rebuild menu
                buildUnifiedMenu(items, pagination, selectedInterval, fontEnabledLocal, allowDifferentFontLocal, allowDifferentModeLocal);
                renderUnifiedSetupScreen(items, cursorIndex, true);
                lastActivityTime = millis(); // Reset timeout

            } else if (currentItem.type == MENU_LABEL && currentItem.fontIndex == -2) {
                // "Select/Deselect all" - update all font checkboxes without rebuilding menu
                bool newState = !currentItem.selected;
                for (size_t i = 0; i < fontEnabledLocal.size(); i++) {
                    fontEnabledLocal[i] = newState;
                }
                // Update selected state of all font checkboxes AND the Select/Deselect all item itself
                for (auto& item : items) {
                    if (item.type == MENU_CHECKBOX && item.fontIndex >= 0) {
                        item.selected = newState;
                    } else if (item.type == MENU_LABEL && item.fontIndex == -2) {
                        item.selected = newState;
                    }
                }
                Serial.printf("Select/Deselect all: %s\n", newState ? "all selected" : "all deselected");
                renderUnifiedSetupScreen(items, cursorIndex);

            } else if (currentItem.type == MENU_CHECKBOX) {
                if (currentItem.fontIndex == -2) {
                    // "Select/Deselect all" special checkbox (deprecated, now using MENU_LABEL)
                    bool newState = !currentItem.selected;
                    for (size_t i = 0; i < fontEnabledLocal.size(); i++) {
                        fontEnabledLocal[i] = newState;
                    }
                    Serial.printf("Select/Deselect all: %s\n", newState ? "all selected" : "all deselected");
                } else if (currentItem.fontIndex == -3) {
                    // "Allow different font" checkbox - toggle without rebuilding menu
                    allowDifferentFontLocal = !allowDifferentFontLocal;
                    currentItem.selected = allowDifferentFontLocal;
                    Serial.printf("Toggled allow different font: %s\n", allowDifferentFontLocal ? "yes" : "no");
                } else if (currentItem.fontIndex == -4) {
                    // "Allow different mode" checkbox - toggle without rebuilding menu
                    allowDifferentModeLocal = !allowDifferentModeLocal;
                    currentItem.selected = allowDifferentModeLocal;
                    Serial.printf("Toggled allow different mode: %s\n", allowDifferentModeLocal ? "yes" : "no");
                } else if (currentItem.fontIndex >= 0) {
                    // Individual font checkbox - toggle without rebuilding menu
                    fontEnabledLocal[currentItem.fontIndex] = !fontEnabledLocal[currentItem.fontIndex];
                    currentItem.selected = fontEnabledLocal[currentItem.fontIndex];
                    Serial.printf("Toggled font %d: %s\n", currentItem.fontIndex,
                                 fontEnabledLocal[currentItem.fontIndex] ? "enabled" : "disabled");

                    // Update "Select/Deselect all" state based on whether all fonts are now selected
                    bool allSelected = true;
                    for (bool enabled : fontEnabledLocal) {
                        if (!enabled) {
                            allSelected = false;
                            break;
                        }
                    }
                    for (auto& item : items) {
                        if (item.type == MENU_LABEL && item.fontIndex == -2) {
                            item.selected = allSelected;
                            break;
                        }
                    }
                }
                renderUnifiedSetupScreen(items, cursorIndex);

            } else if (currentItem.type == MENU_PAGE_NAV) {
                // Page navigation "..."
                if (currentItem.value == -1) {
                    // Previous page
                    pagination.currentPage--;
                } else {
                    // Next page
                    pagination.currentPage++;
                }
                buildUnifiedMenu(items, pagination, selectedInterval, fontEnabledLocal, allowDifferentFontLocal, allowDifferentModeLocal);

                // Position cursor on first item AFTER "Select/Deselect all" in font section
                // Items: Confirm(0), Sep, Refresh, 15min, 10min, 5min, Sep, When, AllowFont, AllowMode, Sep, Select/Deselect(11), ...
                // So first item after Select/Deselect is always at index 12
                cursorIndex = 12;

                Serial.printf("Page navigation: now on page %d/%d, cursor at item %d\n", pagination.currentPage + 1, pagination.totalPages, cursorIndex);
                renderUnifiedSetupScreen(items, cursorIndex);
            }

            delay(200);
        }

        delay(50);
    }

    Serial.println("=== Unified Setup Complete ===\n");
}

// ========================================
// v2.1: Setup Screen - Interval Selection (OLD - DEPRECATED)
// ========================================

// Interactive interval selection screen
// Returns: selected interval in minutes (5, 10, or 15)
int setupScreenIntervalSelection() {
    Serial.println("\n=== Setup Screen: Interval Selection ===");

    // Build menu items
    std::vector<MenuItem> items;

    // Item 0: Confirm button
    items.push_back({MENU_CONFIRM, "Confirm", "", false, 0, -1});

    // Items 1-3: Radio buttons for intervals (different in debug mode)
    if (rtcState.debugMode) {
        // Debug mode: shorter intervals for testing (5, 2, 1 min)
        items.push_back({MENU_RADIO, "5 min", "", false, 5, -1});
        items.push_back({MENU_RADIO, "2 min", "", false, 2, -1});
        items.push_back({MENU_RADIO, "1 min", "", true, 1, -1}); // Default: 1 min
    } else {
        // Normal mode: standard intervals (15, 10, 5 min)
        items.push_back({MENU_RADIO, "5 min", "", false, 5, -1});
        items.push_back({MENU_RADIO, "10 min", "", false, 10, -1});
        items.push_back({MENU_RADIO, "15 min", "", true, 15, -1}); // Default: 15 min
    }

    int cursorIndex = 0; // Start at "Confirm"
    int scrollOffset = 0; // No scroll needed (only 4 items)
    int selectedIntervalIndex = 3; // Default interval (last option)

    // Initial render with full refresh to clear boot screen
    renderMenuScreen("", items, cursorIndex, scrollOffset, true);

    // Navigation loop
    bool confirmed = false;
    while (!confirmed) {
        M5.update();

        // Button UP (BtnL): Move cursor up
        if (M5.BtnL.wasPressed()) {
            Serial.println("BtnL pressed - moving up");
            cursorIndex--;
            if (cursorIndex < 0) {
                cursorIndex = items.size() - 1; // Wrap around to bottom
            }
            renderMenuScreen("", items, cursorIndex, scrollOffset);
            delay(200);
        }

        // Button DOWN (BtnR): Move cursor down
        if (M5.BtnR.wasPressed()) {
            Serial.println("BtnR pressed - moving down");
            cursorIndex++;
            if (cursorIndex >= items.size()) {
                cursorIndex = 0; // Wrap around to top
            }
            renderMenuScreen("", items, cursorIndex, scrollOffset);
            delay(200);
        }

        // Button CENTER (BtnP): Select/Confirm
        if (M5.BtnP.wasPressed()) {
            if (cursorIndex == 0) {
                // Confirm button pressed - exit
                confirmed = true;
                Serial.println("Confirm pressed - exiting interval selection");
            } else {
                // Radio button pressed - deselect all, select current
                for (int i = 1; i < items.size(); i++) {
                    items[i].selected = false;
                }
                items[cursorIndex].selected = true;
                selectedIntervalIndex = cursorIndex;
                renderMenuScreen("", items, cursorIndex, scrollOffset);
                Serial.printf("Selected: %s\n", items[cursorIndex].label.c_str());
            }
            delay(200);
        }

        delay(50);
    }

    int selectedInterval = items[selectedIntervalIndex].value;
    Serial.printf("Interval selection confirmed: %d minutes\n", selectedInterval);
    return selectedInterval;
}

// ========================================
// v2.1: Setup Screen - Font Selection
// ========================================

// Forward declarations needed for font selection
extern std::vector<String> fontPaths;

// Extract font name from path
String getFontName(const String& path) {
    int lastSlash = path.lastIndexOf('/');
    String filename = path.substring(lastSlash + 1);
    int lastDot = filename.lastIndexOf('.');
    if (lastDot > 0) {
        filename = filename.substring(0, lastDot);
    }
    return filename;
}

// Interactive font selection screen with scroll
// Modifies config.fontEnabled based on user selection
void setupScreenFontSelection() {
    Serial.println("\n=== Setup Screen: Font Selection ===");

    // Build menu items
    std::vector<MenuItem> items;

    // Item 0: Confirm button
    items.push_back({MENU_CONFIRM, "Confirm", "", false, 0, -1});

    // Items 1..N: Checkboxes for each font (all enabled by default)
    for (size_t i = 0; i < fontPaths.size(); i++) {
        // Extract font name from path
        String fontName = getFontName(fontPaths[i]);
        String displayName = shortenTextIfNeeded(fontName, UI_MAX_TEXT_WIDTH);
        bool enabled = config.fontEnabled[i];
        items.push_back({MENU_CHECKBOX, fontName, displayName, enabled, 0, i});
    }

    int cursorIndex = 0; // Start at "Confirm"
    int scrollOffset = 0;
    const int maxVisibleItems = 8; // Max items visible on screen
    bool firstMove = true; // Flag to detect first cursor move

    // Initial render with full refresh to clear previous screen
    renderMenuScreen("", items, cursorIndex, scrollOffset, true);

    // Navigation loop
    bool confirmed = false;
    while (!confirmed) {
        M5.update();

        bool cursorMoved = false;

        // Button UP (BtnL): Move cursor up
        if (M5.BtnL.wasPressed()) {
            Serial.println("BtnL pressed - moving up");

            // First move away from Confirm: deselect all fonts
            if (firstMove && cursorIndex == 0) {
                Serial.println("First move from Confirm - deselecting all fonts");
                for (size_t i = 1; i < items.size(); i++) {
                    items[i].selected = false;
                }
                firstMove = false;
            }

            cursorIndex--;
            if (cursorIndex < 0) {
                cursorIndex = items.size() - 1; // Wrap to last item
            }
            cursorMoved = true;
            delay(200);
        }

        // Button DOWN (BtnR): Move cursor down
        if (M5.BtnR.wasPressed()) {
            Serial.println("BtnR pressed - moving down");

            // First move away from Confirm: deselect all fonts
            if (firstMove && cursorIndex == 0) {
                Serial.println("First move from Confirm - deselecting all fonts");
                for (size_t i = 1; i < items.size(); i++) {
                    items[i].selected = false;
                }
                firstMove = false;
            }

            cursorIndex++;
            // Wrap around: if at last item, go back to Confirm
            if (cursorIndex >= items.size()) {
                cursorIndex = 0;
            }

            cursorMoved = true;
            delay(200);
        }

        // Update scroll offset if cursor moved
        if (cursorMoved) {
            // Auto-scroll logic: keep cursor in visible area
            if (cursorIndex < scrollOffset) {
                // Cursor moved above visible area - scroll up
                scrollOffset = cursorIndex;
            } else if (cursorIndex >= scrollOffset + maxVisibleItems) {
                // Cursor moved below visible area - scroll down
                scrollOffset = cursorIndex - maxVisibleItems + 1;
            }

            renderMenuScreen("", items, cursorIndex, scrollOffset);
        }

        // Button CENTER (BtnP): Toggle checkbox or confirm
        if (M5.BtnP.wasPressed()) {
            if (cursorIndex == 0) {
                // Confirm button pressed - save and exit
                confirmed = true;
                Serial.println("Confirm pressed - saving font selections");

                // Update config.fontEnabled from items
                for (size_t i = 0; i < fontPaths.size(); i++) {
                    config.fontEnabled[i] = items[i + 1].selected; // +1 because item 0 is Confirm
                }

                // Count enabled fonts
                int enabledCount = 0;
                for (bool enabled : config.fontEnabled) {
                    if (enabled) enabledCount++;
                }
                Serial.printf("Fonts enabled: %d/%d\n", enabledCount, fontPaths.size());

            } else {
                // Checkbox pressed - toggle
                items[cursorIndex].selected = !items[cursorIndex].selected;
                renderMenuScreen("", items, cursorIndex, scrollOffset);
                Serial.printf("Toggled: %s -> %s\n",
                             items[cursorIndex].label.c_str(),
                             items[cursorIndex].selected ? "enabled" : "disabled");
            }
            delay(200);
        }

        delay(50);
    }

    Serial.println("=== Font Selection Complete ===\n");
}

// ========================================
// FT_Face Access Workaround
// ========================================
// M5EPD_Canvas inherits from TFT_eSPI which has protected static _font_face
// We use external symbol access to bypass protected access control

// External declarations to access TFT_eSPI protected statics
// This is a memory hack but works without modifying the library
extern "C" {
    // These symbols exist in TFT_eSPI compiled code
    // We're just accessing them directly via linker
}

// Helper struct matching font_face_t from font_render.h
struct font_face_accessor {
    FT_Face ft_face;
    uint16_t pixel_size;
};

// Access function using symbol name mangling knowledge
FT_Face getFontFaceFromCanvas() {
    // TFT_eSPI::_font_face is a static member
    // We can access it via external linkage using the mangled name
    // But this is compiler-specific and fragile

    // ALTERNATIVE: Use the canvas object directly as a memory accessor
    // Cast canvas to access its parent class protected members
    // This works because M5EPD_Canvas IS-A TFT_eSPI

    // SIMPLEST WORKAROUND: Access via canvas's internal methods
    // The font is already loaded, we just need the FT_Face pointer

    // For now, use a friend class hack via template specialization
    // Actually, let's try the reinterpret_cast approach

    // HACK: Get pointer to TFT_eSPI's static _font_face via memory layout
    // This assumes the symbol is accessible via linker (it should be)

    // Import the external symbol (defined in TFT_eSPI compiled code)
    extern font_face_t _ZN8TFT_eSPI10_font_faceE; // mangled name for TFT_eSPI::_font_face

    return _ZN8TFT_eSPI10_font_faceE.ft_face;
}

// Refresh logic state
uint8_t partialRefreshCount = 0;
unsigned long firstPartialAfterFullTime = 0;
bool hasPartialSinceLastFull = false;
const uint8_t MAX_PARTIAL_BEFORE_FULL = 5;
const unsigned long FULL_REFRESH_TIMEOUT_MS = 10000; // 10 seconds from first partial

// Power management state
unsigned long lastFullRefreshTime = 0;
unsigned long lastButtonActivityTime = 0;
const unsigned long DEEP_SLEEP_TIMEOUT_MS = 10000; // 10 seconds from last full refresh
bool isAutoWakeSession = false; // Flag: if true, skip idle wait and sleep immediately after render

// View mode enumeration
// Font management
std::vector<String> fontPaths;
int currentFontIndex = 0;
bool fontLoaded = false;

// Glyph rendering
const int GLYPH_SIZE = 375; // Balanced size for specimen display
uint32_t currentGlyphCodepoint = 0x0041; // Start with 'A'

// STEP 5: Current view mode
ViewMode currentViewMode = BITMAP;  // Start in bitmap mode (default)

// Scan microSD for font files
void scanFonts() {
    fontPaths.clear();

    File fontsDir = SD.open("/fonts");
    if (!fontsDir) {
        Serial.println("ERROR: Cannot open /fonts directory");
        return;
    }

    Serial.println("\n=== Scanning for fonts ===");
    File entry;
    while (entry = fontsDir.openNextFile()) {
        if (!entry.isDirectory()) {
            String filename = String(entry.name());
            // Skip macOS hidden files (._*)
            if (filename.startsWith("._")) {
                Serial.printf("  Skipping hidden file: %s\n", filename.c_str());
                entry.close();
                continue;
            }
            if (filename.endsWith(".ttf") || filename.endsWith(".otf") ||
                filename.endsWith(".TTF") || filename.endsWith(".OTF")) {
                String fullPath = "/fonts/" + filename;
                fontPaths.push_back(fullPath);
                Serial.printf("  Found: %s\n", fullPath.c_str());
            }
        }
        entry.close();
    }
    fontsDir.close();

    Serial.printf("Total fonts found: %d\n", fontPaths.size());
}

// Load font at current index
bool loadCurrentFont() {
    if (fontPaths.empty()) {
        Serial.println("ERROR: No fonts available");
        return false;
    }

    // Unload previous font if loaded - IMPORTANT: destroy render first!
    if (fontLoaded) {
        Serial.println("Unloading previous font...");
        canvas.destoryRender(24);           // Free label cache
        canvas.unloadFont();                // Then unload font
        fontLoaded = false;
        delay(100);  // Give system time to free memory
    }

    // v2.2.1: Check if fontPaths is empty (SD not available but cache might be)
    String fontPath = "";
    if (currentFontIndex < fontPaths.size()) {
        fontPath = fontPaths[currentFontIndex];
        Serial.printf("\n=== Loading font: %s ===\n", fontPath.c_str());
    } else {
        Serial.printf("\n=== Loading font index %d (SD unavailable - cache only) ===\n", currentFontIndex);
    }

    // ========================================
    // Battery Optimization: Check font cache first
    // ========================================

    // 1. Check if this font is already cached in RAM
    for (auto& entry : fontCache) {
        if (entry.fontIndex == currentFontIndex) {
            Serial.printf("Cache HIT: Loading font %d from RAM (SD not needed!)\n", currentFontIndex);
            esp_err_t loadResult = canvas.loadFont(entry.data, entry.size);
            if (loadResult == ESP_OK) {
                Serial.printf("Font loaded from cache (%d bytes)\n", entry.size);
                // Move to end (LRU - most recently used)
                FontCacheEntry temp = entry;
                fontCache.erase(std::find_if(fontCache.begin(), fontCache.end(),
                    [&](const FontCacheEntry& e) { return e.fontIndex == currentFontIndex; }));
                fontCache.push_back(temp);

                // Create render cache for labels
                esp_err_t renderResult24 = canvas.createRender(24, 64);
                if (renderResult24 != ESP_OK) {
                    Serial.println("ERROR: Failed to create render cache for size 24");
                    canvas.unloadFont();
                    return false;
                }

                fontLoaded = true;
                Serial.println("=== Font loaded from cache successfully! ===\n");
                return true;
            } else {
                Serial.println("WARNING: Cache load failed, falling back to SD");
                // Continue to SD load below
                break;
            }
        }
    }

    // 2. Cache MISS - load from SD
    Serial.printf("RAM cache MISS for font %d\n", currentFontIndex);
    Serial.printf("Loading font %d from SD\n", currentFontIndex);

    // Check if SD is available and path is valid
    if (fontPath.isEmpty()) {
        Serial.println("ERROR: Font path is empty!");
        Serial.println("Cannot load font - device needs reset with SD card");
        return false;
    }

    // Check if file exists on SD
    if (!SD.exists(fontPath)) {
        Serial.println("ERROR: Font file does not exist on SD!");
        return false;
    }

    File fontFile = SD.open(fontPath);
    if (!fontFile) {
        Serial.println("ERROR: Cannot open font file!");
        return false;
    }
    size_t fontFileSize = fontFile.size();
    Serial.printf("Font file size: %d bytes\n", fontFileSize);

    // 3. Decide if we should cache this font
    bool shouldCache = (fontFileSize <= MAX_FONT_CACHE_SIZE);
    uint8_t* fontData = nullptr;

    if (shouldCache) {
        // 4. Make room in cache if needed (LRU eviction)
        while (totalCacheSize + fontFileSize > MAX_FONT_CACHE_SIZE && !fontCache.empty()) {
            // Remove oldest entry (front of vector = least recently used)
            Serial.printf("Evicting font %d from cache (%d bytes) to make room\n",
                          fontCache[0].fontIndex, fontCache[0].size);
            totalCacheSize -= fontCache[0].size;
            free(fontCache[0].data);
            fontCache.erase(fontCache.begin());
        }

        // 5. Try to allocate cache memory
        if (totalCacheSize + fontFileSize <= MAX_FONT_CACHE_SIZE) {
            fontData = (uint8_t*)malloc(fontFileSize);
            if (fontData) {
                // Read font into cache
                fontFile.read(fontData, fontFileSize);
                Serial.printf("Font read into cache buffer (%d bytes)\n", fontFileSize);
            } else {
                Serial.println("WARNING: malloc failed for font cache, loading directly from SD");
                shouldCache = false;
            }
        } else {
            Serial.println("WARNING: Not enough cache space, loading directly from SD");
            shouldCache = false;
        }
    } else {
        Serial.printf("Font too large to cache (>%d bytes), loading directly from SD\n", MAX_FONT_CACHE_SIZE);
    }

    fontFile.close();

    // 6. Load font (either from cache buffer or SD)
    esp_err_t loadResult;
    if (shouldCache && fontData) {
        Serial.println("Loading font from cache buffer...");
        loadResult = canvas.loadFont(fontData, fontFileSize);

        if (loadResult == ESP_OK) {
            // Successfully loaded from cache buffer - add to cache list
            fontCache.push_back({currentFontIndex, fontData, fontFileSize});
            totalCacheSize += fontFileSize;
            Serial.printf("Font cached successfully! (Total cache: %d/%d bytes, %d fonts)\n",
                          totalCacheSize, MAX_FONT_CACHE_SIZE, fontCache.size());
        } else {
            Serial.println("ERROR: Failed to load font from cache buffer");
            free(fontData);
            return false;
        }
    } else {
        // Load directly from SD (no caching)
        Serial.println("Loading font directly from SD...");
        loadResult = canvas.loadFont(fontPath, SD);
        if (loadResult != ESP_OK) {
            Serial.println("ERROR: Failed to load font from SD");
            return false;
        }
        Serial.println("Font loaded from SD successfully (not cached)");
    }

    // Create render cache for label size (24px)
    // Note: Large glyph rendering uses direct FreeType API (drawPixel/drawLine),
    // so we only need cache for small label text
    Serial.printf("Creating render cache for labels (size=24, cache=64)...\n");
    esp_err_t renderResult24 = canvas.createRender(24, 64);
    Serial.printf("createRender(24) returned: %d\n", renderResult24);

    if (renderResult24 != ESP_OK) {
        Serial.println("ERROR: Failed to create render cache for size 24");
        canvas.unloadFont();
        return false;
    }

    fontLoaded = true;
    Serial.println("=== Font loaded successfully! ===\n");
    return true;
}

// Free all cached fonts (useful for cleanup or memory pressure)
void clearFontCache() {
    Serial.printf("Clearing font cache (%d fonts, %d bytes)...\n", fontCache.size(), totalCacheSize);
    for (auto& entry : fontCache) {
        free(entry.data);
    }
    fontCache.clear();
    totalCacheSize = 0;
    Serial.println("Font cache cleared");
}

// ========================================
// STEP 2+3: Outline Parsing and Rendering
// ========================================

// Maximum points/segments we can store (adjust if needed)
#define MAX_OUTLINE_POINTS 200
#define MAX_OUTLINE_SEGMENTS 200

// Point storage for outline rendering
struct OutlinePoint {
    float x, y;
    bool is_control; // true = control point, false = on-curve point
};

// Segment types
enum SegmentType {
    SEG_MOVE,
    SEG_LINE,
    SEG_CONIC,
    SEG_CUBIC
};

// Segment storage
struct OutlineSegment {
    SegmentType type;
    float x, y;           // Endpoint (or start for MOVE)
    float cx, cy;         // Control point 1 (for CONIC/CUBIC)
    float cx2, cy2;       // Control point 2 (for CUBIC only)
};

// Global storage for parsed outline
OutlinePoint g_outline_points[MAX_OUTLINE_POINTS];
OutlineSegment g_outline_segments[MAX_OUTLINE_SEGMENTS];
int g_num_points = 0;
int g_num_segments = 0;

// Callback context for outline decomposition
struct OutlineDecomposeContext {
    int segment_count;
    int moveto_count;
    int lineto_count;
    int conicto_count;
    int cubicto_count;
    float scale;          // Scale factor from font units to pixels
    float offset_x;       // X offset for centering
    float offset_y;       // Y offset for centering
};

// Callback: MoveTo (start new contour)
int outlineMoveTo(const FT_Vector* to, void* user) {
    OutlineDecomposeContext* ctx = (OutlineDecomposeContext*)user;
    ctx->segment_count++;
    ctx->moveto_count++;

    if (g_num_segments < MAX_OUTLINE_SEGMENTS) {
        g_outline_segments[g_num_segments].type = SEG_MOVE;
        g_outline_segments[g_num_segments].x = to->x * ctx->scale + ctx->offset_x;
        g_outline_segments[g_num_segments].y = -to->y * ctx->scale + ctx->offset_y; // Flip Y
        g_num_segments++;
    }

    // Save on-curve point
    if (g_num_points < MAX_OUTLINE_POINTS) {
        g_outline_points[g_num_points].x = to->x * ctx->scale + ctx->offset_x;
        g_outline_points[g_num_points].y = -to->y * ctx->scale + ctx->offset_y; // Flip Y
        g_outline_points[g_num_points].is_control = false; // On-curve
        g_num_points++;
    }
    return 0;
}

// Callback: LineTo (straight line segment)
int outlineLineTo(const FT_Vector* to, void* user) {
    OutlineDecomposeContext* ctx = (OutlineDecomposeContext*)user;
    ctx->segment_count++;
    ctx->lineto_count++;

    if (g_num_segments < MAX_OUTLINE_SEGMENTS) {
        g_outline_segments[g_num_segments].type = SEG_LINE;
        g_outline_segments[g_num_segments].x = to->x * ctx->scale + ctx->offset_x;
        g_outline_segments[g_num_segments].y = -to->y * ctx->scale + ctx->offset_y; // Flip Y
        g_num_segments++;
    }

    // Save on-curve point
    if (g_num_points < MAX_OUTLINE_POINTS) {
        g_outline_points[g_num_points].x = to->x * ctx->scale + ctx->offset_x;
        g_outline_points[g_num_points].y = -to->y * ctx->scale + ctx->offset_y; // Flip Y
        g_outline_points[g_num_points].is_control = false; // On-curve
        g_num_points++;
    }
    return 0;
}

// Callback: ConicTo (quadratic Bézier curve - TrueType)
int outlineConicTo(const FT_Vector* control, const FT_Vector* to, void* user) {
    OutlineDecomposeContext* ctx = (OutlineDecomposeContext*)user;
    ctx->segment_count++;
    ctx->conicto_count++;

    if (g_num_segments < MAX_OUTLINE_SEGMENTS) {
        g_outline_segments[g_num_segments].type = SEG_CONIC;
        g_outline_segments[g_num_segments].x = to->x * ctx->scale + ctx->offset_x;
        g_outline_segments[g_num_segments].y = -to->y * ctx->scale + ctx->offset_y; // Flip Y
        g_outline_segments[g_num_segments].cx = control->x * ctx->scale + ctx->offset_x;
        g_outline_segments[g_num_segments].cy = -control->y * ctx->scale + ctx->offset_y; // Flip Y
        g_num_segments++;
    }

    // Save control point (off-curve)
    if (g_num_points < MAX_OUTLINE_POINTS) {
        g_outline_points[g_num_points].x = control->x * ctx->scale + ctx->offset_x;
        g_outline_points[g_num_points].y = -control->y * ctx->scale + ctx->offset_y; // Flip Y
        g_outline_points[g_num_points].is_control = true; // Off-curve (control)
        g_num_points++;
    }

    // Save endpoint (on-curve)
    if (g_num_points < MAX_OUTLINE_POINTS) {
        g_outline_points[g_num_points].x = to->x * ctx->scale + ctx->offset_x;
        g_outline_points[g_num_points].y = -to->y * ctx->scale + ctx->offset_y; // Flip Y
        g_outline_points[g_num_points].is_control = false; // On-curve
        g_num_points++;
    }
    return 0;
}

// Callback: CubicTo (cubic Bézier curve - PostScript/OpenType)
int outlineCubicTo(const FT_Vector* control1, const FT_Vector* control2, const FT_Vector* to, void* user) {
    OutlineDecomposeContext* ctx = (OutlineDecomposeContext*)user;
    ctx->segment_count++;
    ctx->cubicto_count++;

    if (g_num_segments < MAX_OUTLINE_SEGMENTS) {
        g_outline_segments[g_num_segments].type = SEG_CUBIC;
        g_outline_segments[g_num_segments].x = to->x * ctx->scale + ctx->offset_x;
        g_outline_segments[g_num_segments].y = -to->y * ctx->scale + ctx->offset_y; // Flip Y
        g_outline_segments[g_num_segments].cx = control1->x * ctx->scale + ctx->offset_x;
        g_outline_segments[g_num_segments].cy = -control1->y * ctx->scale + ctx->offset_y; // Flip Y
        g_outline_segments[g_num_segments].cx2 = control2->x * ctx->scale + ctx->offset_x;
        g_outline_segments[g_num_segments].cy2 = -control2->y * ctx->scale + ctx->offset_y; // Flip Y
        g_num_segments++;
    }

    // Save control point 1 (off-curve)
    if (g_num_points < MAX_OUTLINE_POINTS) {
        g_outline_points[g_num_points].x = control1->x * ctx->scale + ctx->offset_x;
        g_outline_points[g_num_points].y = -control1->y * ctx->scale + ctx->offset_y; // Flip Y
        g_outline_points[g_num_points].is_control = true; // Off-curve
        g_num_points++;
    }

    // Save control point 2 (off-curve)
    if (g_num_points < MAX_OUTLINE_POINTS) {
        g_outline_points[g_num_points].x = control2->x * ctx->scale + ctx->offset_x;
        g_outline_points[g_num_points].y = -control2->y * ctx->scale + ctx->offset_y; // Flip Y
        g_outline_points[g_num_points].is_control = true; // Off-curve
        g_num_points++;
    }

    // Save endpoint (on-curve)
    if (g_num_points < MAX_OUTLINE_POINTS) {
        g_outline_points[g_num_points].x = to->x * ctx->scale + ctx->offset_x;
        g_outline_points[g_num_points].y = -to->y * ctx->scale + ctx->offset_y; // Flip Y
        g_outline_points[g_num_points].is_control = false; // On-curve
        g_num_points++;
    }
    return 0;
}

// ========================================
// Helper: Find a valid glyph in the current font
// ========================================

// Try to find a glyph that exists in the font, starting from the preferred codepoint
// Returns the found codepoint, or 0 if no valid glyph found
uint32_t findValidGlyph(uint32_t preferredCodepoint) {
    FT_Face face = getFontFaceFromCanvas();
    if (!face) {
        return 0;
    }

    // First try the preferred codepoint
    FT_UInt glyph_index = FT_Get_Char_Index(face, preferredCodepoint);
    if (glyph_index != 0) {
        return preferredCodepoint; // Found!
    }

    Serial.printf("Glyph U+%04X not in font, searching for alternative...\n", preferredCodepoint);

    // v2.2: Try to find ANY valid glyph in enabled Unicode ranges
    for (int i = 0; i < numGlyphRanges; i++) {
        // Skip disabled ranges
        if (i >= config.rangeEnabled.size() || !config.rangeEnabled[i]) {
            continue;
        }

        for (uint32_t codepoint = glyphRanges[i].start; codepoint <= glyphRanges[i].end; codepoint++) {
            glyph_index = FT_Get_Char_Index(face, codepoint);
            if (glyph_index != 0) {
                Serial.printf("Found alternative glyph: U+%04X from %s\n", codepoint, glyphRanges[i].name);
                return codepoint;
            }
        }
    }

    // If still nothing, try the first glyph in the font (usually space or .notdef)
    FT_UInt agindex;
    FT_ULong charcode = FT_Get_First_Char(face, &agindex);
    if (agindex != 0 && charcode != 0) {
        Serial.printf("Found first available glyph in font: U+%04lX (index %u)\n", charcode, agindex);
        return (uint32_t)charcode;
    }

    Serial.println("ERROR: No valid glyphs found in font!");
    return 0; // No glyphs found at all
}

// ========================================
// STEP 1+2: FT_Face Access and Outline Decompose Test
// ========================================

// Test function to access FT_Face and print glyph outline info
void testGlyphOutlineAccess(uint32_t codepoint) {
    Serial.println("\n=== STEP 1: Testing FT_Face Access ===");

    // Get FT_Face from protected member via external symbol access
    FT_Face face = getFontFaceFromCanvas();
    if (!face) {
        Serial.println("ERROR: FT_Face is NULL (font not loaded?)");
        return;
    }

    Serial.printf("✓ FT_Face accessed successfully!\n");
    Serial.printf("  Font family: %s\n", face->family_name);
    Serial.printf("  Font style: %s\n", face->style_name);
    Serial.printf("  Num glyphs in font: %ld\n", face->num_glyphs);

    // Load the glyph for the current codepoint
    FT_UInt glyph_index = FT_Get_Char_Index(face, codepoint);
    if (glyph_index == 0) {
        Serial.printf("WARNING: Glyph U+%04X not found in font\n", codepoint);
        return;
    }

    Serial.printf("  Glyph index for U+%04X: %u\n", codepoint, glyph_index);

    // Load glyph with FT_LOAD_NO_SCALE to get raw outline data
    FT_Error error = FT_Load_Glyph(face, glyph_index, FT_LOAD_NO_SCALE);
    if (error) {
        Serial.printf("ERROR: Failed to load glyph (error %d)\n", error);
        return;
    }

    // Check if glyph has outline data
    if (face->glyph->format != FT_GLYPH_FORMAT_OUTLINE) {
        Serial.printf("WARNING: Glyph is not outline format (format=%c%c%c%c)\n",
                     (face->glyph->format >> 24) & 0xFF,
                     (face->glyph->format >> 16) & 0xFF,
                     (face->glyph->format >> 8) & 0xFF,
                     face->glyph->format & 0xFF);
        return;
    }

    // Access outline data
    FT_Outline* outline = &face->glyph->outline;

    Serial.printf("\n✓ Glyph U+%04X outline data:\n", codepoint);
    Serial.printf("  Contours: %d\n", outline->n_contours);
    Serial.printf("  Points: %d\n", outline->n_points);
    Serial.printf("  Flags available: %s\n", outline->flags ? "yes" : "no");

    // Show first few points as proof (Step 1 verification)
    if (outline->n_points > 0) {
        Serial.println("\n  First 5 points (raw font units):");
        int max_points = outline->n_points < 5 ? outline->n_points : 5;
        for (int i = 0; i < max_points; i++) {
            char point_type = (outline->tags[i] & FT_CURVE_TAG_ON) ? 'O' : 'C'; // O=on-curve, C=control
            Serial.printf("    [%d] (%ld, %ld) [%c]\n",
                         i,
                         outline->points[i].x,
                         outline->points[i].y,
                         point_type);
        }
    }

    Serial.println("\n=== STEP 1: FT_Face Access SUCCESS! ===");

    // ========================================
    // STEP 2: Decompose outline into segments
    // ========================================
    Serial.println("\n=== STEP 2: Decomposing Outline ===");

    // Setup callback function table
    FT_Outline_Funcs callbacks;
    callbacks.move_to = (FT_Outline_MoveToFunc)outlineMoveTo;
    callbacks.line_to = (FT_Outline_LineToFunc)outlineLineTo;
    callbacks.conic_to = (FT_Outline_ConicToFunc)outlineConicTo;
    callbacks.cubic_to = (FT_Outline_CubicToFunc)outlineCubicTo;
    callbacks.shift = 0;  // No coordinate shift
    callbacks.delta = 0;  // No coordinate delta

    // Context to track decomposition stats
    OutlineDecomposeContext ctx = {0, 0, 0, 0, 0};

    // Decompose outline into segments
    Serial.printf("\nDecomposing %d contours into segments:\n", outline->n_contours);
    Serial.println("(Coordinates in font units, not yet scaled to pixels)\n");

    FT_Error decompose_error = FT_Outline_Decompose(outline, &callbacks, &ctx);

    if (decompose_error) {
        Serial.printf("ERROR: FT_Outline_Decompose failed (error %d)\n", decompose_error);
    } else {
        Serial.println("\n✓ Outline decomposition complete!");
        Serial.printf("  Total segments: %d\n", ctx.segment_count);
        Serial.printf("  - MoveTo (contour start): %d\n", ctx.moveto_count);
        Serial.printf("  - LineTo (straight lines): %d\n", ctx.lineto_count);
        Serial.printf("  - ConicTo (quadratic Bézier): %d\n", ctx.conicto_count);
        Serial.printf("  - CubicTo (cubic Bézier): %d\n", ctx.cubicto_count);

        // Determine font type from curve usage
        if (ctx.conicto_count > 0 && ctx.cubicto_count == 0) {
            Serial.println("  Font type: TrueType (quadratic Bézier curves)");
        } else if (ctx.cubicto_count > 0 && ctx.conicto_count == 0) {
            Serial.println("  Font type: PostScript/OpenType (cubic Bézier curves)");
        } else if (ctx.conicto_count == 0 && ctx.cubicto_count == 0) {
            Serial.println("  Font type: Simple polygonal outline (no curves)");
        } else {
            Serial.println("  Font type: Mixed (both quadratic and cubic curves - rare!)");
        }
    }

    Serial.println("\n=== STEP 2: Outline Decompose SUCCESS! ===\n");
}

// ========================================
// STEP 3: Parse and Render Outline
// ========================================

// Parse glyph outline and store scaled segments
bool parseGlyphOutline(uint32_t codepoint) {
    // Reset storage
    g_num_segments = 0;
    g_num_points = 0;

    // Get FT_Face
    FT_Face face = getFontFaceFromCanvas();
    if (!face) {
        Serial.println("ERROR: FT_Face is NULL");
        return false;
    }

    // Load glyph
    FT_UInt glyph_index = FT_Get_Char_Index(face, codepoint);
    if (glyph_index == 0) {
        Serial.printf("WARNING: Glyph U+%04X not found\n", codepoint);
        return false;
    }

    FT_Error error = FT_Load_Glyph(face, glyph_index, FT_LOAD_NO_SCALE);
    if (error) {
        Serial.printf("ERROR: Failed to load glyph (error %d)\n", error);
        return false;
    }

    if (face->glyph->format != FT_GLYPH_FORMAT_OUTLINE) {
        Serial.println("WARNING: Glyph is not outline format");
        return false;
    }

    FT_Outline* outline = &face->glyph->outline;

    // Calculate bounding box for scaling
    FT_BBox bbox;
    FT_Outline_Get_CBox(outline, &bbox);

    float width = bbox.xMax - bbox.xMin;
    float height = bbox.yMax - bbox.yMin;

    // Target size: 400px (scale each glyph to fill screen optimally)
    float target_size = 400.0f;
    float scale = target_size / (width > height ? width : height);

    // Center on display (540x960 vertical)
    int centerX = 270;
    int centerY = 480;

    // Calculate center of glyph bounding box
    float bbox_center_x = (bbox.xMin + bbox.xMax) / 2.0f;
    float bbox_center_y = (bbox.yMin + bbox.yMax) / 2.0f;

    // Offset to center the glyph on display
    // Y axis is flipped (font coords increase upward, screen coords increase downward)
    float offset_x = centerX - bbox_center_x * scale;
    float offset_y = centerY + bbox_center_y * scale; // Y is negated in callbacks, so add here

    Serial.printf("Parsing outline: scale=%.4f, offset=(%.1f, %.1f)\n", scale, offset_x, offset_y);

    // Setup callback context
    OutlineDecomposeContext ctx = {0, 0, 0, 0, 0, scale, offset_x, offset_y};

    // Setup callbacks
    FT_Outline_Funcs callbacks;
    callbacks.move_to = (FT_Outline_MoveToFunc)outlineMoveTo;
    callbacks.line_to = (FT_Outline_LineToFunc)outlineLineTo;
    callbacks.conic_to = (FT_Outline_ConicToFunc)outlineConicTo;
    callbacks.cubic_to = (FT_Outline_CubicToFunc)outlineCubicTo;
    callbacks.shift = 0;
    callbacks.delta = 0;

    // Decompose outline
    FT_Error decompose_error = FT_Outline_Decompose(outline, &callbacks, &ctx);
    if (decompose_error) {
        Serial.printf("ERROR: FT_Outline_Decompose failed (error %d)\n", decompose_error);
        return false;
    }

    Serial.printf("Parsed %d segments (MoveTo:%d LineTo:%d Conic:%d Cubic:%d)\n",
                 g_num_segments, ctx.moveto_count, ctx.lineto_count,
                 ctx.conicto_count, ctx.cubicto_count);

    return true;
}

// Helper function to draw a dashed line
void drawDashedLine(float x1, float y1, float x2, float y2, uint8_t color) {
    // Calculate line length and direction
    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = sqrt(dx * dx + dy * dy);

    if (length < 0.5f) return; // Too short to draw

    // Normalize direction
    dx /= length;
    dy /= length;

    // Dash pattern: 10 pixels on, 5 pixels off (longer for better visibility)
    float dash_length = 10.0f;
    float gap_length = 5.0f;
    float pattern_length = dash_length + gap_length;

    float distance = 0.0f;
    bool drawing = true;

    while (distance < length) {
        float segment_length = drawing ? dash_length : gap_length;
        float end_distance = distance + segment_length;
        if (end_distance > length) end_distance = length;

        if (drawing) {
            float sx = x1 + dx * distance;
            float sy = y1 + dy * distance;
            float ex = x1 + dx * end_distance;
            float ey = y1 + dy * end_distance;
            canvas.drawLine(sx, sy, ex, ey, color);
        }

        distance = end_distance;
        drawing = !drawing;
    }
}

// Render outline on canvas
void renderGlyphOutline() {
    if (!fontLoaded) {
        Serial.println("ERROR: No font loaded");
        return;
    }

    Serial.println("\n=== STEP 3: Rendering Outline ===");

    // Parse outline (populates g_outline_segments)
    if (!parseGlyphOutline(currentGlyphCodepoint)) {
        Serial.println("ERROR: Failed to parse outline");
        return;
    }

    // Clear canvas - white background
    canvas.fillCanvas(0); // 0 = white

    // Draw outline segments
    float curr_x = 0, curr_y = 0;
    int lines_drawn = 0;

    for (int i = 0; i < g_num_segments; i++) {
        OutlineSegment& seg = g_outline_segments[i];

        switch (seg.type) {
            case SEG_MOVE:
                curr_x = seg.x;
                curr_y = seg.y;
                break;

            case SEG_LINE:
                canvas.drawLine(curr_x, curr_y, seg.x, seg.y, 12); // 12 = dark gray
                curr_x = seg.x;
                curr_y = seg.y;
                lines_drawn++;
                break;

            case SEG_CONIC:
                // Approximate quadratic Bézier with line segments
                {
                    int steps = 10;
                    for (int t = 1; t <= steps; t++) {
                        float u = t / (float)steps;
                        float u1 = 1.0f - u;
                        // Quadratic Bézier: B(t) = (1-t)²P0 + 2(1-t)t·P1 + t²P2
                        float bx = u1*u1*curr_x + 2*u1*u*seg.cx + u*u*seg.x;
                        float by = u1*u1*curr_y + 2*u1*u*seg.cy + u*u*seg.y;
                        canvas.drawLine(curr_x, curr_y, bx, by, 12); // Dark gray outline
                        curr_x = bx;
                        curr_y = by;
                        lines_drawn++;
                    }
                }
                break;

            case SEG_CUBIC:
                // Approximate cubic Bézier with line segments
                {
                    int steps = 15;
                    for (int t = 1; t <= steps; t++) {
                        float u = t / (float)steps;
                        float u1 = 1.0f - u;
                        // Cubic Bézier: B(t) = (1-t)³P0 + 3(1-t)²t·P1 + 3(1-t)t²P2 + t³P3
                        float bx = u1*u1*u1*curr_x + 3*u1*u1*u*seg.cx + 3*u1*u*u*seg.cx2 + u*u*u*seg.x;
                        float by = u1*u1*u1*curr_y + 3*u1*u1*u*seg.cy + 3*u1*u*u*seg.cy2 + u*u*u*seg.y;
                        canvas.drawLine(curr_x, curr_y, bx, by, 12); // Dark gray outline
                        curr_x = bx;
                        curr_y = by;
                        lines_drawn++;
                    }
                }
                break;
        }
    }

    Serial.printf("Drew %d line segments\n", lines_drawn);

    // ========================================
    // STEP 7: Draw construction lines (dashed lines from control points to anchors)
    // ========================================

    int construction_lines_drawn = 0;

    // For each segment with control points, draw dashed lines
    for (int i = 0; i < g_num_segments; i++) {
        OutlineSegment& seg = g_outline_segments[i];

        if (seg.type == SEG_CONIC) {
            // Quadratic Bézier: draw dashed line from start point to control point to end point
            // We need to find the start point (previous segment's endpoint or last MOVE)
            float start_x = 0, start_y = 0;

            // Find the start point
            if (i > 0) {
                if (g_outline_segments[i-1].type == SEG_MOVE) {
                    start_x = g_outline_segments[i-1].x;
                    start_y = g_outline_segments[i-1].y;
                } else {
                    start_x = g_outline_segments[i-1].x;
                    start_y = g_outline_segments[i-1].y;
                }
            }

            // Draw dashed line from start to control point
            drawDashedLine(start_x, start_y, seg.cx, seg.cy, 15); // Black for visibility
            construction_lines_drawn++;

            // Draw dashed line from control point to end
            drawDashedLine(seg.cx, seg.cy, seg.x, seg.y, 15);
            construction_lines_drawn++;

        } else if (seg.type == SEG_CUBIC) {
            // Cubic Bézier: draw dashed lines for both control points
            float start_x = 0, start_y = 0;

            if (i > 0) {
                if (g_outline_segments[i-1].type == SEG_MOVE) {
                    start_x = g_outline_segments[i-1].x;
                    start_y = g_outline_segments[i-1].y;
                } else {
                    start_x = g_outline_segments[i-1].x;
                    start_y = g_outline_segments[i-1].y;
                }
            }

            // Draw dashed line from start to first control point
            drawDashedLine(start_x, start_y, seg.cx, seg.cy, 15);
            construction_lines_drawn++;

            // Draw dashed line from first control point to second control point
            drawDashedLine(seg.cx, seg.cy, seg.cx2, seg.cy2, 15);
            construction_lines_drawn++;

            // Draw dashed line from second control point to end
            drawDashedLine(seg.cx2, seg.cy2, seg.x, seg.y, 15);
            construction_lines_drawn++;
        }
    }

    Serial.printf("Drew %d construction lines\n", construction_lines_drawn);

    // ========================================
    // STEP 4: Draw on-curve and off-curve points
    // ========================================

    int on_curve_count = 0;
    int off_curve_count = 0;

    for (int i = 0; i < g_num_points; i++) {
        OutlinePoint& pt = g_outline_points[i];

        if (pt.is_control) {
            // Off-curve point (control point): hollow circle
            canvas.drawCircle(pt.x, pt.y, 4, 15); // Outer circle (black)
            canvas.fillCircle(pt.x, pt.y, 3, 0);  // Inner fill (white) - creates hollow effect
            off_curve_count++;
        } else {
            // On-curve point (anchor point): filled circle
            canvas.fillCircle(pt.x, pt.y, 4, 15); // Filled black circle
            on_curve_count++;
        }
    }

    Serial.printf("Drew %d points: %d on-curve (filled), %d off-curve (hollow)\n",
                 g_num_points, on_curve_count, off_curve_count);

    // Draw labels (font name, Unicode) - same as bitmap mode
    String fontName = getFontName(fontPaths[currentFontIndex]);
    char codepointStr[32];
    sprintf(codepointStr, "U+%04X", currentGlyphCodepoint);

    // Truncate font name if it exceeds margins (30px left + 30px right = 480px max)
    // Note: shortenTextIfNeeded() uses bitmap font textSize=3 for measurement (calibrated to match FreeType 24px)
    String displayFontName = shortenTextIfNeeded(fontName, 480, 3);

    // Reload font for label rendering to ensure correct state
    canvas.loadFont(fontPaths[currentFontIndex], SD);
    canvas.createRender(24, 64); // Recreate 24px cache
    canvas.setTextSize(24);
    canvas.setTextColor(15);
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString(displayFontName, 270, 30); // Top label

    canvas.setTextDatum(BC_DATUM);
    canvas.drawString(codepointStr, 270, 930); // Bottom label

    // Push to display with full refresh (clean display, no ghosting)
    canvas.pushCanvas(0, 0, UPDATE_MODE_GC16);

    // Track first partial after full refresh to start 10s timer
    if (!hasPartialSinceLastFull) {
        firstPartialAfterFullTime = millis();
        hasPartialSinceLastFull = true;
        Serial.println("First partial after full - starting 10s timer");
    }

    // Track refresh counts
    partialRefreshCount++;

    // Trigger full refresh if:
    // A) 5 partials reached, OR
    // B) 10 seconds passed since FIRST partial AND at least 1 partial happened
    unsigned long timeSinceFirstPartial = millis() - firstPartialAfterFullTime;
    if (partialRefreshCount >= MAX_PARTIAL_BEFORE_FULL ||
        (hasPartialSinceLastFull && timeSinceFirstPartial >= FULL_REFRESH_TIMEOUT_MS)) {

        Serial.printf("Full refresh triggered (count=%d, time since first partial=%lums)\n",
                      partialRefreshCount, timeSinceFirstPartial);
        M5.EPD.UpdateFull(UPDATE_MODE_GC16);

        // Reset all counters
        partialRefreshCount = 0;
        hasPartialSinceLastFull = false;

        // Track last full refresh for power management
        lastFullRefreshTime = millis();
    }

    Serial.printf("Rendered outline: U+%04X with font %s (partial #%d)\n",
                 currentGlyphCodepoint,
                 getFontName(fontPaths[currentFontIndex]).c_str(),
                 partialRefreshCount);

    Serial.println("=== STEP 3: Outline Rendered ===\n");
}

// Generate random glyph codepoint from common ranges
uint32_t getRandomGlyphCodepoint() {
    // Re-seed with more entropy each time for better randomness
    randomSeed(analogRead(0) ^ millis() ^ micros());

    // v2.2: Build list of enabled ranges
    std::vector<int> enabledRanges;
    for (int i = 0; i < numGlyphRanges; i++) {
        if (i < config.rangeEnabled.size() && config.rangeEnabled[i]) {
            enabledRanges.push_back(i);
        }
    }

    // If no ranges enabled, fallback to first 6 (safety)
    if (enabledRanges.empty()) {
        Serial.println("WARNING: No ranges enabled, using defaults");
        for (int i = 0; i < 6 && i < numGlyphRanges; i++) {
            enabledRanges.push_back(i);
        }
    }

    // Pick random enabled range
    int randomIndex = random(0, enabledRanges.size());
    int rangeIndex = enabledRanges[randomIndex];
    const UnicodeRange& range = glyphRanges[rangeIndex];

    // Pick random codepoint in range
    uint32_t preferredCodepoint = random(range.start, range.end + 1);

    Serial.printf("Random glyph: U+%04X from %s (range %d/%d enabled)\n",
                  preferredCodepoint, range.name, rangeIndex + 1, enabledRanges.size());

    // Verify the glyph exists in the current font, or find an alternative
    uint32_t validCodepoint = findValidGlyph(preferredCodepoint);
    if (validCodepoint == 0) {
        Serial.println("ERROR: No valid glyphs found in font, skipping to next font");
        return 0; // Signal to skip this font
    }

    if (validCodepoint != preferredCodepoint) {
        Serial.printf("Using alternative glyph: U+%04X\n", validCodepoint);
    }

    return validCodepoint;
}

// Convert Unicode codepoint to UTF-8 string
String codepointToString(uint32_t codepoint) {
    String result = "";

    if (codepoint <= 0x7F) {
        // 1-byte UTF-8
        result += (char)codepoint;
    } else if (codepoint <= 0x7FF) {
        // 2-byte UTF-8
        result += (char)(0xC0 | (codepoint >> 6));
        result += (char)(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0xFFFF) {
        // 3-byte UTF-8
        result += (char)(0xE0 | (codepoint >> 12));
        result += (char)(0x80 | ((codepoint >> 6) & 0x3F));
        result += (char)(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0x10FFFF) {
        // 4-byte UTF-8
        result += (char)(0xF0 | (codepoint >> 18));
        result += (char)(0x80 | ((codepoint >> 12) & 0x3F));
        result += (char)(0x80 | ((codepoint >> 6) & 0x3F));
        result += (char)(0x80 | (codepoint & 0x3F));
    }

    return result;
}

// Render current glyph (bitmap mode - custom FreeType rendering)
void renderGlyphBitmap() {
    if (!fontLoaded) {
        Serial.println("ERROR: No font loaded");
        return;
    }

    Serial.println("\n=== Custom Bitmap Rendering ===");

    // Get FreeType face
    FT_Face face = getFontFaceFromCanvas();

    if (!face) {
        Serial.println("ERROR: FT_Face not available");
        return;
    }

    // Load glyph with no scaling to get metrics
    FT_UInt glyph_index = FT_Get_Char_Index(face, currentGlyphCodepoint);
    if (glyph_index == 0) {
        Serial.printf("WARNING: Glyph U+%04X not found\n", currentGlyphCodepoint);
        return;
    }

    FT_Error error = FT_Load_Glyph(face, glyph_index, FT_LOAD_NO_SCALE);
    if (error) {
        Serial.printf("ERROR: Failed to load glyph (error %d)\n", error);
        return;
    }

    // Calculate bounding box
    FT_BBox bbox;
    FT_Outline_Get_CBox(&face->glyph->outline, &bbox);

    float width = bbox.xMax - bbox.xMin;
    float height = bbox.yMax - bbox.yMin;

    // Target size: 400px (scale each glyph to fill screen optimally)
    float target_size = 400.0f;

    // Calculate pixel size needed to achieve target_size
    // Formula: pixel_size = (target_size * units_per_EM) / max_glyph_dimension
    float max_dim = (width > height ? width : height);
    int pixel_size = (int)((target_size * face->units_per_EM) / max_dim);

    if (pixel_size < 1) pixel_size = 1;
    if (pixel_size > 2000) pixel_size = 2000; // Safety limit (increased for small glyphs)

    Serial.printf("Glyph bbox: w=%.1f h=%.1f, units_per_EM=%d, pixel_size=%d\n",
                  width, height, face->units_per_EM, pixel_size);

    // Set pixel size and load glyph for rendering
    error = FT_Set_Pixel_Sizes(face, 0, pixel_size);
    if (error) {
        Serial.printf("ERROR: Failed to set pixel size (error %d)\n", error);
        return;
    }

    error = FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER);
    if (error) {
        Serial.printf("ERROR: Failed to render glyph (error %d)\n", error);
        return;
    }

    FT_Bitmap* bitmap = &face->glyph->bitmap;

    Serial.printf("Bitmap: %dx%d, pitch=%d, mode=%d\n",
                  bitmap->width, bitmap->rows, bitmap->pitch, bitmap->pixel_mode);

    // Clear canvas
    canvas.fillCanvas(0); // 0 = white

    // Calculate centering
    int centerX = 270;
    int centerY = 480;
    int draw_x = centerX - bitmap->width / 2;
    int draw_y = centerY - bitmap->rows / 2;

    // Draw bitmap on canvas (convert 8bpp grayscale to 4bpp)
    for (unsigned int y = 0; y < bitmap->rows; y++) {
        for (unsigned int x = 0; x < bitmap->width; x++) {
            unsigned char gray8 = bitmap->buffer[y * bitmap->pitch + x];

            // Convert 8bpp (0-255) to 4bpp (0-15)
            // 0 = white, 15 = black in M5EPD
            unsigned char gray4 = (gray8 * 15) / 255;

            int screen_x = draw_x + x;
            int screen_y = draw_y + y;

            // Bounds check
            if (screen_x >= 0 && screen_x < 540 && screen_y >= 0 && screen_y < 960) {
                canvas.drawPixel(screen_x, screen_y, gray4);
            }
        }
    }

    // Draw labels
    String fontName = getFontName(fontPaths[currentFontIndex]);
    char codepointStr[32];
    sprintf(codepointStr, "U+%04X", currentGlyphCodepoint);

    // Truncate font name if it exceeds margins (30px left + 30px right = 480px max)
    // Note: shortenTextIfNeeded() uses bitmap font textSize=3 for measurement (calibrated to match FreeType 24px)
    String displayFontName = shortenTextIfNeeded(fontName, 480, 3);

    // IMPORTANT: After using FT_Set_Pixel_Sizes() directly for large glyph rendering,
    // we need to reload font at size 24 for label rendering
    // (FreeType state was modified, so we must reset it)
    canvas.loadFont(fontPaths[currentFontIndex], SD);
    canvas.createRender(24, 64); // Recreate 24px cache
    canvas.setTextSize(24);
    canvas.setTextColor(15);
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString(displayFontName, centerX, 30);

    canvas.setTextSize(24);
    canvas.setTextColor(15);
    canvas.setTextDatum(BC_DATUM);
    canvas.drawString(codepointStr, centerX, 930);

    // Push to display with full refresh (clean display, no ghosting)
    canvas.pushCanvas(0, 0, UPDATE_MODE_GC16);

    // Track first partial after full refresh
    if (!hasPartialSinceLastFull) {
        firstPartialAfterFullTime = millis();
        hasPartialSinceLastFull = true;
        Serial.println("First partial after full - starting 10s timer");
    }

    partialRefreshCount++;

    // Trigger full refresh if needed
    unsigned long timeSinceFirstPartial = millis() - firstPartialAfterFullTime;
    if (partialRefreshCount >= MAX_PARTIAL_BEFORE_FULL ||
        (hasPartialSinceLastFull && timeSinceFirstPartial >= FULL_REFRESH_TIMEOUT_MS)) {

        Serial.printf("Full refresh triggered (count=%d, time since first partial=%lums)\n",
                      partialRefreshCount, timeSinceFirstPartial);
        M5.EPD.UpdateFull(UPDATE_MODE_GC16);

        partialRefreshCount = 0;
        hasPartialSinceLastFull = false;
        lastFullRefreshTime = millis();
    }

    Serial.printf("Rendered bitmap: U+%04X with font %s\n",
                  currentGlyphCodepoint, fontName.c_str());
}

// ========================================
// STEP 5: Unified Render Function (chooses bitmap or outline)
// ========================================

void renderGlyph() {
    if (currentViewMode == BITMAP) {
        renderGlyphBitmap();
    } else {
        renderGlyphOutline();
    }
}

// Change to next font (skip fonts that don't have the current glyph)
void nextFont() {
    if (fontPaths.empty()) return;

    int startIndex = currentFontIndex;
    int attempts = 0;

    // Try to find a font that has the current glyph
    do {
        currentFontIndex = (currentFontIndex + 1) % fontPaths.size();
        attempts++;

        Serial.printf("Trying font %d/%d\n", currentFontIndex + 1, fontPaths.size());

        if (loadCurrentFont()) {
            // Check if this font has the current glyph
            FT_Face face = getFontFaceFromCanvas();
            if (face) {
                FT_UInt glyph_index = FT_Get_Char_Index(face, currentGlyphCodepoint);
                if (glyph_index != 0) {
                    // Found a font with this glyph!
                    Serial.printf("Font %d has glyph U+%04X\n", currentFontIndex + 1, currentGlyphCodepoint);
                    renderGlyph();
                    return;
                } else {
                    Serial.printf("Font %d doesn't have glyph U+%04X, skipping...\n",
                                  currentFontIndex + 1, currentGlyphCodepoint);
                }
            }
        }

        // Check if we've tried all fonts
        if (attempts >= fontPaths.size()) {
            Serial.println("No font has this glyph! Returning to original font.");
            currentFontIndex = startIndex;
            loadCurrentFont();
            renderGlyph();
            return;
        }
    } while (true);
}

// Change to previous font (skip fonts that don't have the current glyph)
void previousFont() {
    if (fontPaths.empty()) return;

    int startIndex = currentFontIndex;
    int attempts = 0;

    // Try to find a font that has the current glyph
    do {
        currentFontIndex--;
        if (currentFontIndex < 0) {
            currentFontIndex = fontPaths.size() - 1;
        }
        attempts++;

        Serial.printf("Trying font %d/%d\n", currentFontIndex + 1, fontPaths.size());

        if (loadCurrentFont()) {
            // Check if this font has the current glyph
            FT_Face face = getFontFaceFromCanvas();
            if (face) {
                FT_UInt glyph_index = FT_Get_Char_Index(face, currentGlyphCodepoint);
                if (glyph_index != 0) {
                    // Found a font with this glyph!
                    Serial.printf("Font %d has glyph U+%04X\n", currentFontIndex + 1, currentGlyphCodepoint);
                    renderGlyph();
                    return;
                } else {
                    Serial.printf("Font %d doesn't have glyph U+%04X, skipping...\n",
                                  currentFontIndex + 1, currentGlyphCodepoint);
                }
            }
        }

        // Check if we've tried all fonts
        if (attempts >= fontPaths.size()) {
            Serial.println("No font has this glyph! Returning to original font.");
            currentFontIndex = startIndex;
            loadCurrentFont();
            renderGlyph();
            return;
        }
    } while (true);
}

// Generate and display new random glyph
void randomGlyph() {
    if (!fontLoaded) return;

    currentGlyphCodepoint = getRandomGlyphCodepoint();

    // If no valid glyph found in current font, try next font
    if (currentGlyphCodepoint == 0) {
        Serial.println("No valid glyphs in current font, switching to next font");
        nextFont(); // This will automatically skip fonts without glyphs
        return;
    }

    renderGlyph();
}

// Change to random font AND random glyph (used for auto-wake)
void randomFont() {
    if (fontPaths.empty()) return;

    // Pick a random font index
    currentFontIndex = random(0, fontPaths.size());
    Serial.printf("Random font selected: %d/%d\n", currentFontIndex + 1, fontPaths.size());

    // Load the font and generate random glyph
    if (loadCurrentFont()) {
        currentGlyphCodepoint = getRandomGlyphCodepoint();
        renderGlyph();
    }
}

// Check battery level and return percentage (0-100)
float getBatteryPercentage() {
    // Take average of 5 readings to reduce ADC noise
    uint32_t voltageSum = 0;
    for (int i = 0; i < 5; i++) {
        voltageSum += M5.getBatteryVoltage();
        delay(10); // Small delay between readings
    }
    uint32_t voltage = voltageSum / 5;
    Serial.printf("Battery voltage: %dmV (averaged)\n", voltage);

    // LiPo voltage range: 4200mV (100%) to 3300mV (0%)
    // 5% threshold = ~3345mV
    if (voltage >= 4200) return 100.0;
    if (voltage <= 3300) return 0.0;

    float percentage = ((float)(voltage - 3300) / (4200 - 3300)) * 100.0;
    return percentage;
}

// Log battery data to .battery file on SD card
void logBatteryData(uint32_t voltage, float percentage, bool isReset) {
    File file = SD.open("/.battery", FILE_APPEND);
    if (!file) {
        Serial.println("WARNING: Could not open .battery file for logging");
        return;
    }

    if (isReset) {
        // Write reset separator with timestamp (uptime resets to 0)
        file.println();
        file.println("=== RESET ===");
        file.println();
        Serial.println("Battery log: RESET marker written");
    } else {
        // Calculate total uptime: accumulated time in RTC + current session time
        uint64_t totalMillis = rtcState.totalMillis + millis();
        uint32_t totalSeconds = totalMillis / 1000; // Convert to seconds
        uint32_t days = totalSeconds / (24 * 60 * 60);
        uint32_t hours = (totalSeconds % (24 * 60 * 60)) / (60 * 60);
        uint32_t minutes = (totalSeconds % (60 * 60)) / 60;
        uint32_t seconds = totalSeconds % 60;

        // Format: "DDd HHh MMm SSs | 4236mV | 100.0% | BITMAP"
        char line[80];
        snprintf(line, sizeof(line), "%02lud %02luh %02lum %02lus | %4lumV | %5.1f%% | %s",
                 days, hours, minutes, seconds, voltage, percentage,
                 currentViewMode == BITMAP ? "BITMAP" : "OUTLINE");

        file.println(line);
        Serial.printf("Battery log: %s\n", line);
    }

    file.close();
}

// Display low battery icon and shutdown
void lowBatteryShutdown() {
    Serial.println("\n!!! LOW BATTERY - SHUTTING DOWN !!!");

    // Clear screen with full refresh
    M5.EPD.Clear(true);
    delay(1000); // Wait for Clear() to complete physically on e-ink

    canvas.fillCanvas(0); // White background

    // Draw battery icon (stylized AA battery shape)
    // Battery body: 120x60 rectangle centered at (270, 480)
    int battX = 210;  // Left edge
    int battY = 450;  // Top edge
    int battW = 120;  // Width
    int battH = 60;   // Height
    int tipW = 10;    // Width of protruding tip
    int tipH = 20;    // Height of tip (centered)

    // Draw main battery body (hollow rectangle)
    canvas.fillRect(battX, battY, battW, battH, 15);           // Black outline
    canvas.fillRect(battX + 3, battY + 3, battW - 6, battH - 6, 0); // White inside

    // Draw battery tip (right side protrusion)
    int tipX = battX + battW;
    int tipY = battY + (battH - tipH) / 2;
    canvas.fillRect(tipX, tipY, tipW, tipH, 15); // Black tip

    // Draw very thin red bar on left side (5% indicator)
    int barW = 6; // Very thin bar
    int barH = battH - 12; // Leave margin
    int barX = battX + 6;
    int barY = battY + 6;
    canvas.fillRect(barX, barY, barW, barH, 12); // Gray-ish (simulating red on grayscale)

    // Add text below battery
    canvas.setTextSize(26);
    canvas.setTextColor(15);
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString("LOW BATTERY", 270, 380);
    canvas.setTextSize(20);
    canvas.drawString("Please charge device", 270, 540);

    // Push to display with full refresh
    canvas.pushCanvas(0, 0, UPDATE_MODE_GC16);

    // Wait for e-ink refresh to physically complete (~500ms), then immediate shutdown
    delay(500); // Hardware refresh completion

    // Complete shutdown (only RTC stays powered)
    Serial.println("Entering complete shutdown...");
    M5.shutdown();
}

// Save state and enter deep sleep
void enterDeepSleep() {
    Serial.println("\n>>> Preparing for deep sleep...");

    // Accumulate current session time to total uptime (only in debug mode, for battery logging)
    if (rtcState.debugMode) {
        uint32_t currentMillis = millis();
        Serial.printf("DEBUG: Before accumulation: rtcState.totalMillis=%llu, millis()=%lu\n",
                      rtcState.totalMillis, currentMillis);
        rtcState.totalMillis += currentMillis;
        Serial.printf("DEBUG: After accumulation: rtcState.totalMillis=%llu\n", rtcState.totalMillis);

        uint32_t totalMinutes = rtcState.totalMillis / 60000;
        Serial.printf("Accumulated uptime: %lu minutes (%lud %02luh %02lum)\n",
                      totalMinutes,
                      totalMinutes / (24 * 60),
                      (totalMinutes % (24 * 60)) / 60,
                      totalMinutes % 60);
    }

    // Save current state to RTC memory
    rtcState.isValid = true;
    rtcState.currentFontIndex = currentFontIndex;
    rtcState.currentGlyphCodepoint = currentGlyphCodepoint;
    rtcState.viewMode = currentViewMode;  // STEP 5: Save view mode
    Serial.printf("State saved: font=%d, glyph=U+%04X, mode=%s\n",
                  rtcState.currentFontIndex, rtcState.currentGlyphCodepoint,
                  currentViewMode == BITMAP ? "BITMAP" : "OUTLINE");

    // Put display controller in standby
    M5.EPD.StandBy();
    Serial.println("Display controller in StandBy");

    // Keep GPIO2 (M5EPD_MAIN_PWR_PIN) HIGH during deep sleep to maintain power
    gpio_hold_en((gpio_num_t)M5EPD_MAIN_PWR_PIN);
    gpio_deep_sleep_hold_en();
    Serial.println("GPIO hold enabled");

    // Configure wake sources:
    // 1) GPIO38 (center button press) - user interaction
    // 2) Timer (config interval) - auto refresh
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_38, LOW); // Wake when button pressed (goes LOW)

    // Enable timer wakeup from config (5, 10, or 15 minutes)
    uint64_t wakeup_time_us = config.wakeIntervalMinutes * 60 * 1000000ULL; // minutes to microseconds
    esp_sleep_enable_timer_wakeup(wakeup_time_us);
    Serial.printf("Wake sources configured: GPIO38 (button) + Timer (%dmin)\n", config.wakeIntervalMinutes);

    Serial.println(">>> Entering deep sleep now...\n");
    delay(100); // Give serial time to flush

    // Enter deep sleep
    esp_deep_sleep_start();
}

// ========================================
// Shutdown Screen with QR Code
// ========================================

// Draw QR code at specified position with specified pixel size
// Note: pixelSizeX can be different from pixelSizeY to compensate for display aspect ratio
void drawQRCode(int centerX, int centerY, int pixelSizeY, int pixelSizeX = 0) {
    if (pixelSizeX == 0) pixelSizeX = pixelSizeY; // Default to square if not specified

    int qrDisplayWidth = QR_SIZE * pixelSizeX;
    int qrDisplayHeight = QR_SIZE * pixelSizeY;
    int startX = centerX - qrDisplayWidth / 2;
    int startY = centerY - qrDisplayHeight / 2;

    Serial.printf("QR Code: size=%d×%d, pixelSize=%d×%d, display=%d×%d, start=(%d,%d)\n",
                  QR_SIZE, QR_SIZE, pixelSizeX, pixelSizeY, qrDisplayWidth, qrDisplayHeight, startX, startY);
    Serial.printf("Canvas: width=%d, height=%d\n", canvas.width(), canvas.height());

    // Debug: print ALL 29 rows to verify complete QR data
    Serial.println("Complete QR code data:");
    for (int row = 0; row < QR_SIZE; row++) {
        Serial.printf("Row %2d: ", row);
        for (int x = 0; x < QR_SIZE; x++) {
            int byteIndex = row * 4 + (x / 8);
            int bitIndex = 7 - (x % 8);
            uint8_t byte = pgm_read_byte(&qrcode_data[byteIndex]);
            bool isBlack = (byte >> bitIndex) & 1;
            Serial.print(isBlack ? "█" : " ");
        }
        Serial.println();
    }

    // Draw QR code pixel by pixel
    int blackModules = 0;
    int pixelsDrawn = 0;

    for (int moduleY = 0; moduleY < QR_SIZE; moduleY++) {
        for (int moduleX = 0; moduleX < QR_SIZE; moduleX++) {
            // Read bit from PROGMEM
            int byteIndex = moduleY * 4 + (moduleX / 8);
            int bitIndex = 7 - (moduleX % 8);
            uint8_t byte = pgm_read_byte(&qrcode_data[byteIndex]);
            bool isBlack = (byte >> bitIndex) & 1;

            // Draw this module as pixelSizeX × pixelSizeY block using drawPixel
            if (isBlack) {
                blackModules++;
                int baseX = startX + moduleX * pixelSizeX;
                int baseY = startY + moduleY * pixelSizeY;

                // Fill the pixelSizeX × pixelSizeY rectangle pixel by pixel
                for (int dy = 0; dy < pixelSizeY; dy++) {
                    for (int dx = 0; dx < pixelSizeX; dx++) {
                        canvas.drawPixel(baseX + dx, baseY + dy, 15);
                        pixelsDrawn++;
                    }
                }
            }
        }
    }
    Serial.printf("QR Code drawing complete: %d black modules, %d pixels drawn\n", blackModules, pixelsDrawn);
}

// Show shutdown screen with QR code and power off
void shutdownWithScreen() {
    Serial.println("\n=== Shutdown Requested (Long Press) ===");

    // Use existing canvas but unload any FreeType font
    canvas.unloadFont(); // Remove FreeType renderer
    canvas.fillCanvas(0); // White background

    // Draw QR code first
    Serial.println("Drawing QR code...");
    drawQRCode(270, 480, 6, 6); // QR code: 6×6 px per module

    // Draw text with built-in bitmap font (no FreeType)
    Serial.println("Drawing text labels...");
    canvas.setFreeFont(NULL); // Force use of built-in font
    canvas.setTextFont(1); // Font 1 = default bitmap font
    canvas.setTextSize(2); // Bitmap font size 2 = 16px height
    canvas.setTextColor(15); // Black text

    // Top label: "PaperSpecimen"
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString("PaperSpecimen", 270, 30);
    Serial.println("Top label drawn");

    // Bottom label: "v2.2.3" (or "v2.2.3*" in debug mode)
    canvas.setTextDatum(BC_DATUM);
    canvas.drawString(rtcState.debugMode ? "v2.2.3*" : "v2.2.3", 270, 930);
    Serial.println("Bottom label drawn");

    // Full refresh to clear any ghosting
    canvas.pushCanvas(0, 0, UPDATE_MODE_GC16);
    Serial.println("Shutdown screen displayed with QR code");

    delay(2000); // Show for 2 seconds

    // Deep sleep indefinitely (no timer wake)
    Serial.println("Entering deep sleep (shutdown mode)");

    // Configure wake on button press only
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_38, 0); // GPIO38 = center button, LOW = pressed

    // Put display to sleep
    M5.EPD.Sleep();

    // Power management
    digitalWrite(M5EPD_MAIN_PWR_PIN, LOW);
    gpio_hold_en((gpio_num_t)M5EPD_MAIN_PWR_PIN);
    gpio_deep_sleep_hold_en();

    delay(100);
    esp_deep_sleep_start();
}

void setup() {
    // Check wake reason BEFORE initializing anything (to decide if Serial is needed)
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    bool isWakeFromSleep = (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER || wakeup_reason == ESP_SLEEP_WAKEUP_EXT0);

    // Determine if Serial should be enabled:
    // - Always enable on cold boot (needed for setup and debug mode trigger)
    // - On wake from sleep: only enable if debug mode is active
    bool enableSerial = !isWakeFromSleep || rtcState.debugMode;

    // Initialize M5Paper with minimal power consumption
    // Parameters: touchEnable, SDEnable, SerialEnable, BatteryADCEnable, I2CEnable
    M5.begin(false, true, enableSerial, false, true);
    // false: Touch disabled (saves power)
    // true:  SD enabled (needed for fonts)
    // enableSerial: Serial enabled conditionally (for debugging in debug mode or cold boot)
    // false: Battery ADC disabled initially (enable only when needed)
    // true:  I2C enabled (needed for RTC)

    M5.RTC.begin();

    // Setup Serial only if enabled
    if (enableSerial) {
        Serial.begin(115200);
        delay(100); // Wait for serial to be ready
    }

    // Determine wake source
    bool isAutoWake = false;

    if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        // Woke from timer (auto-wake)
        isAutoWake = true;
        isAutoWakeSession = true; // Flag to skip idle wait and sleep immediately
        Serial.println("\n\n=== PaperSpecimen Auto-Wake (Timer) ===");
        Serial.println("Woke by ESP32 timer (configured interval)");
        if (!enableSerial) {
            // Serial not initialized - debug mode is off
        }
    } else if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        // Woke from button press
        isAutoWake = false;
        isAutoWakeSession = false; // Normal behavior: wait for user interaction
        Serial.println("\n\n=== PaperSpecimen Wake from Deep Sleep (Button) ===");
        Serial.println("Woke by GPIO38 (center button)");
    } else {
        // Cold boot (reset or first power on)
        isAutoWakeSession = false; // Normal behavior: wait for user interaction
        Serial.println("\n\n=== PaperSpecimen Cold Boot ===");
    }

    // Wake from sleep: reactivate hardware
    if (isWakeFromSleep) {
        // Disable GPIO hold to allow normal operation
        gpio_deep_sleep_hold_dis();
        gpio_hold_dis((gpio_num_t)M5EPD_MAIN_PWR_PIN);

        // Wake display controller
        M5.EPD.Active();
        Serial.println("Display controller reactivated");

        // Give display controller time to fully stabilize after deep sleep
        // (voltage regulators, temperature sensor, waveform initialization)
        delay(200); // 200ms stabilization time
        Serial.println("Display controller stabilized");

        // DEBUG: Print RTC state immediately after wake
        Serial.printf("DEBUG: rtcState.totalMillis = %llu (isValid=%d)\n",
                      rtcState.totalMillis, rtcState.isValid);

        // If this is a timer wake, we need to load config first to know the sleep duration
        // For now, we'll add the sleep time later after config is loaded
    }

    // Read battery level on wake from sleep (but check after canvas init)
    float batteryLevel = 0.0;
    uint32_t batteryVoltage = 0;
    if (isWakeFromSleep) {
        M5.BatteryADCBegin(); // Initialize battery ADC
        batteryVoltage = M5.getBatteryVoltage();
        batteryLevel = getBatteryPercentage();
        Serial.printf("Battery level: %.1f%%\n", batteryLevel);
    }

    // WiFi and Bluetooth are disabled by default (not initialized)
    // WiFi.mode(WIFI_OFF);  // Removed: WiFi library not included
    // btStop();             // Removed: Bluetooth library not included
    Serial.println("WiFi and Bluetooth not initialized (power saving)");

    // Disable external power (sensors like SHT30 temperature/humidity)
    M5.disableEXTPower();
    Serial.println("External sensors power disabled");

    // Set display to VERTICAL orientation
    M5.EPD.SetRotation(90);

    // Create canvas - full screen
    canvas.createCanvas(540, 960);

    // Check battery AFTER canvas is initialized (needed for lowBatteryShutdown display)
    if (isWakeFromSleep && batteryLevel < 5.0) {
        lowBatteryShutdown();
        // Function never returns - device shuts down
    }

    // Show boot message ONLY on cold boot (not on wake from sleep)
    if (!isWakeFromSleep) {
        M5.EPD.Clear(true);     // Clear with full refresh
        Serial.println("Display initialized (vertical orientation)");

        canvas.fillCanvas(0); // white background
        canvas.setTextSize(2); // Bitmap font size 2 = 16px height (consistent with UI)
        canvas.setTextColor(15); // black text

        // Top label: "PaperSpecimen" (same position as font name)
        canvas.setTextDatum(TC_DATUM);
        canvas.drawString("PaperSpecimen", 270, 30);

        // QR code in center
        drawQRCode(270, 480, 6, 6); // 6×6 px per module

        // Bottom label: "v2.2.3" (will show "v2.2.3*" after boot if debug mode activated)
        canvas.setTextDatum(BC_DATUM);
        canvas.drawString("v2.2.3", 270, 930);  // Boot splash always shows "v2.2.3" (asterisk appears after activation)

        canvas.pushCanvas(0, 0, UPDATE_MODE_GC16);
        Serial.println("Boot splash v2.2.3 with QR code displayed");

        // Wait 5 seconds and detect button presses to enter debug mode
        Serial.println("Waiting 5 seconds for debug mode trigger (2+ button presses)...");
        int buttonPressCount = 0;
        unsigned long startTime = millis();
        bool lastButtonState = false;

        while (millis() - startTime < 5000) {
            M5.update(); // Update button states
            bool currentButtonState = M5.BtnP.isPressed();

            // Detect rising edge (button press)
            if (currentButtonState && !lastButtonState) {
                buttonPressCount++;
                Serial.printf("Button press detected (%d/2)\n", buttonPressCount);
            }

            lastButtonState = currentButtonState;
            delay(50); // Check every 50ms
        }

        // Activate debug mode if 2 or more presses detected
        if (buttonPressCount >= 2) {
            rtcState.debugMode = true;
            Serial.println("\n*** DEBUG MODE ACTIVATED ***");
            Serial.println("Debug mode will persist across wake cycles until device reset");

            // Log reset marker to battery log (only in debug mode)
            logBatteryData(0, 0.0, true); // isReset=true
        } else {
            rtcState.debugMode = false;
            Serial.println("Normal mode (debug mode not activated)");
            delay(100); // Allow serial buffer to flush
            Serial.end(); // Disable serial immediately in normal mode to save CPU
        }
    } else {
        Serial.println("Skipping boot screen (wake from sleep)");
    }

    // Initialize refresh tracking
    partialRefreshCount = 0;
    hasPartialSinceLastFull = false;
    firstPartialAfterFullTime = millis();

    // Initialize power management tracking
    lastFullRefreshTime = millis();
    lastButtonActivityTime = millis();

    // Initialize random seed (only on cold boot, not on wake - wake has its own seed init above)
    if (!isWakeFromSleep) {
        uint32_t seed = 0;
        for(int i = 0; i < 10; i++) {
            seed ^= analogRead(0);
            delayMicroseconds(100);
        }
        seed ^= millis() ^ micros();
        randomSeed(seed);
        Serial.printf("Random seed initialized: 0x%08X\n", seed);
    }

    // v2.2.1: SD card access conditional on boot type and cache validity
    // On cold boot: SD is mandatory for initial setup
    // On wake with valid cache: SD is optional (everything loads from cache)
    bool sdAvailable = false;

    if (!isWakeFromSleep) {
        // COLD BOOT: SD card is mandatory
        Serial.println("\n=== Testing microSD (required for cold boot) ===");
        if (!SD.begin()) {
            Serial.println("ERROR: microSD initialization failed!");
            M5.EPD.Clear(true); // Full refresh to clear boot splash
            canvas.fillCanvas(15);
            canvas.setTextColor(0);
            canvas.setTextDatum(CC_DATUM);
            canvas.setTextSize(3);
            canvas.drawString("SD CARD ERROR", 270, 400);
            canvas.setTextSize(2);
            canvas.drawString("Please insert microSD", 270, 500);
            canvas.drawString("with /fonts directory", 270, 540);
            canvas.pushCanvas(0, 0, UPDATE_MODE_GC16);
            while(1) delay(1000); // halt
        }
        Serial.println("microSD initialized successfully");
        sdAvailable = true;

        // Check if /fonts directory exists
        if (!SD.exists("/fonts")) {
            Serial.println("WARNING: /fonts directory not found");
            Serial.println("Creating /fonts directory...");
            SD.mkdir("/fonts");
        }

        // Scan for font files
        scanFonts();

        // Check if fonts were found
        if (fontPaths.empty()) {
            Serial.println("ERROR: No fonts found!");
            M5.EPD.Clear(true); // Full refresh to clear boot splash
            canvas.fillCanvas(15);
            canvas.setTextColor(0);
            canvas.setTextDatum(CC_DATUM);
            canvas.setTextSize(3);
            canvas.drawString("NO FONTS FOUND", 270, 400);
            canvas.setTextSize(2);
            canvas.drawString("Add .ttf or .otf files", 270, 500);
            canvas.drawString("to /fonts directory", 270, 540);
            canvas.pushCanvas(0, 0, UPDATE_MODE_GC16);
            while(1) delay(1000); // halt
        }
    } else {
        // WAKE FROM SLEEP: SD required
        Serial.println("\n=== Testing microSD (required) ===");
        sdAvailable = SD.begin();

        if (sdAvailable) {
            Serial.println("microSD available");
            scanFonts();
        } else {
            Serial.println("✗ ERROR: SD card not available at wake!");

            M5.EPD.Clear(true); // Full refresh to clear previous font specimen
            canvas.fillCanvas(15);
            canvas.setTextColor(0);
            canvas.setTextDatum(CC_DATUM);
            canvas.setTextSize(3);
            canvas.drawString("SD CARD ERROR", 270, 400);
            canvas.setTextSize(2);
            canvas.drawString("Insert SD card and reset", 270, 500);
            canvas.pushCanvas(0, 0, UPDATE_MODE_GC16);
            while(1) delay(1000); // halt
        }
    }

    // v2.2: Config handling
    if (!isWakeFromSleep) {
        // COLD BOOT: Always run unified setup screen
        Serial.println("\n=== Cold Boot: Running Setup ===");

        // Initialize default config
        initDefaultConfig(fontPaths.size());

        // Unified setup screen (interval + font selection in one)
        setupScreenUnified();

        // Save config to SD (will be used for wake from sleep)
        if (saveConfig()) {
            Serial.println("Setup complete - config saved for wake cycles");
        } else {
            Serial.println("WARNING: Failed to save config file");
        }
    } else {
        // WAKE FROM SLEEP: Load config from previous session
        Serial.println("\n=== Wake from Sleep: Loading Config ===");
        bool configLoaded = loadConfig();

        if (!configLoaded) {
            // Should not happen - use defaults
            Serial.println("WARNING: Config file missing after wake - using defaults");
            initDefaultConfig(fontPaths.size());
        } else {
            Serial.println("Config loaded successfully");
        }
    }

    // Apply config: filter fontPaths to only enabled fonts (always, for both cold boot and wake)
    Serial.println("\n=== Applying Config ===");
    std::vector<String> enabledFontPaths;
    for (size_t i = 0; i < fontPaths.size() && i < config.fontEnabled.size(); i++) {
        if (config.fontEnabled[i]) {
            enabledFontPaths.push_back(fontPaths[i]);
            Serial.printf("  Enabled: %s\n", fontPaths[i].c_str());
        } else {
            Serial.printf("  Disabled: %s\n", fontPaths[i].c_str());
        }
    }

    // Replace fontPaths with filtered list
    fontPaths = enabledFontPaths;
    Serial.printf("Active fonts: %d\n", fontPaths.size());

    // Check if at least one font is enabled
    if (fontPaths.empty()) {
        Serial.println("ERROR: No fonts enabled in config!");
        canvas.fillCanvas(15);
        canvas.setTextColor(0);
        canvas.setTextDatum(CC_DATUM);
        canvas.setTextSize(3);
        canvas.drawString("NO FONTS ENABLED", 270, 400);
        canvas.setTextSize(2);
        canvas.drawString("Delete /paperspecimen.cfg", 270, 500);
        canvas.drawString("and restart to reconfigure", 270, 540);
        canvas.pushCanvas(0, 0, UPDATE_MODE_GC16);
        while(1) delay(1000); // halt
    }

    // Handle wake from sleep scenarios
    if (isWakeFromSleep && rtcState.isValid) {
        // STEP 5: Restore view mode from RTC memory
        currentViewMode = rtcState.viewMode;
        Serial.printf("Restored view mode: %s\n", currentViewMode == BITMAP ? "BITMAP" : "OUTLINE");

        // Add sleep duration to total uptime (only for timer wakes, not button wakes)
        if (isAutoWake) {
            uint64_t sleepMillis = (uint64_t)config.wakeIntervalMinutes * 60 * 1000;
            Serial.printf("DEBUG: Adding sleep time to uptime: %llu ms (%d minutes)\n",
                          sleepMillis, config.wakeIntervalMinutes);
            rtcState.totalMillis += sleepMillis;
            Serial.printf("DEBUG: Updated rtcState.totalMillis = %llu\n", rtcState.totalMillis);
        }

        // Calculate and display current uptime (accumulated + current session)
        uint64_t currentTotalMillis = rtcState.totalMillis + millis();
        uint32_t totalMinutes = currentTotalMillis / 60000;
        Serial.printf("Uptime: %lu minutes (%lud %02luh %02lum)\n",
                      totalMinutes,
                      totalMinutes / (24 * 60),
                      (totalMinutes % (24 * 60)) / 60,
                      totalMinutes % 60);

        // Log battery data to .battery file (only in debug mode)
        if (rtcState.debugMode) {
            logBatteryData(batteryVoltage, batteryLevel, false);
        }

        if (isAutoWake) {
            // Auto-wake from RTC alarm: randomize based on config settings
            Serial.println("\n=== Auto-wake: Applying randomization settings ===");
            Serial.printf("Allow different font: %s\n", config.allowDifferentFont ? "yes" : "no");
            Serial.printf("Allow different mode: %s\n", config.allowDifferentMode ? "yes" : "no");

            // Randomize font if allowed
            if (config.allowDifferentFont) {
                currentFontIndex = random(0, fontPaths.size());
                Serial.printf("Random font selected: %d/%d\n", currentFontIndex + 1, fontPaths.size());
            } else {
                // Keep current font from RTC memory
                currentFontIndex = rtcState.currentFontIndex;
                if (currentFontIndex >= fontPaths.size()) {
                    Serial.println("WARNING: Saved font index out of range, resetting to 0");
                    currentFontIndex = 0;
                }
                Serial.printf("Keeping current font: %d/%d\n", currentFontIndex + 1, fontPaths.size());
            }

            // Randomize mode if allowed
            if (config.allowDifferentMode) {
                // Use ESP32 hardware RNG instead of Arduino random() which has issues after deep sleep
                currentViewMode = ((esp_random() % 2) == 0) ? BITMAP : OUTLINE;
                Serial.printf("Random mode selected: %s\n", currentViewMode == BITMAP ? "BITMAP" : "OUTLINE");
            } else {
                // Keep current mode from RTC memory (already restored above)
                Serial.printf("Keeping current mode: %s\n", currentViewMode == BITMAP ? "BITMAP" : "OUTLINE");
            }

            // Load font and generate random glyph
            if (loadCurrentFont()) {
                currentGlyphCodepoint = getRandomGlyphCodepoint();

                // If no valid glyph found, try other fonts
                int attempts = 0;
                while (currentGlyphCodepoint == 0 && attempts < fontPaths.size()) {
                    Serial.println("No valid glyphs in this font, trying next...");
                    currentFontIndex = (currentFontIndex + 1) % fontPaths.size();
                    if (loadCurrentFont()) {
                        currentGlyphCodepoint = getRandomGlyphCodepoint();
                    }
                    attempts++;
                }

                if (currentGlyphCodepoint != 0) {
                    renderGlyph();
                } else {
                    Serial.println("WARNING: No fonts with valid glyphs, skipping render");
                }
            }
        } else {
            // Button wake: restore previous state
            Serial.println("\n=== Restoring state from RTC memory ===");
            currentFontIndex = rtcState.currentFontIndex;
            currentGlyphCodepoint = rtcState.currentGlyphCodepoint;

            // Validate font index
            if (currentFontIndex >= fontPaths.size()) {
                Serial.println("WARNING: Saved font index out of range, resetting to 0");
                currentFontIndex = 0;
            }

            Serial.printf("Restored: font=%d/%d, glyph=U+%04X\n",
                          currentFontIndex + 1, fontPaths.size(), currentGlyphCodepoint);

            // Load the font and render the glyph
            if (loadCurrentFont()) {
                renderGlyph();
            } else {
                Serial.println("ERROR: Failed to restore font");
            }
        }
    } else {
        // Cold boot: Load first font and display random glyph
        Serial.println("\n=== Loading initial font ===");

        // Randomize initial view mode if allowed (applies to cold boot only)
        if (config.allowDifferentMode) {
            // Use ESP32 hardware RNG for better randomness
            currentViewMode = ((esp_random() % 2) == 0) ? BITMAP : OUTLINE;
            Serial.printf("Initial random mode selected: %s\n", currentViewMode == BITMAP ? "BITMAP" : "OUTLINE");
        } else {
            // Keep default BITMAP mode
            Serial.printf("Initial mode (default): %s\n", currentViewMode == BITMAP ? "BITMAP" : "OUTLINE");
        }

        if (loadCurrentFont()) {
            currentGlyphCodepoint = getRandomGlyphCodepoint();

            // If no valid glyph found in first font, try fonts until we find one with glyphs
            int attempts = 0;
            while (currentGlyphCodepoint == 0 && attempts < fontPaths.size()) {
                Serial.println("No valid glyphs in this font, trying next...");
                currentFontIndex = (currentFontIndex + 1) % fontPaths.size();
                if (loadCurrentFont()) {
                    currentGlyphCodepoint = getRandomGlyphCodepoint();
                }
                attempts++;
            }

            if (currentGlyphCodepoint == 0) {
                Serial.println("CRITICAL ERROR: No fonts with valid glyphs found!");
                // Show error on screen
                canvas.fillCanvas(15);
                canvas.setTextColor(0);
                canvas.setTextDatum(CC_DATUM);
                canvas.setTextSize(2);
                canvas.drawString("NO COMPATIBLE FONTS", 270, 400);
                canvas.drawString("Check font files", 270, 450);
                canvas.pushCanvas(0, 0, UPDATE_MODE_GC16);
                while(1) delay(1000); // Halt
            }

            renderGlyph();

            // Full refresh after first render to clear boot screen ghosting
            Serial.println("Initial full refresh to clear boot screen ghosting");
            M5.EPD.UpdateFull(UPDATE_MODE_GC16);
            lastFullRefreshTime = millis();

            // STEP 1 TEST: Try to access FT_Face outline data
            testGlyphOutlineAccess(currentGlyphCodepoint);
        } else {
            Serial.println("ERROR: Failed to load initial font");
            canvas.fillCanvas(15);
            canvas.setTextColor(0);
            canvas.setTextDatum(CC_DATUM);
            canvas.setTextSize(2);
            canvas.drawString("FONT LOAD ERROR", 270, 400);
            canvas.drawString("Check Serial output", 270, 450);
            canvas.drawString("for details", 270, 480);
            canvas.pushCanvas(0, 0, UPDATE_MODE_GC16);
            Serial.println("\n!!! HALTED - Check font files !!!");
            while(1) delay(1000); // halt
        }
    }

    Serial.println("\n=== Setup Complete ===");
    Serial.println("Controls:");
    Serial.println("  Wheel UP (BtnR): Next font");
    Serial.println("  Wheel DOWN (BtnL): Previous font");
    Serial.println("  Wheel PUSH (BtnP): Random glyph");
    Serial.println("");
}

void loop() {
    M5.update(); // Update button states

    // Auto full refresh after 10s timeout if there was at least 1 partial
    if (hasPartialSinceLastFull &&
        (millis() - firstPartialAfterFullTime >= FULL_REFRESH_TIMEOUT_MS)) {
        Serial.println(">>> Auto full refresh after 10s timeout - cleaning ghosting");
        M5.EPD.UpdateFull(UPDATE_MODE_GC16);
        partialRefreshCount = 0;
        hasPartialSinceLastFull = false;
        lastFullRefreshTime = millis(); // Track for power management
    }

    // Power management: Enter deep sleep based on context
    // Special case: Auto-wake session (timer wake) - sleep immediately after full refresh
    if (isAutoWakeSession && millis() - lastFullRefreshTime >= 600) {
        // Give 600ms for full refresh (GC16 ~450-500ms) to complete physically, then sleep immediately
        // No need to wait 10s idle time - this is automatic operation
        Serial.println(">>> Auto-wake session: entering deep sleep immediately after refresh");
        enterDeepSleep(); // This function never returns (enters deep sleep)
    }

    // Normal case: Enter deep sleep after 10s from last full refresh if no button activity
    unsigned long timeSinceFullRefresh = millis() - lastFullRefreshTime;
    unsigned long timeSinceButtonActivity = millis() - lastButtonActivityTime;

    // Enter deep sleep only if:
    // 1) 10 seconds passed since last full refresh
    // 2) No button activity after the last full refresh
    if (!isAutoWakeSession &&
        timeSinceFullRefresh >= DEEP_SLEEP_TIMEOUT_MS &&
        lastButtonActivityTime <= lastFullRefreshTime) {
        enterDeepSleep(); // This function never returns (enters deep sleep)
    }

    // Button L (Wheel DOWN): Previous font (keep same glyph)
    if (M5.BtnL.wasPressed()) {
        lastButtonActivityTime = millis();
        isAutoWakeSession = false; // User interaction: cancel auto-wake session immediate sleep
        Serial.println("\n>>> Button L (DOWN) pressed - Previous font");
        previousFont();
        delay(300); // Simple delay to prevent multiple triggers
    }

    // Button R (Wheel UP): Next font (keep same glyph)
    if (M5.BtnR.wasPressed()) {
        lastButtonActivityTime = millis();
        isAutoWakeSession = false; // User interaction: cancel auto-wake session immediate sleep
        Serial.println("\n>>> Button R (UP) pressed - Next font");
        nextFont();
        delay(300); // Simple delay to prevent multiple triggers
    }

    // STEP 5: Button P - Very long press (5s) = shutdown, Long press = toggle view, Short press = random glyph
    static unsigned long btnPressStartTime = 0;
    static bool btnPWasDown = false;
    const unsigned long SHUTDOWN_PRESS_THRESHOLD = 5000; // 5 seconds for shutdown
    const unsigned long LONG_PRESS_THRESHOLD = 800; // 800ms for long press

    if (M5.BtnP.isPressed() && !btnPWasDown) {
        // Button just pressed
        btnPressStartTime = millis();
        btnPWasDown = true;
    }

    // Check for shutdown (5 seconds) while button is held
    if (M5.BtnP.isPressed() && btnPWasDown) {
        unsigned long pressDuration = millis() - btnPressStartTime;
        if (pressDuration >= SHUTDOWN_PRESS_THRESHOLD) {
            // Very long press (5s): Shutdown with screen
            btnPWasDown = false; // Reset state
            shutdownWithScreen(); // This function never returns
        }
    }

    if (!M5.BtnP.isPressed() && btnPWasDown) {
        // Button just released
        unsigned long pressDuration = millis() - btnPressStartTime;
        btnPWasDown = false;
        lastButtonActivityTime = millis();
        isAutoWakeSession = false; // User interaction: cancel auto-wake session immediate sleep

        if (pressDuration >= LONG_PRESS_THRESHOLD) {
            // Long press (800ms): Toggle view mode
            Serial.println("\n>>> Button P LONG PRESS - Toggle view mode");
            currentViewMode = (currentViewMode == BITMAP) ? OUTLINE : BITMAP;
            Serial.printf("Switched to %s mode\n", currentViewMode == BITMAP ? "BITMAP" : "OUTLINE");

            // Re-render with new mode
            renderGlyph();
        } else {
            // Short press: Random glyph
            Serial.println("\n>>> Button P (PUSH) pressed - Random glyph");
            randomGlyph();
        }
        delay(300); // Simple delay to prevent multiple triggers
    }

    delay(50); // Reduced delay for more responsive buttons
}