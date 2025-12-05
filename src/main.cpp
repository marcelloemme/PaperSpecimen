#include <M5EPD.h>
#include <WiFi.h>
#include <SD.h>
#include <vector>
#include <esp_sleep.h>

// FreeType headers for outline access
#include "freetype/freetype.h"
#include "freetype/ftoutln.h"

// Canvas for rendering
M5EPD_Canvas canvas(&M5.EPD);  // Single full screen canvas for everything

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

// RTC memory structure to persist state across deep sleep
RTC_DATA_ATTR struct {
    bool isValid;
    int currentFontIndex;
    uint32_t currentGlyphCodepoint;
} rtcState = {false, 0, 0x0041};

// Font management
std::vector<String> fontPaths;
int currentFontIndex = 0;
bool fontLoaded = false;

// Glyph rendering
const int GLYPH_SIZE = 375; // Balanced size for specimen display
uint32_t currentGlyphCodepoint = 0x0041; // Start with 'A'

// Unicode ranges for random glyph selection
struct UnicodeRange {
    uint32_t start;
    uint32_t end;
    const char* name;
};

// Common Unicode ranges for typography specimens
// Weighted to favor ranges most likely to exist in fonts
const UnicodeRange glyphRanges[] = {
    {0x0041, 0x005A, "Latin Uppercase"},       // A-Z (highly likely)
    {0x0061, 0x007A, "Latin Lowercase"},       // a-z (highly likely)
    {0x0030, 0x0039, "Digits"},                // 0-9 (highly likely)
    {0x0021, 0x002F, "Basic Punctuation 1"},   // !"#$%&'()*+,-./
    {0x003A, 0x0040, "Basic Punctuation 2"},   // :;<=>?@
    {0x005B, 0x0060, "Basic Punctuation 3"},   // [\]^_`
    {0x007B, 0x007E, "Basic Punctuation 4"},   // {|}~
    {0x00A1, 0x00BF, "Latin-1 Punctuation"},   // ¡-¿
    {0x00C0, 0x00FF, "Latin-1 Letters"},       // À-ÿ accented
};
const int numGlyphRanges = sizeof(glyphRanges) / sizeof(glyphRanges[0]);

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
        canvas.destoryRender(GLYPH_SIZE);   // Free glyph cache
        canvas.unloadFont();                // Then unload font
        fontLoaded = false;
        delay(100);  // Give system time to free memory
    }

    String fontPath = fontPaths[currentFontIndex];
    Serial.printf("\n=== Loading font: %s ===\n", fontPath.c_str());

    // Check if file exists
    if (!SD.exists(fontPath)) {
        Serial.println("ERROR: Font file does not exist!");
        return false;
    }

    File fontFile = SD.open(fontPath);
    if (!fontFile) {
        Serial.println("ERROR: Cannot open font file!");
        return false;
    }
    Serial.printf("Font file size: %d bytes\n", fontFile.size());
    fontFile.close();

    // Load font from SD onto canvas
    Serial.println("Calling canvas.loadFont()...");
    esp_err_t loadResult = canvas.loadFont(fontPath, SD);
    Serial.printf("loadFont() returned: %d (ESP_OK=%d)\n", loadResult, ESP_OK);

    if (loadResult != ESP_OK) {
        Serial.println("ERROR: Failed to load font from SD");
        return false;
    }
    Serial.println("Font loaded from SD successfully");

    // Create render cache for label size (24px)
    Serial.printf("Creating render cache for labels (size=24, cache=64)...\n");
    esp_err_t renderResult24 = canvas.createRender(24, 64);
    Serial.printf("createRender(24) returned: %d\n", renderResult24);

    if (renderResult24 != ESP_OK) {
        Serial.println("ERROR: Failed to create render cache for size 24");
        canvas.unloadFont();
        return false;
    }

    // Create render cache for glyph size (375px)
    Serial.printf("Creating render cache for glyph (size=%d, cache=12)...\n", GLYPH_SIZE);
    esp_err_t renderResult375 = canvas.createRender(GLYPH_SIZE, 12);
    Serial.printf("createRender(%d) returned: %d\n", GLYPH_SIZE, renderResult375);

    if (renderResult375 != ESP_OK) {
        Serial.println("ERROR: Failed to create render cache for glyph size");
        canvas.destoryRender(24);  // Clean up label cache
        canvas.unloadFont();
        return false;
    }

    fontLoaded = true;
    Serial.println("=== Font loaded successfully! ===\n");
    return true;
}

// ========================================
// STEP 1: FT_Face Access Test (Proof of Concept)
// ========================================

// Test function to access FT_Face and print glyph outline info
void testGlyphOutlineAccess(uint32_t codepoint) {
    Serial.println("\n=== STEP 1: Testing FT_Face Access ===");

    // WORKAROUND: Access protected _font_face from TFT_eSPI
    // We need to extract FT_Face from M5EPD_Canvas which inherits from TFT_eSPI
    // The _font_face member is protected, so we use a memory layout trick

    // TFT_eSPI has a static font_face_t _font_face member
    // We can access it via pointer arithmetic (HACK but works without library mod)

    // Alternative: Try to get it through public methods first
    // canvas.loadFont() loads the font into _font_face, but doesn't expose it

    // For now, let's try to call FreeType functions directly
    // We'll need the FT_Face pointer which is inside canvas object

    // TEMPORARY TEST: Just verify we can include FreeType headers
    Serial.println("FreeType headers included successfully");
    Serial.printf("Testing with codepoint: U+%04X\n", codepoint);

    // TODO: Next step will be to actually access FT_Face
    // For now, this is just a compilation test

    Serial.println("=== STEP 1: Compilation test PASSED ===\n");
}

// Generate random glyph codepoint from common ranges
uint32_t getRandomGlyphCodepoint() {
    // Re-seed with more entropy each time for better randomness
    randomSeed(analogRead(0) ^ millis() ^ micros());

    // Pick random range
    int rangeIndex = random(0, numGlyphRanges);
    const UnicodeRange& range = glyphRanges[rangeIndex];

    // Pick random codepoint in range
    uint32_t codepoint = random(range.start, range.end + 1);

    Serial.printf("Random glyph: U+%04X from %s (range %d/%d)\n",
                  codepoint, range.name, rangeIndex + 1, numGlyphRanges);
    return codepoint;
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

// Render current glyph
void renderGlyph() {
    if (!fontLoaded) {
        Serial.println("ERROR: No font loaded");
        return;
    }

    // STRATEGY: Single canvas with mixed fonts
    //           - Built-in font for labels (top/bottom)
    //           - TTF font for main glyph (center)

    int centerX = 270;
    int centerY = 480;

    // Clear canvas - white background
    canvas.fillCanvas(0); // 0 = white (inverted on e-paper)

    String fontName = getFontName(fontPaths[currentFontIndex]);
    char codepointStr[32];
    sprintf(codepointStr, "U+%04X", currentGlyphCodepoint);

    // Draw top label with TTF font (small size)
    canvas.setTextSize(24);
    canvas.setTextColor(15); // 15 = black (inverted on e-paper)
    canvas.setTextDatum(TC_DATUM); // Top-center
    canvas.drawString(fontName, centerX, 30);

    // Draw main glyph with TTF font (center)
    canvas.setTextSize(GLYPH_SIZE);
    canvas.setTextColor(15); // 15 = black (inverted on e-paper)
    canvas.setTextDatum(CC_DATUM); // Center-center alignment

    String glyphStr = codepointToString(currentGlyphCodepoint);
    canvas.drawString(glyphStr, centerX, centerY);

    // Draw bottom label with TTF font (small size)
    canvas.setTextSize(24);
    canvas.setTextColor(15); // 15 = black (inverted on e-paper)
    canvas.setTextDatum(BC_DATUM); // Bottom-center
    canvas.drawString(codepointStr, centerX, 930);

    // Push entire canvas to display
    canvas.pushCanvas(0, 0, UPDATE_MODE_GL16);

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

    Serial.printf("Rendered: %s (U+%04X) with font %s\n",
                  glyphStr.c_str(), currentGlyphCodepoint, fontName.c_str());
}

// Change to next font
void nextFont() {
    if (fontPaths.empty()) return;

    currentFontIndex = (currentFontIndex + 1) % fontPaths.size();
    Serial.printf("Switching to font %d/%d\n", currentFontIndex + 1, fontPaths.size());

    if (loadCurrentFont()) {
        renderGlyph();
    }
}

// Change to previous font
void previousFont() {
    if (fontPaths.empty()) return;

    currentFontIndex--;
    if (currentFontIndex < 0) {
        currentFontIndex = fontPaths.size() - 1;
    }
    Serial.printf("Switching to font %d/%d\n", currentFontIndex + 1, fontPaths.size());

    if (loadCurrentFont()) {
        renderGlyph();
    }
}

// Generate and display new random glyph
void randomGlyph() {
    if (!fontLoaded) return;

    currentGlyphCodepoint = getRandomGlyphCodepoint();
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
    uint32_t voltage = M5.getBatteryVoltage();
    Serial.printf("Battery voltage: %dmV\n", voltage);

    // LiPo voltage range: 4200mV (100%) to 3300mV (0%)
    // 5% threshold = ~3345mV
    if (voltage >= 4200) return 100.0;
    if (voltage <= 3300) return 0.0;

    float percentage = ((float)(voltage - 3300) / (4200 - 3300)) * 100.0;
    return percentage;
}

// Display low battery icon and shutdown
void lowBatteryShutdown() {
    Serial.println("\n!!! LOW BATTERY - SHUTTING DOWN !!!");

    // Clear screen
    M5.EPD.Clear(true);
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

    // Push to display
    canvas.pushCanvas(0, 0, UPDATE_MODE_GC16);
    delay(2000); // Show for 2 seconds

    // Complete shutdown (only RTC stays powered)
    Serial.println("Entering complete shutdown...");
    M5.shutdown();
}

// Save state and enter deep sleep
void enterDeepSleep() {
    Serial.println("\n>>> Preparing for deep sleep...");

    // Save current state to RTC memory
    rtcState.isValid = true;
    rtcState.currentFontIndex = currentFontIndex;
    rtcState.currentGlyphCodepoint = currentGlyphCodepoint;
    Serial.printf("State saved: font=%d, glyph=U+%04X\n",
                  rtcState.currentFontIndex, rtcState.currentGlyphCodepoint);

    // Put display controller in standby
    M5.EPD.StandBy();
    Serial.println("Display controller in StandBy");

    // Keep GPIO2 (M5EPD_MAIN_PWR_PIN) HIGH during deep sleep to maintain power
    gpio_hold_en((gpio_num_t)M5EPD_MAIN_PWR_PIN);
    gpio_deep_sleep_hold_en();
    Serial.println("GPIO hold enabled");

    // Configure wake sources:
    // 1) GPIO38 (center button press) - user interaction
    // 2) Timer (15 minutes) - auto refresh
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_38, LOW); // Wake when button pressed (goes LOW)

    // Enable timer wakeup: 15 minutes = 900 seconds = 900,000,000 microseconds
    uint64_t wakeup_time_us = 15 * 60 * 1000000ULL; // 15 minutes in microseconds
    esp_sleep_enable_timer_wakeup(wakeup_time_us);
    Serial.println("Wake sources configured: GPIO38 (button) + Timer (15min)");

    Serial.println(">>> Entering deep sleep now...\n");
    delay(100); // Give serial time to flush

    // Enter deep sleep
    esp_deep_sleep_start();
}

void setup() {
    // Initialize M5Paper with minimal power consumption
    // Parameters: touchEnable, SDEnable, SerialEnable, BatteryADCEnable, I2CEnable
    M5.begin(false, true, true, false, true);
    // false: Touch disabled (saves power)
    // true:  SD enabled (needed for fonts)
    // true:  Serial enabled (for debugging)
    // false: Battery ADC disabled initially (enable only when needed)
    // true:  I2C enabled (needed for RTC)

    M5.RTC.begin();

    // Setup Serial for debugging
    Serial.begin(115200);
    delay(100); // Wait for serial to be ready

    // Check wake reason
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    // Determine wake source
    bool isWakeFromSleep = false;
    bool isAutoWake = false;

    if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        // Woke from timer (15 minute auto-wake)
        isWakeFromSleep = true;
        isAutoWake = true;
        Serial.println("\n\n=== PaperSpecimen Auto-Wake (Timer) ===");
        Serial.println("Woke by ESP32 timer after 15 minutes");
    } else if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        // Woke from button press
        isWakeFromSleep = true;
        isAutoWake = false;
        Serial.println("\n\n=== PaperSpecimen Wake from Deep Sleep (Button) ===");
        Serial.println("Woke by GPIO38 (center button)");
    } else {
        // Cold boot (reset or first power on)
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
    }

    // Check battery level on wake from sleep
    if (isWakeFromSleep) {
        M5.BatteryADCBegin(); // Initialize battery ADC
        float batteryLevel = getBatteryPercentage();
        Serial.printf("Battery level: %.1f%%\n", batteryLevel);

        // If battery is below 5%, show low battery screen and shutdown
        if (batteryLevel < 5.0) {
            lowBatteryShutdown();
            // Function never returns - device shuts down
        }
    }

    // Disable WiFi and Bluetooth for power saving
    WiFi.mode(WIFI_OFF);
    btStop();
    Serial.println("WiFi and Bluetooth disabled");

    // Disable external power (sensors like SHT30 temperature/humidity)
    M5.disableEXTPower();
    Serial.println("External sensors power disabled");

    // Set display to VERTICAL orientation
    M5.EPD.SetRotation(90);

    // Create canvas - full screen
    canvas.createCanvas(540, 960);

    // Show boot message ONLY on cold boot (not on wake from sleep)
    if (!isWakeFromSleep) {
        M5.EPD.Clear(true);     // Clear with full refresh
        Serial.println("Display initialized (vertical orientation)");

        canvas.fillCanvas(15); // white background
        canvas.setTextSize(4);
        canvas.setTextColor(0); // black text
        canvas.setTextDatum(CC_DATUM);
        canvas.drawString("PaperSpecimen", 270, 400);
        canvas.setTextSize(2);
        canvas.drawString("Booting...", 270, 500);
        canvas.pushCanvas(0, 0, UPDATE_MODE_GC16);
        Serial.println("Boot message displayed");
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

    // Initialize random seed with multiple entropy sources
    uint32_t seed = 0;
    for(int i = 0; i < 10; i++) {
        seed ^= analogRead(0);
        delayMicroseconds(100);
    }
    seed ^= millis() ^ micros();
    randomSeed(seed);
    Serial.printf("Random seed initialized: 0x%08X\n", seed);

    // Test microSD access
    Serial.println("\n=== Testing microSD ===");
    if (!SD.begin()) {
        Serial.println("ERROR: microSD initialization failed!");
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

    // Handle wake from sleep scenarios
    if (isWakeFromSleep && rtcState.isValid) {
        if (isAutoWake) {
            // Auto-wake from RTC alarm: randomize BOTH font and glyph
            Serial.println("\n=== Auto-wake: Generating random font + glyph ===");
            randomFont(); // This loads random font and random glyph
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
        if (loadCurrentFont()) {
            currentGlyphCodepoint = getRandomGlyphCodepoint();
            renderGlyph();

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

    // Power management: Enter deep sleep after 10s from last full refresh if no button activity
    unsigned long timeSinceFullRefresh = millis() - lastFullRefreshTime;
    unsigned long timeSinceButtonActivity = millis() - lastButtonActivityTime;

    // Enter deep sleep only if:
    // 1) 10 seconds passed since last full refresh
    // 2) No button activity after the last full refresh
    if (timeSinceFullRefresh >= DEEP_SLEEP_TIMEOUT_MS &&
        lastButtonActivityTime <= lastFullRefreshTime) {
        enterDeepSleep(); // This function never returns (enters deep sleep)
    }

    // Button L (Wheel DOWN): Previous font (keep same glyph)
    if (M5.BtnL.wasPressed()) {
        lastButtonActivityTime = millis();
        Serial.println("\n>>> Button L (DOWN) pressed - Previous font");
        previousFont();
        delay(300); // Simple delay to prevent multiple triggers
    }

    // Button R (Wheel UP): Next font (keep same glyph)
    if (M5.BtnR.wasPressed()) {
        lastButtonActivityTime = millis();
        Serial.println("\n>>> Button R (UP) pressed - Next font");
        nextFont();
        delay(300); // Simple delay to prevent multiple triggers
    }

    // Button P (Wheel PUSH): Random glyph (same font)
    if (M5.BtnP.wasPressed()) {
        lastButtonActivityTime = millis();
        Serial.println("\n>>> Button P (PUSH) pressed - Random glyph");
        randomGlyph();
        delay(300); // Simple delay to prevent multiple triggers
    }

    delay(50); // Reduced delay for more responsive buttons
}