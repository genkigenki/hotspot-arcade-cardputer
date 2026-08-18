// Cardputer host UI: the screens the Flipper build draws with ViewDispatcher +
// SceneManager, redone for a 240x135 colour panel and a 56-key keyboard.
//
// Drawing happens only from loop(), never from an async callback: the UI takes a
// snapshot of the host mirror under the engine lock, releases it, and draws from
// the copy. That keeps the ~10ms sprite push off the lock, so the WebSocket task
// is never blocked by the screen.
#pragma once
#include <M5Cardputer.h>
#include <Preferences.h>
#include "ha_host.h"

// ---- implemented in the .ino (they touch the engine / WiFi under the lock) ----
void haHostSelectGame(uint8_t game);
void haHostResetScores();
void haHostRoundEnd();
void haHostApplySsid(const char* ssid);
void haHostTogglePortal();
void haCfgSave(); // persist SSID/audio/language to the SD card
const char* haHostSsid();
String haHostIp();
void haHostSnapshot(HaHost& dst);

// Same list, same order as the Flipper's game_select scene. `duel` marks the 1v1
// challenge games (they pair players off into matches); `desc` is a one-line blurb
// shown under the selection.
extern uint8_t haLang; // 0 = en, 1 = de (defined in the .ino); also declared below

struct HaGameItem {
    uint8_t id;
    const char* label;
    const char* desc;
    bool duel;
    const char* label_de;
    const char* desc_de;
};
static const HaGameItem HA_UI_GAMES[] = {
    {HA_GAME_TRIVIA, "Trivia", "Quiz, fastest right wins", false, "Trivia", "Quiz, schnellste richtig gewinnt"},
    {HA_GAME_WYR, "Would You Rather", "Group vote, A or B", false, "Entweder oder", "Gruppenwahl, A oder B"},
    {HA_GAME_SCRAMBLE, "Word Scramble", "Unscramble the word", false, "Wortsalat", "Wort entwirren"},
    {HA_GAME_SPECTRUM, "Spectrum", "Give a clue, dial to guess", false, "Spektrum", "Hinweis geben, Regler raten"},
    {HA_GAME_KMK, "Kiss Marry Kill", "Predict a player's picks", false, "Kiss Marry Kill", "Errate die Wahl eines Spielers"},
    {HA_GAME_REACT, "Reaction Duel", "Tap on green, fastest wins", false, "Reaktionsduell", "Bei Gruen tippen, Schnellster gewinnt"},
    {HA_GAME_CONNECT4, "Connect Four", "Four in a row", true, "Vier gewinnt", "Vier in einer Reihe"},
    {HA_GAME_TICTACTOE, "Tic-Tac-Toe", "Three in a row", true, "Tic-Tac-Toe", "Drei in einer Reihe"},
    {HA_GAME_DOTS, "Dots & Boxes", "Close the most boxes", true, "Kaestchen", "Die meisten Kaestchen schliessen"},
    {HA_GAME_REVERSI, "Reversi", "Flip discs, most wins", true, "Reversi", "Steine drehen, meiste gewinnt"},
    {HA_GAME_DRAW, "Drawing", "Draw it, others guess", false, "Malen", "Malen, andere raten"},
    {HA_GAME_PONG, "Pong", "Classic paddle rally", true, "Pong", "Klassisches Paddel-Duell"},
    {HA_GAME_GUESSCOLOR, "Guess the Color", "Match the RGB colour", false, "Farbe raten", "RGB-Farbe treffen"},
    {HA_GAME_BATTLESHIP, "Battleship", "Hide a fleet, sink theirs", true, "Schiffe versenken", "Flotte verstecken, versenken"},
    {HA_GAME_CHESS, "Chess", "1v1, full chess rules", true, "Schach", "1v1, volle Schachregeln"},
    {HA_GAME_SECRETS, "Secrets", "Hidden vote: guess the yes-count", false, "Secrets", "Verdeckt: rate die Ja-Zahl"},
    {HA_GAME_FILLBLANK, "Fill the Blank", "A judge picks the funniest card", false, "Lueckenfueller", "Jury kuert die lustigste Karte"},
    {HA_GAME_WEREWOLF, "Werewolf", "Hidden roles, 5+ players", false, "Werwolf", "Geheime Rollen, ab 5 Spielern"},
    {HA_GAME_SPYFALL, "Spyfall", "Find the spy, 3+ players", false, "Spyfall", "Findet den Spion, ab 3 Spielern"},
    {HA_GAME_FRANKENDRAW, "Draw a Monster", "Three hands, one creature", false, "Monster malen", "Drei Haende, ein Wesen"},
    {HA_GAME_NONE, "None (lobby)", "Just the join lobby", false, "Keins (Lobby)", "Nur die Lobby"},
};
static const int HA_UI_GAME_COUNT = sizeof(HA_UI_GAMES) / sizeof(HA_UI_GAMES[0]);

// Host-UI string picker: German when the Settings language is German, else English.
static inline const char* hu(const char* en, const char* de) { return haLang == 1 ? de : en; }
static inline const char* hgLabel(const HaGameItem& g) { return haLang == 1 ? g.label_de : g.label; }
static inline const char* hgDesc(const HaGameItem& g) { return haLang == 1 ? g.desc_de : g.desc; }

// Upstream PR #18 lets the phones vote the game away from under the host. The engine
// announces the winner by NAME ({"gamevote":"approved","game":"wyr"}), so the host needs
// the inverse of the engine's gameName() to move its own screen. Kept here beside the
// game table it belongs to; the strings are the engine's, copied from gameName().
struct HaEngName {
    uint8_t id;
    const char* eng;
};
static const HaEngName HA_ENG_NAMES[] = {
    {HA_GAME_TRIVIA, "trivia"},         {HA_GAME_CONNECT4, "connect4"},
    {HA_GAME_TICTACTOE, "tictactoe"},   {HA_GAME_DOTS, "dots"},
    {HA_GAME_DRAW, "draw"},             {HA_GAME_PONG, "pong"},
    {HA_GAME_REACT, "react"},           {HA_GAME_WYR, "wyr"},
    {HA_GAME_SCRAMBLE, "scramble"},     {HA_GAME_REVERSI, "reversi"},
    {HA_GAME_GUESSCOLOR, "gc"},         {HA_GAME_BATTLESHIP, "bs"},
    {HA_GAME_SPECTRUM, "spectrum"},     {HA_GAME_KMK, "kmk"},
    {HA_GAME_CHESS, "chess"},           {HA_GAME_SECRETS, "secrets"},
    {HA_GAME_FILLBLANK, "fillblank"},   {HA_GAME_WEREWOLF, "werewolf"},
    {HA_GAME_SPYFALL, "spyfall"},       {HA_GAME_FRANKENDRAW, "frankendraw"},
    {HA_GAME_NONE, "none"},
};

static uint8_t haUiGameIdByName(const char* n) {
    for(unsigned i = 0; i < sizeof(HA_ENG_NAMES) / sizeof(HA_ENG_NAMES[0]); i++)
        if(strcmp(HA_ENG_NAMES[i].eng, n) == 0) return HA_ENG_NAMES[i].id;
    return HA_GAME_NONE;
}

static const char* haUiGameLabel(uint8_t id) {
    for(int i = 0; i < HA_UI_GAME_COUNT; i++)
        if(HA_UI_GAMES[i].id == id) return hgLabel(HA_UI_GAMES[i]);
    return hu("None", "Keins");
}

enum HaUiView {
    HA_VIEW_DASH,
    HA_VIEW_GAMES,
    HA_VIEW_BOARD,
    HA_VIEW_CONSOLE,
    HA_VIEW_SSID,
    HA_VIEW_SETTINGS
};

// Audio level (0 off / 1 low / 2 high) lives in the .ino (the speaker jingles are
// there); the settings screen reads and cycles it.
extern uint8_t haAudioLevel;

// Content language, selected once in Settings. The firmware bakes every language's
// packs; the host streams only the selected one (English fallback per game). haLang
// indexes the tables below; the .ino owns the variable, persists it, and re-streams
// the packs when haLangDirty is set. Interface text stays English for now.
extern uint8_t haLang;
extern bool haLangDirty; // set by Settings; the .ino reloads packs and clears it
#define HA_LANG_COUNT 3
static const char* const HA_LANG_CODE[HA_LANG_COUNT] = {"en", "de", "pt-br"};
// "Portugues" without the circumflex on purpose: the host panel font is ASCII-only and
// would draw a placeholder box. The phone UI is unaffected -- it has real fonts.
static const char* const HA_LANG_NAME[HA_LANG_COUNT] = {"English", "Deutsch", "Portugues"};

#define HA_SET_COUNT 5 // settings rows: SSID, Audio, Language, AP, Event log

static M5Canvas haUiCanvas(&M5Cardputer.Display);
static bool haUiSprite = false;
static HaUiView haUiView = HA_VIEW_DASH;
static int haUiCursor = 0;
static int haUiScroll = 0;
static int haUiDashScroll = 0; // dashboard player-list scroll offset
static char haUiEdit[33] = "";
static uint8_t haGameSort = 0;       // game picker order: 0 alphabetical, 1 most played
// Indexed by game id, so it must cover the HIGHEST id, not the game count: Secrets
// is id 16 and was already writing one past the old [16] -- silently, into whatever
// static came next. 32 leaves room for upstream's next games too.
static uint16_t haGamePlays[32] = {}; // rough per-game play count (indexed by game id)
static int haGamesOrder[HA_UI_GAME_COUNT]; // display order, filled per sort mode
static int haHistIdx = 0;            // leaderboard/history: 0 = newest loaded session
static HaHost haUiSnap; // draw source; never touched by the async task
static uint32_t haUiDrawnRev = 0xFFFFFFFF;
static uint32_t haUiLastDraw = 0;
static bool haUiForce = true;

#define HA_UI_W 240
#define HA_UI_H 135
#define HA_UI_ROW 10 // px per list row at the 6x8 font

static lgfx::LovyanGFX* haUiG() {
    return haUiSprite ? (lgfx::LovyanGFX*)&haUiCanvas : (lgfx::LovyanGFX*)&M5Cardputer.Display;
}

static void haUiBegin() {
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setBrightness(90);
    // 8bpp keeps the off-screen buffer at ~32KB. 16bpp would be 65KB, which is a
    // lot to hold alongside the WiFi stack and eight WebSocket clients on a board
    // with no PSRAM. If it still can't be had, fall back to drawing direct (which
    // flickers, but works).
    haUiCanvas.setPsram(false);
    haUiCanvas.setColorDepth(8);
    haUiSprite = haUiCanvas.createSprite(HA_UI_W, HA_UI_H) != nullptr;
    haUiG()->setTextFont(1);
    haUiG()->setTextSize(1);
}

// ---- drawing ---------------------------------------------------------------

// The phone client's palette is monochrome + one hot orange (#FF8200). Match it on
// the host screen: orange is the single accent (title bar, selection, leader) on a
// black field with white text. 0xFC00 is #FF8200 in RGB565. AP up/down stays
// green/red -- that's a status, and the web uses the same good/bad colours.
#define HA_ORANGE 0xFC00

// Which firmware is on this board, readable across a table and in a screenshot.
//
// BUMP THIS BY ONE ON EVERY BUILD THAT GETS FLASHED. That is the whole point: with a
// dozen builds in an evening, "it does not work" is unanswerable unless we both know
// which one. A git hash is more precise and useless out loud; a small number you can
// read off the screen and say is worth more here.
#define HA_BUILD_NO 27

// The title the header last drew, so the heap ticker can refresh JUST the header
// strip in place. Redrawing the whole screen every 2s was fine while the offscreen
// sprite absorbed it; in direct-draw mode (sprite dropped for heap) it flickered
// the entire panel every tick.
static char haUiLastTitle[28] = "";

static void haUiHeader(lgfx::LovyanGFX* g, const char* title) {
    strlcpy(haUiLastTitle, title, sizeof(haUiLastTitle));
    g->fillRect(0, 0, HA_UI_W, 12, HA_ORANGE);
    g->setTextColor(TFT_BLACK, HA_ORANGE);
    g->drawString(title, 3, 2);
    { // build number and free heap beside the title: dim, present without competing.
        // The heap is the number that explains "phones cannot connect": below roughly
        // 30k lwIP stops answering DHCP while the AP keeps beaconing.
        char b[20];
        snprintf(b, sizeof(b), "#%d %uk", HA_BUILD_NO, (unsigned)(ESP.getFreeHeap() / 1024));
        g->setTextColor(0x7BEF, HA_ORANGE);
        g->drawString(b, 3 + (int)g->textWidth(title) + 6, 2);
        g->setTextColor(TFT_BLACK, HA_ORANGE);
    }
    char bat[8];
    snprintf(bat, sizeof(bat), "%d%%", (int)M5Cardputer.Power.getBatteryLevel());
    g->drawString(bat, HA_UI_W - 6 * (int)strlen(bat) - 3, 2);
}

// The heap ticker's refresh: repaint ONLY the header strip, and only when the
// shown kilobyte value actually changed. A full forced redraw every 2s was
// visible flicker whenever drawing direct (and pointless work with the sprite).
// Restricted to the dashboard -- the resting view where the number matters;
// other views repaint their header on their own state changes anyway.
static void haUiHeaderRefresh() {
    if(haUiView != HA_VIEW_DASH || !haUiLastTitle[0]) return;
    static unsigned lastK = ~0u;
    unsigned k = (unsigned)(ESP.getFreeHeap() / 1024);
    if(k == lastK) return;
    lastK = k;
    haUiHeader((lgfx::LovyanGFX*)&M5Cardputer.Display, haUiLastTitle);
}

static void haUiFooter(lgfx::LovyanGFX* g, const char* hint) {
    g->fillRect(0, HA_UI_H - 11, HA_UI_W, 11, TFT_DARKGREY);
    g->setTextColor(TFT_WHITE, TFT_DARKGREY);
    g->drawString(hint, 3, HA_UI_H - 9);
}

// Player order for both the dashboard and the leaderboard: score desc, then pid,
// so the board doesn't reshuffle on every tie.
static int haUiSorted(uint8_t* out) {
    int n = 0;
    for(uint8_t pid = 1; pid <= HA_MAX_PLAYERS; pid++)
        if(haUiSnap.p[pid].used) out[n++] = pid;
    for(int i = 1; i < n; i++) {
        uint8_t k = out[i];
        int j = i - 1;
        while(j >= 0 && haUiSnap.p[out[j]].score < haUiSnap.p[k].score) {
            out[j + 1] = out[j];
            j--;
        }
        out[j + 1] = k;
    }
    return n;
}

// Two-column live scoreboard at the small font, so all 10 (the softAP max) fit on
// one screen. Columns fill in rank order down the left, then down the right; each
// cell reads "rank.nick:score".
static void haUiDrawScoreCols(lgfx::LovyanGFX* g, uint8_t* order, int n, int top, int rowsPerCol) {
    g->setTextSize(1);
    const int rowH = 13;
    for(int i = 0; i < n && i < rowsPerCol * 2; i++) {
        int col = i / rowsPerCol, row = i % rowsPerCol;
        int x = col ? HA_UI_W / 2 + 4 : 3;
        int y = top + row * rowH;
        const HaHostPlayer& p = haUiSnap.p[order[i]];
        g->setTextColor(i == 0 ? HA_ORANGE : TFT_WHITE, TFT_BLACK);
        char nk[10], cell[24];
        snprintf(nk, sizeof(nk), "%s", p.nick); // clip nick to ~9 chars per column
        snprintf(cell, sizeof(cell), "%d.%s:%ld", i + 1, nk, (long)p.score);
        g->drawString(cell, x, y);
    }
}

static void haUiDrawDash(lgfx::LovyanGFX* g) {
    haUiHeader(g, "HOTSPOT ARCADE");
    g->setTextFont(1);
    g->setTextSize(1);

    // Line 1: SSID + the join URL, on one line.
    char line[80];
    g->setTextColor(haUiSnap.portalRunning ? TFT_GREEN : TFT_RED, TFT_BLACK);
    snprintf(line, sizeof(line), "%s  http://%s", haHostSsid(), haHostIp().c_str());
    g->drawString(line, 3, 15);

    // Line 2: active game + player count.
    uint8_t order[HA_MAX_PLAYERS + 1];
    int n = haUiSorted(order);
    g->setTextColor(TFT_WHITE, TFT_BLACK);
    snprintf(line, sizeof(line), hu("Game: %s", "Spiel: %s"), haUiGameLabel(haUiSnap.activeGame));
    g->drawString(line, 3, 27);
    char pl[20]; // players pinned to the right edge so the two never crowd
    snprintf(pl, sizeof(pl), hu("Players: %d", "Spieler: %d"), n);
    g->drawString(pl, HA_UI_W - 6 * (int)strlen(pl) - 3, 27);

    g->drawFastHLine(0, 38, HA_UI_W, TFT_DARKGREY);

    if(n == 0) {
        g->setTextColor(TFT_DARKGREY, TFT_BLACK);
        g->drawString(hu("waiting for phones to join...", "warte auf Handys..."), 3, 46);
    } else {
        haUiDrawScoreCols(g, order, n, 44, 5); // 2 columns x 5 = up to 10
        if(n > 10) {
            g->setTextColor(TFT_DARKGREY, TFT_BLACK);
            snprintf(line, sizeof(line), hu("+%d more", "+%d weitere"), n - 10);
            g->drawString(line, 3, HA_UI_H - 22);
        }
    }

    if(haUiSnap.lastEvent[0]) {
        g->setTextFont(1);
        g->setTextSize(1);
        g->setTextColor(HA_ORANGE, TFT_BLACK);
        g->drawString(haUiSnap.lastEvent, 3, HA_UI_H - 22);
    }
    haUiFooter(g, hu("G game   L board   S settings   E end",
                     "G Spiel   L Rang   S Optionen   E Ende"));
}

#define HA_GAMES_ROW 16 // px per game row at text size 2

// Fill haGamesOrder for the current sort mode. "None (lobby)" is always kept last.
static void haUiComputeGamesOrder() {
    int m = 0;
    for(int i = 0; i < HA_UI_GAME_COUNT; i++)
        if(HA_UI_GAMES[i].id != HA_GAME_NONE) haGamesOrder[m++] = i;
    // insertion sort: alphabetical by label, or by play count desc
    for(int a = 1; a < m; a++) {
        int k = haGamesOrder[a], j = a - 1;
        while(j >= 0) {
            bool swap;
            if(haGameSort == 1)
                swap = haGamePlays[HA_UI_GAMES[haGamesOrder[j]].id] <
                       haGamePlays[HA_UI_GAMES[k].id];
            else
                swap = strcmp(hgLabel(HA_UI_GAMES[haGamesOrder[j]]), hgLabel(HA_UI_GAMES[k])) > 0;
            if(!swap) break;
            haGamesOrder[j + 1] = haGamesOrder[j];
            j--;
        }
        haGamesOrder[j + 1] = k;
    }
    for(int i = 0; i < HA_UI_GAME_COUNT; i++) // append None last
        if(HA_UI_GAMES[i].id == HA_GAME_NONE) haGamesOrder[m++] = i;
}

static void haUiDrawGames(lgfx::LovyanGFX* g) {
    haUiComputeGamesOrder();
    char title[32];
    snprintf(title, sizeof(title), hu("GAMES - %s", "SPIELE - %s"),
             haGameSort == 1 ? hu("MOST PLAYED", "MEISTGESPIELT") : "A-Z");
    haUiHeader(g, title);

    int descY = HA_UI_H - 22;
    int rows = (descY - 14) / HA_GAMES_ROW;
    if(rows < 1) rows = 1;
    if(haUiCursor < haUiScroll) haUiScroll = haUiCursor;
    if(haUiCursor >= haUiScroll + rows) haUiScroll = haUiCursor - rows + 1;

    g->setTextSize(2);
    int y = 15;
    for(int i = haUiScroll; i < HA_UI_GAME_COUNT && i < haUiScroll + rows; i++) {
        const HaGameItem& it = HA_UI_GAMES[haGamesOrder[i]];
        bool sel = (i == haUiCursor);
        bool live = (it.id == haUiSnap.activeGame);
        if(sel) g->fillRect(0, y - 1, HA_UI_W, HA_GAMES_ROW, HA_ORANGE);
        g->setTextColor(sel ? TFT_BLACK : (live ? HA_ORANGE : TFT_WHITE), sel ? HA_ORANGE : TFT_BLACK);
        char nm[24];
        snprintf(nm, sizeof(nm), "%s%s", live ? "*" : "", hgLabel(it));
        g->drawString(nm, 3, y);
        if(it.duel) {
            g->setTextSize(1);
            g->setTextColor(sel ? TFT_BLACK : HA_ORANGE, sel ? HA_ORANGE : TFT_BLACK);
            g->drawString("1v1", HA_UI_W - 22, y + 4);
            g->setTextSize(2);
        }
        y += HA_GAMES_ROW;
    }
    g->setTextSize(1);

    // Selected game's one-line description.
    g->fillRect(0, descY - 2, HA_UI_W, 12, TFT_BLACK);
    g->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    g->drawString(hgDesc(HA_UI_GAMES[haGamesOrder[haUiCursor]]), 3, descY);

    haUiFooter(g, hu(";/. move  S sort  ENTER pick  ESC back",
                     ";/. Wahl  S Sort  ENTER Start  ESC zurueck"));
}

// The Leaderboard always shows the current session's live standings (they're
// auto-saved to the SD card when this screen is opened, so they survive a restart).
// R clears the scores to start a new session.
static void haUiDrawBoard(lgfx::LovyanGFX* g) {
    haUiHeader(g, hu("LEADERBOARD", "RANGLISTE"));
    uint8_t order[HA_MAX_PLAYERS + 1];
    int n = haUiSorted(order);
    if(n == 0) {
        g->setTextColor(TFT_DARKGREY, TFT_BLACK);
        g->drawString(hu("no players yet", "noch keine Spieler"), 3, 18);
    } else {
        haUiDrawScoreCols(g, order, n, 16, 5); // same 2 columns x 5 as the dashboard
    }
    haUiFooter(g, haSdOk ? hu("R reset scores   ESC back", "R zuruecksetzen   ESC zurueck")
                         : hu("no SD   R reset   ESC back", "keine SD   R reset   ESC zurueck"));
}

static const char* haUiAudioName() {
    return haAudioLevel == 0 ? "off" : haAudioLevel == 1 ? "low" : "high";
}

// One option of a multi-choice setting (audio off/low/high, AP on/off). The current
// choice is filled -- orange on the selected (editable) row, grey otherwise; the rest
// are outlined. Returns the x just past the pill, so options tile left to right.
static int haUiOptPill(lgfx::LovyanGFX* g, int x, int y, const char* txt, bool current, bool rowSel) {
    int w = (int)g->textWidth(txt) + 8;
    if(current) {
        uint16_t bg = rowSel ? HA_ORANGE : TFT_DARKGREY;
        g->fillRoundRect(x, y, w, 13, 2, bg);
        g->setTextColor(rowSel ? TFT_BLACK : TFT_WHITE, bg);
    } else {
        g->drawRoundRect(x, y, w, 13, 2, TFT_DARKGREY);
        g->setTextColor(TFT_DARKGREY, TFT_BLACK);
    }
    g->drawString(txt, x + 4, y + 3);
    return x + w + 4;
}

// A single value pill (language, SSID, event-log). Orange fill on the selected row.
// `arrows` frames it with < > (a cycle-able value like language) when it's selected.
static void haUiValPill(lgfx::LovyanGFX* g, int x, int y, const char* txt, bool rowSel, bool arrows) {
    int tx = x;
    if(arrows && rowSel) {
        g->setTextColor(HA_ORANGE, TFT_BLACK);
        g->drawString("<", x, y + 3);
        tx = x + 10;
    }
    int w = (int)g->textWidth(txt) + 8;
    if(rowSel) {
        g->fillRoundRect(tx, y, w, 13, 2, HA_ORANGE);
        g->setTextColor(TFT_BLACK, HA_ORANGE);
    } else {
        g->drawRoundRect(tx, y, w, 13, 2, TFT_DARKGREY);
        g->setTextColor(TFT_WHITE, TFT_BLACK);
    }
    g->drawString(txt, tx + 4, y + 3);
    if(arrows && rowSel) {
        g->setTextColor(HA_ORANGE, TFT_BLACK);
        g->drawString(">", tx + w + 3, y + 3);
    }
}

static void haUiDrawSettings(lgfx::LovyanGFX* g) {
    haUiHeader(g, hu("SETTINGS", "OPTIONEN"));
    g->setTextSize(1);
    const int VALX = 92, y0 = 20, rowH = 20;
    const char* labels[HA_SET_COUNT] = {
        "SSID", "Audio", hu("Language", "Sprache"),
        hu("Access Point", "Netzwerk"), hu("Event log", "Ereignisse")};
    for(int i = 0; i < HA_SET_COUNT; i++) {
        bool sel = (i == haUiCursor);
        int y = y0 + i * rowH;
        g->setTextColor(sel ? HA_ORANGE : TFT_WHITE, TFT_BLACK);
        g->drawString(labels[i], 4, y + 3);
        int cx = VALX;
        switch(i) {
        case 0: { // SSID -- value shown; ENTER opens the text editor
            // This row ignores VALX and starts right after its own label. Every other row
            // holds option pills that read better in one aligned column, but the SSID is a
            // single free-text value up to 32 bytes, and column alignment was costing it two
            // thirds of the screen: the old fixed "%.12s" clipped the default name to
            // "Hotspot Arc". From x=34 to the right edge, minus the pill's 8px padding and a
            // 6px margin, at 6px per glyph, the whole 32-character name fits.
            const int SSIDX = 34;
            char s[34];
            int room = (HA_UI_W - SSIDX - 8 - 6) / 6;
            if(room > (int)sizeof(s) - 1) room = (int)sizeof(s) - 1;
            snprintf(s, sizeof(s), "%.*s", room, haHostSsid());
            haUiValPill(g, SSIDX, y, s, sel, false);
            break;
        }
        case 1: { // Audio -- off / low / high
            const char* o[3] = {hu("off", "aus"), hu("low", "leise"), hu("high", "laut")};
            for(int a = 0; a < 3; a++) cx = haUiOptPill(g, cx, y, o[a], haAudioLevel == a, sel);
            break;
        }
        case 2: // Language -- one box, cycles with < >
            haUiValPill(g, cx, y, HA_LANG_NAME[haLang % HA_LANG_COUNT], sel, true);
            break;
        case 3: { // Access Point -- on / off
            bool up = haUiSnap.portalRunning;
            cx = haUiOptPill(g, cx, y, hu("on", "an"), up, sel);
            cx = haUiOptPill(g, cx, y, hu("off", "aus"), !up, sel);
            break;
        }
        case 4: // Event log -- opens a sub-screen
            haUiValPill(g, cx, y, hu("GO >", "LOS >"), sel, false);
            break;
        }
    }
    haUiFooter(g, hu(";/. move   ,// change   ENTER open   ESC back",
                     ";/. Wahl   ,// aendern   ENTER OK   ESC zurueck"));
}

static void haUiDrawConsole(lgfx::LovyanGFX* g) {
    haUiHeader(g, hu("EVENT LOG", "EREIGNISSE"));
    int rows = 11;
    uint32_t total = haUiSnap.evTotal;
    uint32_t have = total < HA_EV_MAX ? total : HA_EV_MAX;
    int y = 14;
    g->setTextColor(TFT_WHITE, TFT_BLACK);
    for(uint32_t i = 0; i < have && i < (uint32_t)rows; i++) {
        // newest first
        uint32_t idx = (total - 1 - i) % HA_EV_MAX;
        g->drawString(haUiSnap.ev[idx], 3, y);
        y += HA_UI_ROW;
    }
    if(have == 0) {
        g->setTextColor(TFT_DARKGREY, TFT_BLACK);
        g->drawString(hu("nothing yet", "noch nichts"), 3, 16);
    }
    haUiFooter(g, hu("ESC back", "ESC zurueck"));
}

static void haUiDrawSsid(lgfx::LovyanGFX* g) {
    haUiHeader(g, hu("AP NAME", "AP-NAME"));
    g->setTextColor(TFT_WHITE, TFT_BLACK);
    g->drawString(hu("Type a new SSID:", "Neue SSID eingeben:"), 3, 20);
    g->fillRect(3, 34, HA_UI_W - 6, 14, TFT_DARKGREY);
    g->setTextColor(TFT_WHITE, TFT_DARKGREY);
    char shown[40];
    snprintf(shown, sizeof(shown), "%s_", haUiEdit);
    g->drawString(shown, 6, 37);
    g->setTextColor(TFT_DARKGREY, TFT_BLACK);
    g->drawString(hu("Applying restarts the access point,",
                     "Uebernehmen startet den AP neu,"), 3, 58);
    g->drawString(hu("which drops every connected phone.",
                     "alle verbundenen Handys fliegen raus."), 3, 68);
    haUiFooter(g, hu("ENTER apply   DEL erase   ESC cancel",
                     "ENTER OK   DEL loeschen   ESC abbrechen"));
}

static void haUiDraw() {
    haHostSnapshot(haUiSnap);
    lgfx::LovyanGFX* g = haUiG();
    if(haUiSprite) haUiCanvas.fillSprite(TFT_BLACK);
    else g->fillScreen(TFT_BLACK);
    g->setTextFont(1);
    g->setTextSize(1);
    switch(haUiView) {
    case HA_VIEW_GAMES:
        haUiDrawGames(g);
        break;
    case HA_VIEW_BOARD:
        haUiDrawBoard(g);
        break;
    case HA_VIEW_CONSOLE:
        haUiDrawConsole(g);
        break;
    case HA_VIEW_SSID:
        haUiDrawSsid(g);
        break;
    case HA_VIEW_SETTINGS:
        haUiDrawSettings(g);
        break;
    default:
        haUiDrawDash(g);
        break;
    }
    if(haUiSprite) haUiCanvas.pushSprite(0, 0);
    haUiDrawnRev = haUiSnap.rev;
    haUiLastDraw = millis();
    haUiForce = false;
}

// ---- input -----------------------------------------------------------------

static void haUiOpen(HaUiView v) {
    haUiView = v;
    haUiCursor = 0;
    haUiScroll = 0;
    if(v == HA_VIEW_GAMES) {
        haUiComputeGamesOrder(); // cursor is a position in the sorted display order
        for(int i = 0; i < HA_UI_GAME_COUNT; i++)
            if(HA_UI_GAMES[haGamesOrder[i]].id == haUiSnap.activeGame) haUiCursor = i;
    } else if(v == HA_VIEW_BOARD) {
        haHistSaveCurrent(); // persist the current standings whenever the board is opened
    }
    haUiForce = true;
}

// ESC/back: SSID and the event log are reached from Settings, so they return there;
// everything else returns to the dashboard.
static void haUiBack() {
    if(haUiView == HA_VIEW_SSID || haUiView == HA_VIEW_CONSOLE)
        haUiOpen(HA_VIEW_SETTINGS);
    else
        haUiOpen(HA_VIEW_DASH);
}

// Change the value of the selected settings row in place (the ,/ left-right keys).
// SSID and Event log aren't values -- they open a screen on ENTER, so adjust skips them.
static void haUiSettingAdjust(int dir) {
    switch(haUiCursor) {
    case 1: // Audio off/low/high
        haAudioLevel = (uint8_t)((haAudioLevel + 3 + dir) % 3);
        haCfgSave();
        break;
    case 2: // Language
        haLang = (uint8_t)((haLang + HA_LANG_COUNT + dir) % HA_LANG_COUNT);
        haLangDirty = true;
        {
            Preferences p; // NVS fallback for a board with no SD card
            if(p.begin("ha_cfg", false)) { p.putUChar("lang", haLang); p.end(); }
        }
        haCfgSave();
        break;
    case 3: // Access Point on/off
        haHostTogglePortal();
        break;
    default:
        return;
    }
    haUiForce = true;
}

static void haUiChar(char c) {
    if(haUiView == HA_VIEW_SSID) {
        if(c == '`') { // esc -> back to settings
            haUiBack();
            return;
        }
        size_t n = strlen(haUiEdit);
        if(c >= 0x20 && c < 0x7F && n < sizeof(haUiEdit) - 1) {
            haUiEdit[n] = c;
            haUiEdit[n + 1] = '\0';
            haUiForce = true;
        }
        return;
    }

    switch(c) {
    case '`': // esc
        haUiBack();
        return;
    case ';': // up
        if(haUiView == HA_VIEW_GAMES && haUiCursor > 0) haUiCursor--;
        else if(haUiView == HA_VIEW_SETTINGS && haUiCursor > 0) haUiCursor--;
        haUiForce = true;
        return;
    case '.': // down
        if(haUiView == HA_VIEW_GAMES && haUiCursor < HA_UI_GAME_COUNT - 1) haUiCursor++;
        else if(haUiView == HA_VIEW_SETTINGS && haUiCursor < HA_SET_COUNT - 1) haUiCursor++;
        haUiForce = true;
        return;
    case ',': // left: page up in the picker, or decrement a setting value
        if(haUiView == HA_VIEW_GAMES) haUiCursor = haUiCursor > 6 ? haUiCursor - 6 : 0;
        else if(haUiView == HA_VIEW_SETTINGS) haUiSettingAdjust(-1);
        haUiForce = true;
        return;
    case '/': // right: page down in the picker, or increment a setting value
        if(haUiView == HA_VIEW_GAMES)
            haUiCursor = haUiCursor + 6 < HA_UI_GAME_COUNT ? haUiCursor + 6 : HA_UI_GAME_COUNT - 1;
        else if(haUiView == HA_VIEW_SETTINGS) haUiSettingAdjust(1);
        haUiForce = true;
        return;
    case 'g':
    case 'G':
        haUiOpen(HA_VIEW_GAMES);
        return;
    case 'l':
    case 'L':
        haUiOpen(HA_VIEW_BOARD);
        return;
    case 's':
    case 'S':
        if(haUiView == HA_VIEW_GAMES) { // in the picker, S toggles the sort order
            haGameSort ^= 1;
            haUiCursor = 0;
            haUiScroll = 0;
        } else {
            haUiOpen(HA_VIEW_SETTINGS);
        }
        haUiForce = true;
        return;
    case 'r':
    case 'R':
        if(haUiView == HA_VIEW_BOARD) haHostResetScores(); // reset lives on the board
        haUiForce = true;
        return;
    case 'e':
    case 'E':
        haHostRoundEnd();
        haUiForce = true;
        return;
    default:
        return;
    }
}

static void haUiEnter() {
    if(haUiView == HA_VIEW_GAMES) {
        const HaGameItem& it = HA_UI_GAMES[haGamesOrder[haUiCursor]];
        if(it.id != HA_GAME_NONE) haGamePlays[it.id]++; // rough play tally for "most played"
        haHostSelectGame(it.id);
        haUiOpen(HA_VIEW_DASH);
    } else if(haUiView == HA_VIEW_SSID) {
        if(haUiEdit[0]) haHostApplySsid(haUiEdit);
        haUiOpen(HA_VIEW_SETTINGS); // back to settings, where SSID lives
    } else if(haUiView == HA_VIEW_SETTINGS) {
        switch(haUiCursor) {
        case 0: // SSID -> open the editor
            strlcpy(haUiEdit, haHostSsid(), sizeof(haUiEdit));
            haUiView = HA_VIEW_SSID;
            break;
        case 1: // Audio -> cycle off/low/high
            haAudioLevel = (uint8_t)((haAudioLevel + 1) % 3);
            haCfgSave();
            break;
        case 2: // Language -> cycle, persist, and ask the .ino to re-stream packs
            haLang = (uint8_t)((haLang + 1) % HA_LANG_COUNT);
            haLangDirty = true;
            {
                Preferences p; // NVS fallback for a board with no SD card
                if(p.begin("ha_cfg", false)) { p.putUChar("lang", haLang); p.end(); }
            }
            haCfgSave();
            break;
        case 3: // AP -> toggle
            haHostTogglePortal();
            break;
        case 4: // Event log
            haUiView = HA_VIEW_CONSOLE;
            break;
        }
    }
    haUiForce = true;
}

static void haUiDel() {
    if(haUiView == HA_VIEW_SSID) {
        size_t n = strlen(haUiEdit);
        if(n) haUiEdit[n - 1] = '\0';
    } else {
        haUiBack();
    }
    haUiForce = true;
}

static void haUiPumpKeys() {
    if(!M5Cardputer.Keyboard.isChange()) return;
    if(!M5Cardputer.Keyboard.isPressed()) return;
    auto st = M5Cardputer.Keyboard.keysState();
    for(auto c : st.word) haUiChar(c);
    if(st.del) haUiDel();
    if(st.enter) haUiEnter();
}

// Redraw when the mirror moved or a key changed the view, rate-limited so a busy
// game (pong ticks at 30Hz) can't spend all its time pushing pixels. The 1Hz
// floor keeps the battery percentage honest.
static void haUiTick() {
    uint32_t now = millis();
    bool changed = haUiForce || haHost.rev != haUiDrawnRev;
    if(changed && now - haUiLastDraw < 100) return;
    if(!changed && now - haUiLastDraw < 1000) return;
    haUiDraw();
}
