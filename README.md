# PaperSpecimen

Font glyph viewer per M5Paper - Visualizza e confronta glifi da font TrueType/OpenType con rendering anti-aliased di alta qualità.

## Setup MicroSD

1. **Formatta la microSD in FAT32**
2. **Crea la cartella** `/fonts` nella root della SD
3. **Copia i tuoi font** (`.ttf` o `.otf`) nella cartella `/fonts`

Esempio struttura:
```
/
└── fonts/
    ├── Helvetica-Regular.ttf
    ├── Helvetica-Bold.ttf
    ├── TimesNewRoman.ttf
    └── YourFont.ttf
```

## Controlli

### Rotella Laterale
- **Su (BtnR)**: Font successivo (mantiene stesso glifo)
- **Giù (BtnL)**: Font precedente (mantiene stesso glifo)

### Rotella Centrale
- **Pressione (BtnP)**: Glifo casuale (mantiene stesso font)

## Funzionalità

### Display
- **Orientamento**: Verticale (540x960)
- **Qualità rendering**: 16 livelli di grigio con anti-aliasing
- **Dimensione glifi**: 250px per massima leggibilità

### Refresh Intelligente
- **Partial refresh** (UPDATE_MODE_GL16): rendering normale veloce
- **Full refresh** (UPDATE_MODE_GC16): automatico dopo:
  - 5 partial refresh E almeno 10 secondi trascorsi
  - Oppure 10 secondi trascorsi (anche con <5 partial)
- Permette uso rapido senza flickering eccessivo

### Info Visualizzate
- **Glifo**: Centrato a schermo, dimensione grande
- **Nome font**: In basso (es. "Helvetica-Bold")
- **Codepoint Unicode**: In basso (es. "U+0041")
- **Posizione font**: Angolo basso-destra (es. "2/5")

### Unicode Ranges Supportati
Il generatore random seleziona glifi da 14 ranges Unicode comuni:
- Basic Latin (ASCII)
- Latin Extended A/B
- Latin-1 Supplement (caratteri accentati)
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

## Compilazione & Upload

### Compilare
```bash
pio run
```

### Upload su M5Paper
```bash
pio run --target upload
```

### Monitor Seriale (debug)
```bash
pio device monitor -b 115200
```

## Specifiche Tecniche

### Hardware
- **Device**: M5Paper (ESP32, non S3)
- **Display**: 960x540 4.7" e-ink, 16 grayscale levels
- **Board**: m5stack-core-esp32

### Memoria
- **Flash**: 82.3% utilizzato (1078749 / 1310720 bytes)
- **RAM**: 15.1% utilizzato (49624 / 327680 bytes)
- Ottimo margine per font di grandi dimensioni grazie a PSRAM

### Risparmio Energetico
- WiFi disabilitato
- Bluetooth disabilitato
- Touchscreen non inizializzato
- Solo pulsanti fisici attivi

## Risoluzione Problemi

### "SD CARD ERROR"
- Verifica che la microSD sia inserita correttamente
- Assicurati che sia formattata in FAT32
- Prova a riformattare la SD

### "NO FONTS FOUND"
- Verifica che i font siano nella cartella `/fonts`
- Controlla che abbiano estensione `.ttf` o `.otf`
- I nomi file sono case-insensitive (TTF/ttf funzionano entrambi)

### "FONT LOAD ERROR"
- Il font potrebbe essere corrotto
- Prova con un altro font per verificare
- Alcuni font molto complessi potrebbero non caricarsi

### Ghosting sul Display
- Il firmware fa automaticamente full refresh ogni 10 secondi
- Se vedi troppo ghosting, attendi il prossimo refresh automatico
- Il ghosting è normale per e-ink dopo molti partial updates

## Output Seriale (Debug)

Connetti il monitor seriale (115200 baud) per vedere:
- Lista font trovati all'avvio
- Font caricato correntemente
- Ogni cambio glifo con codepoint e range Unicode
- Tipo di refresh (partial/full) ad ogni update

## Crediti

- **Hardware**: M5Stack M5Paper
- **Librerie**: M5EPD (include FreeType per rendering TTF)
- **Framework**: Arduino ESP32 via PlatformIO

## Licenza

Codice progetto: usa come preferisci
