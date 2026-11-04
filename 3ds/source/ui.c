/* Original vector interface. GPL-3.0-or-later. Game sprites are read only
 * from the user's EEPROM at runtime; no retail artwork is bundled. */
#include "ui.h"
#include "species.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifndef UI_PREVIEW
#include <citro2d.h>
static C3D_RenderTarget *targets[2];
static C2D_TextBuf textbuf, measurebuf;
#else
static FILE *canvas;
void uiPreviewOpen(const char *p, int bottom) {
    canvas = fopen(p, "w");
    fprintf(canvas, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"240\">",
            bottom ? 320 : 400);
}
void uiPreviewClose(void) {
    fputs("</svg>", canvas);
    fclose(canvas);
}
#endif
#define RGB(r, g, b) ((uint32_t)(r) | ((uint32_t)(g) << 8) | ((uint32_t)(b) << 16) | 0xff000000u)
static const uint32_t INK = RGB(233, 246, 240), MUTED = RGB(139, 172, 170),
                      TEAL = RGB(68, 221, 179), GOLD = RGB(252, 202, 102), PANEL = RGB(23, 48, 59),
                      RED = RGB(237, 111, 111), LCD = RGB(198, 219, 168);
static void rect(float x, float y, float w, float h, uint32_t c) {
#ifndef UI_PREVIEW
    C2D_DrawRectSolid(x, y, 0, w, h, c);
#else
    fprintf(canvas, "<rect x=\"%g\" y=\"%g\" width=\"%g\" height=\"%g\" fill=\"#%02x%02x%02x\"/>",
            x, y, w, h, c & 255, (c >> 8) & 255, (c >> 16) & 255);
#endif
}
static void circle(float x, float y, float radius, uint32_t c) {
#ifndef UI_PREVIEW
    C2D_DrawCircleSolid(x, y, 0, radius, c);
#else
    fprintf(canvas, "<circle cx=\"%g\" cy=\"%g\" r=\"%g\" fill=\"#%02x%02x%02x\"/>", x, y, radius,
            c & 255, (c >> 8) & 255, (c >> 16) & 255);
#endif
}
static void triangle(float x, float y, float w, float h, uint32_t c) {
#ifndef UI_PREVIEW
    C2D_DrawTriangle(x, y, c, x - w / 2, y + h, c, x + w / 2, y + h, c, 0);
#else
    fprintf(canvas, "<polygon points=\"%g,%g %g,%g %g,%g\" fill=\"#%02x%02x%02x\"/>", x, y,
            x - w / 2, y + h, x + w / 2, y + h, c & 255, (c >> 8) & 255, (c >> 16) & 255);
#endif
}
/* Bottom-screen text has a readable minimum size. Measure and wrap/clip
 * content instead of shrinking whole sentences into a few pixels. */
static int bottomText;
static float textWidth(const char *str, float scale) {
#ifndef UI_PREVIEW
    C2D_Text text;
    float width;
    C2D_TextBufClear(measurebuf);
    C2D_TextParse(&text, measurebuf, str);
    C2D_TextGetDimensions(&text, scale, scale, &width, 0);
    return width;
#else
    unsigned glyphs = 0;
    for (const unsigned char *p = (const unsigned char *)str; *p; p++)
        if ((*p & 0xc0) != 0x80)
            glyphs++;
    return glyphs * 30 * scale * .55f;
#endif
}
static unsigned previousGlyph(const char *str, unsigned length) {
    if (!length)
        return 0;
    length--;
    while (length && ((unsigned char)str[length] & 0xc0) == 0x80)
        length--;
    return length;
}
static void label(float x, float y, float scale, uint32_t c, const char *str, float maxwidth) {
    char fitted[512];
    {
        float minimum = bottomText ? .55f : .5f;
        if (scale < minimum)
            scale = minimum;
        snprintf(fitted, sizeof(fitted), "%s", str);
        unsigned length = (unsigned)strlen(fitted);
        if (textWidth(fitted, scale) > maxwidth) {
            do {
                length = previousGlyph(fitted, length);
                memcpy(fitted + length, "...", 4);
            } while (length && textWidth(fitted, scale) > maxwidth);
        }
        str = fitted;
    }
#ifndef UI_PREVIEW
    C2D_Text t;
    C2D_TextParse(&t, textbuf, str);
    C2D_TextOptimize(&t);
    float w;
    C2D_TextGetDimensions(&t, scale, scale, &w, 0);
    (void)w;
    C2D_DrawText(&t, C2D_WithColor, x, y, 0, scale, scale, c);
#else
    float size = 30 * scale;
    float estimate = (float)strlen(str) * size * .55f;
    (void)estimate;
    fprintf(canvas,
            "<text x=\"%g\" y=\"%g\" font-size=\"%g\" font-family=\"DejaVu Sans\" "
            "fill=\"#%02x%02x%02x\">",
            x, y + size * .9f, size, c & 255, (c >> 8) & 255, (c >> 16) & 255);
    for (const char *p = str; *p; p++) {
        if (*p == '&')
            fputs("&amp;", canvas);
        else if (*p == '<')
            fputs("&lt;", canvas);
        else if (*p == '>')
            fputs("&gt;", canvas);
        else
            fputc(*p, canvas);
    }
    fputs("</text>", canvas);
#endif
}
static void paragraph(float x, float y, float scale, uint32_t color, const char *text, float width,
                      unsigned lines, float spacing) {
    char line[512];
    if (bottomText && scale < .55f)
        scale = .55f;
    while (*text == ' ')
        text++;
    for (unsigned row = 0; row < lines && *text; row++) {
        unsigned count = 0, lastSpace = 0;
        while (text[count] && text[count] != '\n' && count < sizeof(line) - 5) {
            unsigned next = count + 1;
            while (text[next] && ((unsigned char)text[next] & 0xc0) == 0x80)
                next++;
            memcpy(line, text, next);
            line[next] = 0;
            if (textWidth(line, scale) > width)
                break;
            if (text[count] == ' ')
                lastSpace = count;
            count = next;
        }
        if (text[count] && text[count] != '\n' && lastSpace)
            count = lastSpace;
        if (!count)
            count = 1;
        memcpy(line, text, count);
        line[count] = 0;
        const char *remaining = text + count;
        while (*remaining == ' ' || *remaining == '\n')
            remaining++;
        if (row + 1 == lines && *remaining) {
            unsigned n = count;
            do {
                n = previousGlyph(line, n);
                memcpy(line + n, "...", 4);
            } while (n && textWidth(line, scale) > width);
        }
        label(x, y + row * spacing, scale, color, line, width);
        text = remaining;
    }
}
static void fmt(float x, float y, float scale, uint32_t c, float width, const char *f, ...) {
    char text[200];
    va_list a;
    va_start(a, f);
    vsnprintf(text, sizeof(text), f, a);
    va_end(a);
    label(x, y, scale, c, text, width);
}
static void roundbox(float x, float y, float w, float h, float r, uint32_t c) {
    rect(x + r, y, w - r * 2, h, c);
    rect(x, y + r, w, h - r * 2, c);
    circle(x + r, y + r, r, c);
    circle(x + w - r, y + r, r, c);
    circle(x + r, y + h - r, r, c);
    circle(x + w - r, y + h - r, r, c);
}
static void bar(float x, float y, float w, float h, float f, uint32_t color) {
    if (f < 0)
        f = 0;
    if (f > 1)
        f = 1;
    roundbox(x, y, w, h, h / 2, RGB(37, 65, 72));
    if (f * w > h)
        roundbox(x, y, f * w, h, h / 2, color);
    else if (f > 0)
        rect(x, y, f * w, h, color);
}
static void background(int width, unsigned frame) {
    for (int y = 0; y < 240; y += 4) {
        int r = 11 + y / 32, g = 26 + y / 20, b = 40 + y / 40;
        rect(0, y, width, 4, RGB(r, g, b));
    }
    circle(width - 38, 46, 21, RGB(44, 76, 80));
    circle(width - 31, 40, 19, RGB(14, 31, 44));
    for (int i = 0; i < 12; i++) {
        float x = (i * 47 + 23) % width, y = (i * 23 + 17) % 120;
        circle(x, y, 1, RGB(65, 103, 107));
    }
    for (int i = 0; i < 7; i++) {
        float x = i * width / 6.f;
        triangle(x, 155 + (i % 3) * 9, 110, 100, RGB(24, 55, 62));
    }
    for (int i = 0; i < 5; i++) {
        float x = i * width / 4.f;
        triangle(x, 183 + (i % 2) * 12, 120, 100, RGB(25, 66, 64));
    }
    rect(0, 229, width, 11, RGB(18, 45, 48));
    (void)frame;
}
static void raster(const uint8_t *ee, unsigned off, int width, int height, float x, float y,
                   float scale) {
    if (!ee || off + (unsigned)width * height / 4 > WALKER_FILE_SIZE)
        return;
    const uint32_t colors[4] = {LCD, RGB(153, 180, 132), RGB(92, 126, 91), RGB(43, 69, 57)};
    int flip=width==32 && height==24 && off>=0x91be && off<0x933e && !(ee[WALKER_ROUTE+14]&1);
    for (int py = 0; py < height; py++)
        for (int px = 0; px < width; px++) {
            unsigned pos = off + ((py / 8) * width + px) * 2;
            unsigned bit = py & 7;
            unsigned c = ((ee[pos] >> bit) & 1) * 2 + ((ee[pos + 1] >> bit) & 1);
            if (c)
                rect(x + (flip?width-1-px:px) * scale, y + py * scale, scale, scale, colors[c]);
        }
}
static void device(const UIState *s) {
    circle(113, 120, 87, RGB(7, 19, 30));
    circle(111, 115, 84, RGB(219, 230, 219));
    for (int y = -79; y < 0; y++) {
        float half = sqrtf(80 * 80 - y * y);
        rect(111 - half, 115 + y, half * 2, 1, RGB(208, 74 + (y + 79) / 4, 76));
    }
    roundbox(38, 69, 146, 102, 13, RGB(43, 62, 58));
    roundbox(44, 75, 134, 90, 8, LCD);
    if (s->loaded && s->info.hasPokemon && s->ee) {
        if (s->screen == UI_BATTLE) {
            raster(s->ee, 0x91be + ((s->frame / 24) & 1) * 192, 32, 24, 57, 105, 1.25f);
            raster(s->ee, 0x9a7e + s->battle.encounter * 384 + ((s->frame / 24) & 1) * 192, 32, 24,
                   125, 105, 1.25f);
        } else
            /* Preserve the original large portrait, including its native crop. */
            raster(s->ee, 0x933e + ((s->frame / 24) & 1) * 768, 64, 48, 63, 84, 1.5f);
    } else {
        circle(111, 118, 22, RGB(108, 142, 105));
        rect(88, 115, 46, 6, LCD);
        circle(111, 118, 9, LCD);
        circle(111, 118, 5, RGB(108, 142, 105));
    }
    for (int i = 0; i < 3; i++) {
        circle(78 + i * 33, 185, 9, RGB(53, 77, 73));
        circle(78 + i * 33, 183, 7, RGB(114, 140, 126));
    }
    label(66, 50, .5f, RGB(252, 231, 218), "POKÉWALKER", 110);
}
static void infoTop(const UIState *s) {
    device(s);
    roundbox(212, 57, 168, 55, 10, PANEL);
    label(225, 63, .32f, MUTED, "STROLL STEPS", 142);
    fmt(225, 82, .8f, INK, 140, "%lu", (unsigned long)(s->info.strollSteps + s->pending));
    roundbox(212, 120, 79, 53, 9, PANEL);
    label(223, 128, .3f, MUTED, "WATTS", 60);
    fmt(223, 146, .65f, GOLD, 60, "%u", s->info.watts);
    roundbox(299, 120, 81, 53, 9, PANEL);
    label(310, 128, .3f, MUTED, "TODAY", 60);
    fmt(310, 146, .53f, TEAL, 61, "%lu", (unsigned long)(s->info.todaySteps + s->pending));
    unsigned toward = (s->info.stepsThisWatt + s->pending) % 20;
    float frac = (float)toward / 20;
    if (s->secondsPerStep && s->now >= s->info.lastCredited)
        frac += ((s->now - s->info.lastCredited) % s->secondsPerStep) / (20.f * s->secondsPerStep);
    label(214, 180, .3f, MUTED,
          s->walkingEnabled  ? "NEXT WATT"
          : s->sensorEnabled ? "PEDOMETER"
                             : "PAUSED",
          110);
    fmt(325, 180, .3f, TEAL, 52, "%u/20", toward);
    bar(214, 197, 164, 7, frac, TEAL);
    label(24, 217, .34f, INK, s->info.route[0] ? s->info.route : "YOUR NEXT ADVENTURE", 260);
}
/* Transient notices do not share storage with game prompts or result titles. */
void uiNotify(UIState *s, const char *text, uint64_t now) {
    snprintf(s->toast, sizeof(s->toast), "%s", text);
    s->toastScreen = s->screen;
    s->toastUntil = now + 5000;
}
void uiUpdateClock(UIState *s, uint64_t now) {
    s->clockMs = now;
    if (now >= s->toastUntil || s->screen != s->toastScreen) {
        s->toast[0] = 0;
        s->toastUntil = 0;
    }
}
static void status(const UIState *s) {
    const char *text = (s->screen == UI_SETTINGS || s->screen == UI_PACK) ? s->toast : s->message;
    if (!text[0])
        return;
    roundbox(12, 182, 296, 37, 6, RGB(28, 66, 67));
    paragraph(21, 183, .55f, INK, text, 278, 2, 17);
}
int uiInit(void) {
#ifndef UI_PREVIEW
    gfxInitDefault();
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE))
        return 0;
    if (!C2D_Init(12000))
        return 0;
    C2D_Prepare();
    /* Citro2D applies linear filtering to the shared system font. */
    targets[0] = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    targets[1] = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    textbuf = C2D_TextBufNew(8192);
    measurebuf = C2D_TextBufNew(1024);
    return textbuf && measurebuf && targets[0] && targets[1];
#else
    return 1;
#endif
}
void uiExit(void) {
#ifndef UI_PREVIEW
    C2D_TextBufDelete(textbuf);
    C2D_TextBufDelete(measurebuf);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
#endif
}
void uiBegin(int bottom) {
#ifndef UI_PREVIEW
    if (!bottom) {
        C2D_TextBufClear(textbuf);
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    }
    C2D_TargetClear(targets[bottom], RGB(10, 24, 35));
    C2D_SceneBegin(targets[bottom]);
#else
    (void)bottom;
#endif
}
void uiEnd(void) {
#ifndef UI_PREVIEW
    C3D_FrameEnd(0);
#endif
}
void uiTop(const UIState *s) {
    bottomText = 0;
    background(400, s->frame);
    label(23, 12, .7f, INK, "3DS Pokewalker", 352);
    rect(24, 43, 352, 1, RGB(44, 77, 81));
    if (s->screen == UI_SCAN) {
        device(s);
        label(215, 72, .55f, INK, "A little discovery", 165);
        label(215, 99, .37f, MUTED, "Finding your walkers", 165);
        label(215, 117, .37f, MUTED, "inside /roms only.", 165);
        fmt(215, 151, .4f, TEAL, 165, "%d found", s->count);
        label(24, 215, .32f, MUTED, "Your next adventure is waiting.", 350);
        return;
    }
    if (s->screen == UI_LIST && !s->count) {
        device(s);
        label(214, 74, .55f, INK, "A fresh start", 165);
        label(214, 106, .35f, MUTED, "No walkers in /roms.", 165);
        label(214, 128, .35f, MUTED, "Start one in HGSS first.", 165);
        return;
    }
    if(s->screen==UI_LIST && s->loaded) {
        device(s);
        label(214,65,.48f,TEAL,"TRAINER",165);
        label(214,91,.55f,INK,s->info.registered&&s->info.trainer[0]?s->info.trainer:"Unavailable",165);
        if(s->info.registered)fmt(214,124,.48f,GOLD,165,"ID %05lu",(unsigned long)s->info.trainerId);
        else label(214,124,.44f,GOLD,"ID unavailable",165);
        label(214,158,.46f,INK,s->info.hasPokemon?speciesName(s->info.species):"No companion",165);
        label(24,216,.4f,MUTED,s->filename,350);return;
    }
    infoTop(s);
    if (s->screen == UI_BATTLE) {
        roundbox(211, 57, 170, 116, 10, PANEL);
        fmt(224, 66, .55f, INK, 145, "%s",
            speciesName(walkerRoutePokemonId(s->ee, s->battle.encounter)));
        label(225, 94, .3f, MUTED, "WILD", 60);
        label(225, 132, .3f, MUTED, "PARTNER", 80);
        for (int i = 0; i < 4; i++) {
            roundbox(225 + i * 35, 110, 29, 10, 3, i < s->battle.enemyHp ? GOLD : RGB(45, 65, 70));
            roundbox(225 + i * 35, 148, 29, 10, 3, i < s->battle.playerHp ? TEAL : RGB(45, 65, 70));
        }
        return;
    }
    if (s->screen == UI_RADAR) {
        label(215, 63, .48f, INK, "Poké Radar", 160);
        fmt(215, 86, .34f, MUTED, 160, "Rustle %d / %d", s->round + 1, s->rounds);
        label(215, 181, .3f, GOLD, "RESPONSE WINDOW", 160);
        bar(214, 197, 164, 7, s->progress, GOLD);
    }
}
static void tile(float x, float y, float w, float h, const char *a, const char *b, int chosen,
                 uint32_t accent) {
    roundbox(x, y, w, h, 9, chosen ? RGB(34, 87, 86) : PANEL);
    rect(x + 7, y + 7, 4, h - 14, chosen ? GOLD : accent);
    if (chosen) {
        rect(x + 8, y, w - 16, 2, GOLD);
        rect(x + 8, y + h - 2, w - 16, 2, GOLD);
    }
    label(x + 19, y + 5, .55f, chosen ? INK : RGB(205, 226, 221), a, w - 25);
    label(x + 19, y + 25, .55f, INK, b, w - 25);
}
void uiBottom(const UIState *s) {
    bottomText = 1;
    background(320, s->frame);
    if (s->saving) {
        label(17, 12, .7f, INK, "Saving...", 290);
        roundbox(12, 48, 296, 150, 8, PANEL);
        paragraph(23, 60, .6f, INK, s->saveStage, 274, 3, 22);
        paragraph(23, 128, .55f, GOLD,
                  "Please wait. Keep the console on until saving finishes.", 274, 3, 20);
        label(17, 221, .5f, INK, "Controls locked while saving", 287);
        return;
    }
    if (s->saveError) {
        label(17, 12, .7f, INK, s->discardPrompt ? "Discard changes?" : "Save failed", 290);
        roundbox(12, 48, 296, 125, 8, RGB(64, 29, 36));
        if (s->discardPrompt) {
            paragraph(23, 60, .6f, INK, "Discard unsaved changes and reload the SD file?", 274, 3,
                      22);
            paragraph(23, 137, .55f, INK, "A Discard changes. B Keep changes.", 274, 2, 20);
        } else {
            paragraph(23, 57, .6f, INK, s->saveStage, 274, 2, 22);
            paragraph(23, 106, .55f, INK, s->saveReason, 274, 3, 20);
        }
        if (!s->discardPrompt) {
            for (int n = 0; n < 2; n++) {
                roundbox(15 + n * 153, 181, 137, 30, 6,
                         s->saveChoice == n ? RGB(34, 97, 87) : PANEL);
                label(27 + n * 153, 185, .6f, s->saveChoice == n ? GOLD : INK,
                      n ? "Discard..." : "Retry save", 113);
            }
        }
        label(17, 218, .55f, INK,
              s->discardPrompt ? "A Discard   B Keep" : "D-pad  A Select  Y Retry", 290);
        return;
    }
    if (s->screen == UI_SCAN) {
        label(20, 29, .63f, INK, s->validation ? "Checking walkers" : "Exploring /roms", 280);
        paragraph(20, 64, .55f, INK,
                  s->validation ? "Reading the 64 KiB EEPROM files"
                                : "Subfolders included. System folders skipped.",
                  280, 2, 18);
        roundbox(20, 110, 280, 14, 7, RGB(37, 65, 72));
        if (s->validation)
            bar(20, 110, 280, 14, s->progress, TEAL);
        else {
            float x = 20 + (s->frame % 120) / 119.f * 210;
            roundbox(x, 110, 70, 14, 7, TEAL);
        }
        fmt(20, 135, .43f, INK, 280, "%d folders   ·   %d files", s->dirs, s->files);
        if (s->validation)
            fmt(20, 159, .36f, TEAL, 280, "%d of %d walkers checked", s->done, s->total);
        label(20, 214, .3f, MUTED, "START  Cancel search", 280);
        return;
    }
    if (s->screen == UI_LIST) {
        label(17, 12, .55f, INK, "Your walkers", 210);
        fmt(232, 17, .55f, TEAL, 75, "%d", s->count);
        if (!s->count) {
            label(20, 58, .43f, INK, "No .walker files yet", 280);
            paragraph(20, 90, .55f, INK,
                      "Enable Virtual Pokewalker in HGSS's Y menu. Keep its file under /roms.", 280,
                      4, 20);
        }
        for (int row = 0; row < 7; row++) {
            int index = s->page + row;
            if (index >= s->count)
                break;
            float y = 43 + row * 24;
            int selected = index == s->cursor;
            roundbox(12, y, 296, 22, 5, selected ? RGB(37, 97, 89) : PANEL);
            const char *p = strrchr(s->paths[index], '/');
            p = p ? p + 1 : s->paths[index];
            label(23, y + 3, .55f, INK, p, s->statuses[index] >= 2 ? 205 : 250);
            if (s->statuses[index] >= 2)
                label(239, y + 3, .55f, GOLD, s->statuses[index] == 3 ? "Phone" : "Empty", 60);
            else
                circle(292, y + 11, 3, s->statuses[index] == 1 ? TEAL : RED);
        }
        if (s->scanLimited)
            label(16, 212, .55f, GOLD, "Search limit reached", 288);
        label(16, 223, .55f, INK, "A Open  Y Games  SELECT Recovery", 288);
        return;
    }
    if(s->screen==UI_COURSE_CONFIRM){
        label(17,12,.6f,INK,"Special/event courses",290);
        paragraph(20,45,.5f,INK,"Activate special/event courses supported by this HGSS save's language. Normal watt unlocks stay unchanged. The game save is verified and backed up first.",280,6,20);
        paragraph(20,168,.42f,GOLD,s->phonePaths[s->phoneCursor],280,2,17);label(17,220,.5f,INK,"A Activate  B Cancel",290);return;
    }
    if(s->screen==UI_PHONE_TARGET || s->screen==UI_PHONE_RECEIVE_CONFIRM){
        label(17,12,.6f,INK,s->screen==UI_PHONE_TARGET?"Return destination":"Receive phone copy?",290);
        if(s->screen==UI_PHONE_TARGET){
            for(int row=0;row<6;row++){int n=(s->phoneCursor/6)*6+row;if(n>=s->phoneCount)break;
                const char *name=strrchr(s->phonePaths[n],'/');roundbox(12,42+row*27,296,24,5,n==s->phoneCursor?RGB(34,87,86):PANEL);label(20,46+row*27,.45f,INK,name?name+1:s->phonePaths[n],280);
            }
            label(17,220,.5f,INK,"A Phone  X Courses  B Cancel",290);
        }else{
            paragraph(20,45,.5f,INK,"Use only if the console walker is missing. Receive the phone's current copy into this game's saves folder. Existing active walkers cannot be overwritten.",280,6,20);
            paragraph(20,168,.42f,GOLD,s->phonePaths[s->phoneCursor],280,2,17);label(17,220,.48f,INK,"A Receive phone  B Cancel",290);
        }return;
    }
    if(s->screen==UI_REWARD_SETTINGS) {
        label(17,12,.6f,INK,"Dowsing rewards",290);
        paragraph(20,45,.5f,INK,"Adds Master Ball, PP items, TMs, vitamins, Rare Candy, EV-training items, evolution items, held items and Heart Scales. 92% of Dowsing finds still use the course. Off by default.",280,7,20);
        label(20,191,.5f,TEAL,s->expandedItems?"Currently enabled":"Currently disabled",280);
        label(17,220,.48f,INK,"A Opt in  X Disable  B Back",290);return;
    }
    if(s->screen==UI_RESTORE_BACKUP) {
        label(17,12,.6f,INK,"Restore selected backup?",290);
        paragraph(20,48,.5f,INK,"A restores the backup and revokes the phone. X receives matching phone progress into a missing target. Old data may duplicate Pokemon already received in HGSS.",280,6,19);
        paragraph(20,168,.43f,GOLD,s->backupName,280,2,18);
        label(17,220,.48f,INK,"A Restore  X From phone  B Cancel",290);return;
    }
    if(s->screen==UI_RECOVER_SAVE) {
        label(17,12,.6f,INK,"Recover interrupted save?",290);
        paragraph(20,48,.5f,INK,"The final walker is missing. Choose a verified file to restore. The originals are kept.",280,3,19);
        paragraph(20,109,.48f,GOLD,s->backupName,280,2,18);
        paragraph(20,153,.48f,INK,s->backupDetail,280,2,18);
        label(17,220,.43f,INK,"L/R File  A Recover  B Cancel",290);return;
    }
    if(s->screen==UI_BACKUPS || s->screen==UI_BACKUP_DELETE || s->screen==UI_DELETE_ACTIVE) {
        int active=s->screen==UI_DELETE_ACTIVE,confirm=s->screen!=UI_BACKUPS;
        label(17,12,.6f,INK,active?"Delete active walker?":confirm?"Delete recovery copy?":"Recovery copies",290);
        if(confirm) {
            paragraph(20,50,.55f,INK,active?"Remove this device's active walker? A recovery copy is preserved first. A walker on the phone keeps its return permission.":"Permanently delete this recovery snapshot? The active walker and ownership records stay intact.",280,5,21);
            paragraph(20,161,.42f,GOLD,active?s->backupWalker:s->backupName,280,2,18);
            label(17,220,.55f,INK,"A Confirm delete    B Cancel",290);
        } else {
            fmt(20,44,.5f,TEAL,280,"%d / %d copies%s",s->backupCount?s->backupIndex+1:0,s->backupCount,s->backupLimited?" (first 128)":"");
            paragraph(20,68,.4f,INK,s->backupCount?s->backupName:"No recovery snapshots yet.",280,2,18);
            paragraph(20,100,.5f,MUTED,s->backupDetail,280,2,18);
            label(20,136,.4f,TEAL,"START Restore  SELECT Missing file",280);
              roundbox(12,150,296,27,7,PANEL);label(22,155,.5f,INK,"A Delete selected backup",275);
            roundbox(12,181,296,27,7,PANEL);label(22,186,.5f,GOLD,"X Delete active walker",275);
            label(17,222,.48f,INK,"L/R Copy   Y Walker   B Back",290);
        }
        return;
    }
    if (s->screen == UI_SYNC) {
        label(17, 12, .65f, INK, "Phone Wi-Fi sync", 290);
        fmt(20, 48, .6f, TEAL, 280, "%s:30820", s->syncAddress);
        label(20, 77, .55f, GOLD, "Enter this IP on your phone.", 280);
        paragraph(20, 110, .55f, INK, s->syncStatus, 280, 3, 20);
        paragraph(20, 174, .55f, MUTED, "Keep both devices on the same local Wi-Fi.", 280, 2, 18);
        label(17, 221, .55f, INK, s->phoneLocked ? "B Back  X Force unlock" : "B Back  START Save and exit",
              287);
        return;
    }
    if (s->screen == UI_LOCKED || s->screen == UI_FORCE) {
        int force = s->screen == UI_FORCE;
        label(17, 12, .65f, INK, force ? "Force unlock?" : "Walker is on phone", 290);
        paragraph(20, 53, .55f, INK,
                  force ? "Restore the console snapshot? Recovery copies are kept. The old "
                          "phone session can no longer sync back until a fresh handoff."
                        : "Console loading is locked while a phone owns this walker. Return it "
                          "over Wi-Fi, or recover the local snapshot if the phone is unavailable.",
                  280, 7, 20);
        label(17, 218, .55f, GOLD,
              force ? "A Restore and revoke  B Cancel" : "A Sync  X Recover  R Backups  B Back", 287);
        return;
    }
    const char *heading = s->screen == UI_HOME       ? "A little adventure"
                          : s->screen == UI_RADAR    ? "Find the rustling patch"
                          : s->screen == UI_DOWSE    ? "Dowsing"
                          : s->screen == UI_BATTLE   ? "Choose your move"
                          : s->screen == UI_PACK     ? "Pokémon & items"
                          : s->screen == UI_REPLACE  ? "Pack full — choose a slot"
                          : s->screen == UI_CARD     ? "Trainer card"
                          : s->screen == UI_SETTINGS ? "Settings 1.5"
                                                     : "Adventure journal";
    label(17, 12, .52f, INK, heading, 290);
    if (s->screen == UI_HOME) {
        const char *a[] = {"Poké Radar",   "Dowsing",    "Your pack",
                           "Trainer card", "Sync steps", "Settings"};
        const char *b[] = {"10 watts",     "3 watts",       "Your rewards",
                           "Stroll stats", "Save progress", "Step rate"};
        if (s->pendingEncounter) {
            a[0] = "Wild Pokémon!";
            b[0] = "Free encounter";
        }
        for (int i = 0; i < 6; i++)
            tile(12 + (i % 2) * 154, 44 + (i / 2) * 50, 142, 46, a[i], b[i], i == s->selection,
                 i < 2 ? GOLD : TEAL);
        label(16, 198, .55f, s->saveError ? RED : TEAL,
              s->saveError ? "Save failed: Y retry, X discard" : s->toast, 288);
    } else if (s->screen == UI_RADAR || s->screen == UI_DOWSE) {
        int radar = s->screen == UI_RADAR, n = radar ? 4 : 6, cols = radar ? 2 : 3;
        float w = radar ? 137 : 91;
        for (int i = 0; i < n; i++) {
            float x = 14 + (i % cols) * (radar ? 155 : 101), y = 57 + (i / cols) * 62;
            int active = i == s->activePatch, sel = i == s->selectedPatch;
            roundbox(x, y, w, 52, 8, active ? RGB(94, 111, 61) : sel ? RGB(34, 91, 83) : PANEL);
            triangle(x + w * .35f, y + 13, 20, 22, active ? GOLD : RGB(71, 135, 103));
            triangle(x + w * .58f, y + 9, 24, 28, active ? GOLD : RGB(88, 158, 115));
            if (active)
                label(x + w - 24, y + 7, .55f, INK, "!", 20);
            if (sel)
                rect(x + 10, y + 46, w - 20, 2, TEAL);
        }
        if (!radar)
            fmt(18, 43, .3f, GOLD, 280, "%d attempts left", s->attempts);
        status(s);
    } else if (s->screen == UI_BATTLE) {
        tile(12, 57, 94, 67, "Attack", "A", s->selection == 0, TEAL);
        tile(113, 57, 94, 67, "Evade", "A", s->selection == 1, TEAL);
        tile(214, 57, 94, 67, "Catch", "A", s->selection == 2, GOLD);
        paragraph(20, 141, .55f, INK, "Lower HP makes catching easier.", 280, 2, 18);
        status(s);
    } else if (s->screen == UI_PACK || s->screen == UI_REPLACE) {
        int replace=s->screen==UI_REPLACE;
        for(int i=0;i<3;i++) {
            unsigned id=replace&&s->replaceKind?walkerItemId(s->ee,i):walkerPokemonId(s->ee,i);
            const char *name=replace&&s->replaceKind?itemName(id):(id&&id!=65535?speciesName(id):"Empty");
            int y=replace?48+i*44:42+i*26;
            roundbox(12,y,296,replace?38:24,6,replace&&s->selection==i?RGB(34,87,86):PANEL);
            label(22,y+3,.55f,INK,name,replace?270:205);
            if(replace)fmt(22,y+23,.38f,TEAL,270,"Replace slot %d",i+1);
            else label(239,y+5,.4f,TEAL,"Pokémon",60);
        }
        if(!replace) {
            for(int i=0;i<3;i++) {
                int y=134+i*26;
                roundbox(12,y,296,24,6,PANEL);
                label(22,y+3,.55f,INK,itemName(walkerItemId(s->ee,i)),210);
                label(260,y+5,.4f,GOLD,"Item",38);
            }
        } else status(s);
    } else if (s->screen == UI_CARD) {
        fmt(20, 52, .53f, INK, 280, "Trainer  %s",
            s->info.trainer[0] ? s->info.trainer : "Not registered");
        if(s->info.hasPokemon) fmt(20, 85, .55f, INK, 280, "%s · Lv %u", speciesName(s->info.species), s->info.level);
        else label(20,85,.55f,INK,"Walking solo · a wild Pokémon may join",280);
        fmt(20, 112, .43f, MUTED, 280, "Bond  %u / 255", s->info.friendship);
        fmt(20, 139, .43f, MUTED, 280, "%u Pokémon   ·   %u items packed", s->info.pokemonCount,
            s->info.itemCount);
        fmt(20, 167, .55f, TEAL, 280, "Lifetime: %lu steps", (unsigned long)s->info.totalSteps);
    } else if (s->screen == UI_SETTINGS) {
        const char *labels[] = {"Time-based", "Pedometer", "Step rate", "Phone sync"};
        for (int n = 0; n < 4; n++) {
            roundbox(12, 40 + n * 39, 296, 33, 6, s->selection == n ? RGB(34, 87, 86) : PANEL);
            label(22, 47 + n * 39, .55f, INK, labels[n], 170);
            if (n == 3)
                label(224, 47 + n * 39, .55f, TEAL, "Open", 74);
            else if (n == 2)
                fmt(218, 47 + n * 39, .55f, TEAL, 82, "%us", s->secondsPerStep);
            else
                label(224, 47 + n * 39, .55f, TEAL,
                      n == 0                ? (s->walkingEnabled ? "On" : "Off")
                      : !s->sensorAvailable ? "N/A"
                      : s->sensorEnabled    ? "On"
                                            : "Off",
                      74);
        }

        roundbox(12,198,296,20,5,PANEL);
        label(22,200,.5f,TEAL,"X Backups   SELECT Rewards",280);
    } else if (s->screen == UI_RESULT) {
        roundbox(15, 43, 290, 168, 10, PANEL);
        paragraph(26, 49, .6f, INK, s->message, 268, 2, 21);
        paragraph(26, 96, .55f, INK, s->detail, 268, 6, 18);
        label(20, 220, .55f, TEAL, "A Continue   B Back", 280);
    }
    if (s->screen != UI_RESULT)
        label(17, 222, .55f, INK,
              s->saveError ? "Y Retry save   ·   Keep app open"
              : s->screen == UI_HOME ? "START Save and exit   B Back"
                           : "D-pad / Touch   A Select   B Back",
              287);
}
