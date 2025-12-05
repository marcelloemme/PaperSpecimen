#include <M5EPD.h>
#include <WiFi.h>
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

// View mode enumeration
enum ViewMode {
    BITMAP,    // Normal bitmap rendering (v1.0 style)
    OUTLINE    // Vector outline with points (v2.0)
};

// RTC memory structure to persist state across deep sleep
RTC_DATA_ATTR struct {
    bool isValid;
    int currentFontIndex;
    uint32_t currentGlyphCodepoint;
    ViewMode viewMode;  // STEP 5: Persist view mode across sleep
} rtcState = {false, 0, 0x0041, BITMAP};  // Default: BITMAP mode

// Font management
std::vector<String> fontPaths;
int currentFontIndex = 0;
bool fontLoaded = false;

// Glyph rendering
const int GLYPH_SIZE = 375; // Balanced size for specimen display
uint32_t currentGlyphCodepoint = 0x0041; // Start with 'A'

// STEP 5: Current view mode
ViewMode currentViewMode = BITMAP;  // Start in bitmap mode (v1.0 default)

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

// Extract font name from path (forward declaration needed)
String getFontName(const String& path) {
    int lastSlash = path.lastIndexOf('/');
    String filename = path.substring(lastSlash + 1);
    int lastDot = filename.lastIndexOf('.');
    if (lastDot > 0) {
        filename = filename.substring(0, lastDot);
    }
    return filename;
}

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

    // Target size: 375px (scale each glyph to fill screen optimally)
    float target_size = 375.0f;
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
                canvas.drawLine(curr_x, curr_y, seg.x, seg.y, 15); // 15 = black
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
                        canvas.drawLine(curr_x, curr_y, bx, by, 15);
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
                        canvas.drawLine(curr_x, curr_y, bx, by, 15);
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

    canvas.setTextSize(24);
    canvas.setTextColor(15);
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString(fontName, 270, 30); // Top label

    canvas.setTextDatum(BC_DATUM);
    canvas.drawString(codepointStr, 270, 930); // Bottom label

    // Push to display with smart refresh logic
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

    // Target size: 375px (scale each glyph to fill screen optimally)
    float target_size = 375.0f;
    float scale_factor = target_size / (width > height ? width : height);

    // Calculate pixel size needed
    int pixel_size = (int)(scale_factor * face->units_per_EM / 64.0f);
    if (pixel_size < 1) pixel_size = 1;
    if (pixel_size > 500) pixel_size = 500; // Safety limit

    Serial.printf("Glyph bbox: w=%.1f h=%.1f, scale=%.4f, pixel_size=%d\n",
                  width, height, scale_factor, pixel_size);

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

    canvas.setTextSize(24);
    canvas.setTextColor(15);
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString(fontName, centerX, 30);

    canvas.setTextSize(24);
    canvas.setTextColor(15);
    canvas.setTextDatum(BC_DATUM);
    canvas.drawString(codepointStr, centerX, 930);

    // Push to display
    canvas.pushCanvas(0, 0, UPDATE_MODE_GL16);

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
        // STEP 5: Restore view mode from RTC memory
        currentViewMode = rtcState.viewMode;
        Serial.printf("Restored view mode: %s\n", currentViewMode == BITMAP ? "BITMAP" : "OUTLINE");

        if (isAutoWake) {
            // Auto-wake from RTC alarm: randomize BOTH font and glyph (keep view mode)
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

    // STEP 5: Button P - Long press = toggle view, Short press = random glyph
    static unsigned long btnPressStartTime = 0;
    static bool btnPWasDown = false;
    const unsigned long LONG_PRESS_THRESHOLD = 800; // 800ms for long press

    if (M5.BtnP.isPressed() && !btnPWasDown) {
        // Button just pressed
        btnPressStartTime = millis();
        btnPWasDown = true;
    }

    if (!M5.BtnP.isPressed() && btnPWasDown) {
        // Button just released
        unsigned long pressDuration = millis() - btnPressStartTime;
        btnPWasDown = false;
        lastButtonActivityTime = millis();

        if (pressDuration >= LONG_PRESS_THRESHOLD) {
            // Long press: Toggle view mode
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