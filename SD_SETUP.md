# Setup MicroSD per PaperSpecimen

## Preparazione SD Card

### 1. Formattazione
- **Formato richiesto**: FAT32
- **Tool consigliati**:
  - Windows: formattazione integrata o SD Card Formatter
  - macOS: Disk Utility (seleziona MS-DOS (FAT))
  - Linux: `mkfs.vfat -F 32 /dev/sdX1`

### 2. Struttura Directory
Crea questa struttura sulla SD:
```
/
└── fonts/
    └── (qui i tuoi font .ttf o .otf)
```

### 3. Font Supportati
- **Formati**: `.ttf` (TrueType) e `.otf` (OpenType)
- **Case-insensitive**: `Font.TTF`, `font.ttf`, `FONT.TtF` funzionano tutti
- **Nomi file**: usa nomi descrittivi es. `Helvetica-Bold.ttf`

### 4. Esempio Font da Testare

Scarica font gratuiti da:
- **Google Fonts**: https://fonts.google.com
  - Roboto, Open Sans, Lato, Montserrat...
- **Font Squirrel**: https://www.fontsquirrel.com
- **Adobe Fonts** (se hai abbonamento)

Font consigliati per specimen test:
- **Sans-serif**: Roboto, Helvetica, Arial
- **Serif**: Times New Roman, Garamond, Merriweather
- **Monospace**: Courier, Fira Code, JetBrains Mono
- **Display**: Playfair Display, Bebas Neue

### 5. Organizzazione Consigliata

Puoi mettere tutti i font direttamente in `/fonts` oppure organizzarli, ma il firmware cerca solo nella root di `/fonts`:

```
/fonts/
├── Roboto-Regular.ttf
├── Roboto-Bold.ttf
├── Roboto-Italic.ttf
├── TimesNewRoman-Regular.ttf
├── TimesNewRoman-Bold.ttf
└── FiraCode-Regular.ttf
```

### 6. Test Veloce

Per testare rapidamente:
1. Scarica 2-3 font da Google Fonts
2. Estrai i file `.ttf` dallo zip
3. Copia nella cartella `/fonts` della SD
4. Inserisci SD nell'M5Paper
5. Avvia il dispositivo

### 7. Troubleshooting

**SD non riconosciuta?**
- Verifica sia FAT32 (non exFAT o NTFS)
- Prova a riformattare
- Verifica contatti fisici SD card

**Font non caricati?**
- Controlla che siano in `/fonts` (non sottocartelle)
- Verifica estensione sia `.ttf` o `.otf`
- Prova con font più semplici (es. Roboto) per test
- Controlla output seriale per errori specifici

**Font corretto ma errore di caricamento?**
- Alcuni font molto complessi potrebbero fallire
- Font con protezioni DRM non funzioneranno
- Prova con font open-source per sicurezza

### 8. Quanti Font?

Il firmware supporta un numero illimitato di font (limitato solo dalla memoria SD).
- La RAM usata è solo per il font correntemente caricato
- Puoi avere 100+ font sulla SD senza problemi
- La scansione all'avvio potrebbe richiedere qualche secondo in più

### 9. Dimensioni Font File

- Font tipico: 50-500 KB
- Font con molti glifi (CJK): 5-20 MB
- SD consigliata: 1GB o più (anche se bastano pochi MB)

## Quick Start

```bash
# macOS/Linux: formatta SD e crea struttura
diskutil eraseDisk FAT32 PAPERSPEC MBRFormat /dev/disk2  # adjust disk number
mkdir -p /Volumes/PAPERSPEC/fonts
cp ~/Downloads/MyFont.ttf /Volumes/PAPERSPEC/fonts/

# Verifica
ls -lh /Volumes/PAPERSPEC/fonts/
```

## Font Gratuiti Consigliati per Typography Specimen

1. **Inter** - eccellente sans-serif moderna
2. **Crimson Text** - serif classico elegante
3. **Fira Code** - monospace con ligature
4. **Playfair Display** - display serif alta contrastazione
5. **Montserrat** - geometric sans-serif

Tutti scaricabili gratuitamente da Google Fonts.
