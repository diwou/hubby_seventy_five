#pragma once
#include <Arduino.h>
#include <sys/time.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "config.h"
#include "weather.h"

// ============================================================
// Display-Instanz
// ============================================================
MatrixPanel_I2S_DMA* dma_display = nullptr;

namespace DisplayPins {
static constexpr int PIN_R1  = 4;
static constexpr int PIN_G1  = 5;
static constexpr int PIN_B1  = 6;
static constexpr int PIN_R2  = 7;
static constexpr int PIN_G2  = 8;
static constexpr int PIN_B2  = 9;
static constexpr int PIN_A   = 10;
static constexpr int PIN_B   = 11;
static constexpr int PIN_C   = 12;
static constexpr int PIN_D   = 13;
static constexpr int PIN_CLK = 14;
static constexpr int PIN_LAT = 17;
static constexpr int PIN_OE  = 18;
}

static constexpr uint8_t DISPLAY_BOOT_BRIGHTNESS = 4;
static constexpr int     PANEL_W = 64;
static constexpr int     PANEL_H = 32;

static uint8_t displayPanelColumns() {
    return (cfg.display_panel_cols < 1) ? 1 : ((cfg.display_panel_cols > 2) ? 2 : cfg.display_panel_cols);
}

static uint8_t displayPanelRows() {
    // Die Konfiguration ist bereits fuer 2x2 vorbereitet. Die aktuelle Matrix-
    // Initialisierung unterstuetzt bewusst nur eine Panel-Zeile; das Web-UI
    // bietet deshalb vorerst nur 1x1 und 2x1 an.
    (void)cfg.display_panel_rows;
    return 1;
}

static uint8_t displayPanelMapping() {
    return (cfg.display_panel_map > 1) ? 0 : cfg.display_panel_map;
}

static int displayPanelCount() { return displayPanelColumns() * displayPanelRows(); }
static int displayWidth() { return PANEL_W * displayPanelColumns(); }
static int displayHeight() { return PANEL_H * displayPanelRows(); }

#define DISPLAY_PANELS (displayPanelCount())
#define DISPLAY_W      (displayWidth())
#define DISPLAY_H      (displayHeight())

static void displayPreparePins() {
    const int pins[] = {
        DisplayPins::PIN_R1, DisplayPins::PIN_G1, DisplayPins::PIN_B1,
        DisplayPins::PIN_R2, DisplayPins::PIN_G2, DisplayPins::PIN_B2,
        DisplayPins::PIN_A,  DisplayPins::PIN_B,  DisplayPins::PIN_C, DisplayPins::PIN_D,
        DisplayPins::PIN_CLK, DisplayPins::PIN_LAT, DisplayPins::PIN_OE
    };

    for (int pin : pins) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }

    // OE ist aktiv-low: HIGH haelt das Panel dunkel, bis DMA sauber laeuft.
    digitalWrite(DisplayPins::PIN_OE, HIGH);
}

static uint8_t displayStartupBrightness() {
    return (cfg.brightness < DISPLAY_BOOT_BRIGHTNESS) ? cfg.brightness : DISPLAY_BOOT_BRIGHTNESS;
}

static void displaySetBrightness(uint8_t brightness) {
    if (dma_display) dma_display->setBrightness8(brightness);
}

static void displayRampBrightness(uint8_t targetBrightness,
                                  uint16_t durationMs = 900,
                                  uint16_t stepMs = 30) {
    if (!dma_display) return;

    uint8_t startBrightness = displayStartupBrightness();
    if (targetBrightness <= startBrightness) {
        displaySetBrightness(targetBrightness);
        return;
    }

    uint16_t steps = durationMs / stepMs;
    if (steps == 0) steps = 1;

    for (uint16_t step = 1; step <= steps; ++step) {
        uint8_t level = startBrightness +
                        (uint16_t)(targetBrightness - startBrightness) * step / steps;
        displaySetBrightness(level);
        delay(stepMs);
    }
}

void displayInit() {
    HUB75_I2S_CFG::i2s_pins pinmap = {
        DisplayPins::PIN_R1, DisplayPins::PIN_G1, DisplayPins::PIN_B1,
        DisplayPins::PIN_R2, DisplayPins::PIN_G2, DisplayPins::PIN_B2,
        DisplayPins::PIN_A, DisplayPins::PIN_B, DisplayPins::PIN_C, DisplayPins::PIN_D, -1,
        DisplayPins::PIN_LAT, DisplayPins::PIN_OE, DisplayPins::PIN_CLK
    };
    // double_buff=false: mit 2 Panels reicht der DMA-Speicher (internes SRAM) nicht fuer double buffering
    HUB75_I2S_CFG mxconfig(PANEL_W, PANEL_H, DISPLAY_PANELS, pinmap, HUB75_I2S_CFG::FM6126A,
                           HUB75_I2S_CFG::TYPE138, false);
    (void)displayPanelMapping(); // vorbereitet fuer spaetere 2x2-/ZigZag-Mapping-Unterstuetzung

    dma_display = new MatrixPanel_I2S_DMA(mxconfig);
    dma_display->begin();
    displaySetBrightness(displayStartupBrightness());
    dma_display->clearScreen();
    dma_display->setTextWrap(false);
}

void displayFlip() { dma_display->flipDMABuffer(); }

// ============================================================
// Farben
// ============================================================
static uint16_t C_DIGIT, C_DIGIT_DIM, C_COLON;
static uint16_t C_DATE, C_TEMP, C_INFO, C_WARN, C_BLACK;
static uint16_t C_SEC_5, C_SEC_10;
static uint16_t C_DIGIT_FADE[7];  // Cross-Dissolve: Amber → Schwarz (7 Stufen)

static uint16_t _hexToColor(const String& h) {
    long v = strtol(h.c_str() + 1, nullptr, 16);
    return dma_display->color565((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
}

void colorsInit() {
    C_DIGIT  = _hexToColor(cfg.color_digit);
    C_COLON  = _hexToColor(cfg.color_sec);
    C_DATE   = _hexToColor(cfg.color_date);
    C_TEMP   = _hexToColor(cfg.color_temp);
    C_INFO   = _hexToColor(cfg.color_info);
    C_WARN   = _hexToColor(cfg.color_warn);
    C_BLACK  = dma_display->color565(0, 0, 0);
    C_SEC_5  = _hexToColor(cfg.color_sec5);
    C_SEC_10 = _hexToColor(cfg.color_sec10);
    // C_DIGIT_DIM: 1/6 Helligkeit der Ziffernfarbe
    long dv = strtol(cfg.color_digit.c_str() + 1, nullptr, 16);
    C_DIGIT_DIM = dma_display->color565(
        ((dv >> 16) & 0xFF) / 6,
        ((dv >>  8) & 0xFF) / 6,
        ( dv        & 0xFF) / 6);
    // Fade-Stufen: C_DIGIT_FADE[0]=voll, C_DIGIT_FADE[6]=schwarz
    for (int i = 0; i < 7; i++) {
        C_DIGIT_FADE[i] = dma_display->color565(
            ((dv >> 16) & 0xFF) * (6 - i) / 6,
            ((dv >>  8) & 0xFF) * (6 - i) / 6,
            ( dv        & 0xFF) * (6 - i) / 6);
    }
}

// ============================================================
// Layout (64×32):
//
//  Zeile    0:   Gap (1px oben)
//  Zeilen  1-8:  Uhrzeit-Inhalt (10×8px Bitmap-Ziffern, Slot-Clear 0-12)
//  Zeilen  9-12: Slot-Tail (von Slot-Clear abgedeckt)
//  Zeilen 10-11: Sekundenanzeige (2px, Mode 0-2, im Slot-Tail, wird nach Flip neu gezeichnet)
//  Zeilen 13-19: Datum (y=13, textSize=1)
//  Zeilen 20-22: Gap (3px — Platz für Großbuchstaben-Umlautpunkte bei y=22)
//  Zeilen 22-31: Wetter (y=23, fillRect ab 22; Großumlaut-Punkte y=22, Körper y=24-30)
//
//  HH:MM (seconds_mode 0-2), DIGIT_W=10, 2px Abstand zwischen Zifferpaaren:
//  H1=7  H2=19  :(4px)=31  M1=35  M2=47
//
//  HH:MM:SS (seconds_mode 3), 9px Ziffern, 2×2px Doppelpunkte:
//  H1=0..8  gap=9  H2=10..18  colon=19..20  M1=21..29  gap=30
//  M2=31..39  colon=40..41  S1=42..50  gap=51  S2=52..60
// ============================================================
#define DIGIT_W  10
#define DIGIT_H  14
static constexpr int DIGIT_VISIBLE_H_HHMM   = 16;
static constexpr int DIGIT_VISIBLE_H_HHMMSS = 14;
static constexpr char NARROW_SPACE_MARKER   = '\x1f';
static constexpr int  NARROW_SPACE_PX       = 3;

// ============================================================
// Bitmap-Font 10×14: modern, bold (3px horizontal, 2px vertikal)
// Bit 9 (MSB) = Spalte 0 (links), Bit 0 = Spalte 9 (rechts)
// Zeilenformat: 14 × uint16_t pro Ziffer
// Chamfered corners: Zeile 0 + 13 → 8px (Spalte 1-8), Rest voll
// ============================================================
// Dottie 8×8-Font, konvertiert: (uint16_t)dottie_byte << 1
// Inhalt in Spalten 1-8 (bit8..bit1), 1 Leerzeile oben, 5 unten
// (Inhalt 2px nach oben verschoben gegenüber Ursprungsversion)
static const uint16_t _digits[11][14] PROGMEM = {
//  0: {0x3C,0x66,0x66,0x66,0x66,0x66,0x66,0x3C}
    { 0x000, 0x078,0x0CC,0x0CC,0x0CC,0x0CC,0x0CC,0x0CC,0x078, 0x000,0x000,0x000,0x000,0x000 },
//  1: {0x18,0x38,0x18,0x18,0x18,0x18,0x18,0x7E}
    { 0x000, 0x030,0x070,0x030,0x030,0x030,0x030,0x030,0x0FC, 0x000,0x000,0x000,0x000,0x000 },
//  2: {0x3C,0x66,0x06,0x0C,0x18,0x30,0x60,0x7E}
    { 0x000, 0x078,0x0CC,0x00C,0x018,0x030,0x060,0x0C0,0x0FC, 0x000,0x000,0x000,0x000,0x000 },
//  3: {0x3C,0x66,0x06,0x1C,0x06,0x06,0x66,0x3C}
    { 0x000, 0x078,0x0CC,0x00C,0x038,0x00C,0x00C,0x0CC,0x078, 0x000,0x000,0x000,0x000,0x000 },
//  4: {0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x0C,0x0C}
    { 0x000, 0x018,0x038,0x078,0x0D8,0x0FC,0x018,0x018,0x018, 0x000,0x000,0x000,0x000,0x000 },
//  5: {0x7E,0x60,0x60,0x7C,0x06,0x06,0x66,0x3C}
    { 0x000, 0x0FC,0x0C0,0x0C0,0x0F8,0x00C,0x00C,0x0CC,0x078, 0x000,0x000,0x000,0x000,0x000 },
//  6: {0x1C,0x30,0x60,0x7C,0x66,0x66,0x66,0x3C}
    { 0x000, 0x038,0x060,0x0C0,0x0F8,0x0CC,0x0CC,0x0CC,0x078, 0x000,0x000,0x000,0x000,0x000 },
//  7: {0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x30}
    { 0x000, 0x0FC,0x00C,0x018,0x030,0x060,0x060,0x060,0x060, 0x000,0x000,0x000,0x000,0x000 },
//  8: {0x3C,0x66,0x66,0x3C,0x66,0x66,0x66,0x3C}
    { 0x000, 0x078,0x0CC,0x0CC,0x078,0x0CC,0x0CC,0x0CC,0x078, 0x000,0x000,0x000,0x000,0x000 },
//  9: {0x3C,0x66,0x66,0x66,0x3E,0x06,0x0C,0x38}
    { 0x000, 0x078,0x0CC,0x0CC,0x0CC,0x07C,0x00C,0x018,0x070, 0x000,0x000,0x000,0x000,0x000 },
// 10: leer (führende Null im 12h-Modus)
    { 0x000, 0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000, 0x000,0x000,0x000,0x000,0x000 },
};

// ============================================================
// Ziffer zeichnen: Bitmap-Pixel ab (x, yOff) in Farbe color
// cols: Anzahl gezeichneter Spalten (DIGIT_W=10 oder 9 für HH:MM:SS)
// Zeile 13 wird nie gezeichnet (Clip dy >= DIGIT_H-1) → 1px Gap
// ============================================================
static inline bool _useLegacySinglePanelHHMMSS() {
    return cfg.seconds_mode == 3 && DISPLAY_W < 128;
}

static inline bool _useTallHHMM() { return cfg.seconds_mode != 3; }

static inline int _digitVisibleHeight() {
    if (_useLegacySinglePanelHHMMSS()) return 9;
    return _useTallHHMM() ? DIGIT_VISIBLE_H_HHMM : DIGIT_VISIBLE_H_HHMMSS;
}

static inline int _secondsBarY() {
    return (cfg.seconds_mode == 3) ? 10 : 19;
}

static inline int _clockAreaHeight() {
    return (cfg.seconds_mode == 3) ? DIGIT_VISIBLE_H_HHMMSS : (_secondsBarY() + 2);
}

static inline int _maxClockAreaHeight() {
    return DIGIT_VISIBLE_H_HHMM + 5;
}

static inline int _clockYOffset() {
    if (_useLegacySinglePanelHHMMSS()) return 0;
    return (cfg.seconds_mode == 3) ? 3 : 0;
}

static void _drawDigitAtCompact(int x, int idx, int yOff, uint16_t color) {
    if (idx < 0 || idx > 10) return;
    if (_useLegacySinglePanelHHMMSS()) {
        for (int row = 0; row < DIGIT_H; row++) {
            int dy = row + yOff;
            if (dy < 0 || dy >= 9) continue;
            uint16_t bits = pgm_read_word(&_digits[idx][row]);
            uint8_t compactBits = (uint8_t)((bits >> 1) & 0xFF);
            for (int col = 0; col < 8; col++) {
                if (compactBits & (1u << (7 - col)))
                    dma_display->drawPixel(x + col, dy, color);
            }
        }
        return;
    }

    int clipH = DIGIT_VISIBLE_H_HHMMSS + _clockYOffset();
    static const uint8_t rowMap[DIGIT_VISIBLE_H_HHMMSS] = {
        1, 1, 2, 2, 3, 4, 4, 5, 6, 6, 7, 7, 8, 8
    };
    for (int row = 0; row < DIGIT_VISIBLE_H_HHMMSS; row++) {
        int dy = row + yOff;
        if (dy < 0 || dy >= clipH) continue;
        uint16_t bits = pgm_read_word(&_digits[idx][rowMap[row]]);
        uint8_t compactBits = (uint8_t)((bits >> 1) & 0xFF);  // mittlere 8 Spalten
        for (int col = 0; col < 8; col++) {
            if (compactBits & (1u << (7 - col)))
                dma_display->drawPixel(x + col, dy, color);
        }
    }
}

static void _drawDigitAtTall(int x, int idx, int yOff, uint16_t color) {
    if (idx < 0 || idx > 10) return;
    int clipH = DIGIT_VISIBLE_H_HHMM + _clockYOffset();
    static const uint8_t rowMap[DIGIT_VISIBLE_H_HHMM] = {
        1, 1, 2, 2, 3, 3, 4, 4,
        5, 5, 6, 6, 7, 7, 8, 8
    };
    for (int row = 0; row < DIGIT_VISIBLE_H_HHMM; row++) {
        int dy = row + yOff;
        if (dy < 0 || dy >= clipH) continue;
        uint16_t bits = pgm_read_word(&_digits[idx][rowMap[row]]);
        bits |= (bits >> 1);           // leicht verbreitern
        bits &= 0x1FE;                 // innerhalb der 10 Spalten halten
        for (int col = 0; col < DIGIT_W; col++) {
            if (bits & (1u << (DIGIT_W - 1 - col)))
                dma_display->drawPixel(x + col, dy, color);
        }
    }
}

static void _drawDigitAt(int x, int idx, int yOff, uint16_t color, int cols = DIGIT_W) {
    if (_useTallHHMM()) _drawDigitAtTall(x, idx, yOff, color);
    else _drawDigitAtCompact(x, idx, yOff, color);
}

// ============================================================
// Animations-Zustand: 6 Stellen (HH:MM:SS), letzten 2 nur bei Mode 3
// ============================================================
struct FlipDigit {
    char     cur    = '0';
    char     target = '0';
    int8_t   frame  = -1;
    uint32_t last_t =  0;
};
static FlipDigit flip[6];
static int8_t   colonFrame  = -1;  // -1=statisch hell, 0..13=Cross-Dissolve-Puls
static uint32_t colonFrameMs = 0;
static int      colonLastSec = -1;

// Dirty-Counter: 2 = beide DMA-Buffer müssen noch aktualisiert werden
// Startet bei 2, damit beim Boot beide Buffer korrekt initialisiert werden
static uint8_t digitDirty[6] = {2, 2, 2, 2, 2, 2};

// ============================================================
// Eine Stelle zeichnen (inkl. laufender Animation)
// Löscht den Slot per fillRect bevor gezeichnet wird.
// cols: 10 (HH:MM) oder 9 (HH:MM:SS, damit 1px Abstand entsteht)
// ============================================================
static void _drawClockDigit(int x, const FlipDigit& d, int cols) {
    int ci = (d.cur    == ' ') ? 10 : (d.cur    - '0');
    int ti = (d.target == ' ') ? 10 : (d.target - '0');
    int visibleH = _digitVisibleHeight();
    int baseY = _clockYOffset();

    // Slot schwarz löschen inklusive vertikalem Offset.
    dma_display->fillRect(x, 0, cols, visibleH + baseY, C_BLACK);

    if (d.frame < 0) {
        _drawDigitAt(x, ci, baseY, C_DIGIT, cols);
    } else if (cfg.anim_mode == 0) {
        if (d.frame <= visibleH - 1)
            _drawDigitAt(x, ci, baseY - d.frame, C_DIGIT, cols);
        else
            _drawDigitAt(x, ti, baseY + (visibleH * 2 - 2) - d.frame, C_DIGIT, cols);
    } else {
        // Cross-Dissolve
        if (d.frame <= 6)
            _drawDigitAt(x, ci, baseY, C_DIGIT_FADE[d.frame],      cols);
        else
            _drawDigitAt(x, ti, baseY, C_DIGIT_FADE[13 - d.frame], cols);
    }
}

// ============================================================
// Uhrzeit zeichnen: HH:MM oder HH:MM:SS je nach seconds_mode
// Nur animierende oder dirty Ziffern werden neu gezeichnet.
//
// HH:MM:SS Layout: 8px Ziffern (Dottie-Inhalt in Spalten 1-7) + 2×2px Kolons.
// 6×8 + 2×2 + 7×1px Gaps = 59px, zentriert ab x=2 (2px links, 3px rechts frei).
// Alle 7 Abstände je 1px garantiert!
//  x=2..9   gap=10  x=11..18  gap=19  C1=20..21  gap=22
//  x=23..30  gap=31  x=32..39  gap=40  C2=41..42  gap=43  x=44..51  gap=52  x=53..60
// ============================================================
static void _drawFlipClock() {
    uint32_t now          = millis();
    uint32_t ms_per_frame = (cfg.anim_mode == 0) ? 15 : 18;
    int      visibleH     = _digitVisibleHeight();
    int      maxFrame     = (cfg.anim_mode == 0) ? (visibleH * 2 - 1) : 14;
    int      nDigits      = (cfg.seconds_mode == 3) ? 6 : 4;
    int      cols         = (cfg.seconds_mode == 3) ? DIGIT_W - 2 : DIGIT_W;

    static const int posHHMM[4]   = { 7, 19, 35, 47 };
    static const int posHHMMSS[6] = { 2, 11, 23, 32, 44, 53 };
    const int* pos = (cfg.seconds_mode == 3) ? posHHMMSS : posHHMM;

    // Frames vorwärts schalten; beim Abschluss dirty setzen
    for (int i = 0; i < nDigits; i++) {
        FlipDigit& d = flip[i];
        if (d.frame >= 0 && (now - d.last_t) >= ms_per_frame) {
            d.last_t += ms_per_frame;  // += statt =now: kein Drift, sync bleibt erhalten
            if (++d.frame >= maxFrame) {
                d.cur = d.target; d.frame = -1;
                digitDirty[i] = 2;  // beide Buffer nach Animation aktualisieren
            }
        }
    }

    // Nur animierende oder dirty Ziffern zeichnen
    for (int i = 0; i < nDigits; i++) {
        if (flip[i].frame >= 0) {
            // Animierend: immer zeichnen
            _drawClockDigit(pos[i], flip[i], cols);
        } else if (digitDirty[i] > 0) {
            // Gerade statisch geworden: noch dirty-Counter-mal zeichnen
            digitDirty[i]--;
            _drawClockDigit(pos[i], flip[i], cols);
        }
        // else: wirklich statisch → nicht anfassen, kein Flicker
    }

    // Kolon: Cross-Dissolve-Puls (14 Frames wie Ziffern, 18ms/Frame)
    // Frame 0-6: hell→dunkel, Frame 7-13: dunkel→hell, -1: statisch hell
    uint16_t cc;
    if (colonFrame < 0)
        cc = C_DIGIT;
    else if (colonFrame <= 6)
        cc = C_DIGIT_FADE[colonFrame];
    else
        cc = C_DIGIT_FADE[13 - colonFrame];
    if (cfg.seconds_mode == 3) {
        // HH:MM:SS: 2×2px Kolon-Punkte
        // C1=20..21 (gap=19 vor, gap=22 nach Kolon)
        // C2=41..42 (gap=40 vor, gap=43 nach Kolon)
        if (_useLegacySinglePanelHHMMSS()) {
            dma_display->fillRect(21, 2, 2, 2, cc);
            dma_display->fillRect(21, 7, 2, 2, cc);
            dma_display->fillRect(42, 2, 2, 2, cc);
            dma_display->fillRect(42, 7, 2, 2, cc);
        } else {
            dma_display->fillRect(20, 3 + _clockYOffset(), 2, 2, cc);
            dma_display->fillRect(20, 9 + _clockYOffset(), 2, 2, cc);
            dma_display->fillRect(41, 3 + _clockYOffset(), 2, 2, cc);
            dma_display->fillRect(41, 9 + _clockYOffset(), 2, 2, cc);
        }
    } else {
        // HH:MM: 2×2px Kolon-Punkte (je 3px Abstand zu den Zifferblöcken)
        dma_display->fillRect(31, 4 + _clockYOffset(), 2, 3, cc);
        dma_display->fillRect(31, 10 + _clockYOffset(), 2, 3, cc);
    }
}

// startFrame: Frame an dem die Animation beginnt (Subsekunden-Kompensation).
// msIntoFrame: wie viele ms wir schon in diesem Frame sind → last_t rückdatieren,
//              damit alle NTP-Uhren ihre Frame-Timer synchron zur Sekunde laufen.
static void _setFlipDigit(int i, char c, int startFrame = 0, int msIntoFrame = 0) {
    if (c != flip[i].cur && flip[i].frame < 0) {
        flip[i].target = c;
        if (cfg.anim_mode == 2) {
            // Keine Animation: Ziffer sofort wechseln
            flip[i].cur   = c;
            flip[i].frame = -1;
            digitDirty[i] = 2;
        } else {
            flip[i].frame  = startFrame;
            flip[i].last_t = millis() - (uint32_t)msIntoFrame;
        }
    }
}

static void _setFlipTime(int h, int m, int s) {
    // Subsekunden-Kompensation: Animation synchron zur NTP-Sekunde starten.
    // gettimeofday() liefert die exakten µs in der aktuellen Sekunde.
    // Beide NTP-Uhren starten dadurch am selben Frame — unabhängig vom Loop-Timing.
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    int msInSec     = (int)(tv.tv_usec / 1000);  // 0..999 ms in der aktuellen Sekunde
    int mspf        = (cfg.anim_mode == 0) ? 15 : 18;
    int visibleH    = _digitVisibleHeight();
    int maxFr       = (cfg.anim_mode == 0) ? (visibleH * 2 - 1) : 14;
    int startFr     = (cfg.anim_mode == 2) ? 0 : min(msInSec / mspf, maxFr - 1);
    int msIntoFrame = (cfg.anim_mode == 2) ? 0 : (msInSec % mspf);

    int h_disp = h;
    if (cfg.time_12h) {
        h_disp = (h == 0) ? 12 : (h > 12) ? h - 12 : h;
    }
    _setFlipDigit(0, (cfg.time_12h && h_disp < 10) ? ' ' : ('0' + h_disp / 10), startFr, msIntoFrame);
    _setFlipDigit(1, '0' + h_disp % 10, startFr, msIntoFrame);
    _setFlipDigit(2, '0' + m / 10,      startFr, msIntoFrame);
    _setFlipDigit(3, '0' + m % 10,      startFr, msIntoFrame);
    if (cfg.seconds_mode == 3) {
        _setFlipDigit(4, '0' + s / 10,  startFr, msIntoFrame);
        _setFlipDigit(5, '0' + s % 10,  startFr, msIntoFrame);
    }
}

// ============================================================
// Sekundenanzeige (y=14..15, 2px), 4 Modi
// Mode 0: Bar     — füllt sich von links (Farb-Marken bei 5s/10s)
// Mode 1: Pulse   — ein Leuchtpunkt wandert in 60s nach rechts
// Mode 2: Dots    — 60 Punkte, leuchtend wenn Sekunde erreicht
// Mode 3: (kein Balken — Sekunden sind im Zeitdisplay)
// ============================================================
static inline uint16_t _secColor(int i) {
    return ((i + 1) % 10 == 0) ? C_SEC_10
         : ((i + 1) %  5 == 0) ? C_SEC_5
         : C_COLON;
}

static void _drawSeconds(int sec) {
    if (cfg.seconds_mode == 3) return;  // Sekunden im Zeitdisplay
    int y = _secondsBarY();

    if (cfg.seconds_mode == 0) {
        // Bar: ausgefüllt bis sec
        for (int i = 0; i < sec; i++) {
            dma_display->drawPixel(i + 2, y,     _secColor(i));
            dma_display->drawPixel(i + 2, y + 1, _secColor(i));
        }

    } else if (cfg.seconds_mode == 1) {
        // Pulse: ein wandernder Punkt mit Schweif
        int x = sec + 2;
        dma_display->fillRect(x,     y, 1, 2, C_DIGIT);
        if (sec > 0) dma_display->fillRect(x - 1, y, 1, 2, C_COLON);
        if (sec > 1) dma_display->fillRect(x - 2, y, 1, 2, C_DIGIT_DIM);

    } else if (cfg.seconds_mode == 2) {
        // Dots: alle vergangenen Sekunden leuchten, Rest dunkel
        for (int i = 0; i < 60; i++) {
            uint16_t col = (i < sec) ? _secColor(i) : C_DIGIT_DIM;
            dma_display->drawPixel(i + 2, y,     col);
            dma_display->drawPixel(i + 2, y + 1, col);
        }
    }
}

// ============================================================
// Datum-Zeile (y=13)
// "Mo07.03.26" — kein Leerzeichen, 10 Zeichen × 6px = 60px, zentriert
// Wochentag in C_WARN, Datum in C_DATE
// ============================================================
static const char* _weekday(int wday) {
    if (cfg.lang == "de") {
        static const char* d[] = {"So","Mo","Di","Mi","Do","Fr","Sa"};
        return d[wday];
    } else {
        static const char* d[] = {"Su","Mo","Tu","We","Th","Fr","Sa"};
        return d[wday];
    }
}

static const char* _weekdayFull(int wday) {
    if (cfg.lang == "de") {
        static const char* d[] = {"Sonntag","Montag","Dienstag","Mittwoch","Donnerstag","Freitag","Samstag"};
        return d[wday];
    } else {
        static const char* d[] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
        return d[wday];
    }
}

// Hilfsfunktion: 2×2-Pixel-Datumspunkt bei (x, y=22)
static inline void _dateDot(int x) { dma_display->fillRect(x, 18, 2, 2, C_DATE); }

static void _drawDate(struct tm* t) {
    char dd[3], mm[3], yy[3], yyyy[5];
    snprintf(dd,   sizeof(dd),   "%02d", t->tm_mday);
    snprintf(mm,   sizeof(mm),   "%02d", t->tm_mon + 1);
    snprintf(yy,   sizeof(yy),   "%02d", t->tm_year % 100);
    snprintf(yyyy, sizeof(yyyy), "%04d", t->tm_year + 1900);

    dma_display->setTextSize(1);

    switch (cfg.date_format) {

    case 1: {
        // "Mo TT.MM" – Wochentag + Tag.Monat, kein Jahr
        // "Mo"(12) + gap(2) + "TT"(12) + sep(6) + "MM"(12) = 44px → x=11 zentriert
        dma_display->setTextColor(C_WARN);
        dma_display->setCursor(11, 13);
        dma_display->print(_weekday(t->tm_wday));   // cursor → 23
        dma_display->setTextColor(C_DATE);
        dma_display->setCursor(25, 13);             // 2px Abstand
        dma_display->print(dd);                     // cursor → 37, last glyph x=35
        _dateDot(38);                               // 2px leer (36,37), Punkt 38-39
        dma_display->setCursor(42, 13);             // 2px leer (40,41)
        dma_display->print(mm);                     // last glyph x=52
        break;
    }

    case 2: {
        // "TT.MM.JJJJ" – langer Stil ohne Wochentag, zentriert
        // "TT"(12) + sep(6) + "MM"(12) + sep(6) + "JJJJ"(24) = 60px → x=2
        dma_display->setTextColor(C_DATE);
        dma_display->setCursor(2, 13);
        dma_display->print(dd);                     // cursor → 14, last glyph 12
        _dateDot(15);                               // advance gap 13, leer 14, Punkt 15-16
        dma_display->setCursor(19, 13);             // 2px leer (17,18)
        dma_display->print(mm);                     // cursor → 31, last glyph 29
        _dateDot(32);                               // advance gap 30, leer 31, Punkt 32-33
        dma_display->setCursor(36, 13);             // 2px leer (34,35)
        dma_display->print(yyyy);                   // last glyph x=59, cursor 60
        break;
    }

    case 3: {
        // "MM/TT/JJ" – US-Format ohne Wochentag, zentriert
        // 8 Zeichen × 6px = 48px → x=8
        char buf[9];
        snprintf(buf, sizeof(buf), "%s/%s/%s", mm, dd, yy);
        dma_display->setTextColor(C_DATE);
        dma_display->setCursor(8, 13);
        dma_display->print(buf);
        break;
    }

    default: // case 0: "Mo TT.MM.JJ" (Standard)
        // "Mo"(12) + gap(2) + "TT"(12) + sep(6) + "MM"(12) + sep(6) + "JJ"(12) = 62px → x=2
        dma_display->setTextColor(C_WARN);
        dma_display->setCursor(2, 13);
        dma_display->print(_weekday(t->tm_wday));   // cursor → 14
        dma_display->setTextColor(C_DATE);
        dma_display->setCursor(16, 13);             // 2px Abstand
        dma_display->print(dd);                     // cursor → 28, last glyph 26
        _dateDot(29);                               // advance gap 27, leer 28, Punkt 29-30
        dma_display->setCursor(33, 13);             // 2px leer (31,32)
        dma_display->print(mm);                     // cursor → 45, last glyph 43
        _dateDot(46);                               // advance gap 44, leer 45, Punkt 46-47
        dma_display->setCursor(50, 13);             // 2px leer (48,49)
        dma_display->print(yy);                     // last glyph 60
        break;
    }
}

// Datum rechtes Panel: Wochentag (y=3) + "TT.MM.JJJJ" (y=13), jeweils zentriert in X=64..127
static void _drawDateRight(struct tm* t) {
    char dd[3], mm[3], yyyy[5];
    snprintf(dd,   sizeof(dd),   "%02d", t->tm_mday);
    snprintf(mm,   sizeof(mm),   "%02d", t->tm_mon + 1);
    snprintf(yyyy, sizeof(yyyy), "%04d", t->tm_year + 1900);

    dma_display->setTextSize(1);

    // Zeile 1: voller Wochentag, zentriert in X=64..127
    const char* wday = _weekdayFull(t->tm_wday);
    int xw = 64 + max(0, (64 - (int)strlen(wday) * 6) / 2);
    dma_display->setTextColor(C_WARN);
    dma_display->setCursor(xw, 3);
    dma_display->print(wday);

    // Zeile 2: "TT.MM.JJJJ" (60px) zentriert → x=66
    dma_display->setTextColor(C_DATE);
    dma_display->setCursor(66, 13);
    dma_display->print(dd);                    // cursor → 78
    dma_display->fillRect(79, 18, 2, 2, C_DATE);
    dma_display->setCursor(83, 13);
    dma_display->print(mm);                    // cursor → 95
    dma_display->fillRect(96, 18, 2, 2, C_DATE);
    dma_display->setCursor(100, 13);
    dma_display->print(yyyy);                  // last glyph x=123
}

// ============================================================
// Wetter-Zeile (y=23)
// Modus 0: aktuelles Wetter scrollend
// Modi 1-3: Vorhersage +1/+2/+3 Tage statisch mit Sonne/Mond-Icons
// ============================================================
static uint8_t   weatherMode      = 0;
static uint32_t  weatherLastMs    = 0;
static int       weatherScrollX   = DISPLAY_W;
static uint32_t  weatherScrollMs  = 0;
static String    weatherScrollStr = "";
static bool      forecastTransitionActive = false;
static uint8_t   forecastTransitionFrom    = 1;
static uint8_t   forecastTransitionTo      = 1;
static uint8_t   forecastTransitionStep    = 0;
static uint32_t  forecastTransitionMs      = 0;
// Datum-Dirty-Counter auf Modulebene, damit displayReset() ihn zurücksetzen kann
static int       datLastMday  = -1;
static uint8_t   datDirty     = 2;
static bool      displayOtaActive = false;

static void _invalidateStaticDisplay() {
    for (int i = 0; i < 6; i++) {
        digitDirty[i] = 2;
    }
    datDirty = 2;
}

static uint16_t _fadeColor(uint16_t color, uint8_t fade) {
    if (fade >= 255) return color;
    if (fade == 0) return C_BLACK;

    uint8_t r = (uint8_t)(((color >> 11) & 0x1F) * fade / 255);
    uint8_t g = (uint8_t)(((color >>  5) & 0x3F) * fade / 255);
    uint8_t b = (uint8_t)(( color        & 0x1F) * fade / 255);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

// ============================================================
// Umlaut-fähige Text-Ausgabe (ersetzt dma_display->print für
// Strings die Latin-1-Umlaute aus _sanitizeUtf8 enthalten können)
//
// Buchstabe immer bei normalem y (keine Höhenverschiebung, bündig mit anderen Zeichen).
// Punkte je nach Font-Body-Start:
//   Kleinbuchstaben (a/o/u): Body ab Row 2 → Punkte bei y (Row 0), Gap = y+1 (Row 1)
//   Großbuchstaben  (A/O/U): Body ab Row 0 → Punkte bei y-2 (2px über Zelle), Gap = y-1
// fillRect(0, 21, 64, 11) deckt y-2=21 ab (weather-y=23).
// ============================================================
static void _drawCharEx(int x, int y, uint8_t c, uint16_t color) {
    if (c == (uint8_t)NARROW_SPACE_MARKER) return;
    char base = 0;
    bool upper = false;
    switch (c) {
        case 0xE4: base = 'a'; break;                // ä
        case 0xF6: base = 'o'; break;                // ö
        case 0xFC: base = 'u'; break;                // ü
        case 0xC4: base = 'A'; upper = true; break;  // Ä
        case 0xD6: base = 'O'; upper = true; break;  // Ö
        case 0xDC: base = 'U'; upper = true; break;  // Ü
    }
    if (base) {
        dma_display->drawChar(x, y, base, color, C_BLACK, 1);
        int dotY = upper ? y - 2 : y;  // Groß: 2px über Zelle (Gap y-1); Klein: Row 0 (Gap y+1)
        dma_display->drawPixel(x + 1, dotY, color);
        dma_display->drawPixel(x + 3, dotY, color);
    } else {
        dma_display->drawChar(x, y, (char)c, color, C_BLACK, 1);
    }
}

static inline int _glyphAdvance(uint8_t c) {
    return (c == (uint8_t)NARROW_SPACE_MARKER) ? NARROW_SPACE_PX : 6;
}

static int _textWidthEx(const String& s) {
    int width = 0;
    const uint8_t* p = (const uint8_t*)s.c_str();
    while (*p) width += _glyphAdvance(*p++);
    return width;
}

// Gibt String mit Umlaut-Unterstützung aus; liefert End-x zurück.
static int _printStrEx(int x, int y, const String& s, uint16_t color) {
    const uint8_t* p = (const uint8_t*)s.c_str();
    while (*p) {
        uint8_t c = *p++;
        _drawCharEx(x, y, c, color);
        x += _glyphAdvance(c);
    }
    return x;
}

static int _drawTempWithUnit(int x, int y, const char* temp, uint16_t tempColor,
                             const char* unit, uint16_t unitColor) {
    int tempPx = (int)strlen(temp) * 6;
    dma_display->setTextColor(tempColor);
    dma_display->setCursor(x, y);
    dma_display->print(temp);
    dma_display->setTextColor(unitColor);
    dma_display->setCursor(x + tempPx + NARROW_SPACE_PX, y);
    dma_display->print(unit);
    return x + tempPx + NARROW_SPACE_PX + 6;
}

static void _buildCurrentStr() {
    if (!weather.valid || cfg.owm_apikey.isEmpty()) {
        weatherScrollStr = cfg.owm_apikey.isEmpty() ? "Kein API-Key" : "Kein Wetter";
        return;
    }
    const char* u = (cfg.owm_unit == 0) ? "C" : "F";
    char buf[128];
    // Heutige Tages-Vorhersage anhängen, falls verfügbar
    bool todayOk = (weather.today.temp_day != 0 || weather.today.temp_night != 0);
    if (todayOk) {
        snprintf(buf, sizeof(buf), "Aktuell: %+.1f%c%s  %s / Heute: %+d%c%s / %+d%c%s",
                 weather.current_temp, NARROW_SPACE_MARKER, u, weather.current_cond.c_str(),
                 (int)roundf(weather.today.temp_day),
                 NARROW_SPACE_MARKER, u,
                 (int)roundf(weather.today.temp_night), NARROW_SPACE_MARKER, u);
    } else {
        snprintf(buf, sizeof(buf), "Aktuell: %+.1f%c%s  %s",
                 weather.current_temp, NARROW_SPACE_MARKER, u, weather.current_cond.c_str());
    }
    weatherScrollStr = String(buf);
}

// Vorhersage idx=0..2 (+1/+2/+3 Tage) statisch, ganzzahlige Temps
// Format: "Mo +18/-3" zentriert; Tag=Amber, Nacht=Blau
// yOff: vertikaler Versatz, fade: 0..255 gegen Schwarz
static void _drawForecastStatic(int idx, int yOff = 0, uint8_t fade = 255) {
    char dayBuf[6], nightBuf[6];
    const char* unit = (cfg.owm_unit == 0) ? "C" : "F";
    snprintf(dayBuf,   sizeof(dayBuf),   "%+d", (int)roundf(weather.forecast[idx].temp_day));
    snprintf(nightBuf, sizeof(nightBuf), "%+d", (int)roundf(weather.forecast[idx].temp_night));
    String dayName = forecastDayName(idx + 1);

    // Breite: "Fre"(18) + " "(6) + dayT + " C / " + nightT + " C" → zentriert
    int dayPx   = strlen(dayBuf)   * 6;
    int nightPx = strlen(nightBuf) * 6;
    int unitPx  = NARROW_SPACE_PX + 6;
    int dayNamePx = dayName.length() * 6;
    int totalW  = dayNamePx + 6 + dayPx + unitPx + 18 + nightPx + unitPx;
    int x       = max(0, (DISPLAY_W - totalW) / 2);
    const int y = 23 + yOff;
    uint16_t warnCol = _fadeColor(C_WARN, fade);
    uint16_t tempCol = _fadeColor(C_TEMP, fade);
    uint16_t infoCol = _fadeColor(C_INFO, fade);
    uint16_t dateCol = _fadeColor(C_DATE, fade);

    dma_display->setTextSize(1);
    dma_display->setTextColor(warnCol);
    dma_display->setCursor(x, y);
    dma_display->print(dayName);
    x += dayNamePx + 6;

    x = _drawTempWithUnit(x, y, dayBuf, tempCol, unit, infoCol);

    dma_display->setTextColor(infoCol);
    dma_display->setCursor(x, y);
    dma_display->print(" / ");
    x += 18;

    x = _drawTempWithUnit(x, y, nightBuf, dateCol, unit, infoCol);
}

static void _drawForecastTransition(uint32_t now) {
    const uint8_t transitionSteps = 10;
    const uint32_t transitionMs = 20;

    while (forecastTransitionActive && now - forecastTransitionMs >= transitionMs) {
        forecastTransitionMs += transitionMs;
        if (++forecastTransitionStep > transitionSteps) {
            forecastTransitionActive = false;
            weatherMode = forecastTransitionTo;
            weatherLastMs = now;
            if (weatherMode == 0) {
                _buildCurrentStr();
                weatherScrollX = DISPLAY_W;
            }
            break;
        }
    }

    if (!forecastTransitionActive) {
        _drawForecastStatic(weatherMode - 1, 0, 255);
        return;
    }

    uint8_t step = forecastTransitionStep;
    uint8_t fadeIn  = (uint8_t)(step * 255 / transitionSteps);
    uint8_t fadeOut = (uint8_t)(255 - fadeIn);
    int inY  = cfg.forecast_scroll_up ? (transitionSteps - step) : 0;
    int outY = (forecastTransitionFrom == 0) ? 0 : (cfg.forecast_scroll_up ? -((int)step) : 0);

    if (forecastTransitionFrom == 0) {
        dma_display->setTextSize(1);
        _printStrEx(weatherScrollX, 23, weatherScrollStr, _fadeColor(C_TEMP, fadeOut));
    } else {
        _drawForecastStatic(forecastTransitionFrom - 1, outY, fadeOut);
    }
    _drawForecastStatic(forecastTransitionTo - 1, inY, fadeIn);
}

static void _drawDateArea(struct tm* t) {
    if (DISPLAY_W >= 128) {
        dma_display->fillRect(64, 0, 64, 21, C_BLACK);
        _drawDateRight(t);
    } else {
        dma_display->fillRect(0, 13, 64, 8, C_BLACK);
        _drawDate(t);
    }
}

static void _drawWeatherLine() {
    uint32_t now      = millis();
    uint32_t cyclems  = (uint32_t)cfg.bottom_cycle_sec * 1000UL;
    uint8_t  maxMode  = (weather.valid && !cfg.owm_apikey.isEmpty()) ? 4 : 1;

    // Erstinitialisierung
    if (weatherScrollStr.isEmpty()) {
        weatherLastMs = now;
        _buildCurrentStr();
    }

    if (forecastTransitionActive) {
        _drawForecastTransition(now);
    } else if (weatherMode == 0) {
        // --- Modus 0: aktuelles Wetter scrollend ---
        if (now - weatherScrollMs >= cfg.scroll_speed_ms) {
            weatherScrollMs = now;
            int textPx = _textWidthEx(weatherScrollStr);
            if (textPx > DISPLAY_W - 4) {
                weatherScrollX--;
                if (weatherScrollX < -textPx) {
                    weatherScrollX = -textPx;  // einfrieren
                    if (now - weatherLastMs >= cyclems) {
                        weatherLastMs = now;
                        if (maxMode > 1) {
                            forecastTransitionActive = true;
                            forecastTransitionFrom = 0;
                            forecastTransitionTo = 1;
                            forecastTransitionStep = 0;
                            forecastTransitionMs = now;
                        } else {
                            _buildCurrentStr();  // kein Forecast: neu aufbauen
                        }
                        weatherScrollX = DISPLAY_W;
                    }
                }
            } else {
                weatherScrollX = (DISPLAY_W - textPx) / 2;
                if (now - weatherLastMs >= cyclems) {
                    weatherLastMs = now;
                    if (maxMode > 1) {
                        forecastTransitionActive = true;
                        forecastTransitionFrom = 0;
                        forecastTransitionTo = 1;
                        forecastTransitionStep = 0;
                        forecastTransitionMs = now;
                        weatherScrollX = DISPLAY_W;
                    } else {
                        _buildCurrentStr();
                    }
                }
            }
        }
        dma_display->setTextSize(1);
        _printStrEx(weatherScrollX, 23, weatherScrollStr, C_TEMP);
    } else {
        // --- Modi 1-3: Vorhersage mit Ein-/Ausblend-Übergang ---
        if (weatherMode == 0) {
            _printStrEx(weatherScrollX, 23, weatherScrollStr, C_TEMP);
        } else if (!forecastTransitionActive) {
            _drawForecastStatic(weatherMode - 1, 0, 255);
            if (now - weatherLastMs >= cyclems) {
                weatherLastMs = now;
                forecastTransitionActive = true;
                forecastTransitionFrom = weatherMode;
                forecastTransitionTo = (weatherMode % 3) + 1;
                forecastTransitionStep = 0;
                forecastTransitionMs = now;
            }
        }
    }
}

// ============================================================
// OTA-Fortschrittsanzeige
// ============================================================
void displaySetOtaActive(bool active) {
    displayOtaActive = active;
    if (!active) _invalidateStaticDisplay();
}

void displayShowOTA(uint32_t progress, uint32_t total) {
    displayOtaActive = true;
    _invalidateStaticDisplay();
    dma_display->clearScreen();
    dma_display->setTextSize(1);
    dma_display->setTextColor(C_WARN);
    dma_display->setCursor(4, 4);
    dma_display->print("OTA Update");

    // Fortschrittsbalken: 60px breit, y=14..17 (4px hoch), x=2..61
    uint32_t filled = (total > 0) ? ((uint32_t)60 * progress / total) : 0;
    dma_display->drawRect(1, 13, 62, 6, C_INFO);
    if (filled > 0)
        dma_display->fillRect(2, 14, (int)filled, 4, C_TEMP);

    // Prozent-Anzeige
    char pct[5];
    uint32_t pctVal = (total > 0) ? (progress * 100 / total) : 0;
    snprintf(pct, sizeof(pct), "%3u%%", (unsigned)pctVal);
    dma_display->setTextColor(C_DATE);
    dma_display->setCursor(16, 22);
    dma_display->print(pct);

    dma_display->flipDMABuffer();
}

// ============================================================
// Scrolling-Text fürs Setup (IP-Anzeige)
// ============================================================
void displayScrollText(const String& text, uint16_t color, int speed_ms) {
    _invalidateStaticDisplay();
    int pixelLen = text.length() * 6;
    for (int x = DISPLAY_W; x > -(pixelLen); x--) {
        dma_display->clearScreen();
        dma_display->setTextSize(1);
        dma_display->setTextColor(C_DATE);
        dma_display->setCursor(0, 1);
        dma_display->print("Verbunden!");
        dma_display->setTextColor(color);
        dma_display->setCursor(x, 13);
        dma_display->print(text);
        dma_display->flipDMABuffer();
        delay(speed_ms);
    }
}

// ============================================================
// AP-Modus-Anzeige
// ============================================================
void displayShowAP(const String& ssid, const String& ip) {
    _invalidateStaticDisplay();
    dma_display->clearScreen();
    dma_display->setTextSize(1);
    dma_display->setTextWrap(false);
    dma_display->setTextColor(C_WARN);
    dma_display->setCursor(0, 1);   dma_display->print("AP-Modus:");
    dma_display->setTextColor(C_INFO);
    dma_display->setCursor(0, 9);   dma_display->print(ssid.substring(0, 10));
    dma_display->setTextColor(C_DATE);
    dma_display->setCursor(0, 13);  dma_display->print(ip);
    dma_display->setTextColor(C_INFO);
    dma_display->setCursor(0, 25);  dma_display->print("->config");
    dma_display->flipDMABuffer();
}

void displayShowMessage(const String& msg, uint16_t color) {
    _invalidateStaticDisplay();
    dma_display->fillRect(0, 11, DISPLAY_W, 21, C_BLACK);
    dma_display->setTextSize(1);
    dma_display->setTextColor(color);
    dma_display->setCursor(0, 23);
    dma_display->print(msg.substring(0, 10));
}

// ============================================================
// Reset aller statischen Display-Zustände + beide DMA-Buffer leeren.
// MUSS aus setup() aufgerufen werden, damit nach ESP.restart() kein
// RAM-Überbleibsel aus dem Vorstart zu Ghosting führt.
// ============================================================
void displayReset() {
    displayOtaActive = false;
    for (int i = 0; i < 6; i++) {
        flip[i] = FlipDigit();
        digitDirty[i] = 2;
    }
    colonFrame = -1; colonFrameMs = 0; colonLastSec = -1;
    weatherMode = 0; weatherLastMs = 0;
    weatherScrollX = DISPLAY_W; weatherScrollMs = 0;
    weatherScrollStr = "";
    forecastTransitionActive = false;
    forecastTransitionFrom = 1;
    forecastTransitionTo = 1;
    forecastTransitionStep = 0;
    forecastTransitionMs = 0;
    datLastMday = -1; datDirty = 2;  // Datum in beiden DMA-Buffern neu zeichnen
    dma_display->clearScreen(); dma_display->flipDMABuffer();
    dma_display->clearScreen(); dma_display->flipDMABuffer();
}

// ============================================================
// Haupt-Update-Schleife
// Kein clearScreen() — nur geänderte Bereiche werden neu gezeichnet,
// damit statische Ziffern nie flackern.
// ============================================================
void displayUpdate(struct tm* t) {
    if (displayOtaActive) return;

    // Bei Wechsel des seconds_mode: gesamten Ziffernbereich in BEIDEN DMA-Buffern leeren.
    // clearModeBuf=2 sorgt dafür, dass fillRect in den nächsten 2 Frames ausgeführt wird
    // (je einer pro Buffer), damit keine Geisterpixel des alten Layouts übrigbleiben.
    static uint8_t lastSecondsMode = 255;
    static uint8_t clearModeBuf   = 0;
    if (cfg.seconds_mode != lastSecondsMode) {
        lastSecondsMode = cfg.seconds_mode;
        clearModeBuf = 2;
        for (int i = 0; i < 6; i++) {
            flip[i] = FlipDigit();
            digitDirty[i] = 2;
        }
        colonFrame   = -1;
        colonLastSec = -1;
    }
    if (clearModeBuf > 0) {
        clearModeBuf--;
        dma_display->fillRect(0, 0, 64, _maxClockAreaHeight(), C_BLACK);
    }

    _setFlipTime(t->tm_hour, t->tm_min, t->tm_sec);

    uint32_t now = millis();

    // Kolon: Cross-Dissolve-Puls synchron zum Sekundenwechsel
    // Exakt wie Ziffern: 14 Frames à 18ms, Frame 0-6 hell→dunkel, 7-13 dunkel→hell
    if (t->tm_sec != colonLastSec) {
        colonLastSec = t->tm_sec;
        colonFrame   = 0;
        colonFrameMs = now;
    }
    if (colonFrame >= 0 && (now - colonFrameMs >= 18)) {
        colonFrameMs = now;
        if (++colonFrame >= 14) colonFrame = -1;
    }

    // Uhr-Ziffern: nur Geänderte/Animierende werden neu gezeichnet
    _drawFlipClock();

    // Sekundenbalken nur in Mode 0-2 neu zeichnen; bei HH:MM:SS liegen dort Ziffernpixel.
    if (cfg.seconds_mode != 3) {
        dma_display->fillRect(0, _secondsBarY(), 64, 2, C_BLACK);
        _drawSeconds(t->tm_sec);
    }

    // Datum: nur neu wenn geändert (Dirty-Counter auf Modulebene für displayReset())
        if (t->tm_mday != datLastMday) { datLastMday = t->tm_mday; datDirty = 2; }
        if (datDirty > 0) {
            datDirty--;
            _drawDateArea(t);
        }

    // Wetter-Zeile: immer neu (scrollt); ab y-2=21 damit Grossbuchstaben-Umlautpunkte (y-2) geloescht werden
    dma_display->fillRect(0, 21, DISPLAY_W, 11, C_BLACK);
    _drawWeatherLine();
    if (forecastTransitionActive) {
        _drawDateArea(t);
    }

    // AM/PM-Indikator nur im HH:MM-Modus
    if (cfg.time_12h && cfg.seconds_mode != 3) {
        uint16_t ampmCol = (t->tm_hour < 12) ? C_DATE : C_TEMP;
        dma_display->fillRect(59, 3, 4, 4, ampmCol);
    }

    dma_display->flipDMABuffer();
}
