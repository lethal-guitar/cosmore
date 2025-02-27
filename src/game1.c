/**
 * Cosmore
 * Copyright (c) 2020-2022 Scott Smitelli
 *
 * Based on COSMO{1..3}.EXE distributed with "Cosmo's Cosmic Adventure"
 * Copyright (c) 1992 Apogee Software, Ltd.
 *
 * This source code is licensed under the MIT license found in the LICENSE file
 * in the root directory of this source tree.
 */

/*****************************************************************************
 *                           COSMORE "GAME1" UNIT                            *
 *                                                                           *
 * This file contains the bulk of the game code. Notable elements that are   *
 * *not* present here include the AdLib service, UI utility functions,       *
 * joystick input, status bar, config/group file functions, and the majority *
 * of the in-game text screens. All of these are in GAME2.                   *
 *                                                                           *
 * This file pushes the TCC compiler to the limits of available memory.      *
 *****************************************************************************/

#include "glue.h"

/*
Overarching game control variables.
*/
bbool isInGame = false;
bool winGame;
dword gameScore, gameStars;
static bbool isNewGame;
static bool winLevel;

/*
Memory content indicators. Used to determine if the values currently held in
different storage areas is acceptable for re-use, or if they need to be reloaded
from a group entry.
*/
static bbool isCartoonDataLoaded = false;
word miscDataContents = IMAGE_NONE;

/*
Player position and game interaction variables. Most of these are under direct
or almost-direct control of the player.
*/
word playerHealth, playerMaxHealth, playerBombs;
static word playerX, playerY, scrollX, scrollY;
static word playerFaceDir, playerBombDir;
static word playerBaseFrame = PLAYER_BASE_WEST;
static word playerFrame = PLAYER_WALK_1;
static word playerForceFrame;
static byte playerClingDir;
static bool canPlayerCling, isPlayerNearHintGlobe, isPlayerNearTransporter;

/*
Player one-shot variables. Each of these controls the "happens only once"
behavior of something in the game. Some are reset when a new level is started,
others persist for the whole game (and into save files).
*/
static bbool sawAutoHintGlobe;
static bool sawJumpPadBubble, sawMonumentBubble, sawScooterBubble;
static bool sawTransporterBubble, sawPipeBubble, sawBossBubble;
static bool sawPusherRobotBubble, sawBearTrapBubble, sawMysteryWallBubble;
static bool sawTulipLauncherBubble, sawHamburgerBubble, sawHurtBubble = false;
static bool usedCheatCode, sawBombHint, sawHealthHint;
static word pounceHintState;

/*
Debug mode and demo record/playback variables.
*/
byte demoState;
static word demoDataLength, demoDataPos;
static bbool isDebugMode = false;

/*
X any Y move component tables for DIR8_* directions.
*/
static int dir8X[] = {0,  0,  1,  1,  1,  0, -1, -1, -1};
static int dir8Y[] = {0, -1, -1,  0,  1,  1,  1,  0, -1};

/*
Free-running counters. These generally keep counting no matter what's going on
elsewhere in the game.
*/
word activePage = 0;
word gameTickCount;
static word randStepCount;
static dword paletteStepCount;

/*
Player pain and death variables.
*/
bool isGodMode = false;
static bbool playerIsInvincible;
static word playerHurtCooldown, playerDeadTime;
static byte playerFallDeadTime = 0;

/*
String storage.
*/
char *stnGroupFilename = FILENAME_BASE ".STN";
char *volGroupFilename = FILENAME_BASE ".VOL";
static char *fullscreenImageNames[] = {
    "PRETITLE.MNI", TITLE_SCREEN, "CREDIT.MNI", "BONUS.MNI", END_SCREEN,
    "ONEMOMNT.MNI"
};
static char *backdropNames[] = {
    "bdblank.mni", "bdpipe.MNI", "bdredsky.MNI", "bdrocktk.MNI", "bdjungle.MNI",
    "bdstar.MNI", "bdwierd.mni", "bdcave.mni", "bdice.mni", "bdshrum.mni",
    "bdtechms.mni", "bdnewsky.mni", "bdstar2.mni", "bdstar3.mni",
    "bdforest.mni", "bdmountn.mni", "bdguts.mni", "bdbrktec.mni",
    "bdclouds.mni", "bdfutcty.mni", "bdice2.mni", "bdcliff.mni", "bdspooky.mni",
    "bdcrystl.mni", "bdcircut.mni", "bdcircpc.mni"
};
static char *mapNames[] = MAP_NAMES;
char *musicNames[] = {
    "mcaves.mni", "mscarry.mni", "mboss.mni", "mrunaway.mni", "mcircus.mni",
    "mtekwrd.mni", "measylev.mni", "mrockit.mni", "mhappy.mni", "mdevo.mni",
    "mdadoda.mni", "mbells.mni", "mdrums.mni", "mbanjo.mni", "measy2.mni",
    "mteck2.mni", "mteck3.mni", "mteck4.mni", "mzztop.mni"
};
static char *starBonusRanks[] = {
    "    Not Bad!    ", "    Way Cool    ", "     Groovy     ",
    "    Radical!    ", "     Insane     ", "     Gnarly     ",
    "   Outrageous   ", "   Incredible   ", "    Awesome!    ",
    "   Brilliant!   ", "    Profound    ", "    Towering    ",
    "Rocket Scientist"
};

/*
Player speed/movement variables. These control vertical movement in the form of
jumping, pouncing, and falling. Horizontal movement takes the form of east/
west sliding for slippery surfaces. There are also variables for involuntary
pushes and "dizzy" immobilization.
*/
static word playerMomentumNorth = 0, playerMomentumSaved;
static bool isPlayerLongJumping, isPlayerRecoiling = false;
static bool isPlayerSlidingEast, isPlayerSlidingWest;
static bbool isPlayerFalling;
static int playerFallTime;
#define playerJumpTime soundPriority[0]  /* HACK: `static byte playerJumpTime` would be sane */
static word playerPushDir, playerPushMaxTime, playerPushTime, playerPushSpeed;
static bbool canCancelPlayerPush;
static bool isPlayerPushed, stopPlayerPushAtWall;
static bool queuePlayerDizzy;
static word playerDizzyLeft;

/* ====================== STATIC DATA DEMARCATION LINE ====================== */

/*
System variables.
*/
dword totalMemFreeBefore, totalMemFreeAfter;
static InterruptFunction savedInt9;
static char *writePath;

/*
BSS storage areas. All of these are big ol' fixed-sized arrays.
*/
HighScoreName highScoreNames[11];
dword highScoreValues[11];
static byte soundPriority[80 + 1];
static Platform platforms[MAX_PLATFORMS];
static Fountain fountains[MAX_FOUNTAINS];
static Light lights[MAX_LIGHTS];
static Actor actors[MAX_ACTORS];
static Shard shards[MAX_SHARDS];
static word explosions[MAX_EXPLOSIONS][sizeof(Explosion) / sizeof(word)];  /* this one's weird */
static Spawner spawners[MAX_SPAWNERS];
static Decoration decorations[MAX_DECORATIONS];
/* Holds each decoration's currently displayed frame. Why this isn't in the Decoration struct, who knows. */
static word decorationFrame[MAX_DECORATIONS];
static word backdropTable[2880];
static char joinPathBuffer[80];

/*
Heap storage areas. Space for all of these is allocated on startup.
*/
byte *fontTileData, *maskedTileData, *miscData;
static byte *actorTileData[3], *playerTileData, *tileAttributeData;
static word *actorInfoData, *playerInfoData, *cartoonInfoData;
static word *soundData1, *soundData2, *soundData3, *soundDataPtr[80];
static union {byte *b; word *w;} mapData;

/*
Pass-by-global variables. If you see one of these in use, some earlier function
wants to influence the behavior of a subsequently called function.
*/
dword lastGroupEntryLength;
static word nextActorIndex, nextDrawMode;

/*
Keyboard and joystick variables. Also includes player immobilization and jump
lockout flags.
*/
byte lastScancode;
bbool isKeyDown[BYTE_MAX];
bool isJoystickReady;
bbool cmdWest, cmdEast, cmdNorth, cmdSouth, cmdJump, cmdBomb;
static bbool blockMovementCmds, cmdJumpLatch;
static bool blockActionCmds;

/*
Customizable options. These are saved and persist across restarts.
*/
bool isMusicEnabled, isSoundEnabled;
byte scancodeWest, scancodeEast, scancodeNorth, scancodeSouth, scancodeJump, scancodeBomb;

/*
PC speaker sound effect and AdLib music variables.
*/
Music *activeMusic;
static word activeSoundIndex, activeSoundPriority;
static bool isNewSound, enableSpeaker;

/*
Level/map control and global world flags.
*/
static word levelNum, mapFlags, musicNum;
static word mapWidth, mapHeight, mapYPower;  /* y power = map width expressed as 2^n. */
static bool hasLightSwitch, hasRain, hasHScrollBackdrop, hasVScrollBackdrop;
static bool areForceFieldsActive, areLightsActive, arePlatformsActive;
static byte paletteAnimationNum;

/*
Actor (and similar) counts, as well as a few odds and ends for actors that
interact heavily with the outside world.
*/
word numActors;
static word numPlatforms, numFountains, numLights;
static word numBarrels, numEyePlants, pounceStreak;
static word mysteryWallTime;
static word activeTransporter, transporterTimeLeft;
static word scooterMounted;  /* Acts like bool, except at moment of mount (decrements 4->1) */
static bool isPounceReady;
static bbool isPlayerInPipe;

/*
Inline functions.
*/
#define MAP_CELL_ADDR(x, y)   (mapData.w + ((y) << mapYPower) + x)
#define SET_PLAYER_DIZZY()    { queuePlayerDizzy = true; }
#define TILE_BLOCK_SOUTH(val) (*(tileAttributeData + ((val) / 8)) & 0x01)
#define TILE_BLOCK_NORTH(val) (*(tileAttributeData + ((val) / 8)) & 0x02)
#define TILE_BLOCK_WEST(val)  (*(tileAttributeData + ((val) / 8)) & 0x04)
#define TILE_BLOCK_EAST(val)  (*(tileAttributeData + ((val) / 8)) & 0x08)
#define TILE_SLIPPERY(val)    (*(tileAttributeData + ((val) / 8)) & 0x10)
#define TILE_IN_FRONT(val)    (*(tileAttributeData + ((val) / 8)) & 0x20)
#define TILE_SLOPED(val)      (*(tileAttributeData + ((val) / 8)) & 0x40)
#define TILE_CAN_CLING(val)   (*(tileAttributeData + ((val) / 8)) & 0x80)

/*
Prototypes for "private" functions where strictly required.
*/
void DrawSprite(word, word, word, word, word);
void DrawPlayer(byte, word, word, word);
void DrawCartoon(byte, word, word);
word GetMapTile(word, word);
void SetMapTile(word, word, word);
void NewActor(word, word, word);
void NewShard(word, word, word, word);
void NewExplosion(word, word);
bool IsNearExplosion(word, word, word, word);
void NewSpawner(word, word, word);
void NewDecoration(word, word, word, word, word, word);
void HurtPlayer(void);
void NewPounceDecoration(word, word);
void DestroyBarrel(word);
void ClearPlayerPush(void);
void SetPlayerPush(word, word, word, word, bool, bool);
char *JoinPath(char *, char *);
bbool LoadGameState(char);
byte ProcessGameInput(byte);
void SwitchLevel(word);
void InitializeGame(void);

/*
Get the file size of the named group entry, in bytes.
*/
dword GroupEntryLength(char *entry_name)
{
    fclose(GroupEntryFp(entry_name));

    return lastGroupEntryLength;
}

/*
Reset all variables for the "player dizzy/shaking head" immobilization.
*/
void ClearPlayerDizzy(void)
{
    queuePlayerDizzy = false;
    playerDizzyLeft = 0;
}

/*
Random number generator for world events.

Unlike rand() or the random() macro, this function returns deterministic results
for consistent demo recording/playback.

The upper bound for return value is on the order of 4,000.
*/
word GameRand(void)
{
    static word randtable[] = {
        31,  12,  17,  233, 99,  8,   64,  12,  199, 49,  5,   6,
        143, 1,   35,  46,  52,  5,   8,   21,  44,  8,   3,   77,
        2,   103, 34,  23,  78,  2,   67,  2,   79,  46,  1,   98
    };

    randStepCount++;
    if (randStepCount > 35) randStepCount = 0;

    return randtable[randStepCount] + scrollX + scrollY + randStepCount + playerX + playerY;
}

/*
Read the next color from the palette animation array and load it in.
*/
void StepPalette(byte *pal_table)
{
    paletteStepCount++;
    if (pal_table[(word)paletteStepCount] == END_ANIMATION) paletteStepCount = 0;

    /* Jump by 8 converts COLORS into MODE1_COLORS */
    SetPaletteRegister(
        PALETTE_KEY_INDEX,
        pal_table[(word)paletteStepCount] < 8 ?
            pal_table[(word)paletteStepCount] : pal_table[(word)paletteStepCount] + 8
    );
}

/*
Handle palette animation for this frame.
*/
void AnimatePalette(void)
{
    static byte lightningState = 0;

#ifdef EXPLOSION_PALETTE
    if (paletteAnimationNum == PALANIM_EXPLOSIONS) return;
#endif  /* EXPLOSION_PALETTE */

    switch (paletteAnimationNum) {
    case PALANIM_LIGHTNING:
        if (lightningState == 2) {
            lightningState = 0;
            SetPaletteRegister(PALETTE_KEY_INDEX, MODE1_DARKGRAY);
        } else if (lightningState == 1) {
            lightningState = 2;
            SetPaletteRegister(PALETTE_KEY_INDEX, MODE1_LIGHTGRAY);
        } else if (rand() < 1500) {
            SetPaletteRegister(PALETTE_KEY_INDEX, MODE1_WHITE);
            StartSound(SND_THUNDER);
            lightningState = 1;
        } else {
            SetPaletteRegister(PALETTE_KEY_INDEX, MODE1_BLACK);
            lightningState = 0;
        }
        break;

    /*
    Palette tables passed to StepPalette() must use COLORS, *not* MODE1_COLORS!
    */
    case PALANIM_R_Y_W:  /* red-yellow-white */
        {
            static byte rywTable[] = {
                RED, RED, LIGHTRED, LIGHTRED, YELLOW, YELLOW, WHITE, WHITE,
                YELLOW, YELLOW, LIGHTRED, LIGHTRED, END_ANIMATION
            };

            StepPalette(rywTable);
        }
        break;

    case PALANIM_R_G_B:  /* red-green-blue */
        {
            static byte rgbTable[] = {
                BLACK, BLACK, RED, RED, LIGHTRED, RED, RED,
                BLACK, BLACK, GREEN, GREEN, LIGHTGREEN, GREEN, GREEN,
                BLACK, BLACK, BLUE, BLUE, LIGHTBLUE, BLUE, BLUE, END_ANIMATION
            };

            StepPalette(rgbTable);
        }
        break;

    case PALANIM_MONO:  /* monochrome */
        {
            static byte monoTable[] = {
                BLACK, BLACK, DARKGRAY, LIGHTGRAY, WHITE, LIGHTGRAY, DARKGRAY,
                END_ANIMATION
            };

            StepPalette(monoTable);
        }
        break;

    case PALANIM_W_R_M:  /* white-red-magenta */
        {
            static byte wrmTable[] = {
                WHITE, WHITE, WHITE, WHITE, WHITE, WHITE, RED, LIGHTMAGENTA,
                END_ANIMATION
            };

            StepPalette(wrmTable);
        }
        break;
    }
}

/*
Draw a single line of text with the first character at the specified X/Y origin.

A primitive form of markup is supported (all values are zero-padded decimal):
- \xFBnnn: Draw cartoon image nnn at the current position.
- \xFCnnn: Wait nnn times WaitHard(3) before drawing each character and play a
  per-character typewriter sound effect.
- \xFDnnn: Draw player sprite nnn at the current position.
- \xFEnnniii: Draw sprite type nnn, frame iii at the current position.

The draw position is not adjusted when cartoons/sprites are inserted -- the
caller must follow each of these sequences with an appropriate number of space
characters to clear the graphic before writing additional text.

NOTE: C's parser treats strings like "\xFC003" as hex literals with more than
two digits, resulting in a compile-time error. In calling code, you'll see text
broken up like "\xFC""003". This is intentional, and the only reasonable
workaround. (Unless we go octal...)
*/
void DrawTextLine(word x_origin, word y_origin, char *text)
{
    register int x = 0;
    register word delay = 0;
    word delayleft = 0;

    EGA_MODE_DEFAULT();

    while (text[x] != '\0') {
        if (text[x] == '\xFE' || text[x] == '\xFB' || text[x] == '\xFD' || text[x] == '\xFC') {
            char lookahead[4];
            word sequence1, sequence2;

            lookahead[0] = text[x + 1];
            lookahead[1] = text[x + 2];
            lookahead[2] = text[x + 3];
            lookahead[3] = '\0';
            sequence1 = atoi(lookahead);

            /*
            Be careful in here! The base address of the `text` array changes:

                (assume `text` contains the string "demonstrate")
                text[0] == 'd';  (true)
                text += 4;
                text[0] == 'n';  (also true!)
            */
            if (text[x] == '\xFD') {  /* draw player sprite */
                DrawPlayer(sequence1, x_origin + x, y_origin, DRAWMODE_ABSOLUTE);
                text += 4;

            } else if (text[x] == '\xFB') {  /* draw cartoon image */
                DrawCartoon(sequence1, x_origin + x, y_origin);
                text += 4;

            } else if (text[x] == '\xFC') {  /* inter-character wait + sound */
                text += 4;
                /* atoi() here wastefully recalculates the value in `sequence1` */
                delayleft = delay = atoi(lookahead);

            } else {  /* \xFE; draw actor sprite */
                lookahead[0] = text[x + 4];
                lookahead[1] = text[x + 5];
                lookahead[2] = text[x + 6];
                lookahead[3] = '\0';
                sequence2 = atoi(lookahead);

                DrawSprite(sequence1, sequence2, x_origin + x, y_origin, DRAWMODE_ABSOLUTE);
                text += 7;
            }

            continue;
        }

        if (delay != 0 && lastScancode == SCANCODE_SPACE) {
            WaitHard(1);
        } else if (delayleft != 0) {
            WaitHard(3);

            delayleft--;
            if (delayleft != 0) continue;
            delayleft = delay;

            if (text[x] != ' ') {
                StartSound(SND_TEXT_TYPEWRITER);
            }
        }

        if (text[x] >= 'a') {  /* lowercase */
            DrawSpriteTile(
                /* `FONT_LOWER_A` is equal to ASCII 'a' */
                fontTileData + FONT_LOWER_A + ((text[x] - 'a') * 40), x_origin + x, y_origin
            );
        } else {  /* uppercase, digits, and symbols */
            DrawSpriteTile(
                /* `FONT_UP_ARROW` is equal to ASCII (CP437) "upwards arrow" */
                fontTileData + FONT_UP_ARROW + ((text[x] - '\x18') * 40), x_origin + x, y_origin
            );
        }

        x++;
    }
}

/*
Load font data into system memory.

The font data on disk has an inverted transparency mask relative to what the
rest of the game expects, so negate those bits while loading.
*/
void LoadFontTileData(char *entry_name, byte *dest, word length)
{
    int i;
    FILE *fp = GroupEntryFp(entry_name);

    fread(dest, length, 1, fp);
    fclose(fp);

    /* Ideally should be `length`, not a literal 4000. */
    for (i = 0; i < 4000; i += 5) {
        *(dest + i) = ~*(dest + i);
    }
}

/*
Replace the entire screen contents with a full-size (320x200) image.
*/
void DrawFullscreenImage(word image_num)
{
    byte *destbase = MK_FP(0xa000, 0);

    if (image_num != IMAGE_TITLE && image_num != IMAGE_CREDITS) {
        StopMusic();
    }

    if (image_num != miscDataContents) {
        FILE *fp = GroupEntryFp(fullscreenImageNames[image_num]);

        miscDataContents = image_num;

        fread(miscData, 32000, 1, fp);
        fclose(fp);
    }

    EGA_MODE_DEFAULT();
    EGA_BIT_MASK_DEFAULT();
    FadeOut();
    SelectDrawPage(0);

    {  /* for scope */
        register word srcbase;
        register int i;
        word mask = 0x0100;

        for (srcbase = 0; srcbase < 32000; srcbase += 8000) {
            outport(0x03c4, 0x0002 | mask);

            for (i = 0; i < 8000; i++) {
                *(destbase + i) = *(miscData + i + srcbase);
            }

            mask <<= 1;
        }
    }

    SelectActivePage(0);
    FadeIn();
}

/*
Load sound data into system memory.

There are three group entries with sound data. `skip` allows them to be arranged
into a single linear pointer array.
*/
void LoadSoundData(char *entry_name, word *dest, int skip)
{
    int i;
    FILE *fp = GroupEntryFp(entry_name);

    fread(dest, (word)GroupEntryLength(entry_name), 1, fp);
    fclose(fp);

    for (i = 0; i < 23; i++) {
        soundDataPtr[i + skip] = dest + (*(dest + (i * 8) + 8) >> 1);
        soundPriority[i + skip + 1] = (byte)*(dest + (i * 8) + 9);
    }
}

/*
Trigger playback of a new sound.

If a sound is playing, it will be interrupted if the new sound has equal or
greater priority.
*/
void StartSound(word sound_num)
{
    if (soundPriority[sound_num] < activeSoundPriority) return;

    activeSoundPriority = soundPriority[sound_num];
    isNewSound = true;
    activeSoundIndex = sound_num - 1;
    enableSpeaker = false;
}

/*
Read a group entry into system memory.
*/
void LoadGroupEntryData(char *entry_name, byte *dest, word length)
{
    FILE *fp = GroupEntryFp(entry_name);

    fread(dest, length, 1, fp);
    fclose(fp);
}

/*
Load actor tile data into system memory.
*/
void LoadActorTileData(char *entry_name)
{
    FILE *fp = GroupEntryFp(entry_name);

    fread(actorTileData[0], WORD_MAX, 1, fp);
    fread(actorTileData[1], WORD_MAX, 1, fp);
    /* CAREFUL: Wraparound. Also, "ACTORS.MNI" should ideally use entry_name. */
    fread(actorTileData[2], (word)GroupEntryLength("ACTORS.MNI") + 2, 1, fp);
    fclose(fp);
}

/*
Load row-planar tile image data into EGA memory.
*/
void CopyTilesToEGA(byte *source, word dest_length, word dest_offset)
{
    word i;
    word mask;
    byte *src = source;
    byte *dest = MK_FP(0xa000, dest_offset);

    for (i = 0; i < dest_length; i++) {
        for (mask = 0x0100; mask < 0x1000; mask = mask << 1) {
            outport(0x03c4, mask | 0x0002);

            *(dest + i) = *(src++);
        }
    }
}

/*
Read a group entry containing "info" data into system memory.

Used when loading {ACTR,CART,PLYR}INFO.MNI entries.

NOTE: This is identical to the more general-purpose LoadGroupEntryData(), except
that here `dest` is a word pointer.
*/
void LoadInfoData(char *entry_name, word *dest, word length)
{
    FILE *fp = GroupEntryFp(entry_name);

    fread(dest, length, 1, fp);
    fclose(fp);
}

/*
Draw the static game world (backdrop plus all solid/masked map tiles), windowed
to the current scroll position.
*/
void DrawMapRegion(void)
{
    register int xtile;
    word destoff = 321;
    register word ymap;
    word ymapmax;
    word ytile = 1;
    word *mapcell;
    word ybd;
    word bdbase = 0x6300;

    if (hasHScrollBackdrop) {
        if (scrollX % 2 != 0) {
            bdbase = 0x7980;
        } else {
            bdbase = 0x6300;
        }
    }

    if (scrollY > mapHeight) scrollY = mapHeight;

    if (hasVScrollBackdrop && scrollY % 2 != 0) {
        bdbase += 0x2d00;
    }

    ybd =
        (hasVScrollBackdrop ? ((scrollY / 2) % 18) * 80 : 0) +
        (hasHScrollBackdrop ?  (scrollX / 2) % 40       : 0);

    EGA_MODE_LATCHED_WRITE();

    ymapmax = (scrollY + SCROLLH) << mapYPower;
    ymap = scrollY << mapYPower;

    do {
        xtile = 0;
        do {
            mapcell = mapData.w + ymap + xtile + scrollX;

            if (*mapcell < TILE_STRIPED_PLATFORM) {
                /* "Air" tile or platform direction command; show just backdrop */
                DrawSolidTile(*(backdropTable + ybd + xtile) + bdbase, xtile + destoff);
            } else if (*mapcell >= TILE_MASKED_0) {
                /* Masked tile with backdrop showing through transparent areas */
                DrawSolidTile(*(backdropTable + ybd + xtile) + bdbase, xtile + destoff);
                DrawMaskedTile(maskedTileData + *mapcell, xtile + 1, ytile);
            } else {
                /* Solid map tile */
                DrawSolidTile(*mapcell, xtile + destoff);
            }

            xtile++;
        } while (xtile < SCROLLW);

        destoff += 320;
        ytile++;
        ybd += 80;
        ymap += mapWidth;
    } while (ymap < ymapmax);
}

/*
Is any part of the sprite frame at x,y visible within the screen's scroll area?
*/
bool IsSpriteVisible(word sprite, word frame, word x, word y)
{
    register word width, height;
    word offset = *(actorInfoData + sprite) + (frame * 4);

    height = *(actorInfoData + offset);
    width = *(actorInfoData + offset + 1);

    if ((
        (scrollX <= x && scrollX + SCROLLW > x) ||
        (scrollX >= x && x + width > scrollX)
    ) && (
        (scrollY + SCROLLH > (y - height) + 1 && scrollY + SCROLLH <= y) ||
        (y >= scrollY && scrollY + SCROLLH > y)
    )) {
        return true;
    }

    return false;
}

/*
Can the passed sprite frame move to x,y considering the direction, and how?

NOTE: `dir` does not adjust the x,y values. Therefore, the passed x,y should
always reflect the location the sprite wants to move into, and *not* the
location where it currently is.
*/
word TestSpriteMove(word dir, word sprite, word frame, word x, word y)
{
    register word i;
    word *mapcell;
    word width;
    register word height;
    word offset = *(actorInfoData + sprite) + (frame * 4);

    height = *(actorInfoData + offset);
    width = *(actorInfoData + offset + 1);

    switch (dir) {
    case DIR4_NORTH:
        mapcell = MAP_CELL_ADDR(x, y - height + 1);

        for (i = 0; i < width; i++) {
            if (TILE_BLOCK_NORTH(*(mapcell + i))) return MOVE_BLOCKED;
        }

        break;

    case DIR4_SOUTH:
        mapcell = MAP_CELL_ADDR(x, y);

        for (i = 0; i < width; i++) {
            if (TILE_SLOPED(*(mapcell + i))) return MOVE_SLOPED;
            if (TILE_BLOCK_SOUTH(*(mapcell + i))) return MOVE_BLOCKED;
        }

        break;

    case DIR4_WEST:
        if (x == 0) return MOVE_BLOCKED;

        mapcell = MAP_CELL_ADDR(x, y);

        for (i = 0; i < height; i++) {
            if (
                i == 0 &&
                TILE_SLOPED(*mapcell) &&
                !TILE_BLOCK_WEST(*(mapcell - mapWidth))
            ) return MOVE_SLOPED;

            if (TILE_BLOCK_WEST(*mapcell)) return MOVE_BLOCKED;
            mapcell -= mapWidth;
        }

        break;

    case DIR4_EAST:
        if (x + width == mapWidth) return MOVE_BLOCKED;

        mapcell = MAP_CELL_ADDR(x + width - 1, y);

        for (i = 0; i < height; i++) {
            if (
                i == 0 &&
                TILE_SLOPED(*mapcell) &&
                !TILE_BLOCK_EAST(*(mapcell - mapWidth))
            ) return MOVE_SLOPED;

            if (TILE_BLOCK_EAST(*mapcell)) return MOVE_BLOCKED;
            mapcell -= mapWidth;
        }

        break;
    }

    return MOVE_FREE;
}

/*
Can the player move to x,y considering the direction, and how?

NOTE: `dir` does not adjust the x,y values. Therefore, the passed x,y should
always reflect the location the player wants to move into, and *not* the
location where they currently are.
*/
word TestPlayerMove(word dir, word x, word y)
{
    word i;
    word *mapcell;

    isPlayerSlidingEast = false;
    isPlayerSlidingWest = false;

    switch (dir) {
    case DIR4_NORTH:
        if (playerY - 3 == 0 || playerY - 2 == 0) return MOVE_BLOCKED;

        mapcell = MAP_CELL_ADDR(x, y - 4);

        for (i = 0; i < 3; i++) {
            if (TILE_BLOCK_NORTH(*(mapcell + i))) return MOVE_BLOCKED;
        }

        break;

    case DIR4_SOUTH:
        if (mapHeight + SCROLLH == playerY) return MOVE_FREE;

        mapcell = MAP_CELL_ADDR(x, y);

        if (
            !TILE_BLOCK_SOUTH(*mapcell) &&
            TILE_SLOPED(*mapcell) &&
            TILE_SLIPPERY(*mapcell)
        ) isPlayerSlidingEast = true;

        if (
            !TILE_BLOCK_SOUTH(*(mapcell + 2)) &&
            TILE_SLOPED(*(mapcell + 2)) &&
            TILE_SLIPPERY(*(mapcell + 2))
        ) isPlayerSlidingWest = true;

        for (i = 0; i < 3; i++) {
            if (TILE_SLOPED(*(mapcell + i))) {
                pounceStreak = 0;
                return MOVE_SLOPED;
            }

            if (TILE_BLOCK_SOUTH(*(mapcell + i))) {
                pounceStreak = 0;
                return MOVE_BLOCKED;
            }
        }

        break;

    case DIR4_WEST:
        mapcell = MAP_CELL_ADDR(x, y);
        canPlayerCling = TILE_CAN_CLING(*(mapcell - (mapWidth * 2)));

        for (i = 0; i < 5; i++) {
            if (TILE_BLOCK_WEST(*mapcell)) return MOVE_BLOCKED;

            if (
                i == 0 &&
                TILE_SLOPED(*mapcell) &&
                !TILE_BLOCK_WEST(*(mapcell - mapWidth))
            ) return MOVE_SLOPED;

            mapcell -= mapWidth;
        }

        break;

    case DIR4_EAST:
        mapcell = MAP_CELL_ADDR(x + 2, y);
        canPlayerCling = TILE_CAN_CLING(*(mapcell - (mapWidth * 2)));

        for (i = 0; i < 5; i++) {
            if (TILE_BLOCK_EAST(*mapcell)) return MOVE_BLOCKED;

            if (
                i == 0 &&
                TILE_SLOPED(*mapcell) &&
                !TILE_BLOCK_EAST(*(mapcell - mapWidth))
            ) return MOVE_SLOPED;

            mapcell -= mapWidth;
        }

        break;
    }

    return MOVE_FREE;
}

/*
Is the passed sprite frame at x,y touching the player's sprite?
*/
bool IsTouchingPlayer(word sprite, word frame, word x, word y)
{
    register word height, width;
    word offset;

    if (playerDeadTime != 0) return false;

    offset = *(actorInfoData + sprite) + (frame * 4);
    height = *(actorInfoData + offset);
    width = *(actorInfoData + offset + 1);

    if (x > mapWidth && x <= WORD_MAX && sprite == SPR_EXPLOSION) {
        width = x + width;
        x = 0;
    }

    if ((
        (playerX <= x && playerX + 3 > x) || (playerX >= x && x + width > playerX)
    ) && (
        (y - height < playerY && playerY <= y) || (playerY - 4 <= y && y <= playerY)  /* extra <= */
    )) return true;

    return false;
}

/*
Is the passed sprite frame at x,y (#1) touching another passed sprite (#2)?

Only used by IsNearExplosion().
*/
bool IsIntersecting(
    word sprite1, word frame1, word x1, word y1,
    word sprite2, word frame2, word x2, word y2
) {
    word width1;
    register word height1;
    word offset1;
    word width2;
    register word height2;
    word offset2;

    offset1 = *(actorInfoData + sprite1) + (frame1 * 4);
    height1 = *(actorInfoData + offset1);
    width1 = *(actorInfoData + offset1 + 1);

    offset2 = *(actorInfoData + sprite2) + (frame2 * 4);
    height2 = *(actorInfoData + offset2);
    width2 = *(actorInfoData + offset2 + 1);

    if (x1 > mapWidth && x1 <= WORD_MAX) {
        width1 = x1 + width1;
        x1 = 0;
    }

    if ((
        (x2 <= x1 && x2 + width2 > x1) || (x2 >= x1 && x1 + width1 > x2)
    ) && (
        (y1 - height1 < y2 && y2 <= y1) || (y2 - height2 < y1 && y1 <= y2)
    )) {
        return true;
    }

    return false;
}

/*
Draw an actor sprite frame at {x,y}_origin with the requested mode.
*/
void DrawSprite(word sprite, word frame, word x_origin, word y_origin, word mode)
{
    word x = x_origin;
    word y;
    word height, width;
    word offset;
    byte *src;
    DrawFunction drawfn;

    EGA_MODE_DEFAULT();

    offset = *(actorInfoData + sprite) + (frame * 4);
    height = *(actorInfoData + offset);
    width = *(actorInfoData + offset + 1);

    src = actorTileData[*(actorInfoData + offset + 3)] + *(actorInfoData + offset + 2);

    switch (mode) {
    case DRAWMODE_NORMAL:
    case DRAWMODE_IN_FRONT:
    case DRAWMODE_ABSOLUTE:
        drawfn = DrawSpriteTile;
        break;
    case DRAWMODE_WHITE:
        drawfn = DrawSpriteTileWhite;
        break;
    case DRAWMODE_TRANSLUCENT:
        drawfn = DrawSpriteTileTranslucent;
        break;
    }

    /* `mode` would go to ax if this was a switch, which doesn't happen */
    if (mode == DRAWMODE_FLIPPED)  goto flipped;
    if (mode == DRAWMODE_IN_FRONT) goto infront;
    if (mode == DRAWMODE_ABSOLUTE) goto absolute;

    y = (y_origin - height) + 1;
    for (;;) {
        if (
            x >= scrollX && scrollX + SCROLLW > x &&
            y >= scrollY && scrollY + SCROLLH > y &&
            !TILE_IN_FRONT(*(mapData.w + x + (y << mapYPower)))
        ) {
            drawfn(src, (x - scrollX) + 1, (y - scrollY) + 1);
        }

        src += 40;

        if (x == x_origin + width - 1) {
            if (y == y_origin) {
                EGA_BIT_MASK_DEFAULT();
                break;
            }
            x = x_origin;
            y++;
        } else {
            x++;
        }
    }

    return;

flipped:
    y = y_origin;
    for (;;) {
        if (
            x >= scrollX && scrollX + SCROLLW > x &&
            y >= scrollY && scrollY + SCROLLH > y &&
            !TILE_IN_FRONT(*(mapData.w + x + (y << mapYPower)))
        ) {
            DrawSpriteTileFlipped(src, (x - scrollX) + 1, (y - scrollY) + 1);
        }

        src += 40;

        if (x == x_origin + width - 1) {
            if (y == (y_origin - height) + 1) break;
            x = x_origin;
            y--;
        } else {
            x++;
        }
    }

    return;

infront:
    y = (y_origin - height) + 1;
    for (;;) {
        if (
            x >= scrollX && scrollX + SCROLLW > x &&
            y >= scrollY && scrollY + SCROLLH > y
        ) {
            drawfn(src, (x - scrollX) + 1, (y - scrollY) + 1);
        }

        src += 40;

        if (x == x_origin + width - 1) {
            if (y == y_origin) break;
            x = x_origin;
            y++;
        } else {
            x++;
        }
    }

    return;

absolute:
    y = (y_origin - height) + 1;
    for (;;) {
        DrawSpriteTile(src, x, y);  /* could've been drawfn */

        src += 40;

        if (x == x_origin + width - 1) {
            if (y == y_origin) break;
            x = x_origin;
            y++;
        } else {
            x++;
        }
    }
}

/*
Draw the player sprite frame at {x,y}_origin with the requested mode.
*/
void DrawPlayer(byte frame, word x_origin, word y_origin, word mode)
{
    word x = x_origin;
    word y;
    word height, width;
    word offset;
    byte *src;
    DrawFunction drawfn;

    EGA_MODE_DEFAULT();

    switch (mode) {
    case DRAWMODE_NORMAL:
    case DRAWMODE_IN_FRONT:
    case DRAWMODE_ABSOLUTE:
        drawfn = DrawSpriteTile;
        break;
    case DRAWMODE_WHITE:
        drawfn = DrawSpriteTileWhite;
        break;
    case DRAWMODE_TRANSLUCENT:
        /* never used in this game */
        drawfn = DrawSpriteTileTranslucent;
        break;
    }

    if (mode != DRAWMODE_ABSOLUTE && (
        playerForceFrame == PLAYER_HIDDEN ||
        activeTransporter != 0 ||
        playerHurtCooldown % 2 != 0 ||
        blockActionCmds
    )) return;

    offset = *playerInfoData + (frame * 4);
    height = *(playerInfoData + offset);
    width = *(playerInfoData + offset + 1);

    y = (y_origin - height) + 1;
    src = playerTileData + *(playerInfoData + offset + 2);

    /* `mode` would go to ax if this was a switch, which doesn't happen */
    if (mode == DRAWMODE_IN_FRONT) goto infront;
    if (mode == DRAWMODE_ABSOLUTE) goto absolute;

    for (;;) {
        if (
            x >= scrollX && scrollX + SCROLLW > x &&
            y >= scrollY && scrollY + SCROLLH > y &&
            !TILE_IN_FRONT(*(mapData.w + x + (y << mapYPower)))
        ) {
            drawfn(src, (x - scrollX) + 1, (y - scrollY) + 1);
        }

        src += 40;

        if (x == x_origin + width - 1) {
            if (y == y_origin) break;
            x = x_origin;
            y++;
        } else {
            x++;
        }
    }

    return;

absolute:
    for (;;) {
        DrawSpriteTile(src, x, y);  /* could've been drawfn */

        src += 40;

        if (x == x_origin + width - 1) {
            if (y == y_origin) break;
            x = x_origin;
            y++;
        } else {
            x++;
        }
    }

    return;

infront:
    for (;;) {
        if (
            x >= scrollX && scrollX + SCROLLW > x &&
            y >= scrollY && scrollY + SCROLLH > y
        ) {
            drawfn(src, (x - scrollX) + 1, (y - scrollY) + 1);
        }

        src += 40;

        if (x == x_origin + width - 1) {
            if (y == y_origin) break;
            x = x_origin;
            y++;
        } else {
            x++;
        }
    }
}

/*
Load cartoon data into system memory.
*/
void LoadCartoonData(char *entry_name)
{
    FILE *fp = GroupEntryFp(entry_name);

    fread(mapData.b, (word)GroupEntryLength(entry_name), 1, fp);
    fclose(fp);
}

/*
Draw a cartoon frame at x_origin,y_origin.
*/
void DrawCartoon(byte frame, word x_origin, word y_origin)
{
    word x = x_origin;
    word y;
    word height, width;
    word offset;
    byte *src;

    EGA_BIT_MASK_DEFAULT();
    EGA_MODE_DEFAULT();

    if (isCartoonDataLoaded != true) {  /* explicit compare against 1 */
        isCartoonDataLoaded = true;
        LoadCartoonData("CARTOON.MNI");
    }

    offset = *cartoonInfoData + (frame * 4);
    height = *(cartoonInfoData + offset);
    width = *(cartoonInfoData + offset + 1);

    y = (y_origin - height) + 1;
    src = mapData.b + *(cartoonInfoData + offset + 2);

    for (;;) {
        DrawSpriteTile(src, x, y);

        src += 40;

        if (x == x_origin + width - 1) {
            if (y == y_origin) break;
            x = x_origin;
            y++;
        } else {
            x++;
        }
    }
}

/*
Handle movement of the player when standing on a moving platform/fountain.

There's no real need for x_dir/y_dir to be split up; might be cruft.
*/
void MovePlayerPlatform(word x_west, word x_east, word x_dir, word y_dir)
{
    register word offset;
    register word playerx2;

    if (scooterMounted != 0) return;

    offset = *playerInfoData;  /* read frame 0 regardless of player's real frame */
    playerx2 = *(playerInfoData + offset + 1) + playerX - 1;

    if (
        playerClingDir != DIR4_NONE &&
        TestPlayerMove(DIR4_SOUTH, playerX, playerY + 1) != MOVE_FREE
    ) {
        playerClingDir = DIR4_NONE;
    }

    if (
        (playerX  < x_west || playerX  > x_east) &&
        (playerx2 < x_west || playerx2 > x_east)
    ) return;

    playerX += dir8X[x_dir];
    playerY += dir8Y[y_dir];

    if ((cmdNorth || cmdSouth) && !cmdWest && !cmdEast) {
        if (cmdNorth && scrollY > 0 && playerY - scrollY < SCROLLH - 1) {
            scrollY--;
        }

        if (cmdSouth && (
            scrollY + 4 < playerY || (dir8Y[y_dir] == 1 && scrollY + 3 < playerY)
        )) {
            scrollY++;
        }
    }

    if (playerY - scrollY > SCROLLH - 1) {
        scrollY++;
    } else if (playerY - scrollY < 3) {
        scrollY--;
    }

    if (playerX - scrollX > 23 && mapWidth - SCROLLW > scrollX) {
        scrollX++;
    } else if (playerX - scrollX < 12 && scrollX > 0) {
        scrollX--;
    }

    if (dir8Y[y_dir] == 1 && playerY - scrollY > 14) {
        scrollY++;
    }

    if (dir8Y[y_dir] == -1 && playerY - scrollY < 3) {
        scrollY--;
    }
}

/*
Perform one frame of movement on every platform in the map.

This makes a waaay unsafe assumption about the Platform struct packing.
*/
void MovePlatforms(void)
{
    register word i, x;

    for (i = 0; i < numPlatforms; i++) {
        Platform *plat = platforms + i;
        word newdir;

        for (x = 2; x < 7; x++) {
            SetMapTile(*((word *)plat + x), (plat->x + x) - 4, plat->y);
        }

        newdir = GetMapTile(plat->x, plat->y) / 8;

        if (playerDeadTime == 0 && plat->y - 1 == playerY && arePlatformsActive) {
            MovePlayerPlatform(plat->x - 2, plat->x + 2, newdir, newdir);
        }

        if (arePlatformsActive) {
            plat->x += dir8X[newdir];
            plat->y += dir8Y[newdir];
        }

        for (x = 2; x < 7; x++) {
            *((word *)plat + x) = GetMapTile((plat->x + x) - 4, plat->y);
        }

        for (x = 2; x < 7; x++) {
            SetMapTile(TILE_BLUE_PLATFORM + ((x - 2) * 8), (plat->x + x) - 4, plat->y);
        }
    }
}

/*
Perform SetMapTile(), repeated `count` times horizontally.
*/
void SetMapTileRepeat(word value, word count, word x_origin, word y_origin)
{
    word x;

    for (x = 0; x < count; x++) {
        SetMapTile(value, x_origin + x, y_origin);
    }
}

/*
Perform SetMapTile() four times horizontally, with unique values.
*/
void SetMapTile4(
    word val1, word val2, word val3, word val4, word x_origin, word y_origin
) {
    SetMapTile(val1, x_origin,     y_origin);
    SetMapTile(val2, x_origin + 1, y_origin);
    SetMapTile(val3, x_origin + 2, y_origin);
    SetMapTile(val4, x_origin + 3, y_origin);
}

/*
Perform one frame of movement on every fountain in the map.
*/
void MoveFountains(void)
{
    word i;

    for (i = 0; i < numFountains; i++) {
        Fountain *fnt = fountains + i;

        if (fnt->delayleft != 0) {
            fnt->delayleft--;
            continue;
        }

        fnt->stepcount++;

        if (fnt->stepcount == fnt->stepmax) {
            fnt->stepcount = 0;
            fnt->dir = !fnt->dir;  /* flip between north and south */
            fnt->delayleft = 10;
            continue;
        }

        SetMapTile(TILE_EMPTY, fnt->x,     fnt->y);
        SetMapTile(TILE_EMPTY, fnt->x + 2, fnt->y);

        if (playerDeadTime == 0 && fnt->y - 1 == playerY) {
            if (fnt->dir != DIR4_NORTH) {
                MovePlayerPlatform(fnt->x, fnt->x + 2, DIR8_STATIONARY, DIR8_SOUTH);
            } else {
                MovePlayerPlatform(fnt->x, fnt->x + 2, DIR8_STATIONARY, DIR8_NORTH);
            }
        }

        if (fnt->dir != DIR4_NORTH) {
            fnt->y++;
            fnt->height--;
        } else {
            fnt->y--;
            fnt->height++;
        }

        SetMapTile(TILE_INVISIBLE_PLATFORM, fnt->x,     fnt->y);
        SetMapTile(TILE_INVISIBLE_PLATFORM, fnt->x + 2, fnt->y);
    }
}

/*
Draw all fountains, and handle contact between their streams and the player.
*/
void DrawFountains(void)
{
    static word slowcount = 0;
    static word fastcount = 0;
    word i;

    fastcount++;
    if (fastcount % 2 != 0) {
        slowcount++;
    }

    for (i = 0; i < numFountains; i++) {
        word y;
        Fountain *fnt = fountains + i;

        DrawSprite(SPR_FOUNTAIN, slowcount % 2, fnt->x, fnt->y + 1, DRAWMODE_NORMAL);

        for (y = 0; fnt->height + 1 > y; y++) {
            DrawSprite(SPR_FOUNTAIN, (slowcount % 2) + 2, fnt->x + 1, fnt->y + y + 1, DRAWMODE_NORMAL);

            if (IsTouchingPlayer(SPR_FOUNTAIN, 2, fnt->x + 1, fnt->y + y + 1)) {
                HurtPlayer();
            }
        }
    }
}

/*
Return the map tile value at the passed x,y position.
*/
word GetMapTile(word x, word y)
{
    return *(mapData.w + x + (y << mapYPower));
}

/*
Lighten each area of the map that a light touches.

The map defines the top edge of each column of the cone. This function fills all
tiles south of the defined light actor until a south-blocking tile is hit.
*/
void DrawLights(void)
{
    register word i;

    if (!areLightsActive) return;

    EGA_MODE_DEFAULT();

    for (i = 0; i < numLights; i++) {
        register word y;
        word xorigin, yorigin;
        word side = lights[i].side;

        xorigin = lights[i].x;
        yorigin = lights[i].y;

        if (
            xorigin >= scrollX && scrollX + SCROLLW > xorigin &&
            yorigin >= scrollY && scrollY + SCROLLH - 1 >= yorigin
        ) {
            if (side == SPA_LIGHT_WEST - 6) {
                LightenScreenTileWest(xorigin - scrollX + 1, yorigin - scrollY + 1);
            } else if (side == SPA_LIGHT_MIDDLE - 6) {
                LightenScreenTile(xorigin - scrollX + 1, yorigin - scrollY + 1);
            } else {  /* SPA_LIGHT_EAST */
                LightenScreenTileEast(xorigin - scrollX + 1, yorigin - scrollY + 1);
            }
        }

        for (y = yorigin + 1; yorigin + LIGHT_CAST_DISTANCE > y; y++) {
            if (TILE_BLOCK_SOUTH(GetMapTile(xorigin, y))) break;

            if (
                xorigin >= scrollX && scrollX + SCROLLW > xorigin &&
                y >= scrollY && scrollY + SCROLLH - 1 >= y
            ) {
                LightenScreenTile(xorigin - scrollX + 1, y - scrollY + 1);
            }
        }
    }
}

/*
Create the specified actor at the current nextActorIndex.
*/
void ConstructActor(
    word sprite, word x, word y, bool force_active, bool stay_active,
    bool weighted, bool acrophile, ActorTickFunction tick_func, word data1,
    word data2, word data3, word data4, word data5
) {
    Actor *act;

    if (data2 == SPR_BARREL_SHARDS || data2 == SPR_BASKET_SHARDS) {
        numBarrels++;
    }

    act = actors + nextActorIndex;

    act->sprite = sprite;
    act->frame = 0;
    act->x = x;
    act->y = y;
    act->forceactive = force_active;
    act->stayactive = stay_active;
    act->weighted = weighted;
    act->acrophile = acrophile;
    act->dead = false;
    act->tickfunc = tick_func;
    act->private1 = 0;
    act->private2 = 0;
    act->fallspeed = 0;
    act->data1 = data1;
    act->data2 = data2;
    act->data3 = data3;
    act->data4 = data4;
    act->data5 = data5;
    act->damagecooldown = 0;
}

/*
Ensure that the numbered actor moved to a valid place, and adjust if not.

Actors modified by this function move left/right while negotiating slopes. The
actor should have already been (blindly) moved into the desired location, then
this function will check to see if the actor is in a good place and adjust or
revert the move if not.

Sets private1 to 0 if westward movement was blocked, and 1 if the move was valid
as-is. private2 is changed the same way for eastward movement.
*/
void AdjustActorMove(word index, word dir)
{
    Actor *act = actors + index;
    word offset;
    word width;
    word result = 0;

    offset = *(actorInfoData + act->sprite);
    width = *(actorInfoData + offset + 1);

    if (dir == DIR4_WEST) {
        result = TestSpriteMove(DIR4_WEST, act->sprite, act->frame, act->x, act->y);
        act->private1 = !result;

        if (act->private1 == 0 && result != MOVE_SLOPED) {
            act->x++;
            return;
        } else if (result == MOVE_SLOPED) {
            act->private1 = 1;
            act->y--;
            return;
        }

        if ((word)TestSpriteMove(DIR4_SOUTH, act->sprite, act->frame, act->x, act->y + 1) > 0) {
            act->private1 = 1;
        } else if (
            TILE_SLOPED(GetMapTile(act->x + width, act->y + 1)) &&
            TILE_SLOPED(GetMapTile((act->x + width) - 1, act->y + 2))
        ) {
            if (!TILE_BLOCK_SOUTH(GetMapTile((act->x + width) - 1, act->y + 1))) {
                act->private1 = 1;

                if (!TILE_SLOPED(GetMapTile((act->x + width) - 1, act->y + 1))) {
                    act->y++;
                }
            }
        } else if (act->private1 == 0) {
            act->x++;
        } else if (
            !act->acrophile &&
            TestSpriteMove(DIR4_WEST, act->sprite, act->frame, act->x, act->y + 1) == MOVE_FREE &&
            !TILE_SLOPED(GetMapTile((act->x + width) - 1, act->y + 1))
        ) {
            act->x++;
            act->private1 = 0;
        }
    } else {  /* DIR4_EAST */
        result = TestSpriteMove(DIR4_EAST, act->sprite, act->frame, act->x, act->y);
        act->private2 = !result;

        if (act->private2 == 0 && result != MOVE_SLOPED) {
            act->x--;
            return;
        } else if (result == MOVE_SLOPED) {
            act->private2 = 1;
            act->y--;
            return;
        }

        if ((word)TestSpriteMove(DIR4_SOUTH, act->sprite, act->frame, act->x, act->y + 1) > 0) {
            act->private2 = 1;
        } else if (
            TILE_SLOPED(GetMapTile(act->x - 1, act->y + 1)) &&
            TILE_SLOPED(GetMapTile(act->x, act->y + 2))
        ) {
            if (!TILE_BLOCK_SOUTH(GetMapTile(act->x, act->y + 1))) {
                act->private2 = 1;

                if (!TILE_SLOPED(GetMapTile(act->x, act->y + 1))) {
                    act->y++;
                }
            }
        } else if (act->private2 == 0) {
            act->x--;
        } else if (
            !act->acrophile &&
            TestSpriteMove(DIR4_EAST, act->sprite, act->frame, act->x, act->y + 1) == MOVE_FREE &&
            !TILE_SLOPED(GetMapTile(act->x, act->y + 1))
        ) {
            act->x--;
            act->private2 = 0;
        }
    }
}

/*
Handle one frame of foot switch movement.
*/
void ActFootSwitch(word index)
{
    Actor *act = actors + index;

    /*
    This function is used for a variety of functionless actors, and in those
    cases it behaves as a no-op.
    */
    if (act->sprite != SPR_FOOT_SWITCH) return;

    if (act->private1 == 0) {
        act->private1 = 1;
        /*
        BUG: TILE_SWITCH_BLOCK_* have a horizontal white line on the top edge
        which is not appropriate for the design of the topmost switch position.
        When a switch is pounced for the first time, this is visible for one
        frame until this function runs and adjusts it to TILE_SWITCH_FREE_*N.
        */
        SetMapTile4(
            TILE_SWITCH_BLOCK_1, TILE_SWITCH_BLOCK_2, TILE_SWITCH_BLOCK_3,
            TILE_SWITCH_BLOCK_4, act->x, act->y
        );
    }

    if (act->data4 != 0) {
        act->data4 = 0;
        /* data3 is 64 on first pounce, for TILE_SWITCH_FREE_1N. 0 after. */
        SetMapTile4(
            (TILE_SWITCH_FREE_1L - act->data3),
            (TILE_SWITCH_FREE_1L - act->data3) + 8,
            (TILE_SWITCH_FREE_1L - act->data3) + 16,
            (TILE_SWITCH_FREE_1L - act->data3) + 24,
            act->x, act->y
        );
        act->y++;
        SetMapTile4(
            TILE_SWITCH_BLOCK_1, TILE_SWITCH_BLOCK_2, TILE_SWITCH_BLOCK_3,
            TILE_SWITCH_BLOCK_4, act->x, act->y
        );

        if (act->data1 == 4) {
            StartSound(SND_FOOT_SWITCH_ON);

            switch (act->data5) {
            case ACT_SWITCH_PLATFORMS:
                arePlatformsActive = true;
                break;
            case ACT_SWITCH_MYSTERY_WALL:
                mysteryWallTime = 4;  /* no significance; could've been bool */
                if (!sawMysteryWallBubble) {
                    sawMysteryWallBubble = true;
                    NewActor(ACT_SPEECH_WHOA, playerX - 1, playerY - 5);
                }
                break;
            case ACT_SWITCH_LIGHTS:
                areLightsActive = true;
                break;
            case ACT_SWITCH_FORCE_FIELD:
                areForceFieldsActive = false;
                break;
            }
        } else {
            StartSound(SND_FOOT_SWITCH_MOVE);
        }
    }

    if (
        act->data1 < 4 && act->data4 == 0 &&
        IsNearExplosion(SPR_FOOT_SWITCH, 0, act->x, act->y)
    ) {
        act->data1++;

        if (act->data2 == 0) {
            act->data3 = 64;
            act->data2 = 1;
        } else {
            act->data3 = 0;
        }

        act->data4 = 1;
    }
}

/*
Handle one frame of horizontal saw blade/sharp robot movement.
*/
void ActHorizontalMover(word index)
{
    Actor *act = actors + index;

    act->data3 = !act->data3;

    if (act->sprite == SPR_SAW_BLADE) {
        act->data3 = 1;

        if (IsSpriteVisible(act->sprite, 0, act->x, act->y)) {
            StartSound(SND_SAW_BLADE_MOVE);
        }
    }

    if (act->data4 != 0) act->data4--;

    if (act->data3 == 0) return;

    if (act->data4 == 0) {
        if (act->data2 != DIR2_WEST) {
            act->x++;
            AdjustActorMove(index, DIR4_EAST);
            if (act->private2 == 0) {
                act->data2 = DIR2_WEST;
                act->data4 = act->data1;
            }
        } else {
            act->x--;
            AdjustActorMove(index, DIR4_WEST);
            if (act->private1 == 0) {
                act->data2 = DIR2_EAST;
                act->data4 = act->data1;
            }
        }
    }

    act->frame++;
    if (act->frame > act->data5) act->frame = 0;
}

/*
Handle one frame of floor/ceiling jump pad movement.
*/
void ActJumpPad(word index)
{
    Actor *act = actors + index;

    if (act->data1 > 0) {
        act->frame = 1;
        act->data1--;
    } else {
        act->frame = 0;
    }

    if (act->data5 != 0) {
        nextDrawMode = DRAWMODE_FLIPPED;

        if (act->frame == 0) {
            act->y = act->data3;
        } else {
            act->y = act->data4;
        }
    }
}

/*
Handle one frame of arrow piston movement.
*/
void ActArrowPiston(word index)
{
    Actor *act = actors + index;

    if (act->data1 < 31) {
        act->data1++;
    } else {
        act->data1 = 0;
    }

    if (
        (act->data1 == 29 || act->data1 == 26) &&
        IsSpriteVisible(act->sprite, 0, act->x, act->y)
    ) {
        StartSound(SND_SPIKES_MOVE);
    }

    if (act->data5 == DIR2_WEST) {
        if (act->data1 > 28) {
            act->x++;
        } else if (act->data1 > 25) {
            act->x--;
        }
    } else {
        if (act->data1 > 28) {
            act->x--;
        } else if (act->data1 > 25) {
            act->x++;
        }
    }
}

/*
Handle one frame of fireball movement.

NOTE: This actor consists only of the fireball, which spends its idle frames
hidden behind map tiles. The "launchers" are solid map tiles.
*/
void ActFireball(word index)
{
    Actor *act = actors + index;

    if (act->data1 == 29) {
        StartSound(SND_FIREBALL_LAUNCH);
    }

    if (act->data1 < 30) {
        act->data1++;
    } else {
        if (act->data5 == DIR2_WEST) {
            act->x--;
            act->private1 = !TestSpriteMove(DIR4_WEST, act->sprite, 0, act->x, act->y);
            if (act->private1 == 0) {
                act->data1 = 0;
                NewDecoration(SPR_SMOKE, 6, act->x + 1, act->y, DIR8_NORTH, 1);
                act->x = act->data2;
                act->y = act->data3;
                StartSound(SND_BIG_OBJECT_HIT);
            }
        } else {
            act->x++;
            act->private2 = !TestSpriteMove(DIR4_EAST, act->sprite, 0, act->x, act->y);
            if (act->private2 == 0) {
                act->data1 = 0;
                NewDecoration(SPR_SMOKE, 6, act->x - 2, act->y, DIR8_NORTH, 1);
                act->x = act->data2;
                act->y = act->data3;
                StartSound(SND_BIG_OBJECT_HIT);
            }
        }
    }

    if (!IsSpriteVisible(act->sprite, act->frame, act->x, act->y)) {
        act->data1 = 0;
        act->x = act->data2;
        act->y = act->data3;
    }

    act->frame = !act->frame;
}

/*
Look for any door(s) linked to the passed switch, and unlock them if needed.
*/
void UpdateDoors(word door_sprite, Actor *act_switch)
{
    word i, y;

    for (i = 0; i < numActors; i++) {
        Actor *door = actors + i;

        if (door->sprite != door_sprite) continue;

        if (act_switch->data1 == 2) {
            door->dead = true;
            StartSound(SND_DOOR_UNLOCK);

            NewDecoration(door_sprite, 1, door->x, door->y, DIR8_SOUTH, 5);
        } else if (act_switch->data1 == 1) {
            for (y = 0; y < 5; y++) {
                SetMapTile(*((word *)&door->data1 + y), door->x + 1, door->y - y);
            }
        }
    }
}

/*
Handle one frame of head switch movement.
*/
void ActHeadSwitch(word index)
{
    Actor *act = actors + index;

    if (act->frame == 1) {
        if (act->data1 < 3) act->data1++;

        UpdateDoors(act->data5, act);
    }
}

/*
Handle first-time initialization of a door, then become a no-op.
*/
void ActDoor(word index)
{
    word y;
    Actor *act = actors + index;

    if (act->private1 != 0) return;

    act->private1 = 1;

    for (y = 0; y < 5; y++) {
        *((word *)&act->data1 + y) = GetMapTile(act->x + 1, act->y - y);

        SetMapTile(TILE_DOOR_BLOCK, act->x + 1, act->y - y);
    }
}

/*
Handle one frame of jump pad robot movement.
*/
void ActJumpPadRobot(word index)
{
    Actor *act = actors + index;

    if (act->data1 > 0) {
        act->frame = 2;
        act->data1--;
    } else {
        act->frame = !act->frame;

        if (act->data2 != DIR2_WEST) {
            act->x++;
            AdjustActorMove(index, DIR4_EAST);
            if (act->private2 == 0) {
                act->data2 = DIR2_WEST;
            }
        } else {
            act->x--;
            AdjustActorMove(index, DIR4_WEST);
            if (act->private1 == 0) {
                act->data2 = DIR2_EAST;
            }
        }
    }

    if (!IsSpriteVisible(SPR_JUMP_PAD_ROBOT, 2, act->x, act->y)) {
        act->frame = 0;
    }
}

/*
Handle one frame of reciprocating spike movement.
*/
void ActReciprocatingSpikes(word index)
{
    Actor *act = actors + index;

    act->data2++;
    if (act->data2 == 20) act->data2 = 0;

    if (act->frame == 0 && act->data2 == 0) {
        act->data1 = 0;
        StartSound(SND_SPIKES_MOVE);

    } else if (act->frame == 2 && act->data2 == 0) {
        act->data1 = 1;
        StartSound(SND_SPIKES_MOVE);
        nextDrawMode = DRAWMODE_HIDDEN;

    } else if (act->data1 != 0) {
        if (act->frame > 0) act->frame--;

    } else if (act->frame < 2) {
        act->frame++;
    }

    if (act->frame == 2) {
        nextDrawMode = DRAWMODE_HIDDEN;
    }
}

/*
Handle one frame of vertical saw blade movement.
*/
void ActVerticalMover(word index)
{
    Actor *act = actors + index;

    act->frame = !act->frame;

    if (IsSpriteVisible(act->sprite, 0, act->x, act->y)) {
        StartSound(SND_SAW_BLADE_MOVE);
    }

    if (act->data1 != DIR2_SOUTH) {
        if (TestSpriteMove(DIR4_NORTH, act->sprite, 0, act->x, act->y - 1) != MOVE_FREE) {
            act->data1 = DIR2_SOUTH;
        } else {
            act->y--;
        }
    } else {
        if (TestSpriteMove(DIR4_SOUTH, act->sprite, 0, act->x, act->y + 1) != MOVE_FREE) {
            act->data1 = DIR2_NORTH;
        } else {
            act->y++;
        }
    }
}

/*
Handle one frame of armed bomb movement.
*/
void ActBombArmed(word index)
{
    Actor *act = actors + index;

    if (act->frame == 3) {
        act->data2++;
        act->data1++;
        if (act->data1 % 2 != 0 && act->frame == 3) {
            nextDrawMode = DRAWMODE_WHITE;
        }

        if (act->data2 == 10) {
            act->dead = true;
            NewPounceDecoration(act->x - 2, act->y + 2);
            nextDrawMode = DRAWMODE_HIDDEN;
            NewExplosion(act->x - 2, act->y);
            if (act->data1 % 2 != 0 && act->frame == 3) {
                DrawSprite(SPR_BOMB_ARMED, act->frame, act->x, act->y, DRAWMODE_WHITE);
            }
        }
    } else {
        act->data1++;
        if (act->data1 == 5) {
            act->data1 = 0;
            act->frame++;
        }
    }

    if (TestSpriteMove(DIR4_SOUTH, SPR_BOMB_ARMED, 0, act->x, act->y) != MOVE_FREE) {
        act->y--;
    }
}

/*
Handle one frame of barrel movement.
*/
void ActBarrel(word index)
{
    Actor *act = actors + index;

    if (IsNearExplosion(SPR_BARREL, 0, act->x, act->y)) {
        DestroyBarrel(index);
        AddScore(1600);
        NewActor(ACT_SCORE_EFFECT_1600, act->x, act->y);
    }
}

/*
Handle one frame of cabbage movement.
*/
void ActCabbage(word index)
{
    Actor *act = actors + index;

    if (
        act->data2 == 10 && act->data3 == 3 &&
        TestSpriteMove(DIR4_SOUTH, SPR_CABBAGE, 0, act->x, act->y + 1) == MOVE_FREE
    ) {
        if (act->data4 != 0) {
            act->frame = 3;
        } else {
            act->frame = 1;
        }

    } else if (
        act->data2 < 10 &&
        TestSpriteMove(DIR4_SOUTH, SPR_CABBAGE, 0, act->x, act->y + 1) != MOVE_FREE
    ) {
        act->data2++;
        if (act->x > playerX) {
            act->data4 = act->frame = 0;
        } else {
            act->data4 = act->frame = 2;
        }

    } else if (act->data3 < 3) {
        static signed char yjump[] = {-1, -1, 0};
        act->y += yjump[act->data3];

        if (act->data4 != 0) {
            act->x++;
            AdjustActorMove(index, DIR4_EAST);
        } else {
            act->x--;
            AdjustActorMove(index, DIR4_WEST);
        }

        act->data3++;
        if (act->data4 != 0) {
            act->frame = 3;
        } else {
            act->frame = 1;
        }

    } else {
        act->data2 = 0;
        act->data3 = 0;
        if (act->x > playerX) {
            act->data4 = act->frame = 0;
        } else {
            act->data4 = act->frame = 2;
        }
    }
}

/*
Handle one frame of reciprocating spear movement.
*/
void ActReciprocatingSpear(word index)
{
    Actor *act = actors + index;

    if (act->data1 < 30) {
        act->data1++;
    } else {
        act->data1 = 0;
    }

    if (act->data1 > 22) {
        act->y--;
    } else if (act->data1 > 14) {
        act->y++;
    }
}

/*
Handle one frame of ged/green slime movement.

The repeat rate of dripping slime actors is directly related to how far they
have to travel before falling off the bottom of the screen.
*/
void ActRedGreenSlime(word index)
{
    static word throbframes[] = {0, 1, 2, 3, 2, 1, 0};
    Actor *act = actors + index;

    if (act->data5 != 0) {  /* throb and drip */
        if (act->data4 == 0) {
            act->frame = throbframes[act->data3 % 6];
            act->data3++;

            if (act->data3 == 15) {
                act->data4 = 1;
                act->data3 = 0;
                act->frame = 4;
                if (IsSpriteVisible(SPR_GREEN_SLIME, 6, act->x, act->data2)) {
                    StartSound(SND_DRIP);
                }
            }

        } else if (act->frame < 6) {
            act->frame++;

        } else {
            act->y++;
            if (!IsSpriteVisible(SPR_GREEN_SLIME, 6, act->x, act->y)) {
                act->y = act->data2;
                act->data4 = 0;
                act->frame = 0;
            }
        }

    } else {  /* throb */
        act->frame = throbframes[act->data3];
        act->data3++;

        if (act->data3 == 6) {
            act->data3 = 0;
        }
    }
}

/*
Handle one frame of flying wisp movement.
*/
void ActFlyingWisp(word index)
{
    Actor *act = actors + index;

    act->frame = !act->frame;

    if (act->data1 < 63) {
        act->data1++;
    } else {
        act->data1 = 0;
    }

    if (act->data1 > 50) {
        act->y += 2;

        if (act->data1 < 55) {
            act->y--;
        }

        nextDrawMode = DRAWMODE_FLIPPED;

    } else if (act->data1 > 34) {
        if (act->data1 < 47) {
            act->y--;
        }

        if (act->data1 < 45) {
            act->y--;
        }
    }
}

/*
Handle one frame of "Two Tons" crusher movement.
*/
void ActTwoTonsCrusher(word index)
{
    Actor *act = actors + index;

    if (act->data1 < 20) {
        act->data1++;
    }

    if (act->data1 == 19) {
        act->data2 = 1;
    }

    if (act->data2 == 1) {
        if (act->frame < 3) {
            act->frame++;

            switch (act->frame) {
            case 1:
                act->data3 = 1;
                break;
            case 2:
                act->data3 = 2;
                break;
            case 3:
                act->data3 = 4;
                break;
            }

            act->y += act->data3;

        } else {
            act->data2 = 2;

            if (IsSpriteVisible(SPR_TWO_TONS_CRUSHER, 4, act->x - 1, act->y + 3)) {
                StartSound(SND_OBJECT_HIT);
            }
        }
    }

    if (act->data2 == 2) {
        if (act->frame > 0) {
            act->frame--;

            switch (act->frame) {
            case 0:
                act->data3 = 1;
                break;
            case 1:
                act->data3 = 2;
                break;
            case 2:
                act->data3 = 4;
                break;
            }

            act->y -= act->data3;

        } else {
            act->data2 = 0;
            act->data1 = 0;
            act->data3 = 0;
        }
    }

    if (IsTouchingPlayer(SPR_TWO_TONS_CRUSHER, 4, act->x - 1, act->y + 3)) {
        HurtPlayer();
    }

    DrawSprite(SPR_TWO_TONS_CRUSHER, 4, act->x - 1, act->y + 3, DRAWMODE_NORMAL);
}

/*
Handle one frame of jumping bullet movement.
*/
void ActJumpingBullet(word index)
{
    static int yjump[] = {-2, -2, -2, -2, -1, -1, -1, 0, 0, 1, 1, 1, 2, 2, 2, 2};
    Actor *act = actors + index;

    if (act->data2 == DIR2_WEST) {
        act->x--;
    } else {
        act->x++;
    }

    act->y += yjump[act->data3];
    act->data3++;

    if (act->data3 == 16) {
        act->data2 = !act->data2;  /* flip between west and east */

        if (IsSpriteVisible(SPR_JUMPING_BULLET, 0, act->x, act->y)) {
            StartSound(SND_OBJECT_HIT);
        }

        act->data3 = 0;
    }
}

/*
Handle one frame of stone head crusher movement.
*/
void ActStoneHeadCrusher(word index)
{
    Actor *act = actors + index;

    act->data4 = !act->data4;

    if (act->data1 == 0) {
        if (act->y < playerY && act->x <= playerX + 6 && act->x + 7 > playerX) {
            act->data1 = 1;
            act->data2 = act->y;
            act->frame = 1;
        } else {
            act->frame = 0;
        }

    } else if (act->data1 == 1) {
        act->frame = 1;
        act->y++;

        if (TestSpriteMove(DIR4_SOUTH, SPR_STONE_HEAD_CRUSHER, 0, act->x, act->y) != MOVE_FREE) {
            act->data1 = 2;

            if (IsSpriteVisible(SPR_STONE_HEAD_CRUSHER, 0, act->x, act->y)) {
                StartSound(SND_OBJECT_HIT);
                NewDecoration(SPR_SMOKE, 6, act->x + 1, act->y, DIR8_NORTHEAST, 1);
                NewDecoration(SPR_SMOKE, 6, act->x,     act->y, DIR8_NORTHWEST, 1);
            }

            act->y--;

        } else {
            act->y++;

            if (TestSpriteMove(DIR4_SOUTH, SPR_STONE_HEAD_CRUSHER, 0, act->x, act->y) != MOVE_FREE) {
                act->data1 = 2;

                /* Possible BUG: Missing IsSpriteVisible() causes offscreen sound? */
                StartSound(SND_OBJECT_HIT);
                NewDecoration(SPR_SMOKE, 6, act->x + 1, act->y, DIR8_NORTHEAST, 1);
                NewDecoration(SPR_SMOKE, 6, act->x,     act->y, DIR8_NORTHWEST, 1);

                act->y--;
            }
        }

    } else if (act->data1 == 2) {
        act->frame = 0;

        if (act->y == act->data2) {
            act->data1 = 0;
        } else if (act->data4 != 0) {
            act->y--;
        }
    }
}

/*
Handle one frame of pyramid movement.
*/
void ActPyramid(word index)
{
    Actor *act = actors + index;

    if (act->data5 != 0) {  /* floor mounted */
        nextDrawMode = DRAWMODE_FLIPPED;

    } else if (act->data1 == 0) {
        if (act->y < playerY && act->x <= playerX + 6 && act->x + 5 > playerX) {
            act->data1 = 1;
            act->weighted = true;
        }

    } else if (TestSpriteMove(DIR4_SOUTH, act->sprite, 0, act->x, act->y + 1) != MOVE_FREE) {
        act->dead = true;
        NewDecoration(SPR_SMOKE, 6, act->x, act->y, DIR8_NORTH, 3);
        StartSound(SND_BIG_OBJECT_HIT);
        nextDrawMode = DRAWMODE_HIDDEN;
    }

    if (!act->dead) {
        /* BUG? Non-falling pyramids use a different function; don't propagate explosions */
        if (IsNearExplosion(act->sprite, act->frame, act->x, act->y)) {
            act->data2 = 3;
        }

        if (act->data2 != 0) {
            act->data2--;

            if (act->data2 == 0) {
                NewExplosion(act->x - 1, act->y + 1);
                act->dead = true;
                AddScore(200);
                NewShard(act->sprite, 0, act->x, act->y);
            }
        }
    }
}

/*
Handle one frame of ghost movement.
*/
void ActGhost(word index)
{
    Actor *act = actors + index;

    act->data4++;
    if (act->data4 % 3 == 0) {
        act->data1++;
    }

    if (act->data1 == 4) act->data1 = 0;

    if (playerBaseFrame == PLAYER_BASE_WEST) {
        if (act->x > playerX + 2 && playerClingDir == DIR4_WEST && cmdEast) {
            act->frame = (random(35) == 0 ? 4 : 0) + 2;
        } else if (act->x > playerX) {
            act->frame = act->data1 % 2;
            if (act->data1 == 0) {
                act->x--;
                if (act->y < playerY) {
                    act->y++;
                } else if (act->y > playerY) {
                    act->y--;
                }
            }
        } else {
            act->frame = (random(35) == 0 ? 2 : 0) + 5;
        }
    } else {
        if (act->x < playerX && playerClingDir == DIR4_EAST && cmdWest) {
            act->frame = (random(35) == 0 ? 2 : 0) + 5;
        } else if (act->x < playerX) {
            act->frame = (act->data1 % 2) + 3;
            if (act->data1 == 0) {
                act->x++;
                if (act->y < playerY) {
                    act->y++;
                } else if (act->y > playerY) {
                    act->y--;
                }
            }
        } else {
            act->frame = (random(35) == 0 ? 4 : 0) + 2;
        }
    }
}

/*
Handle one frame of moon movement.
*/
void ActMoon(word index)
{
    Actor *act = actors + index;

    act->data3 = !act->data3;

    if (act->data3 == 0) {
        act->data2++;

        if (act->x < playerX) {
            act->frame = (act->data2 % 2) + 2;
        } else {
            act->frame = act->data2 % 2;
        }
    }
}

/*
Handle one frame of heart plant movement.
*/
void ActHeartPlant(word index)
{
    Actor *act = actors + index;

    if (act->data1 == 0 && act->y > playerY && act->x == playerX) {
        act->data1 = 1;
    }

    if (act->data1 == 1) {
        act->data2++;

        if (act->data2 == 2) {
            act->data2 = 0;
            act->frame++;

            if (act->frame == 3) {
                act->data1 = 0;
                act->frame = 0;
            }

            if (act->frame == 1) {
                act->x--;
                StartSound(SND_PLANT_MOUTH_OPEN);
            }

            if (act->frame == 2) {
                act->x++;
            }
        }
    }
}

/*
Handle one frame of idle bomb movement.
*/
void ActBombIdle(word index)
{
    Actor *act = actors + index;

    if (act->data1 == 2) {
        NewExplosion(act->x - 2, act->y);
        act->dead = true;

    } else {
        if (act->data1 != 0) act->data1++;

        if (act->data1 == 0 && IsNearExplosion(SPR_BOMB_IDLE, 0, act->x, act->y)) {
            act->data1 = 1;
        }
    }
}

/*
Set map tile at x,y to `value`.
*/
void SetMapTile(word value, word x, word y)
{
    *(mapData.w + x + (y << mapYPower)) = value;
}

/*
Handle one frame of mystery wall movement.
*/
void ActMysteryWall(word index)
{
    Actor *act = actors + index;

    if (mysteryWallTime != 0) {
        act->data1 = 1;
        act->forceactive = true;
    }

    if (act->data1 == 0) return;

    if (act->data1 % 2 != 0) {
        SetMapTile(TILE_MYSTERY_BLOCK_NW, act->x,     act->y - 1);
        SetMapTile(TILE_MYSTERY_BLOCK_NE, act->x + 1, act->y - 1);
        SetMapTile(TILE_MYSTERY_BLOCK_SW, act->x,     act->y);
        SetMapTile(TILE_MYSTERY_BLOCK_SE, act->x + 1, act->y);
    }

    if (TestSpriteMove(DIR4_NORTH, act->sprite, 0, act->x, act->y - 1) != MOVE_FREE) {
        if (act->data1 % 2 == 0) {
            SetMapTile(TILE_MYSTERY_BLOCK_SW, act->x,     act->y - 1);
            SetMapTile(TILE_MYSTERY_BLOCK_SE, act->x + 1, act->y - 1);
        }

        act->dead = true;

    } else {
        if (act->data1 % 2 == 0) {
            NewDecoration(SPR_SPARKLE_SHORT, 4, act->x - 1, act->y - 1, DIR8_STATIONARY, 1);
        }

        act->data1++;
        act->y--;
    }
}

/*
Handle one frame of baby ghost movement.
*/
void ActBabyGhost(word index)
{
    Actor *act = actors + index;

    if (act->data4 != 0) {
        act->data4--;

    } else if (act->data1 == DIR2_SOUTH) {
        if (TestSpriteMove(DIR4_SOUTH, SPR_BABY_GHOST, 0, act->x, act->y + 1) != MOVE_FREE) {
            act->weighted = false;
            act->data1 = DIR2_NORTH;
            act->data4 = 3;
            act->data2 = 4;
            act->frame = 1;
            act->data3 = 1;

            if (IsSpriteVisible(SPR_BABY_GHOST, 0, act->x, act->y)) {
                StartSound(SND_BABY_GHOST_LAND);
            }

        } else if (act->data5 == 0) {
            act->frame = 1;
            if (act->data3 == 0) {
                act->data4++;
            }

        } else {
            act->data5--;
        }

    } else if (act->data1 == DIR2_NORTH) {
        act->y--;
        act->frame = 0;

        if (act->data2 == 4 && IsSpriteVisible(SPR_BABY_GHOST, 0, act->x, act->y)) {
            StartSound(SND_BABY_GHOST_JUMP);
        }

        act->data2--;
        if (act->data2 == 0) {
            act->data1 = DIR2_SOUTH;
            act->data5 = 3;
            act->weighted = true;
        }
    }
}

/*
Handle one frame of projectile movement.

Projectiles use a unique DIRP_* system that is incompatible with DIR8_*.
*/
void ActProjectile(word index)
{
    Actor *act = actors + index;

    if (!IsSpriteVisible(SPR_PROJECTILE, 0, act->x, act->y)) {
        act->dead = true;
        return;
    }

    if (act->data1 == 0) {
        act->data1 = 1;
        StartSound(SND_PROJECTILE_LAUNCH);
    }

    act->frame = !act->frame;

    switch (act->data5) {
    case DIRP_WEST:
        act->x--;
        break;

    case DIRP_SOUTHWEST:
        act->x--;
        act->y++;
        break;

    case DIRP_SOUTH:
        act->y++;
        break;

    case DIRP_SOUTHEAST:
        act->x++;
        act->y++;
        break;

    case DIRP_EAST:
        act->x++;
        break;
    }
}

/*
Handle one frame of roamer slug movement.
*/
void ActRoamerSlug(word index)
{
    Actor *act = actors + index;

    if (act->data5 == 0) {
        switch (act->data1) {
        case DIR4_NORTH:
            if (TestSpriteMove(DIR4_NORTH, SPR_ROAMER_SLUG, 0, act->x, act->y - 1) != MOVE_FREE) {
                act->data5 = 1;
            } else {
                act->y--;
            }
            act->data3 = 0;
            break;

        case DIR4_SOUTH:
            if (TestSpriteMove(DIR4_SOUTH, SPR_ROAMER_SLUG, 0, act->x, act->y + 1) != MOVE_FREE) {
                act->data5 = 1;
            } else {
                act->y++;
            }
            act->data3 = 4;
            break;

        case DIR4_WEST:
            if (TestSpriteMove(DIR4_WEST, SPR_ROAMER_SLUG, 0, act->x - 1, act->y) != MOVE_FREE) {
                act->data5 = 1;
            } else {
                act->x--;
            }
            act->data3 = 6;
            break;

        case DIR4_EAST:
            if (TestSpriteMove(DIR4_EAST, SPR_ROAMER_SLUG, 0, act->x + 1, act->y) != MOVE_FREE) {
                act->data5 = 1;
            } else {
                act->x++;
            }
            act->data3 = 2;
            break;
        }

    } else {
        word newdir = GameRand() % 4;

        if (
            newdir == DIR4_NORTH &&
            TestSpriteMove(DIR4_NORTH, SPR_ROAMER_SLUG, 0, act->x, act->y - 1) == MOVE_FREE
        ) {
            act->data5 = 0;
            act->data1 = DIR4_NORTH;
        }

        if (
            newdir == DIR4_SOUTH &&
            TestSpriteMove(DIR4_SOUTH, SPR_ROAMER_SLUG, 0, act->x, act->y + 1) == MOVE_FREE
        ) {
            act->data5 = 0;
            act->data1 = DIR4_SOUTH;
        }

        if (
            newdir == DIR4_WEST &&
            TestSpriteMove(DIR4_WEST, SPR_ROAMER_SLUG, 0, act->x - 1, act->y) == MOVE_FREE
        ) {
            act->data5 = 0;
            act->data1 = DIR4_WEST;
        }

        if (
            newdir == DIR4_EAST &&
            TestSpriteMove(DIR4_EAST, SPR_ROAMER_SLUG, 0, act->x + 1, act->y) == MOVE_FREE
        ) {
            act->data5 = 0;
            act->data1 = DIR4_EAST;
        }
    }

    act->data4 = !act->data4;
    act->frame = act->data3 + act->data4;
}

/*
Pipe corner actors do not perform any meaningful action.
*/
void ActPipeCorner(word index)
{
    nextDrawMode = DRAWMODE_HIDDEN;
#pragma warn -par
}
#pragma warn .par

/*
Handle one frame of baby ghost egg movement.
*/
void ActBabyGhostEgg(word index)
{
    Actor *act = actors + index;

    if (act->data2 != 0) {
        act->frame = 2;
    } else if (GameRand() % 70 == 0 && act->data3 == 0) {
        act->data3 = 2;
    } else {
        act->frame = 0;
    }

    if (act->data3 != 0) {
        act->data3--;
        act->frame = 1;
    }

    if (
        act->data5 == 0 && act->data1 == 0 &&
        act->y <= playerY && act->x - 6 < playerX && act->x + 4 > playerX
    ) {
        act->data1 = 1;
        act->data2 = 20;
        StartSound(SND_BGHOST_EGG_CRACK);
    }

    if (act->data2 > 1) {
        act->data2--;
    } else if (act->data2 == 1) {
        act->dead = true;
        nextDrawMode = DRAWMODE_HIDDEN;

        NewActor(ACT_BABY_GHOST, act->x, act->y);
        NewDecoration(SPR_BGHOST_EGG_SHARD_1, 1, act->x,     act->y - 1, DIR8_NORTHWEST, 5);
        NewDecoration(SPR_BGHOST_EGG_SHARD_2, 1, act->x + 1, act->y - 1, DIR8_NORTHEAST, 5);
        NewDecoration(SPR_BGHOST_EGG_SHARD_3, 1, act->x,     act->y,     DIR8_EAST,      5);
        NewDecoration(SPR_BGHOST_EGG_SHARD_4, 1, act->x + 1, act->y,     DIR8_WEST,      5);
        StartSound(SND_BGHOST_EGG_HATCH);
    }
}

/*
Handle one frame of sharp robot movement.
*/
void ActSharpRobot(word index)
{
    Actor *act = actors + index;

    act->data3 = !act->data3;
    if (act->data3 == 0) return;

    if (act->data4 != 0) {
        act->data4--;

    } else if (act->data2 == DIR2_EAST) {
        if (
            TestSpriteMove(DIR4_EAST, SPR_SHARP_ROBOT_CEIL, 0, act->x + 1, act->y) != MOVE_FREE ||
            TestSpriteMove(DIR4_EAST, SPR_SHARP_ROBOT_CEIL, 0, act->x + 1, act->y - 1) == MOVE_FREE
        ) {
            act->data4 = 4;
            act->data2 = DIR2_WEST;
        } else {
            act->x++;
        }

    } else {
        if (
            TestSpriteMove(DIR4_WEST, SPR_SHARP_ROBOT_CEIL, 0, act->x - 1, act->y) != MOVE_FREE ||
            TestSpriteMove(DIR4_WEST, SPR_SHARP_ROBOT_CEIL, 0, act->x - 1, act->y - 1) == MOVE_FREE
        ) {
            act->data4 = 4;
            act->data2 = DIR2_EAST;
        } else {
            act->x--;
        }
    }

    act->frame = !act->frame;
}

/*
Handle one frame of clam plant movement.
*/
void ActClamPlant(word index)
{
    Actor *act = actors + index;

    nextDrawMode = act->data5;

    if (act->data2 == 1) {
        act->frame++;
        if (act->frame == 1) {
            StartSound(SND_PLANT_MOUTH_OPEN);
        }

        if (act->frame == 4) {
            act->data2 = 2;
        }

    } else if (act->data2 == 2) {
        act->frame--;
        if (act->frame == 1) {
            act->data2 = 0;
            act->data1 = 1;
        }

    } else {
        if (act->data1 < 16) {
            act->data1++;
        } else {
            act->data1 = 0;
        }

        if (act->data1 == 0) {
            act->data2 = 1;
        } else {
            act->frame = 0;
        }
    }
}

/*
Handle one frame of parachute ball movement.

NOTE: This uses a direction system that is incompatible with DIR2_*.
*/
void ActParachuteBall(word index)
{
    Actor *act = actors + index;

    if (act->fallspeed != 0) {
        act->data1 = 0;
        act->data2 = 20;

        if (act->fallspeed < 2) {
            act->frame = 1;  /* no purpose */
        } else if (act->fallspeed >= 2 && act->fallspeed <= 4) {
            DrawSprite(SPR_PARACHUTE_BALL, 8, act->x, act->y - 2, DRAWMODE_NORMAL);
        } else {
            act->y--;
            DrawSprite(SPR_PARACHUTE_BALL, 9, act->x, act->y - 2, DRAWMODE_NORMAL);
        }

        act->frame = 10;

        return;
    }

    if (act->data1 == 0) {
        static byte idleframes[] = {
            2, 2, 2, 0, 3, 3, 3, 0, 0, 2, 2, 0, 0, 1, 1, 0, 1, 3, 3, 3, 0, 1, 1, 0, 1, 1, 1
        };
        act->data2++;
        act->frame = idleframes[act->data2];

        if (act->data2 == 26) {
            act->data2 = 0;

            if (act->y == playerY || GameRand() % 2 == 0) {
                if (act->x >= playerX + 2) {
                    act->data1 = 1;  /* W */
                    act->data2 = 0;
                    act->frame = 2;
                    act->data3 = 6;
                } else if (act->x + 2 <= playerX) {
                    act->data1 = 2;  /* E */
                    act->data2 = 0;
                    act->frame = 3;
                    act->data3 = 6;
                }
            }
        }
    }

    if (act->data3 != 0) {
        act->data3--;

    } else if (act->data1 == 1) {  /* W */
        act->x--;
        AdjustActorMove(index, DIR4_WEST);
        if (act->private1 == 0) {
            act->data1 = 0;
            act->data2 = 0;
            act->frame = 0;
        } else {
            static byte frames[] = {7, 6, 5, 4};
            act->frame = frames[act->data2 % 4];
            act->data2++;
            if (act->data2 == 16) {
                act->data1 = 0;
                act->data2 = 0;
            }
        }

    } else if (act->data1 == 2) {  /* E */
        act->x++;
        AdjustActorMove(index, DIR4_EAST);
        if (act->private2 == 0) {
            act->data1 = 0;
            act->data2 = 0;
            act->frame = 0;
        } else {
            static byte frames[] = {4, 5, 6, 7};
            act->frame = frames[act->data2 % 4];
            act->data2++;
            if (act->data2 == 12) {
                act->data1 = 0;
                act->data2 = 0;
            }
        }
    }
}

/*
Handle one frame of beam robot movement.

NOTE: This uses a direction system that is incompatible with DIR2_*.
*/
void ActBeamRobot(word index)
{
    static word beamframe = 0;
    Actor *act = actors + index;
    int i;

    nextDrawMode = DRAWMODE_HIDDEN;

    if (act->data2 != 0) {
        for (i = 0; act->data2 > i; i += 4) {
            NewExplosion(act->x, act->y - i);
            NewActor(ACT_STAR_FLOAT, act->x, act->y - i);
        }

        act->dead = true;

        return;
    }

    act->data5 = !act->data5;
    act->data4++;

    if (act->data1 != 0) {  /* != E */
        if (act->data4 % 2 != 0) {
            act->x--;
        }

        AdjustActorMove(index, DIR4_WEST);
        if (act->private1 == 0) {
            act->data1 = 0;  /* E */
        }
    } else {
        if (act->data4 % 2 != 0) {
            act->x++;
        }

        AdjustActorMove(index, DIR4_EAST);
        if (act->private2 == 0) {
            act->data1 = 1;  /* W */
        }
    }

    DrawSprite(SPR_BEAM_ROBOT, act->data5, act->x, act->y, DRAWMODE_NORMAL);
    if (IsTouchingPlayer(SPR_BEAM_ROBOT, 0, act->x, act->y)) {
        HurtPlayer();
    }

    beamframe++;

    for (i = 2; i < 21; i++) {
        if (TestSpriteMove(DIR4_NORTH, SPR_BEAM_ROBOT, 2, act->x + 1, act->y - i) != MOVE_FREE) break;

        DrawSprite(SPR_BEAM_ROBOT, (beamframe % 4) + 4, act->x + 1, act->y - i, DRAWMODE_NORMAL);

        if (IsTouchingPlayer(SPR_BEAM_ROBOT, 4, act->x + 1, act->y - i)) {
            HurtPlayer();
        }
    }
    /* value left in i is used below! */

    DrawSprite(SPR_BEAM_ROBOT, act->data5 + 2, act->x + 1, (act->y - i) + 1, DRAWMODE_NORMAL);

    if (IsTouchingPlayer(SPR_BEAM_ROBOT, 0, act->x, act->y + 1)) {
        HurtPlayer();
    }

    if (IsNearExplosion(act->sprite, act->frame, act->x, act->y)) {
        act->data2 = i;
    }
}

/*
Handle one frame of splitting platform movement.
*/
void ActSplittingPlatform(word index)
{
    Actor *act = actors + index;

    act->private1++;

    if (act->data1 == 0) {
        act->data1 = 1;
        SetMapTileRepeat(TILE_BLUE_PLATFORM, 4, act->x, act->y - 1);

    } else if (act->data1 == 1 && act->y - 2 == playerY) {
        if (
            (act->x <= playerX && act->x + 3 >= playerX) ||
            (act->x <= playerX + 2 && act->x + 3 >= playerX + 2)
        ) {
            act->data1 = 2;
            act->data2 = 0;
            ClearPlayerDizzy();
        }

    } else if (act->data1 == 2) {
        if (act->private1 % 2 != 0) {
            act->data2++;
        }

        if (act->data2 == 5) {
            SetMapTileRepeat(TILE_EMPTY, 4, act->x, act->y - 1);
        }

        if (act->data2 >= 5 && act->data2 < 8) {
            nextDrawMode = DRAWMODE_HIDDEN;
            DrawSprite(SPR_SPLITTING_PLATFORM, 1, act->x - (act->data2 - 5), act->y, DRAWMODE_NORMAL);
            DrawSprite(SPR_SPLITTING_PLATFORM, 2, (act->x + act->data2) - 3, act->y, DRAWMODE_NORMAL);
        }

        if (act->data2 == 7) {
            act->data1 = 3;
            act->data2 = 0;
        }
    }

    if (act->data1 == 3) {
        nextDrawMode = DRAWMODE_HIDDEN;
        DrawSprite(SPR_SPLITTING_PLATFORM, 1, (act->x + act->data2) - 2, act->y, DRAWMODE_NORMAL);
        DrawSprite(SPR_SPLITTING_PLATFORM, 2, (act->x + 4) - act->data2, act->y, DRAWMODE_NORMAL);

        if (act->private1 % 2 != 0) {
            act->data2++;
        }

        if (act->data2 == 3) {
            nextDrawMode = DRAWMODE_NORMAL;
            SetMapTileRepeat(TILE_EMPTY, 4, act->x, act->y - 1);
            act->data1 = 0;
        }
    }
}

/*
Handle one frame of spark movment.

NOTE: This uses a direction system that is incompatible with DIR4_*.
*/
void ActSpark(word index)
{
    Actor *act = actors + index;

    act->data5++;
    act->frame = !act->frame;

    if (act->data5 % 2 != 0) return;

    if (act->data1 == 0) {  /* W */
        act->x--;

        if (TestSpriteMove(DIR4_WEST, act->sprite, 0, act->x - 1, act->y) != MOVE_FREE) {
            act->data1 = 2;  /* N */
        } else if (TestSpriteMove(DIR4_SOUTH, act->sprite, 0, act->x, act->y + 1) == MOVE_FREE) {
            act->data1 = 3;  /* S */
        }

    } else if (act->data1 == 1) {  /* E */
        act->x++;

        if (TestSpriteMove(DIR4_EAST, act->sprite, 0, act->x + 1, act->y) != MOVE_FREE) {
            act->data1 = 3;  /* S */
        } else if (TestSpriteMove(DIR4_NORTH, act->sprite, 0, act->x, act->y - 1) == MOVE_FREE) {
            act->data1 = 2;  /* N */
        }

    } else if (act->data1 == 2) {  /* N */
        act->y--;

        if (TestSpriteMove(DIR4_NORTH, act->sprite, 0, act->x, act->y - 1) != MOVE_FREE) {
            act->data1 = 1;  /* E */
        } else if (TestSpriteMove(DIR4_WEST, act->sprite, 0, act->x - 1, act->y) == MOVE_FREE) {
            act->data1 = 0;  /* W */
        }

    } else if (act->data1 == 3) {  /* S */
        act->y++;

        if (TestSpriteMove(DIR4_SOUTH, act->sprite, 0, act->x, act->y + 1) != MOVE_FREE) {
            act->data1 = 0;  /* W */
        } else if (TestSpriteMove(DIR4_EAST, act->sprite, 0, act->x + 1, act->y) == MOVE_FREE) {
            act->data1 = 1;  /* E */
        }
    }
}

/*
Handle one frame of eye plant movement.
*/
void ActEyePlant(word index)
{
    Actor *act = actors + index;

    nextDrawMode = act->data5;

    act->data2 = random(40);
    if (act->data2 > 37) {
        act->data2 = 3;
    } else {
        act->data2 = 0;
    }

    if (act->x - 2 > playerX) {
        act->frame = act->data2;
    } else if (act->x + 1 < playerX) {
        act->frame = act->data2 + 2;
    } else {
        act->frame = act->data2 + 1;
    }
}

/*
Handle one frame of red jumper movement.
*/
void ActRedJumper(word index)
{
    static int jumptable[] = {
        /* even elements = y change; odd elements = actor frame */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, -2, 2,
        -2, 2, -2, 2, -2, 2, -1, 2, -1, 2, -1, 2, 0, 2, 0, 2, 1, 1, 1, 1, 1, 1
    };
    Actor *act = actors + index;

#ifdef HAS_ACT_RED_JUMPER
    int yjump;

    if (act->data2 < 5) {
        if (act->x > playerX) {
            act->data1 = 0;
        } else {
            act->data1 = 3;
        }

    } else if (act->data2 == 14 && IsSpriteVisible(SPR_RED_JUMPER, 0, act->x, act->y)) {
        StartSound(SND_RED_JUMPER_JUMP);

    } else if (act->data2 > 16 && act->data2 < 39) {
        if (
            act->data1 == 0 &&
            TestSpriteMove(DIR4_WEST, SPR_RED_JUMPER, 0, act->x - 1, act->y) == MOVE_FREE
        ) {
            act->x--;
        } else if (
            act->data1 == 3 &&
            TestSpriteMove(DIR4_EAST, SPR_RED_JUMPER, 0, act->x + 1, act->y) == MOVE_FREE
        ) {
            act->x++;
        }
    }

    if (act->data2 > 39) {
        if (
            TestSpriteMove(DIR4_SOUTH, SPR_RED_JUMPER, 0, act->x, act->y + 1) == MOVE_FREE &&
            TestSpriteMove(DIR4_SOUTH, SPR_RED_JUMPER, 0, act->x, ++act->y + 1) == MOVE_FREE
        ) {
            act->y++;
            act->frame = act->data1 + jumptable[act->data2 + 1];

            if (act->data2 < 39) {  /* never occurs */
                act->data2 += 2;
            }
        } else {
            act->data2 = 0;

            if (IsSpriteVisible(SPR_RED_JUMPER, 0, act->x, act->y)) {
                StartSound(SND_RED_JUMPER_LAND);
            }
        }

        return;
    }

    yjump = jumptable[act->data2];

    if (yjump == -1) {
        if (TestSpriteMove(DIR4_NORTH, SPR_RED_JUMPER, 0, act->x, act->y - 1) == MOVE_FREE) {
            act->y--;
        } else {
            act->data2 = 34;
        }
    }

    if (yjump == -2) {
        if (TestSpriteMove(DIR4_NORTH, SPR_RED_JUMPER, 0, act->x, act->y - 1) == MOVE_FREE) {
            act->y--;
        } else {
            act->data2 = 34;
        }

        if (TestSpriteMove(DIR4_NORTH, SPR_RED_JUMPER, 0, act->x, act->y - 1) == MOVE_FREE) {
            act->y--;
        } else {
            act->data2 = 34;
        }
    }

    if (yjump == 1) {
        if (TestSpriteMove(DIR4_SOUTH, SPR_RED_JUMPER, 0, act->x, act->y + 1) == MOVE_FREE) {
            act->y++;
        }
    }

    if (yjump == 2) {
        if (
            TestSpriteMove(DIR4_SOUTH, SPR_RED_JUMPER, 0, act->x, act->y - 1) == MOVE_FREE &&
            TestSpriteMove(DIR4_SOUTH, SPR_RED_JUMPER, 0, act->x, ++act->y - 1) == MOVE_FREE
        ) {
            act->y++;
        } else {
            act->data2 = 0;

            return;
        }
    }

    act->frame = act->data1 + jumptable[act->data2 + 1];

    if (act->data2 < 39) act->data2 += 2;

#else
#   pragma warn -aus
#endif  /* ACT_RED_JUMPER */
}
#pragma warn .aus

/*
Handle one frame of boss movement.
*/
void ActBoss(word index)
{
    static int yjump[] = {2, 2, 1, 0, -1, -2, -2, -2, -2, -1, 0, 1, 2, 2};
    Actor *act = actors + index;

#ifdef HAS_ACT_BOSS
    nextDrawMode = DRAWMODE_HIDDEN;

    if (!sawBossBubble) {
        sawBossBubble = true;
        NewActor(ACT_SPEECH_WHOA, playerX - 1, playerY - 5);
        StopMusic();
        StartGameMusic(MUSIC_BOSS);
    }

    if (act->private2 > 0) {
        act->private2--;
        if (act->private2 < 40) {
            act->y--;
        }

        act->weighted = false;
        act->fallspeed = 0;

        if (
            act->private2 == 1 ||
#ifndef HARDER_BOSS
            act->y == 0 ||
#endif  /* HARDER_BOSS */
            (!IsSpriteVisible(SPR_BOSS, 0, act->x, act->y) && act->private2 < 30)
        ) {
#ifdef HARDER_BOSS
            winGame = true;
#else
            winLevel = true;
#endif  /* HARDER_BOSS */
            AddScore(100000L);
        }

        if (act->private2 < 40 && act->private2 != 0 && act->private2 % 3 == 0) {
            NewDecoration(SPR_SMOKE, 6, act->x,     act->y, DIR8_NORTHWEST, 1);
            NewDecoration(SPR_SMOKE, 6, act->x + 3, act->y, DIR8_NORTHEAST, 1);
            StartSound(SND_BOSS_MOVE);
        }

        if (act->private2 % 2 != 0) {
            DrawSprite(SPR_BOSS, 0, act->x, act->y,     DRAWMODE_WHITE);
            DrawSprite(SPR_BOSS, 5, act->x, act->y - 4, DRAWMODE_WHITE);

            if (act->private2 > 39) {
                NewDecoration(SPR_SMOKE, 6, act->x,     act->y, DIR8_NORTHWEST, 1);
                NewDecoration(SPR_SMOKE, 6, act->x + 3, act->y, DIR8_NORTHEAST, 1);
            }

        } else {
            DrawSprite(SPR_BOSS, 0, act->x, act->y,     DRAWMODE_NORMAL);
            DrawSprite(SPR_BOSS, 5, act->x, act->y - 4, DRAWMODE_NORMAL);
        }

        return;
    }

    if (
#ifdef HARDER_BOSS
        act->data5 == 18
#else
        act->data5 == 12
#endif  /* HARDER_BOSS */
    ) {
        if (TestSpriteMove(DIR4_SOUTH, SPR_BOSS, 0, act->x, act->y + 1) == MOVE_FREE) {
            act->y++;
            if (act->y % 2 != 0) {
                DrawSprite(SPR_BOSS, 0, act->x, act->y,     DRAWMODE_WHITE);
                DrawSprite(SPR_BOSS, 5, act->x, act->y - 4, DRAWMODE_WHITE);
            } else {
                DrawSprite(SPR_BOSS, 0, act->x, act->y,     DRAWMODE_NORMAL);
                DrawSprite(SPR_BOSS, 5, act->x, act->y - 4, DRAWMODE_NORMAL);
            }
        }

        if (TestSpriteMove(DIR4_SOUTH, SPR_BOSS, 0, act->x, act->y + 1) != MOVE_FREE) {
            act->private2 = 80;
        }

        return;
    }

    if (act->private1 != 0) {
        word frame = act->data5 > 3 ? 5 : 1;

        act->private1--;
        if (act->private1 % 2 != 0) {
            DrawSprite(SPR_BOSS, 0,     act->x, act->y,     DRAWMODE_WHITE);
            DrawSprite(SPR_BOSS, frame, act->x, act->y - 4, DRAWMODE_WHITE);
        } else {
            DrawSprite(SPR_BOSS, 0,     act->x, act->y,     DRAWMODE_NORMAL);
            DrawSprite(SPR_BOSS, frame, act->x, act->y - 4, DRAWMODE_NORMAL);
        }
    }

    if (act->data1 == 0) {
        act->y -= 2;

        act->data2++;
        if (act->data2 == 6) {
            act->data1 = 1;
        }

    } else if (act->data1 == 1) {
        if (act->data2 != 0) {
            act->data2--;
        } else {
            act->data1 = 2;
        }

    } else if (act->data1 == 2) {
        if (
            TestSpriteMove(DIR4_SOUTH, SPR_BOSS, 0, act->x, act->y + yjump[act->data3 % 14]) != MOVE_FREE
            && yjump[act->data3 % 14] == 2
        ) {
            act->y -= 2;
        }

        if (
            TestSpriteMove(DIR4_SOUTH, SPR_BOSS, 0, act->x, act->y + yjump[act->data3 % 14]) != MOVE_FREE
            && yjump[act->data3 % 14] == 1
        ) {
            act->y--;
        } else {
            act->y += yjump[act->data3 % 14];
        }

        act->data3++;
        if (act->data3 % 14 == 1) {
            StartSound(SND_BOSS_MOVE);
        }

        act->data2++;
        if (act->data2 > 30 && act->data2 < 201) {
#ifdef HARDER_BOSS
            if (act->data2 > 100 && act->data2 < 104 && act->data2 % 2 != 0) {
                NewSpawner(ACT_PARACHUTE_BALL, act->x + 2, act->y - 5);
                StartSound(SND_BOSS_LAUNCH);
            }
#endif  /* HARDER_BOSS */

            if (act->data4 != 0) {
                if (TestSpriteMove(DIR4_EAST, SPR_BOSS, 0, act->x + 1, act->y) != MOVE_FREE) {
                    act->data4 = 0;
                    StartSound(SND_OBJECT_HIT);
                    NewDecoration(SPR_SMOKE, 6, act->x + 3, act->y - 2, DIR8_SOUTH, 1);
                } else {
                    act->x++;
                }

            } else if (TestSpriteMove(DIR4_WEST, SPR_BOSS, 0, act->x - 1, act->y) == MOVE_FREE) {
                act->x--;

            } else {
                act->data4 = 1;
                StartSound(SND_OBJECT_HIT);
                NewDecoration(SPR_SMOKE, 6, act->x, act->y - 2, DIR8_SOUTH, 1);
            }

        } else if (act->data2 > 199) {
            act->data1 = 3;
            act->data2 = 0;
            act->data3 = 8;
        }

    } else if (act->data1 == 3) {
        act->data2++;

        if (act->data3 < 6) {
            act->data3++;
            act->y -= 2;

        } else if (act->data2 < 102) {
            act->weighted = true;

            if (TestSpriteMove(DIR4_SOUTH, SPR_BOSS, 0, act->x, act->y + 1) != MOVE_FREE) {
                act->data3 = 0;
                act->weighted = false;
                act->fallspeed = 0;
                StartSound(SND_SMASH);
                NewDecoration(SPR_SMOKE, 6, act->x,     act->y, DIR8_NORTHWEST, 1);
                NewDecoration(SPR_SMOKE, 6, act->x + 3, act->y, DIR8_NORTHEAST, 1);

            } else if (act->x + 1 > playerX) {
                if (TestSpriteMove(DIR4_WEST, SPR_BOSS, 0, act->x - 1, act->y) == MOVE_FREE) {
                    act->x--;
                }

            } else if (
                act->x + 3 < playerX &&
                TestSpriteMove(DIR4_EAST, SPR_BOSS, 0, act->x + 1, act->y) == MOVE_FREE
            ) {
                act->x++;
            }

        } else if (
            TestSpriteMove(DIR4_SOUTH, SPR_BOSS, 0, act->x, act->y + 1) != MOVE_FREE ||
            TestSpriteMove(DIR4_SOUTH, SPR_BOSS, 0, act->x, act->y) != MOVE_FREE
        ) {
            act->data1 = 4;
            act->data2 = 0;
            act->data3 = 0;
            act->weighted = false;
            act->fallspeed = 0;
            StartSound(SND_OBJECT_HIT);
            NewDecoration(SPR_SMOKE, 6, act->x,     act->y, DIR8_NORTHWEST, 1);
            NewDecoration(SPR_SMOKE, 6, act->x + 3, act->y, DIR8_NORTHEAST, 1);

        } else {
            act->y++;
        }

    } else if (act->data1 == 4) {
        act->weighted = false;
        act->fallspeed = 0;
        act->y--;

        act->data2++;
        if (act->data2 == 6) {
            act->data1 = 2;
            act->data3 = 0;
            act->data2 = 0;
        }
    }

    if (act->private1 == 0) {
        DrawSprite(SPR_BOSS, 0, act->x, act->y, 0);

        if (act->data5 < 4) {
            DrawSprite(SPR_BOSS, 1, act->x,     act->y - 4, DRAWMODE_NORMAL);
        } else if (act->x + 1 > playerX) {
            DrawSprite(SPR_BOSS, 2, act->x + 1, act->y - 4, DRAWMODE_NORMAL);
        } else if (act->x + 2 < playerX) {
            DrawSprite(SPR_BOSS, 4, act->x + 1, act->y - 4, DRAWMODE_NORMAL);
        } else {
            DrawSprite(SPR_BOSS, 3, act->x + 1, act->y - 4, DRAWMODE_NORMAL);
        }
    }

#else
#   pragma warn -aus
#endif  /* ACT_BOSS */
}
#pragma warn .aus

/*
Handle one frame of pipe end movement.
*/
void ActPipeEnd(word index)
{
    Actor *act = actors + index;

    if (act->data2 == 0) return;

    act->data1++;

    act->data3++;
    if (act->data3 % 2 != 0) {
        act->frame = 4;
    } else {
        act->frame = 0;
    }

    if (act->data1 == 4) act->data1 = 1;

    DrawSprite(SPR_PIPE_END, act->data1, act->x, act->y + 3, DRAWMODE_NORMAL);
}

/*
Return true if there is a close-enough surface above/below this suction walker.
*/
bbool CanSuctionWalkerFlip(word index, word dir)
{
    Actor *act = actors + index;
    word y;

    if (GameRand() % 2 == 0) return false;

    if (dir == DIR4_NORTH) {
        for (y = 0; y < 15; y++) {
            if (
                /* BUG: This should probably be testing north, not west. */
                TILE_BLOCK_WEST(GetMapTile(act->x,     (act->y - y) - 4)) &&
                TILE_BLOCK_WEST(GetMapTile(act->x + 2, (act->y - y) - 4))
            ) {
                return true;
            }
        }
    } else if (dir == DIR4_SOUTH) {
        for (y = 0; y < 15; y++) {
            if (
                TILE_BLOCK_SOUTH(GetMapTile(act->x,     act->y + y)) &&
                TILE_BLOCK_SOUTH(GetMapTile(act->x + 2, act->y + y))
            ) {
                return true;
            }
        }
    }

    return false;
}

/*
Handle one frame of suction walker movement.

NOTE: The _BLOCK_WEST checks should almost certainly be _BLOCK_NORTH instead.
*/
void ActSuctionWalker(word index)
{
    Actor *act = actors + index;
    word move, ledge;

    act->data4 = !act->data4;

    if (act->data1 == DIR2_WEST) {
        if (act->data2 == 0) {  /* facing west, floor */
            if (act->data4 != 0) {
                act->data3 = !act->data3;
                act->frame = act->data3;
            }

            move = TestSpriteMove(DIR4_WEST, SPR_SUCTION_WALKER, 0, act->x - 1, act->y);
            ledge = !TILE_BLOCK_SOUTH(GetMapTile(act->x - 1, act->y + 1));
            if (move != MOVE_FREE || ledge != 0 || GameRand() % 50 == 0) {
                if (CanSuctionWalkerFlip(index, DIR4_NORTH)) {
                    act->data2 = 2;
                    act->frame = 9;
                } else {
                    act->data1 = DIR2_EAST;
                    act->data2 = 0;
                }
            } else if (act->data4 != 0) {
                act->x--;
            }

        } else if (act->data2 == 1) {  /* facing west, ceiling */
            if (act->data4 != 0) {
                act->data3 = !act->data3;
                act->frame = act->data3 + 4;
            }

            move = TestSpriteMove(DIR4_WEST, SPR_SUCTION_WALKER, 0, act->x - 1, act->y);
            ledge = !TILE_BLOCK_WEST(GetMapTile(act->x - 1, act->y - 4));
            if (move == MOVE_SLOPED && act->data4 != 0) {
                act->y--;
                act->x--;
            } else if (move != MOVE_FREE || ledge != 0 || GameRand() % 50 == 0) {
                if (CanSuctionWalkerFlip(index, DIR4_SOUTH)) {
                    act->data2 = 3;
                    act->frame = 9;
                } else {
                    act->data1 = DIR2_EAST;
                    act->data2 = 1;
                }
            } else if (act->data4 != 0) {
                act->x--;
            }

        } else if (act->data2 == 2) {  /* facing west, rising */
            if (TestSpriteMove(DIR4_NORTH, SPR_SUCTION_WALKER, 0, act->x, act->y - 1) != MOVE_FREE) {
                act->data2 = 1;
            } else {
                act->y--;
            }

            if (TestSpriteMove(DIR4_NORTH, SPR_SUCTION_WALKER, 0, act->x, act->y - 1) != MOVE_FREE) {
                act->data2 = 1;
            } else {
                act->y--;
            }

        } else if (act->data2 == 3) {  /* facing west, falling */
            if (TestSpriteMove(DIR4_SOUTH, SPR_SUCTION_WALKER, 0, act->x, act->y + 1) != MOVE_FREE) {
                act->data2 = 0;
            } else {
                act->y++;
            }

            if (TestSpriteMove(DIR4_SOUTH, SPR_SUCTION_WALKER, 0, act->x, act->y + 1) != MOVE_FREE) {
                act->data2 = 0;
            } else {
                act->y++;
            }
        }

    } else if (act->data1 == DIR2_EAST) {
        if (act->data2 == 0) {  /* facing east, floor */
            if (act->data4 != 0) {
                act->data3 = !act->data3;
                act->frame = act->data3 + 2;
            }

            move = TestSpriteMove(DIR4_EAST, SPR_SUCTION_WALKER, 0, act->x + 1, act->y);
            ledge = !TILE_BLOCK_SOUTH(GetMapTile(act->x + 3, act->y + 1));
            if (move != MOVE_FREE || ledge != 0 || GameRand() % 50 == 0) {
                if (CanSuctionWalkerFlip(index, DIR4_NORTH)) {
                    act->data2 = 2;
                    act->frame = 8;
                } else {
                    act->data1 = DIR2_WEST;
                    act->data2 = 0;
                }
            } else if (act->data4 != 0) {
                act->x++;
            }

        } else if (act->data2 == 1) {  /* facing east, ceiling */
            if (act->data4 != 0) {
                act->data3 = !act->data3;
                act->frame = act->data3 + 6;
            }

            move = TestSpriteMove(DIR4_EAST, SPR_SUCTION_WALKER, 0, act->x + 1, act->y);
            ledge = !TILE_BLOCK_WEST(GetMapTile(act->x + 3, act->y - 4));
            if (move != MOVE_FREE || ledge != 0 || GameRand() % 50 == 0) {
                if (CanSuctionWalkerFlip(index, DIR4_SOUTH)) {
                    act->data2 = 3;
                    act->frame = 8;
                } else {
                    act->data1 = DIR2_WEST;
                    act->data2 = 1;
                }
            } else if (act->data4 != 0) {
                act->x++;
            }

        } else if (act->data2 == 2) {  /* facing east, rising */
            if (TestSpriteMove(DIR4_NORTH, SPR_SUCTION_WALKER, 0, act->x, act->y - 1) != MOVE_FREE) {
                act->data2 = 1;
            } else {
                act->y--;
            }

            if (TestSpriteMove(DIR4_NORTH, SPR_SUCTION_WALKER, 0, act->x, act->y - 1) != MOVE_FREE) {
                act->data2 = 1;
            } else {
                act->y--;
            }

        } else if (act->data2 == 3) {  /* facing east, falling */
            if (TestSpriteMove(DIR4_SOUTH, SPR_SUCTION_WALKER, 0, act->x, act->y + 1) != MOVE_FREE) {
                act->data2 = 0;
            } else {
                act->y++;
            }

            if (TestSpriteMove(DIR4_SOUTH, SPR_SUCTION_WALKER, 0, act->x, act->y + 1) != MOVE_FREE) {
                act->data2 = 0;
            } else {
                act->y++;
            }
        }
    }
}

/*
Handle one frame of transporter movement.
*/
void ActTransporter(word index)
{
    Actor *act = actors + index;

    nextDrawMode = DRAWMODE_HIDDEN;

    if (transporterTimeLeft != 0 && random(2U) != 0) {
        DrawSprite(SPR_TRANSPORTER_107, 0, act->x, act->y, DRAWMODE_WHITE);
    } else {
        DrawSprite(SPR_TRANSPORTER_107, 0, act->x, act->y, DRAWMODE_NORMAL);
    }

    if (GameRand() % 2 != 0) {
        DrawSprite(SPR_TRANSPORTER_107, random(2U) + 1, act->x, act->y, DRAWMODE_NORMAL);
    }

    if (transporterTimeLeft == 15) {
        NewDecoration(SPR_SPARKLE_SHORT, 4, playerX - 1, playerY,     DIR8_STATIONARY, 1);
        NewDecoration(SPR_SPARKLE_SHORT, 4, playerX + 1, playerY,     DIR8_STATIONARY, 1);
        NewDecoration(SPR_SPARKLE_SHORT, 4, playerX - 1, playerY - 3, DIR8_STATIONARY, 2);
        NewDecoration(SPR_SPARKLE_SHORT, 4, playerX,     playerY - 2, DIR8_STATIONARY, 3);
        NewDecoration(SPR_SPARKLE_SHORT, 4, playerX + 1, playerY - 3, DIR8_STATIONARY, 3);

        StartSound(SND_TRANSPORTER_ON);
    }

    if (transporterTimeLeft > 1) {
        transporterTimeLeft--;
    } else if (activeTransporter == 3) {
        winLevel = true;
    } else if (activeTransporter != 0 && act->data5 != activeTransporter && act->data5 != 3) {
        playerX = act->x + 1;
        playerY = act->y;

        if ((int) (playerX - 14) < 0) {
            scrollX = 0;
        } else if (playerX - 14 > mapWidth - SCROLLW) {
            scrollX = mapWidth - SCROLLW;
        } else {
            scrollX = playerX - 14;
        }

        if ((int) (playerY - 12) < 0) {
            scrollY = 0;
        } else if (playerY - 12 > mapHeight) {
            scrollY = mapHeight;
        } else {
            scrollY = playerY - 12;
        }

        activeTransporter = 0;
        transporterTimeLeft = 0;
        isPlayerRecoiling = false;

        if (!sawTransporterBubble) {
            sawTransporterBubble = true;
            NewActor(ACT_SPEECH_WHOA, playerX - 1, playerY - 5);
        }
    }
}

/*
Handle one frame of spitting wall plant movement.
*/
void ActSpittingWallPlant(word index)
{
    Actor *act = actors + index;

    act->data4++;

    if (act->data4 == 50) {
        act->data4 = 0;
        act->frame = 0;
    }

    if (act->data4 == 42) {
        act->frame = 1;
    }

    if (act->data4 == 45) {
        act->frame = 2;

        if (act->data5 == DIR4_WEST) {
            NewActor(ACT_PROJECTILE_W, act->x - 1, act->y - 1);
        } else {
            NewActor(ACT_PROJECTILE_E, act->x + 4, act->y - 1);
        }
    }
}

/*
Handle one frame of spitting turret movement.
*/
void ActSpittingTurret(word index)
{
    Actor *act = actors + index;

    act->data2--;
    if (act->data2 == 0) {
        act->data1++;
        act->data2 = 3;

        if (act->data1 != 3) {
            switch (++act->frame) {
            case 2:
                NewActor(ACT_PROJECTILE_W, act->x - 1, act->y - 1);
                break;

            case 5:
                NewActor(ACT_PROJECTILE_SW, act->x - 1, act->y + 1);
                break;

            case 8:
                NewActor(ACT_PROJECTILE_S, act->x + 1, act->y + 1);
                break;

            case 11:
                NewActor(ACT_PROJECTILE_SE, act->x + 5, act->y + 1);
                break;

            case 14:
                NewActor(ACT_PROJECTILE_E, act->x + 5, act->y - 1);
                break;
            }
        }
    }

    if (act->data1 == 0) {
        /* below: a bunch of bare `else` would've sufficed */
        if (act->y >= playerY - 2) {
            if (act->x + 1 > playerX) {
                act->frame = 0;  /* west */
                act->x = act->data3;
            } else if (act->x + 2 <= playerX) {
                act->frame = 12;  /* east */
                act->x = act->data3 + 1;
            }
        } else if (act->y < playerY - 2) {
            if (act->x - 2 > playerX) {
                act->frame = 3;  /* southwest */
                act->x = act->data3;
            } else if (act->x + 3 < playerX) {
                act->frame = 9;  /* southeast */
                act->x = act->data3 + 1;
            } else if (act->x - 2 < playerX && act->x + 3 >= playerX) {
                act->frame = 6;  /* south */
                act->x = act->data3 + 1;
            }

            /* less-or-equal in the previous `else if` would've avoided this */
            if (act->x - 2 == playerX) {
                act->frame = 6;  /* south */
                act->x = act->data3 + 1;
            }
        }
    }

    if (act->data1 == 3) {
        act->data2 = 27;
        act->data1 = 0;
    }

    if (act->frame > 14) {
        act->frame = 14;
    }
}

/*
Handle one frame of scooter movement.
*/
void ActScooter(word index)
{
    Actor *act = actors + index;

    act->frame++;
    act->frame &= 3;

    if (scooterMounted != 0) {
        act->x = playerX;
        act->y = playerY + 1;

    } else {
        act->data2++;
        if (act->data2 % 10 == 0) {
            if (TestSpriteMove(DIR4_SOUTH, SPR_SCOOTER, 0, act->x, act->y + 1) != MOVE_FREE) {
                act->y--;
            } else {
                act->y++;

                if (TestSpriteMove(DIR4_SOUTH, SPR_SCOOTER, 0, act->x, act->y + 1) != MOVE_FREE) {
                    act->y--;
                }
            }
        }
    }
}

/*
Handle one frame of red chomper movement.
*/
void ActRedChomper(word index)
{
    Actor *act = actors + index;

    act->data4 = !act->data4;

    if (GameRand() % 95 == 0) {
        act->data5 = 10;
    } else if (GameRand() % 100 == 0) {
        act->data5 = 11;
    }

    if (act->data5 < 11 && act->data5 != 0) {
        act->data5--;

        if (act->data5 > 8) {
            act->frame = 6;
        } else if (act->data5 == 8) {
            act->frame = 5;
        } else {
            act->data2 = !act->data2;
            act->frame = act->data2 + 6;
        }

        if (act->data5 == 0 && GameRand() % 2 != 0) {
            if (act->x >= playerX) {
                act->data1 = DIR2_WEST;
            } else {
                act->data1 = DIR2_EAST;
            }
        }

    } else if (act->data5 > 10) {
        if (act->data1 == DIR2_WEST) {
            static word searchframes[] = {8, 9, 10, 10, 9, 8};
            act->frame = searchframes[act->data5 - 11];
        } else {
            static word searchframes[] = {10, 9, 8, 8, 9, 10};
            act->frame = searchframes[act->data5 - 11];
        }

        act->data5++;
        if (act->data5 == 17) {
            act->data5 = 0;
        }

    } else {
        if (act->data1 == DIR2_WEST) {
            if (act->data4 != 0) {
                act->frame = !act->frame;
                act->x--;
                AdjustActorMove(index, DIR4_WEST);
                if (act->private1 == 0) {
                    act->data1 = DIR2_EAST;
                    act->frame = 4;
                }
            }
        } else {
            if (act->data4 != 0) {
                act->data3 = !act->data3;
                act->frame = act->data3 + 2;
                act->x++;
                AdjustActorMove(index, DIR4_EAST);
                if (act->private2 == 0) {
                    act->data1 = DIR2_WEST;
                    act->frame = 4;
                }
            }
        }
    }
}

/*
Handle one frame of force field movement.
*/
void ActForceField(word index)
{
    Actor *act = actors + index;

    act->data1 = 0;

    act->data4++;
    if (act->data4 == 3) {
        act->data4 = 0;
    }

    nextDrawMode = DRAWMODE_HIDDEN;

    if (!areForceFieldsActive) {
        act->dead = true;
        return;
    }

    if (act->data5 == 0) {
        for (;; act->data1++) {
            if (IsTouchingPlayer(act->sprite, 0, act->x, act->y - act->data1)) {
                HurtPlayer();
                break;
            }

            if (TILE_BLOCK_NORTH(GetMapTile(act->x, act->y - act->data1))) break;

            DrawSprite(act->sprite, act->data4, act->x, act->y - act->data1, DRAWMODE_NORMAL);
        }

    } else {
        for (;; act->data1++) {
            if (IsTouchingPlayer(act->sprite, 0, act->x + act->data1, act->y)) {
                HurtPlayer();
                break;
            }

            if (TILE_BLOCK_EAST(GetMapTile(act->x + act->data1, act->y))) break;

            DrawSprite(act->sprite, act->data4, act->x + act->data1, act->y, DRAWMODE_NORMAL);
        }
    }
}

/*
Handle one frame of pink worm movement.
*/
void ActPinkWorm(word index)
{
    Actor *act = actors + index;

    if (act->data5 == 0) {  /* always true? */
        act->data4 = !act->data4;
        if (act->data4 != 0) return;
    }

    if (random(40) > 37 && act->data3 == 0 && act->data2 == 0) {
        act->data3 = 4;
    }

    if (act->data3 != 0) {
        act->data3--;

        if (act->data3 == 2) {
            if (act->data1 == DIR2_WEST) {
                act->frame = 2;
            } else if (act->data2 == 0) {
                act->frame = 5;
            }
        } else if (act->data1 == DIR2_WEST) {
            act->frame = 0;
        } else {
            act->frame = 3;
        }

    /*
    Something below makes the worms willing to fall off west ledges but not east
    ledges.
    */
    } else if (act->data1 == DIR2_WEST) {
        act->frame = !act->frame;
        if (act->frame != 0) {
            act->x--;
            AdjustActorMove(index, DIR4_WEST);
            if (act->private1 == 0) {
                act->data1 = DIR2_EAST;
            }
        }

    } else {
        act->data2 = !act->data2;
        if (act->data2 == 0) {
            act->x++;
            act->frame = 1;
            AdjustActorMove(index, DIR4_EAST);
            if (act->private2 == 0) {
                act->data1 = DIR2_WEST;
            }
        }
        act->frame = act->data2 + 3;
    }
}

/*
Handle one frame of hint globe movement.
*/
void ActHintGlobe(word index)
{
    static byte orbframes[] = {0, 4, 5, 6, 5, 4};
    Actor *act = actors + index;

    act->data4 = !act->data4;
    if (act->data4 != 0) {
        act->data3++;
    }

    DrawSprite(SPR_HINT_GLOBE, orbframes[act->data3 % 6], act->x, act->y - 2, DRAWMODE_NORMAL);

    act->data2++;
    if (act->data2 == 4) {
        act->data2 = 1;
    }

    DrawSprite(SPR_HINT_GLOBE, act->data2, act->x, act->y, DRAWMODE_NORMAL);

    nextDrawMode = DRAWMODE_HIDDEN;

    if (IsTouchingPlayer(SPR_HINT_GLOBE, 0, act->x, act->y - 2)) {
        isPlayerNearHintGlobe = true;

        if (demoState != DEMOSTATE_NONE) {
            sawAutoHintGlobe = true;
        }

        if ((cmdNorth && scooterMounted == 0) || !sawAutoHintGlobe) {
            StartSound(SND_HINT_DIALOG_ALERT);
            ShowHintGlobeMessage(act->data5);
        }

        sawAutoHintGlobe = true;
    }
}

/*
Handle one frame of pusher robot movement.
*/
void ActPusherRobot(word index)
{
    Actor *act = actors + index;

    nextDrawMode = DRAWMODE_TRANSLUCENT;
    if (act->data5 == 1) {
        nextDrawMode = DRAWMODE_NORMAL;
    }

    if (act->data2 != 0) {
        act->data2--;
        nextDrawMode = DRAWMODE_NORMAL;
        return;
    }

    if (act->data4 != 0) {
        act->data4--;
    }

    act->data3 = !act->data3;

    if (act->data1 == DIR2_WEST) {
        if (act->y == playerY && act->x - 3 == playerX && act->data4 == 0) {
            act->frame = 2;
            act->data2 = 8;
            SetPlayerPush(DIR8_WEST, 5, 2, PLAYER_BASE_EAST + PLAYER_PUSHED, false, true);
            StartSound(SND_PUSH_PLAYER);
            playerBaseFrame = PLAYER_BASE_EAST;

            act->data4 = 3;
            nextDrawMode = DRAWMODE_NORMAL;
            if (!sawPusherRobotBubble) {
                sawPusherRobotBubble = true;
                NewActor(ACT_SPEECH_UMPH, playerX - 1, playerY - 5);
            }

        } else if (act->data3 != 0) {
            act->x--;
            AdjustActorMove(index, DIR4_WEST);
            if (act->private1 == 0) {
                act->data1 = DIR2_EAST;
                act->frame = (act->x % 2) + 3;
            } else {
                act->frame = !act->frame;
            }
        }

    } else {
        if (act->y == playerY && act->x + 4 == playerX && act->data4 == 0) {
            act->frame = 5;
            act->data2 = 8;
            SetPlayerPush(DIR8_EAST, 5, 2, PLAYER_BASE_WEST + PLAYER_PUSHED, false, true);
            StartSound(SND_PUSH_PLAYER);
            playerBaseFrame = PLAYER_BASE_WEST;

            act->data4 = 3;
            nextDrawMode = DRAWMODE_NORMAL;
            if (!sawPusherRobotBubble) {
                sawPusherRobotBubble = true;
                NewActor(ACT_SPEECH_UMPH, playerX - 1, playerY - 5);
            }

        } else if (act->data3 != 0) {
            act->x++;
            AdjustActorMove(index, DIR4_EAST);
            if (act->private2 == 0) {
                act->frame = !act->frame;
                act->data1 = DIR2_WEST;
            } else {
                act->frame = (act->x % 2) + 3;
            }
        }
    }
}

/*
Handle one frame of sentry robot movement.
*/
void ActSentryRobot(word index)
{
    Actor *act = actors + index;

    if (act->damagecooldown != 0) return;

    act->data3 = !act->data3;
    if (act->data3 != 0) return;

    if (areLightsActive && GameRand() % 50 > 48 && act->data4 == 0) {
        act->data4 = 10;
    }

    if (act->data4 != 0) {
        act->data2 = !act->data2;
        act->data4--;

        if (act->data4 == 1) {
            if (act->x + 1 > playerX) {
                act->data1 = DIR2_WEST;
            } else {
                act->data1 = DIR2_EAST;
            }

            if (act->data1 != DIR2_WEST) {
                NewActor(ACT_PROJECTILE_E, act->x + 3, act->y - 1);
            } else {
                NewActor(ACT_PROJECTILE_W, act->x - 1, act->y - 1);
            }
        }

        if (act->data1 != DIR2_WEST) {
            if (act->data2 != 0) {
                act->frame = 5;
            } else {
                act->frame = 0;
            }
        } else {
            if (act->data2 != 0) {
                act->frame = 6;
            } else {
                act->frame = 2;
            }
        }
    } else {
        if (act->data1 == DIR2_WEST) {
            act->x--;
            AdjustActorMove(index, DIR4_WEST);
            if (act->private1 == 0) {
                act->data1 = DIR2_EAST;
                act->frame = 4;
            } else {
                act->data2 = !act->data2;
                act->frame = act->data2 + 2;
            }
        } else {
            act->x++;
            AdjustActorMove(index, DIR4_EAST);
            if (act->private2 == 0) {
                act->data1 = DIR2_WEST;
                act->frame = 4;
            } else {
                act->frame = !act->frame;
            }
        }
    }
}

/*
Handle one frame of pink worm slime movement.
*/
void ActPinkWormSlime(word index)
{
    Actor *act = actors + index;

    if (act->data5 != 0) {
        act->data5--;
    } else {
        if (act->frame == 8) {
            act->frame = 1;
        }

        act->frame++;
    }
}

/*
Handle one frame of dragonfly movement.
*/
void ActDragonfly(word index)
{
    Actor *act = actors + index;

    if (act->data1 != DIR2_WEST) {
        if (TestSpriteMove(DIR4_EAST, SPR_DRAGONFLY, 0, act->x + 1, act->y) != MOVE_FREE) {
            act->data1 = DIR2_WEST;
        } else {
            act->x++;
            act->data2 = !act->data2;
            act->frame = act->data2 + 2;
        }
    } else {
        if (TestSpriteMove(DIR4_WEST, SPR_DRAGONFLY, 0, act->x - 1, act->y) != MOVE_FREE) {
            act->data1 = DIR2_EAST;
        } else {
            act->x--;
            act->frame = !act->frame;
        }
    }
}

/*
Handle one frame of worm crate movmenet.
*/
void ActWormCrate(word index)
{
    Actor *act = actors + index;

    if (act->data4 == 0) {
        SetMapTileRepeat(TILE_STRIPED_PLATFORM, 4, act->x, act->y - 2);
        act->data4 = 1;

    } else if (TestSpriteMove(DIR4_SOUTH, SPR_WORM_CRATE, 0, act->x, act->y + 1) == MOVE_FREE) {
        SetMapTileRepeat(TILE_EMPTY, 4, act->x, act->y - 2);
        act->y++;
        if (TestSpriteMove(DIR4_SOUTH, SPR_WORM_CRATE, 0, act->x, act->y + 1) != MOVE_FREE) {
            SetMapTileRepeat(TILE_STRIPED_PLATFORM, 4, act->x, act->y - 2);
        }

    } else if (IsSpriteVisible(SPR_WORM_CRATE, 0, act->x, act->y)) {
        if (IsNearExplosion(act->sprite, act->frame, act->x, act->y)) {
            act->data5 = 1;
            act->private2 = WORM_CRATE_EXPLODE;
        }

        if (act->data5 != 0) {
            act->data5--;
        } else {
            act->dead = true;
            if (act->private2 == WORM_CRATE_EXPLODE) {
                NewExplosion(act->x - 1, act->y - 1);
            }

            SetMapTileRepeat(TILE_EMPTY, 4, act->x, act->y - 2);
            NewActor(ACT_PINK_WORM, act->x, act->y);
            nextDrawMode = DRAWMODE_WHITE;
            NewShard(SPR_WORM_CRATE_SHARDS, 0, act->x - 1, act->y + 3);
            NewShard(SPR_WORM_CRATE_SHARDS, 1, act->x,     act->y - 1);
            NewShard(SPR_WORM_CRATE_SHARDS, 2, act->x + 1, act->y);
            NewShard(SPR_WORM_CRATE_SHARDS, 3, act->x,     act->y);
            NewShard(SPR_WORM_CRATE_SHARDS, 4, act->x + 3, act->y + 2);
            NewShard(SPR_WORM_CRATE_SHARDS, 5, act->x,     act->y);
            NewShard(SPR_WORM_CRATE_SHARDS, 6, act->x + 5, act->y + 5);
            StartSound(SND_DESTROY_SOLID);
        }
    }
}

/*
Handle one frame of satellite movement.
*/
void ActSatellite(word index)
{
    Actor *act = actors + index;

    if (act->data2 != 0) {
        act->data2--;

        if (act->data2 != 0) {
            if (act->data2 % 2 != 0) {
                nextDrawMode = DRAWMODE_WHITE;
            }

            return;
        }
    }

    if (IsNearExplosion(SPR_SATELLITE, 0, act->x, act->y)) {
        if (act->data1 == 0) {
            act->data1 = 1;
            act->data2 = 15;
        } else {
            act->dead = true;
            nextDrawMode = DRAWMODE_WHITE;
            StartSound(SND_DESTROY_SATELLITE);

            for (act->data1 = 1; act->data1 < 9; act->data1++) {
                NewDecoration(SPR_SMOKE, 6, act->x + 3, act->y - 3, act->data1, 3);
            }

            NewPounceDecoration(act->x, act->y + 5);
            NewShard(SPR_SATELLITE_SHARDS, 0, act->x,     act->y - 2);
            NewShard(SPR_SATELLITE_SHARDS, 1, act->x + 1, act->y - 2);
            NewShard(SPR_SATELLITE_SHARDS, 2, act->x + 7, act->y + 2);
            NewShard(SPR_SATELLITE_SHARDS, 3, act->x + 3, act->y - 2);
            NewShard(SPR_SATELLITE_SHARDS, 4, act->x - 1, act->y - 8);
            NewShard(SPR_SATELLITE_SHARDS, 5, act->x + 2, act->y + 3);
            NewShard(SPR_SATELLITE_SHARDS, 6, act->x + 6, act->y - 2);
            NewShard(SPR_SATELLITE_SHARDS, 7, act->x - 4, act->y + 1);
            NewSpawner(ACT_HAMBURGER, act->x + 4, act->y);
        }
    }
}

/*
Handle one frame of ivy plant movement.
*/
void ActIvyPlant(word index)
{
    Actor *act = actors + index;

    if (act->data2 != 0) {
        act->y++;
        act->data4++;

        if (act->data4 == 7) {
            act->data2 = 0;
            act->data3 = 0;
            act->data1 = 12;
        }

    } else if (act->data3 < act->data1) {
        act->data3++;

    } else {
        act->data5 = !act->data5;

        act->frame++;
        if (act->frame == 4) act->frame = 0;

        if (act->data4 != 0) {
            if (act->data4 == 7) {
                StartSound(SND_IVY_PLANT_RISE);
            }

            act->data4--;
            act->y--;
        }

        if (IsNearExplosion(SPR_IVY_PLANT, 0, act->x, act->y)) {
            act->data2 = 1;
        }
    }
}

/*
Handle one frame of exit monster (facing west) movement.
*/
void ActExitMonsterWest(word index)
{
    Actor *act = actors + index;

    if (act->data1 == 0) {
        act->data2++;
    }

    if (act->data2 == 10) {
        act->data1 = 1;
        act->data2 = 11;
        act->frame = 1;
        act->data5 = 1;
        StartSound(SND_EXIT_MONSTER_OPEN);
    }

    if (act->frame != 0) {
        static byte tongueframes[] = {2, 3, 4, 3};
        DrawSprite(
            SPR_EXIT_MONSTER_W, tongueframes[act->data3 % 4],
            (act->x + 6) - act->data5, act->y - 3, DRAWMODE_NORMAL
        );
        act->data3++;
    }

    if (!IsSpriteVisible(SPR_EXIT_MONSTER_W, 1, act->x, act->y)) {
        act->frame = 0;
        act->data2 = 0;
        act->data1 = 0;
        act->data5 = 0;
    }

    nextDrawMode = DRAWMODE_HIDDEN;

    DrawSprite(act->sprite, 1, act->x, act->y, DRAWMODE_NORMAL);

    if (act->data5 != 0 && act->data5 < 4) {
        act->data5++;
    }

    DrawSprite(act->sprite, 0, act->x, (act->y - 1) - act->data5, DRAWMODE_NORMAL);
}

/*
Handle interaction between the player and a vertical exit line.
*/
void ActExitLineVertical(word index)
{
    Actor *act = actors + index;

    if (act->x <= playerX + 3) {
        winLevel = true;
    }

    nextDrawMode = DRAWMODE_HIDDEN;
}

/*
Handle interaction between the player and a horizontal exit line.
*/
void ActExitLineHorizontal(word index)
{
    Actor *act = actors + index;

    if (act->y <= playerY && act->data1 == 0) {
        winLevel = true;
    } else if (act->y >= playerY && act->data1 != 0) {  /* player could be dead! */
        winGame = true;
    }

    nextDrawMode = DRAWMODE_HIDDEN;
}

/*
Handle one frame of small flame movement.
*/
void ActSmallFlame(word index)
{
    Actor *act = actors + index;

    act->frame++;
    if (act->frame == 6) act->frame = 0;
}

/*
Handle one frame of prize movement.
*/
void ActPrize(word index)
{
    Actor *act = actors + index;

    if (act->data1 != 0) {
        nextDrawMode = DRAWMODE_FLIPPED;
    }

    if (act->data4 == 0) {
        act->frame++;
    } else {
        act->data3 = !act->data3;

        if (act->data3 != 0) {
            act->frame++;
        }
    }

    if (act->frame == act->data5) act->frame = 0;

    if (act->data5 == 1 && act->sprite != SPR_THRUSTER_JET && act->data4 == 0 && random(64U) == 0) {
        NewDecoration(
            SPR_SPARKLE_LONG, 8,
            random(act->data1) + act->x, random(act->data2) + act->y, DIR8_STATIONARY, 1
        );
    }
}

/*
Handle one frame of bear trap movement.
*/
void ActBearTrap(word index)
{
    Actor *act = actors + index;

    if (act->data2 != 0) {
        static byte frames[] = {
            0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
            2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 0
        };

        if (act->data3 == 1) {
            StartSound(SND_BEAR_TRAP_CLOSE);
        }

        act->frame = frames[act->data3];

        act->data3++;
        if (act->data3 >= 24) {
            blockMovementCmds = false;
        }

        if (act->data3 == 27) {
            act->data3 = 0;
            act->data2 = 0;
            blockMovementCmds = false;
        }

        if (IsNearExplosion(act->sprite, act->frame, act->x, act->y)) {
            AddScore(250);
            NewShard(act->sprite, act->frame, act->x, act->y);
            act->dead = true;
            blockMovementCmds = false;
        }

    } else if (IsNearExplosion(act->sprite, act->frame, act->x, act->y)) {
        AddScore(250);
        NewShard(act->sprite, act->frame, act->x, act->y);
        act->dead = true;
    }
}

/*
Handle one frame of falling floor movement.
*/
void ActFallingFloor(word index)
{
    Actor *act = actors + index;

    if (TestSpriteMove(DIR4_SOUTH, SPR_FALLING_FLOOR, 0, act->x, act->y + 1) != MOVE_FREE) {
        act->dead = true;
        NewShard(SPR_FALLING_FLOOR, 1, act->x, act->y);
        NewShard(SPR_FALLING_FLOOR, 2, act->x, act->y);
        StartSound(SND_DESTROY_SOLID);
        nextDrawMode = DRAWMODE_WHITE;

    } else {
        if (act->data1 == 0) {
            act->private1 = GetMapTile(act->x,     act->y - 1);
            act->private2 = GetMapTile(act->x + 1, act->y - 1);

            SetMapTile(TILE_STRIPED_PLATFORM, act->x,     act->y - 1);
            SetMapTile(TILE_STRIPED_PLATFORM, act->x + 1, act->y - 1);
            act->data1 = 1;
        }

        if (act->y - 2 == playerY && act->x <= playerX + 2 && act->x + 1 >= playerX) {
            act->data2 = 7;
        }

        if (act->data2 != 0) {
            act->data2--;

            if (act->data2 == 0) {
                act->weighted = true;

                SetMapTile(act->private1, act->x,     act->y - 1);
                SetMapTile(act->private2, act->x + 1, act->y - 1);
            }
        }
    }
}

/*
Handle interaction between the player and the Episode 1 end sequence lines.
*/
void ActEpisode1End(word index)
{
    Actor *act = actors + index;

    nextDrawMode = DRAWMODE_HIDDEN;

    if (act->data2 == 0 && act->y <= playerY && act->y >= playerY - 4) {
        ShowE1CliffhangerMessage(act->data1);
        act->data2 = 1;
    }
}

/*
Handle one frame of score effect movement.
*/
void ActScoreEffect(word index)
{
    Actor *act = actors + index;

    nextDrawMode = DRAWMODE_HIDDEN;

    act->data1++;
    act->frame = !act->frame;

    if (act->data1 > 31) {
        static signed char xmoves[] = {-2, -1, 0, 1, 2, 2, 1, 0, -1, -2};
        act->y--;
        act->x += xmoves[(act->data1 - 32) % 10];
    }

    if (act->data1 < 4) {
        act->y--;
    }

    if (
        act->data1 == 100 ||
        !IsSpriteVisible(act->sprite, act->frame, act->x, act->y)
    ) {
        act->dead = true;
        nextDrawMode = DRAWMODE_HIDDEN;
    }

    DrawSprite(act->sprite, act->frame, act->x, act->y, DRAWMODE_IN_FRONT);
}

/*
Handle one frame of exit plant movement.
*/
void ActExitPlant(word index)
{
    Actor *act = actors + index;
    byte tongueframes[] = {5, 6, 7, 8};
    byte swallowframes[] = {1, 1, 1, 1, 1, 1, 1, 2, 3, 4, 1, 1, 1, 1, 1, 1};

    if (act->data3 != 0) {
        act->data3--;
        act->frame = 1;
        if (act->data3 != 0) return;
        act->frame = 0;
    }

    if (act->frame == 0 && act->data5 == 0) {
        DrawSprite(SPR_EXIT_PLANT, tongueframes[act->data1 % 4], act->x + 2, act->y - 3, DRAWMODE_NORMAL);
        act->data1++;
    }

    if (act->data5 != 0) {
        act->frame = swallowframes[act->data5 - 1];

        if (act->data5 == 16) {
            winLevel = true;
        } else {
            act->data5++;
        }
    }

    if (!IsSpriteVisible(SPR_EXIT_PLANT, 1, act->x, act->y)) {
        act->data3 = 30;
        act->data5 = 0;
        act->frame = 1;
    }
}

/*
Handle one frame of bird movement.
*/
void ActBird(word index)
{
    Actor *act = actors + index;

    if (act->data1 == 0) {
        if (act->x + 1 > playerX) {
            if (random(10) == 0) {
                act->data2 = 1;
            } else {
                act->data2 = 0;
            }
        } else {
            if (random(10) == 0) {
                act->data2 = 5;
            } else {
                act->data2 = 4;
            }
        }

        act->frame = act->data2;

        act->data3++;
        if (act->data3 == 30) {
            act->data1 = 1;
            act->data3 = 0;
        }

    } else if (act->data1 == 1) {
        act->data3++;

        if (act->data3 == 20) {
            act->data3 = 0;
            act->data1 = 2;

            if (act->x + 1 > playerX) {
                act->data4 = DIR2_WEST;
            } else {
                act->data4 = DIR2_EAST;
            }
        } else if (act->data3 % 2 != 0 && act->data3 < 10) {
            act->y--;
        }

        if (act->x + 1 > playerX) {
            act->frame = (act->data3 % 2) + 2;
        } else {
            act->frame = (act->data3 % 2) + 6;
        }

    } else if (act->data1 == 2) {
        static signed char yjump[] = {2, 2, 2, 1, 1, 1, 0, 0, 0, -1, -1, -1, -2, -2, -2};

        act->data3++;

        if (act->data4 == DIR2_WEST) {
            act->frame = (act->data3 % 2) + 2;
            act->x--;
        } else {
            act->frame = (act->data3 % 2) + 6;
            act->x++;
        }

        act->y += yjump[act->data3 - 1];

        if (act->data3 == 15) {
            act->data1 = 1;
            act->data3 = 10;
        }
    }
}

/*
Handle one frame of rocket movement.
*/
void ActRocket(word index)
{
    Actor *act = actors + index;

    if (act->data1 != 0) {
        act->data1--;

        if (act->data1 < 30) {
            if (act->data1 % 2 != 0) {
                NewDecoration(SPR_SMOKE, 6, act->x - 1, act->y + 1, DIR8_NORTHWEST, 1);
            } else {
                NewDecoration(SPR_SMOKE, 6, act->x + 1, act->y + 1, DIR8_NORTHEAST, 1);
            }
        }

        return;
    }

    if (act->data2 != 0) {
        if (act->data2 > 7) {
            NewDecoration(SPR_SMOKE, 6, act->x - 1, act->y + 1, DIR8_WEST, 1);
            NewDecoration(SPR_SMOKE, 6, act->x + 1, act->y + 1, DIR8_EAST, 1);
            StartSound(SND_ROCKET_BURN);
        }

        if (act->data2 > 1) {
            act->data2--;
        }

        if (act->data2 < 10) {
            if (TestSpriteMove(DIR4_NORTH, SPR_ROCKET, 0, act->x, act->y - 1) == MOVE_FREE) {
                act->y--;
            } else {
                act->data5 = 1;
            }

            if (IsSpriteVisible(act->sprite, 0, act->x, act->y)) {
                StartSound(SND_ROCKET_BURN);
            }
        }

        if (act->data2 < 5) {
            if (TestSpriteMove(DIR4_NORTH, SPR_ROCKET, 0, act->x, act->y - 1) == MOVE_FREE) {
                act->y--;
            } else {
                act->data5 = 1;
            }

            act->data4 = !act->data4;
            DrawSprite(SPR_ROCKET, act->data4 + 4, act->x, act->y + 6, DRAWMODE_NORMAL);
            if (IsTouchingPlayer(SPR_ROCKET, 4, act->x, act->y + 6)) {
                HurtPlayer();
            }

            if (act->data4 != 0) {
                NewDecoration(SPR_SMOKE, 6, act->x, act->y + 6, DIR8_SOUTH, 1);
            }
        }

        if (act->x == playerX && act->y - 7 <= playerY && act->y - 4 >= playerY) {
            playerMomentumNorth = 16;
            isPlayerRecoiling = true;
            ClearPlayerDizzy();
            isPlayerLongJumping = false;
            if (act->y - 7 == playerY) playerY++;
            if (act->y - 6 == playerY) playerY++;
            if (act->y - 4 == playerY) playerY--;
        }

        if (act->data2 > 4 && act->data2 % 2 != 0) {
            NewDecoration(SPR_SMOKE, 6, act->x, act->y + 2, DIR8_SOUTH, 1);
        }
    }

    if (act->data5 != 0) {
        act->dead = true;
        NewShard(SPR_ROCKET, 1, act->x,     act->y);
        NewShard(SPR_ROCKET, 2, act->x + 1, act->y);
        NewShard(SPR_ROCKET, 3, act->x + 2, act->y);
        NewExplosion(act->x - 4, act->y);
        NewExplosion(act->x + 1, act->y);
        nextDrawMode = DRAWMODE_WHITE;
    }
}

/*
Handle one frame of pedestal movement.
*/
void ActPedestal(word index)
{
    Actor *act = actors + index;
    word i;

    nextDrawMode = DRAWMODE_HIDDEN;

    for (i = 0; act->data1 > i; i++) {
        DrawSprite(SPR_PEDESTAL, 1, act->x, act->y - i, DRAWMODE_NORMAL);
    }

    DrawSprite(SPR_PEDESTAL, 0, act->x - 2, act->y - i, DRAWMODE_NORMAL);
    SetMapTileRepeat(TILE_INVISIBLE_PLATFORM, 5, act->x - 2, act->y - i);

    if (act->data2 == 0 && IsNearExplosion(SPR_PEDESTAL, 1, act->x, act->y)) {
        act->data2 = 3;
    }

    if (act->data2 > 1) {
        act->data2--;
    }

    if (act->data2 == 1) {
        act->data2 = 3;

        SetMapTileRepeat(TILE_EMPTY, 5, act->x - 2, act->y - i);

        act->data1--;

        if (act->data1 == 1) {
            act->dead = true;
            NewShard(SPR_PEDESTAL, 0, act->x, act->y);
        } else {
            NewShard(SPR_PEDESTAL, 1, act->x, act->y);
            NewDecoration(SPR_SMOKE, 6, act->x - 1, act->y + 1, DIR8_NORTH, 1);
        }
    }
}

/*
Handle one frame of invincibility bubble movement.

As long as this actor is alive, it follows the player and gives invincibility.
*/
void ActInvincibilityBubble(word index)
{
    Actor *act = actors + index;
    byte frames[] = {0, 1, 2, 1};

    playerIsInvincible = true;

    act->data1++;
    act->frame = frames[act->data1 % 4];

    if (act->data1 > 200 && act->data1 % 2 != 0) {
        nextDrawMode = DRAWMODE_HIDDEN;
    }

    if (act->data1 == 240) {
        act->dead = true;
        nextDrawMode = DRAWMODE_HIDDEN;
        playerIsInvincible = false;
    } else {
        act->x = playerX - 1;
        act->y = playerY + 1;
    }
}

/*
Handle one frame of monument movement.
*/
void ActMonument(word index)
{
    Actor *act = actors + index;
    int i;

    if (act->data2 != 0) {
        act->dead = true;
        nextDrawMode = DRAWMODE_HIDDEN;
        NewShard(SPR_MONUMENT, 3, act->x,     act->y - 8);
        NewShard(SPR_MONUMENT, 3, act->x,     act->y - 7);
        NewShard(SPR_MONUMENT, 3, act->x,     act->y - 6);
        NewShard(SPR_MONUMENT, 3, act->x,     act->y);
        NewShard(SPR_MONUMENT, 3, act->x + 1, act->y);
        NewShard(SPR_MONUMENT, 3, act->x + 2, act->y);
        NewDecoration(SPR_SMOKE, 6, act->x, act->y,     DIR8_NORTH, 2);
        NewDecoration(SPR_SMOKE, 6, act->x, act->y,     DIR8_NORTHEAST, 2);
        NewDecoration(SPR_SMOKE, 6, act->x, act->y,     DIR8_NORTHWEST, 2);
        NewDecoration(SPR_SMOKE, 6, act->x, act->y - 4, DIR8_NORTH, 3);
        AddScore(25600);
        NewActor(ACT_SCORE_EFFECT_12800, act->x - 2, act->y - 9);
        NewActor(ACT_SCORE_EFFECT_12800, act->x + 2, act->y - 9);
        StartSound(SND_DESTROY_SOLID);

        return;
    }

    if (act->private1 == 0) {
        act->private1 = 1;
        for (i = 0; i < 9; i++) {
            SetMapTile(TILE_SWITCH_BLOCK_1, act->x + 1, act->y - i);
        }
    }

    if (act->data1 != 0) {
        act->data1--;
        if (act->data1 % 2 != 0) {
            nextDrawMode = DRAWMODE_WHITE;
        }
    }

    if (IsNearExplosion(SPR_MONUMENT, 0, act->x, act->y) && act->data1 == 0) {
        act->data1 = 10;
        act->frame++;
        if (act->frame == 3) {
            act->frame = 2;
            act->data2 = 1;
            for (i = 0; i < 9; i++) {
                SetMapTile(TILE_EMPTY, act->x + 1, act->y-i);
            }
        }
    }
}

/*
Handle one frame of tulip launcher movement.
*/
void ActTulipLauncher(word index)
{
    byte launchframes[] = {0, 2, 1, 0, 1};
    Actor *act = actors + index;

    if (act->private2 > 0 && act->private2 < 7) return;

    if (act->data3 != 0) {
        act->data3--;

        if (act->data3 % 2 != 0) {
            nextDrawMode = DRAWMODE_WHITE;
        }

        return;
    }

    if (IsNearExplosion(act->sprite, act->frame, act->x, act->y)) {
        act->data3 = 15;

        act->data5++;
        if (act->data5 == 2) {
            act->dead = true;
            NewShard(SPR_PARACHUTE_BALL, 0, act->x + 2, act->y - 5);
            NewShard(SPR_PARACHUTE_BALL, 2, act->x + 2, act->y - 5);
            NewShard(SPR_PARACHUTE_BALL, 4, act->x + 2, act->y - 5);
            NewShard(SPR_PARACHUTE_BALL, 9, act->x + 2, act->y - 5);
            NewShard(SPR_PARACHUTE_BALL, 3, act->x + 2, act->y - 5);
            NewShard(act->sprite, act->frame, act->x, act->y);

            return;
        }
    }

    if (act->data2 == 0) {
        act->frame = launchframes[act->data1];

        act->data1++;
        if (act->data1 == 2 && act->private1 == 0) {
            NewSpawner(ACT_PARACHUTE_BALL, act->x + 2, act->y - 5);
            StartSound(SND_TULIP_LAUNCH);
        }

        if (act->data1 == 5) {
            act->data2 = 100;
            act->data1 = 0;
            act->private1 = 0;
        }
    } else {
        act->frame = 1;
        act->data2--;
    }
}

/*
Handle one frame of frozen D.N. movement.
*/
void ActFrozenDN(word index)
{
    Actor *act = actors + index;

#ifdef HAS_ACT_FROZEN_DN
    nextDrawMode = DRAWMODE_HIDDEN;

    if (act->data1 == 0) {
        if (IsNearExplosion(SPR_FROZEN_DN, 0, act->x, act->y)) {
            NewShard(SPR_FROZEN_DN, 6,  act->x,     act->y - 6);
            NewShard(SPR_FROZEN_DN, 7,  act->x + 4, act->y);
            NewShard(SPR_FROZEN_DN, 8,  act->x,     act->y - 5);
            NewShard(SPR_FROZEN_DN, 9,  act->x,     act->y - 4);
            NewShard(SPR_FROZEN_DN, 10, act->x + 5, act->y - 6);
            NewShard(SPR_FROZEN_DN, 11, act->x + 5, act->y - 4);
            StartSound(SND_SMASH);
            act->data1 = 1;
            act->x++;
        } else {
            DrawSprite(SPR_FROZEN_DN, 0, act->x, act->y, DRAWMODE_NORMAL);
        }

    } else if (act->data1 == 1) {
        act->data2++;
        if (act->data2 % 2 != 0) {
            act->y--;
        }

        DrawSprite(SPR_FROZEN_DN, (act->data5++ % 2) + 4, act->x, act->y + 5, DRAWMODE_NORMAL);
        DrawSprite(SPR_FROZEN_DN, 2, act->x, act->y, DRAWMODE_NORMAL);
        NewDecoration(SPR_SMOKE, 6, act->x, act->y + 6, DIR8_SOUTH, 1);

        if (act->data2 == 10) {
            act->data1 = 2;
            act->data2 = 0;
        }

    } else if (act->data1 == 2) {
        DrawSprite(SPR_FROZEN_DN, (act->data5++ % 2) + 4, act->x, act->y + 5, DRAWMODE_NORMAL);
        DrawSprite(SPR_FROZEN_DN, 1, act->x, act->y, DRAWMODE_NORMAL);

        act->data2++;
        if (act->data2 == 30) {
            ShowRescuedDNMessage();

            act->data1 = 3;
            act->data2 = 0;
        }

    } else if (act->data1 == 3) {
        act->data2++;

        DrawSprite(SPR_FROZEN_DN, (act->data5++ % 2) + 4, act->x, act->y + 5, DRAWMODE_NORMAL);

        if (act->data2 < 10) {
            DrawSprite(SPR_FROZEN_DN, 1, act->x, act->y, DRAWMODE_NORMAL);
        } else {
            DrawSprite(SPR_FROZEN_DN, 2, act->x, act->y, DRAWMODE_NORMAL);
            NewDecoration(SPR_SMOKE, 6, act->x, act->y + 6, DIR8_SOUTH, 1);
        }

        if (act->data2 == 15) {
            act->data1 = 4;
            act->data2 = 0;
        }

    } else if (act->data1 == 4) {
        act->data2++;
        if (act->data2 == 1) {
            NewSpawner(ACT_HAMBURGER, act->x, act->y);
        }

        act->y--;

        if (act->data2 > 50 || !IsSpriteVisible(SPR_FROZEN_DN, 2, act->x, act->y)) {
            act->dead = true;
        } else {
            DrawSprite(SPR_FROZEN_DN, (act->data5++ % 2) + 4, act->x, act->y + 5, DRAWMODE_NORMAL);
            DrawSprite(SPR_FROZEN_DN, 2, act->x, act->y, DRAWMODE_NORMAL);
            NewDecoration(SPR_SMOKE, 6, act->x, act->y + 6, DIR8_SOUTH, 1);
            StartSound(SND_ROCKET_BURN);
        }
    }
#else
#   pragma warn -aus
#endif  /* ACT_FROZEN_DN */
}
#pragma warn .aus

/*
Handle one frame of flame pulse movement.
*/
void ActFlamePulse(word index)
{
    Actor *act = actors + index;
    byte frames[] = {0, 1, 0, 1, 0, 1, 0, 1, 2, 3, 2, 3, 2, 3, 1, 0};

    if (act->data1 == 0) {
        act->frame = frames[act->data2];

        if (act->frame == 2) {
            NewDecoration(SPR_SMOKE, 6, act->x - act->data5, act->y - 3, DIR8_NORTH, 1);
            StartSound(SND_FLAME_PULSE);
        }

        act->data2++;
        if (act->data2 == 16) {
            act->data1 = 30;
            act->data2 = 0;
        }
    } else {
        act->data1--;
        nextDrawMode = DRAWMODE_HIDDEN;
    }
}

/*
Handle one frame of speech bubble movement.
*/
void ActSpeechBubble(word index)
{
    Actor *act = actors + index;

    nextDrawMode = DRAWMODE_HIDDEN;

    if (act->data1 == 0) {
        StartSound(SND_SPEECH_BUBBLE);

        if (act->sprite == SPR_SPEECH_WOW_50K) {
            AddScore(50000L);
        }
    }

    act->data1++;

    if (act->data1 == 20) {
        act->dead = true;
    } else {
        DrawSprite(act->sprite, 0, playerX - 1, playerY - 5, DRAWMODE_IN_FRONT);
    }
}

/*
Handle one frame of smoke emitter movement.
*/
void ActSmokeEmitter(word index)
{
    Actor *act = actors + index;

    nextDrawMode = DRAWMODE_HIDDEN;

    act->data1 = GameRand() % 32;
    if (act->data1 == 0) {
        if (act->data5 != 0) {
            NewDecoration(SPR_SMOKE, 6, act->x - 1, act->y, DIR8_NORTH, 1);
        } else {
            NewDecoration(SPR_SMOKE_LARGE, 6, act->x - 2, act->y, DIR8_NORTH, 1);
        }
    }
}

/*
Create a new actor of the specified type located at x,y.
Relies on the caller providing the correct index into the main actors array.
*/
bbool NewActorAtIndex(word index, word actor_type, word x, word y)
{
    /* This is probably to save having to pass this 240+ times below. */
    nextActorIndex = index;

#define F false
#define T true

    switch (actor_type) {
    case ACT_BASKET_NULL:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_BASKET_NULL, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_STAR_FLOAT:
        ConstructActor(SPR_STAR, x, y, F, F, F, F, ActPrize, 0, 0, 0, 0, 4);
        break;
    case ACT_JUMP_PAD_FLOOR:
        ConstructActor(SPR_JUMP_PAD, x, y, F, T, T, F, ActJumpPad, 0, 0, 0, 0, 0);
        break;
    case ACT_ARROW_PISTON_W:
        ConstructActor(SPR_ARROW_PISTON_W, x, y, F, T, F, F, ActArrowPiston, 0, 0, 0, 0, DIR2_WEST);
        break;
    case ACT_ARROW_PISTON_E:
        ConstructActor(SPR_ARROW_PISTON_E, x - 4, y, F, T, F, F, ActArrowPiston, 0, 0, 0, 0, DIR2_EAST);
        break;
    case ACT_FIREBALL_W:
        ConstructActor(SPR_FIREBALL, x, y, T, F, F, F, ActFireball, 0, x, y, 0, DIR2_WEST);
        break;
    case ACT_FIREBALL_E:
        ConstructActor(SPR_FIREBALL, x - 1, y, T, F, F, F, ActFireball, 0, x - 1, y, 0, DIR2_EAST);
        break;
    case ACT_HEAD_SWITCH_BLUE:
        ConstructActor(SPR_HEAD_SWITCH_BLUE, x, y + 1, F, F, F, F, ActHeadSwitch, 0, 0, 0, 0, SPR_DOOR_BLUE);
        break;
    case ACT_DOOR_BLUE:
        ConstructActor(SPR_DOOR_BLUE, x, y, F, F, F, F, ActDoor, 0, 0, 0, 0, 0);
        break;
    case ACT_HEAD_SWITCH_RED:
        ConstructActor(SPR_HEAD_SWITCH_RED, x, y + 1, F, F, F, F, ActHeadSwitch, 0, 0, 0, 0, SPR_DOOR_RED);
        break;
    case ACT_DOOR_RED:
        ConstructActor(SPR_DOOR_RED, x, y, F, F, F, F, ActDoor, 0, 0, 0, 0, 0);
        break;
    case ACT_HEAD_SWITCH_GREEN:
        ConstructActor(SPR_HEAD_SWITCH_GREEN, x, y + 1, F, F, F, F, ActHeadSwitch, 0, 0, 0, 0, SPR_DOOR_GREEN);
        break;
    case ACT_DOOR_GREEN:
        ConstructActor(SPR_DOOR_GREEN, x, y, F, F, F, F, ActDoor, 0, 0, 0, 0, 0);
        break;
    case ACT_HEAD_SWITCH_YELLOW:
        ConstructActor(SPR_HEAD_SWITCH_YELLOW, x, y + 1, F, F, F, F, ActHeadSwitch, 0, 0, 0, 0, SPR_DOOR_YELLOW);
        break;
    case ACT_DOOR_YELLOW:
        ConstructActor(SPR_DOOR_YELLOW, x, y, F, F, F, F, ActDoor, 0, 0, 0, 0, 0);
        break;
    case ACT_JUMP_PAD_ROBOT:
        ConstructActor(SPR_JUMP_PAD_ROBOT, x, y, T, F, F, F, ActJumpPadRobot, 0, DIR2_WEST, 0, 0, 0);
        break;
    case ACT_SPIKES_FLOOR:
        ConstructActor(SPR_SPIKES_FLOOR, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_SPIKES_FLOOR_RECIP:
        ConstructActor(SPR_SPIKES_FLOOR_RECIP, x, y, F, F, F, F, ActReciprocatingSpikes, 1, 0, 0, 0, 0);
        break;
    case ACT_SAW_BLADE_VERT:
        ConstructActor(SPR_SAW_BLADE, x, y, F, T, F, T, ActVerticalMover, 0, 0, 0, 0, 0);
        break;
    case ACT_SAW_BLADE_HORIZ:
        ConstructActor(SPR_SAW_BLADE, x, y, T, F, F, T, ActHorizontalMover, 0, 0, 0, 0, 1);
        break;
    case ACT_BOMB_ARMED:
        ConstructActor(SPR_BOMB_ARMED, x, y, T, F, T, T, ActBombArmed, 0, 0, 0, 0, 0);
        break;
    case ACT_CABBAGE:
        ConstructActor(SPR_CABBAGE, x, y, F, T, T, T, ActCabbage, 1, 0, 0, 0, 0);
        break;
    case ACT_POWER_UP_FLOAT:
        ConstructActor(SPR_POWER_UP, x, y, T, F, T, F, ActPrize, 0, 0, 0, 1, 6);
        break;
    case ACT_BARREL_POWER_UP:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_POWER_UP_FLOAT, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_BASKET_GRN_TOMATO:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_GRN_TOMATO, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_BASKET_RED_TOMATO:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_RED_TOMATO, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_BARREL_YEL_PEAR:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_YEL_PEAR, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_BARREL_ONION:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_ONION, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_BARREL_JUMP_PAD_FL:
        ConstructActor(SPR_BARREL, x, y, T, F, T, T, ActBarrel, ACT_JUMP_PAD_FLOOR, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_GRN_TOMATO:
        ConstructActor(SPR_GRN_TOMATO, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_RED_TOMATO:
        ConstructActor(SPR_RED_TOMATO, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_YEL_PEAR:
        ConstructActor(SPR_YEL_PEAR, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_ONION:
        ConstructActor(SPR_ONION, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_EXIT_SIGN:
        ConstructActor(SPR_EXIT_SIGN, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_SPEAR:
        ConstructActor(SPR_SPEAR, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_SPEAR_RECIP:
        ConstructActor(SPR_SPEAR, x, y, F, F, F, F, ActReciprocatingSpear, 0, 0, 0, 0, 0);
        break;
    case ACT_GRN_SLIME_THROB:
        ConstructActor(SPR_GREEN_SLIME, x, y + 1, F, F, F, F, ActRedGreenSlime, 0, 0, 0, 0, 0);
        break;
    case ACT_GRN_SLIME_DRIP:
        ConstructActor(SPR_GREEN_SLIME, x, y + 1, F, T, F, F, ActRedGreenSlime, x, y + 1, 0, 0, 1);
        break;
    case ACT_FLYING_WISP:
        ConstructActor(SPR_FLYING_WISP, x, y, T, F, F, F, ActFlyingWisp, 0, 0, 0, 0, 0);
        break;
    case ACT_TWO_TONS_CRUSHER:
        ConstructActor(SPR_TWO_TONS_CRUSHER, x, y, F, T, F, F, ActTwoTonsCrusher, 0, 0, 0, 0, 0);
        break;
    case ACT_JUMPING_BULLET:
        ConstructActor(SPR_JUMPING_BULLET, x, y, F, T, F, F, ActJumpingBullet, 0, DIR2_WEST, 0, 0, 0);
        break;
    case ACT_STONE_HEAD_CRUSHER:
        ConstructActor(SPR_STONE_HEAD_CRUSHER, x, y, F, T, F, F, ActStoneHeadCrusher, 0, 0, 0, 0, 0);
        break;
    case ACT_PYRAMID_CEIL:
        ConstructActor(SPR_PYRAMID, x, y + 1, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_PYRAMID_FALLING:
        ConstructActor(SPR_PYRAMID, x, y + 1, F, T, F, T, ActPyramid, 0, 0, 0, 0, 0);
        break;
    case ACT_PYRAMID_FLOOR:
        ConstructActor(SPR_PYRAMID, x, y, F, F, F, F, ActPyramid, 0, 0, 0, 0, 1);
        break;
    case ACT_GHOST:
        ConstructActor(SPR_GHOST, x, y, F, T, F, F, ActGhost, 0, 0, 0, 0, 4);
        break;
    case ACT_MOON:
        ConstructActor(SPR_MOON, x, y, F, F, F, T, ActMoon, 0, 0, 0, 0, 4);
        break;
    case ACT_HEART_PLANT:
        ConstructActor(SPR_HEART_PLANT, x, y, F, F, F, F, ActHeartPlant, 0, 0, 0, 0, 0);
        break;
    case ACT_BARREL_BOMB:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_BOMB_IDLE, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_BOMB_IDLE:
        ConstructActor(SPR_BOMB_IDLE, x, y, T, F, T, F, ActBombIdle, 0, 0, 0, 0, 0);
        break;
    case ACT_SWITCH_PLATFORMS:
        ConstructActor(SPR_FOOT_SWITCH, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, ACT_SWITCH_PLATFORMS);
        arePlatformsActive = false;
        break;
    case ACT_SWITCH_MYSTERY_WALL:
        ConstructActor(SPR_FOOT_SWITCH, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, ACT_SWITCH_MYSTERY_WALL);
        break;
    case ACT_MYSTERY_WALL:
        ConstructActor(SPR_MYSTERY_WALL, x, y, T, F, F, F, ActMysteryWall, 0, 0, 0, 0, 0);
        mysteryWallTime = 0;
        break;
    case ACT_BABY_GHOST:
        ConstructActor(SPR_BABY_GHOST, x, y, F, T, T, F, ActBabyGhost, DIR2_SOUTH, 0, 0, 0, 0);
        break;
    case ACT_PROJECTILE_SW:
        ConstructActor(SPR_PROJECTILE, x, y, T, F, F, T, ActProjectile, 0, 0, 0, 0, DIRP_SOUTHWEST);
        break;
    case ACT_PROJECTILE_SE:
        ConstructActor(SPR_PROJECTILE, x, y, T, F, F, T, ActProjectile, 0, 0, 0, 0, DIRP_SOUTHEAST);
        break;
    case ACT_PROJECTILE_S:
        ConstructActor(SPR_PROJECTILE, x, y, T, F, F, T, ActProjectile, 0, 0, 0, 0, DIRP_SOUTH);
        break;
    case ACT_ROAMER_SLUG:
        ConstructActor(SPR_ROAMER_SLUG, x, y, F, T, F, F, ActRoamerSlug, 0, 3, 0, 0, 0);
        break;
    case ACT_PIPE_CORNER_N:
        ConstructActor(SPR_PIPE_CORNER_N, x, y, F, F, F, F, ActPipeCorner, 0, 0, 0, 0, 0);
        break;
    case ACT_PIPE_CORNER_S:
        ConstructActor(SPR_PIPE_CORNER_S, x, y, F, F, F, F, ActPipeCorner, 0, 0, 0, 0, 0);
        break;
    case ACT_PIPE_CORNER_W:
        ConstructActor(SPR_PIPE_CORNER_W, x, y, F, T, F, F, ActPipeCorner, 0, 0, 0, 0, 0);
        break;
    case ACT_PIPE_CORNER_E:
        ConstructActor(SPR_PIPE_CORNER_E, x, y, F, T, F, F, ActPipeCorner, 0, 0, 0, 0, 0);
        break;
    case ACT_BABY_GHOST_EGG_PROX:
        ConstructActor(SPR_BABY_GHOST_EGG, x, y, F, F, F, F, ActBabyGhostEgg, 0, 0, 0, 0, 0);
        break;
    case ACT_BABY_GHOST_EGG:
        ConstructActor(SPR_BABY_GHOST_EGG, x, y, F, F, F, F, ActBabyGhostEgg, 0, 0, 0, 0, 1);
        break;
    case ACT_SHARP_ROBOT_FLOOR:
        ConstructActor(SPR_SHARP_ROBOT_FLOOR, x, y, F, T, F, F, ActHorizontalMover, 8, 0, 0, 0, 1);
        break;
    case ACT_SHARP_ROBOT_CEIL:
        ConstructActor(SPR_SHARP_ROBOT_CEIL, x, y + 2, F, T, F, F, ActSharpRobot, 0, DIR2_WEST, 0, 0, 0);
        break;
    case ACT_BASKET_HAMBURGER:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_HAMBURGER, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_HAMBURGER:
        ConstructActor(SPR_HAMBURGER, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_CLAM_PLANT_FLOOR:
        ConstructActor(SPR_CLAM_PLANT, x, y, F, F, F, F, ActClamPlant, 0, 0, 0, 0, DRAWMODE_NORMAL);
        break;
    case ACT_CLAM_PLANT_CEIL:
        ConstructActor(SPR_CLAM_PLANT, x, y + 2, F, F, F, F, ActClamPlant, 0, 0, 0, 0, DRAWMODE_FLIPPED);
        break;
    case ACT_GRAPES:
        ConstructActor(SPR_GRAPES, x, y + 2, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_PARACHUTE_BALL:
        ConstructActor(SPR_PARACHUTE_BALL, x, y, F, T, T, T, ActParachuteBall, 0, 20, 0, 0, 2);
        break;
    case ACT_SPIKES_E:
        ConstructActor(SPR_SPIKES_E, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_SPIKES_E_RECIP:
        ConstructActor(ACT_SPIKES_E_RECIP, x, y, F, F, F, F, ActReciprocatingSpikes, 1, 0, 0, 0, 0);
        break;
    case ACT_SPIKES_W:
        ConstructActor(SPR_SPIKES_W, x - 3, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_BEAM_ROBOT:
        ConstructActor(SPR_BEAM_ROBOT, x, y, T, F, F, F, ActBeamRobot, 0, 0, 0, 0, 0);
        break;
    case ACT_SPLITTING_PLATFORM:
        ConstructActor(SPR_SPLITTING_PLATFORM, x, y, T, F, F, F, ActSplittingPlatform, 0, 0, 0, 0, 0);
        break;
    case ACT_SPARK:
        ConstructActor(SPR_SPARK, x, y, F, T, F, F, ActSpark, 0, 0, 0, 0, 0);
        break;
    case ACT_BASKET_DANCE_MUSH:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_DANCING_MUSHROOM, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_DANCING_MUSHROOM:
        ConstructActor(SPR_DANCING_MUSHROOM, x, y, T, F, T, F, ActPrize, 0, 0, 0, 1, 2);
        break;
    case ACT_EYE_PLANT_FLOOR:
        ConstructActor(SPR_EYE_PLANT, x, y, F, T, F, F, ActEyePlant, 0, 0, 0, 0, DRAWMODE_NORMAL);
        if (numEyePlants < 15) numEyePlants++;
        break;
    case ACT_EYE_PLANT_CEIL:
        ConstructActor(SPR_EYE_PLANT, x, y + 1, F, F, F, F, ActEyePlant, 0, 0, 0, 0, DRAWMODE_FLIPPED);
        break;
    case ACT_BARREL_CABB_HARDER:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_CABBAGE_HARDER, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_RED_JUMPER:
        ConstructActor(SPR_RED_JUMPER, x, y, F, T, F, F, ActRedJumper, 0, 0, 0, 0, 7);
        break;
    case ACT_BOSS:
        ConstructActor(SPR_BOSS, x, y, F, T, F, F, ActBoss, 0, 0, 0, 0, 0);
        break;
    case ACT_PIPE_OUTLET:
        ConstructActor(SPR_PIPE_END, x - 1, y + 2, T, F, F, F, ActPipeEnd, 0, 0, 0, 0, 0);
        break;
    case ACT_PIPE_INLET:
        ConstructActor(SPR_PIPE_END, x - 1, y + 2, F, T, F, F, ActPipeEnd, 0, 1, 0, 0, 0);
        break;
    case ACT_SUCTION_WALKER:
        ConstructActor(SPR_SUCTION_WALKER, x, y, F, T, F, F, ActSuctionWalker, DIR2_WEST, 0, 0, 0, 0);
        break;
    case ACT_TRANSPORTER_1:
        ConstructActor(SPR_TRANSPORTER_108, x, y, T, F, F, F, ActTransporter, 0, 0, 0, 0, 2);
        break;
    case ACT_TRANSPORTER_2:
        ConstructActor(SPR_TRANSPORTER_108, x, y, T, F, F, F, ActTransporter, 0, 0, 0, 0, 1);
        break;
    case ACT_PROJECTILE_W:
        ConstructActor(SPR_PROJECTILE, x, y, T, F, F, F, ActProjectile, 0, 0, 0, 0, DIRP_WEST);
        break;
    case ACT_PROJECTILE_E:
        ConstructActor(SPR_PROJECTILE, x, y, T, F, F, F, ActProjectile, 0, 0, 0, 0, DIRP_EAST);
        break;
    case ACT_SPIT_WALL_PLANT_W:
        ConstructActor(SPR_SPIT_WALL_PLANT_W, x - 3, y, F, F, F, F, ActSpittingWallPlant, 0, 0, 0, 0, DIR4_WEST);
        break;
    case ACT_SPIT_WALL_PLANT_E:
        ConstructActor(SPR_SPIT_WALL_PLANT_E, x, y, F, F, F, F, ActSpittingWallPlant, 0, 0, 0, 0, DIR4_EAST);
        break;
    case ACT_SPITTING_TURRET:
        ConstructActor(SPR_SPITTING_TURRET, x, y, F, T, F, F, ActSpittingTurret, 0, 10, x, 0, 3);
        break;
    case ACT_SCOOTER:
        ConstructActor(SPR_SCOOTER, x, y, F, T, F, F, ActScooter, 0, 0, 0, 0, 0);
        break;
    case ACT_RED_CHOMPER:
        ConstructActor(SPR_RED_CHOMPER, x, y, F, T, T, F, ActRedChomper, DIR2_WEST, 0, 0, 0, 0);
        break;
    case ACT_SWITCH_LIGHTS:
        ConstructActor(SPR_FOOT_SWITCH, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, ACT_SWITCH_LIGHTS);
        areLightsActive = false;
        hasLightSwitch = true;
        break;
    case ACT_SWITCH_FORCE_FIELD:
        ConstructActor(SPR_FOOT_SWITCH, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, ACT_SWITCH_FORCE_FIELD);
        break;
    case ACT_FORCE_FIELD_VERT:
        ConstructActor(SPR_FORCE_FIELD_VERT, x, y, T, F, F, F, ActForceField, 0, 0, 0, 0, 0);
        break;
    case ACT_FORCE_FIELD_HORIZ:
        ConstructActor(SPR_FORCE_FIELD_HORIZ, x, y, T, F, F, F, ActForceField, 0, 0, 0, 0, 1);
        break;
    case ACT_PINK_WORM:
        ConstructActor(SPR_PINK_WORM, x, y, F, T, T, F, ActPinkWorm, DIR2_WEST, 0, 0, 0, 0);
        break;
    case ACT_HINT_GLOBE_0:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 0);
        break;
    case ACT_PUSHER_ROBOT:
        ConstructActor(SPR_PUSHER_ROBOT, x, y, F, T, F, F, ActPusherRobot, DIR2_WEST, 0, 0, 0, 4);
        break;
    case ACT_SENTRY_ROBOT:
        ConstructActor(SPR_SENTRY_ROBOT, x, y, F, T, F, F, ActSentryRobot, DIR2_WEST, 0, 0, 0, 4);
        break;
    case ACT_PINK_WORM_SLIME:
        ConstructActor(SPR_PINK_WORM_SLIME, x, y, F, F, T, F, ActPinkWormSlime, 0, 0, 0, 0, 3);
        break;
    case ACT_DRAGONFLY:
        ConstructActor(SPR_DRAGONFLY, x, y, F, T, F, F, ActDragonfly, DIR2_WEST, 0, 0, 0, 0);
        break;
    case ACT_WORM_CRATE:
        ConstructActor(SPR_WORM_CRATE, x, y, T, F, F, F, ActWormCrate, 0, 0, 0, 0, ((GameRand() % 20) * 5) + 50);
        break;
    case ACT_BOTTLE_DRINK:
        ConstructActor(SPR_BOTTLE_DRINK, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_GRN_GOURD:
        ConstructActor(SPR_GRN_GOURD, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_BLU_SPHERES:
        ConstructActor(SPR_BLU_SPHERES, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_POD:
        ConstructActor(SPR_POD, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_PEA_PILE:
        ConstructActor(SPR_PEA_PILE, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_LUMPY_FRUIT:
        ConstructActor(SPR_LUMPY_FRUIT, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_HORN:
        ConstructActor(SPR_HORN, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_RED_BERRIES:
        ConstructActor(SPR_RED_BERRIES, x, y + 2, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_BARREL_BOTL_DRINK:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_BOTTLE_DRINK, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_BASKET_GRN_GOURD:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_GRN_GOURD, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_BASKET_BLU_SPHERES:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_BLU_SPHERES, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_BASKET_POD:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_POD, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_BASKET_PEA_PILE:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_PEA_PILE, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_BASKET_LUMPY_FRUIT:
        ConstructActor(SPR_BASKET, x, y, T, F, F, F, ActBarrel, ACT_LUMPY_FRUIT, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_BARREL_HORN:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_HORN, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_SATELLITE:
        ConstructActor(SPR_SATELLITE, x, y, F, F, F, F, ActSatellite, 0, 0, 0, 0, 0);
        break;
    case ACT_IVY_PLANT:
        ConstructActor(SPR_IVY_PLANT, x, y + 7, F, T, F, F, ActIvyPlant, 5, 0, 0, 7, 0);
        break;
    case ACT_YEL_FRUIT_VINE:
        ConstructActor(SPR_YEL_FRUIT_VINE, x, y + 2, T, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_HEADDRESS:
        ConstructActor(SPR_HEADDRESS, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_BASKET_HEADDRESS:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_HEADDRESS, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_EXIT_MONSTER_W:
        ConstructActor(SPR_EXIT_MONSTER_W, x - 4, y, F, T, F, F, ActExitMonsterWest, 0, 0, 0, 0, 0);
        break;
    case ACT_EXIT_LINE_VERT:
        ConstructActor(SPR_150, x, y, T, F, F, F, ActExitLineVertical, 0, 0, 0, 0, 0);
        break;
    case ACT_SMALL_FLAME:
        ConstructActor(SPR_SMALL_FLAME, x, y, F, F, F, F, ActSmallFlame, 0, 0, 0, 0, 0);
        break;
    case ACT_ROTATING_ORNAMENT:
        ConstructActor(SPR_ROTATING_ORNAMENT, x, y, T, F, T, F, ActPrize, 0, 0, 0, 0, 4);
        break;
    case ACT_BLU_CRYSTAL:
        ConstructActor(SPR_BLU_CRYSTAL, x, y, T, F, T, F, ActPrize, 0, 0, 0, 0, 5);
        break;
    case ACT_RED_CRYSTAL_FLOOR:
        ConstructActor(SPR_RED_CRYSTAL, x, y, T, F, T, F, ActPrize, 0, 0, 0, 0, 6);
        break;
    case ACT_BARREL_RT_ORNAMENT:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_ROTATING_ORNAMENT, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_BARREL_BLU_CRYSTAL:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_BLU_CRYSTAL, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_BARREL_RED_CRYSTAL:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_RED_CRYSTAL_FLOOR, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_GRN_TOMATO_FLOAT:
        ConstructActor(SPR_GRN_TOMATO, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_RED_TOMATO_FLOAT:
        ConstructActor(SPR_RED_TOMATO, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_YEL_PEAR_FLOAT:
        ConstructActor(SPR_YEL_PEAR, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_BEAR_TRAP:
        ConstructActor(SPR_BEAR_TRAP, x, y, F, F, F, F, ActBearTrap, 0, 0, 0, 0, 0);
        break;
    case ACT_FALLING_FLOOR:
        ConstructActor(SPR_FALLING_FLOOR, x, y, F, T, F, F, ActFallingFloor, 0, 0, 0, 0, 0);
        break;
    case ACT_EP1_END_1:
    case ACT_EP1_END_2:
    case ACT_EP1_END_3:
        ConstructActor(SPR_164, x, y, T, F, F, F, ActEpisode1End, actor_type, 0, 0, 0, 0);
        break;
    case ACT_ROOT:
        ConstructActor(SPR_ROOT, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_BASKET_ROOT:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_ROOT, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_REDGRN_BERRIES:
        ConstructActor(SPR_REDGRN_BERRIES, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_BASKET_RG_BERRIES:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_REDGRN_BERRIES, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_RED_GOURD:
        ConstructActor(SPR_RED_GOURD, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_BASKET_RED_GOURD:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_RED_GOURD, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_GRN_EMERALD:
        ConstructActor(SPR_GRN_EMERALD, x, y, T, F, T, F, ActPrize, 0, 0, 0, 0, 5);
        break;
    case ACT_BARREL_GRN_EMERALD:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_GRN_EMERALD, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_CLR_DIAMOND:
        ConstructActor(SPR_CLR_DIAMOND, x, y, T, F, T, F, ActPrize, 0, 0, 0, 0, 4);
        break;
    case ACT_BARREL_CLR_DIAMOND:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_CLR_DIAMOND, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_SCORE_EFFECT_100:
        ConstructActor(SPR_SCORE_EFFECT_100, x, y, F, T, F, F, ActScoreEffect, 0, 0, 0, 0, 0);
        break;
    case ACT_SCORE_EFFECT_200:
        ConstructActor(SPR_SCORE_EFFECT_200, x, y, F, T, F, F, ActScoreEffect, 0, 0, 0, 0, 0);
        break;
    case ACT_SCORE_EFFECT_400:
        ConstructActor(SPR_SCORE_EFFECT_400, x, y, F, T, F, F, ActScoreEffect, 0, 0, 0, 0, 0);
        break;
    case ACT_SCORE_EFFECT_800:
        ConstructActor(SPR_SCORE_EFFECT_800, x, y, F, T, F, F, ActScoreEffect, 0, 0, 0, 0, 0);
        break;
    case ACT_SCORE_EFFECT_1600:
        ConstructActor(SPR_SCORE_EFFECT_1600, x, y, F, T, F, F, ActScoreEffect, 0, 0, 0, 0, 0);
        break;
    case ACT_SCORE_EFFECT_3200:
        ConstructActor(SPR_SCORE_EFFECT_3200, x, y, F, T, F, F, ActScoreEffect, 0, 0, 0, 0, 0);
        break;
    case ACT_SCORE_EFFECT_6400:
        ConstructActor(SPR_SCORE_EFFECT_6400, x, y, F, T, F, F, ActScoreEffect, 0, 0, 0, 0, 0);
        break;
    case ACT_SCORE_EFFECT_12800:
        ConstructActor(SPR_SCORE_EFFECT_12800, x, y, F, T, F, F, ActScoreEffect, 0, 0, 0, 0, 0);
        break;
    case ACT_EXIT_PLANT:
        ConstructActor(SPR_EXIT_PLANT, x, y, F, T, F, F, ActExitPlant, 0, 0, 30, 0, 0);
        break;
    case ACT_BIRD:
        ConstructActor(SPR_BIRD, x, y, F, T, F, F, ActBird, 0, 0, 0, DIR2_WEST, 0);
        break;
    case ACT_ROCKET:
        ConstructActor(SPR_ROCKET, x, y, F, T, F, F, ActRocket, 60, 10, 0, 0, 0);
        break;
    case ACT_INVINCIBILITY_CUBE:
        ConstructActor(SPR_INVINCIBILITY_CUBE, x, y, F, F, F, F, ActPrize, 0, 0, 0, 0, 4);
        break;
    case ACT_PEDESTAL_SMALL:
        ConstructActor(SPR_PEDESTAL, x, y, T, F, F, F, ActPedestal, 13, 0, 0, 0, 0);
        break;
    case ACT_PEDESTAL_MEDIUM:
        ConstructActor(SPR_PEDESTAL, x, y, T, F, F, F, ActPedestal, 19, 0, 0, 0, 0);
        break;
    case ACT_PEDESTAL_LARGE:
        ConstructActor(SPR_PEDESTAL, x, y, T, F, F, F, ActPedestal, 25, 0, 0, 0, 0);
        break;
    case ACT_INVINCIBILITY_BUBB:
        ConstructActor(SPR_INVINCIBILITY_BUBB, x, y, F, F, F, F, ActInvincibilityBubble, 0, 0, 0, 0, 0);
        break;
    case ACT_BARREL_CYA_DIAMOND:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_CYA_DIAMOND, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_CYA_DIAMOND:
        ConstructActor(SPR_CYA_DIAMOND, x, y, T, F, T, F, ActPrize, 3, 2, 0, 0, 1);
        break;
    case ACT_BARREL_RED_DIAMOND:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_RED_DIAMOND, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_RED_DIAMOND:
        ConstructActor(SPR_RED_DIAMOND, x, y, T, F, T, F, ActPrize, 2, 2, 0, 0, 1);
        break;
    case ACT_BARREL_GRY_OCTAHED:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_GRY_OCTAHEDRON, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_GRY_OCTAHEDRON:
        ConstructActor(SPR_GRY_OCTAHEDRON, x, y, T, F, T, F, ActPrize, 2, 2, 0, 0, 1);
        break;
    case ACT_BARREL_BLU_EMERALD:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_BLU_EMERALD, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_BLU_EMERALD:
        ConstructActor(SPR_BLU_EMERALD, x, y, T, F, T, F, ActPrize, 2, 2, 0, 0, 1);
        break;
    case ACT_THRUSTER_JET:
        ConstructActor(SPR_THRUSTER_JET, x, y + 2, F, F, F, F, ActPrize, 0, 0, 0, 0, 4);
        break;
    case ACT_EXIT_TRANSPORTER:
        ConstructActor(SPR_TRANSPORTER_108, x, y, T, F, F, F, ActTransporter, 0, 0, 0, 0, 3);
        break;
    case ACT_HINT_GLOBE_1:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 1);
        break;
    case ACT_HINT_GLOBE_2:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 2);
        break;
    case ACT_HINT_GLOBE_3:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 3);
        break;
    case ACT_HINT_GLOBE_4:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 4);
        break;
    case ACT_HINT_GLOBE_5:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 5);
        break;
    case ACT_HINT_GLOBE_6:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 6);
        break;
    case ACT_HINT_GLOBE_7:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 7);
        break;
    case ACT_HINT_GLOBE_8:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 8);
        break;
    case ACT_HINT_GLOBE_9:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 9);
        break;
    case ACT_SPIKES_FLOOR_BENT:
        ConstructActor(SPR_SPIKES_FLOOR_BENT, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_MONUMENT:
        ConstructActor(SPR_MONUMENT, x, y, F, F, F, F, ActMonument, 0, 0, 0, 0, 0);
        break;
    case ACT_CYA_DIAMOND_FLOAT:
        ConstructActor(SPR_CYA_DIAMOND, x, y, F, F, F, F, ActPrize, 3, 2, 0, 0, 1);
        break;
    case ACT_RED_DIAMOND_FLOAT:
        ConstructActor(SPR_RED_DIAMOND, x, y, F, F, F, F, ActPrize, 2, 2, 0, 0, 1);
        break;
    case ACT_GRY_OCTAHED_FLOAT:
        ConstructActor(SPR_GRY_OCTAHEDRON, x, y, F, F, F, F, ActPrize, 2, 2, 0, 0, 1);
        break;
    case ACT_BLU_EMERALD_FLOAT:
        ConstructActor(SPR_BLU_EMERALD, x, y, F, F, F, F, ActPrize, 2, 2, 0, 0, 1);
        break;
    case ACT_TULIP_LAUNCHER:
        ConstructActor(SPR_TULIP_LAUNCHER, x, y, F, F, F, F, ActTulipLauncher, 0, 30, 0, 0, 0);
        break;
    case ACT_JUMP_PAD_CEIL:
        ConstructActor(SPR_JUMP_PAD, x, y, T, F, F, F, ActJumpPad, 0, 0, y + 1, y + 3, 1);
        break;
    case ACT_BARREL_HEADPHONES:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_HEADPHONES, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_HEADPHONES_FLOAT:
        ConstructActor(SPR_HEADPHONES, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_HEADPHONES:
        ConstructActor(SPR_HEADPHONES, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_FROZEN_DN:
        ConstructActor(SPR_FROZEN_DN, x, y, F, F, F, F, ActFrozenDN, 0, 0, 0, 0, 0);
        break;
    case ACT_BANANAS:
        ConstructActor(SPR_BANANAS, x, y + 1, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_BASKET_RED_LEAFY:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_RED_LEAFY, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_RED_LEAFY_FLOAT:
        ConstructActor(SPR_RED_LEAFY, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_RED_LEAFY:
        ConstructActor(SPR_RED_LEAFY, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_BASKET_BRN_PEAR:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_BRN_PEAR, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_BRN_PEAR_FLOAT:
        ConstructActor(SPR_BRN_PEAR, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_BRN_PEAR:
        ConstructActor(SPR_BRN_PEAR, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_BASKET_CANDY_CORN:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_CANDY_CORN, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_CANDY_CORN_FLOAT:
        ConstructActor(SPR_CANDY_CORN, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_CANDY_CORN:
        ConstructActor(SPR_CANDY_CORN, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_FLAME_PULSE_W:
        ConstructActor(SPR_FLAME_PULSE_W, x - 1, y, F, F, F, F, ActFlamePulse, 0, 0, 0, 0, 1);
        break;
    case ACT_FLAME_PULSE_E:
        ConstructActor(SPR_FLAME_PULSE_E, x, y, F, F, F, F, ActFlamePulse, 0, 0, 0, 0, 0);
        break;
    case ACT_RED_SLIME_THROB:
        ConstructActor(SPR_RED_SLIME, x, y + 1, F, F, F, F, ActRedGreenSlime, 0, 0, 0, 0, 0);
        break;
    case ACT_RED_SLIME_DRIP:
        ConstructActor(SPR_RED_SLIME, x, y + 1, F, T, F, F, ActRedGreenSlime, x, y + 1, 0, 0, 1);
        break;
    case ACT_HINT_GLOBE_10:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 10);
        break;
    case ACT_HINT_GLOBE_11:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 11);
        break;
    case ACT_HINT_GLOBE_12:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 12);
        break;
    case ACT_HINT_GLOBE_13:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 13);
        break;
    case ACT_HINT_GLOBE_14:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 14);
        break;
    case ACT_HINT_GLOBE_15:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 15);
        break;
    case ACT_SPEECH_OUCH:
        ConstructActor(SPR_SPEECH_OUCH, x, y, T, F, F, F, ActSpeechBubble, 0, 0, 0, 0, 0);
        break;
    case ACT_SPEECH_WHOA:
        ConstructActor(SPR_SPEECH_WHOA, x, y, T, F, F, F, ActSpeechBubble, 0, 0, 0, 0, 0);
        break;
    case ACT_SPEECH_UMPH:
        ConstructActor(SPR_SPEECH_UMPH, x, y, T, F, F, F, ActSpeechBubble, 0, 0, 0, 0, 0);
        break;
    case ACT_SPEECH_WOW_50K:
        ConstructActor(SPR_SPEECH_WOW_50K, x, y, T, F, F, F, ActSpeechBubble, 0, 0, 0, 0, 0);
        break;
    case ACT_EXIT_MONSTER_N:
        ConstructActor(SPR_EXIT_MONSTER_N, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_SMOKE_EMIT_SMALL:
        ConstructActor(SPR_248, x, y, F, F, F, F, ActSmokeEmitter, 0, 0, 0, 0, 1);
        break;
    case ACT_SMOKE_EMIT_LARGE:
        ConstructActor(SPR_249, x, y, F, F, F, F, ActSmokeEmitter, 1, 0, 0, 0, 0);
        break;
    case ACT_EXIT_LINE_HORIZ:
        ConstructActor(SPR_250, x, y, T, F, F, F, ActExitLineHorizontal, 0, 0, 0, 0, 0);
        break;
    case ACT_CABBAGE_HARDER:
        ConstructActor(SPR_CABBAGE, x, y, T, F, T, T, ActCabbage, 2, 0, 0, 0, 0);
        break;
    case ACT_RED_CRYSTAL_CEIL:
        ConstructActor(SPR_RED_CRYSTAL, x, y + 1, F, F, F, F, ActPrize, 1, 0, 0, 0, 6);
        break;
    case ACT_HINT_GLOBE_16:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 16);
        break;
    case ACT_HINT_GLOBE_17:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 17);
        break;
    case ACT_HINT_GLOBE_18:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 18);
        break;
    case ACT_HINT_GLOBE_19:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 19);
        break;
    case ACT_HINT_GLOBE_20:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 20);
        break;
    case ACT_HINT_GLOBE_21:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 21);
        break;
    case ACT_HINT_GLOBE_22:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 22);
        break;
    case ACT_HINT_GLOBE_23:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 23);
        break;
    case ACT_HINT_GLOBE_24:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 24);
        break;
    case ACT_HINT_GLOBE_25:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 25);
        break;
    case ACT_POWER_UP:
        ConstructActor(SPR_POWER_UP, x, y, F, T, T, F, ActPrize, 0, 0, 0, 1, 6);
        break;
    case ACT_STAR:
        ConstructActor(SPR_STAR, x, y, F, T, T, F, ActPrize, 0, 0, 0, 0, 4);
        break;
    case ACT_EP2_END_LINE:
        ConstructActor(SPR_265, x, y + 3, T, F, F, F, ActExitLineHorizontal, 1, 0, 0, 0, 0);
        break;
    default:
        return false;
    }

#undef F
#undef T

    return true;
}

/*
Add a new actor of the specified type at x,y. This function finds a free slot.
*/
void NewActor(word actor_type, word x, word y)
{
    Actor *act;
    word i;

    for (i = 0; i < numActors; i++) {
        act = actors + i;

        if (act->dead) {
            NewActorAtIndex(i, actor_type, x, y);

            if (actor_type == ACT_PARACHUTE_BALL) {
                act->forceactive = true;
            }

            return;
        }
    }

    if (numActors < MAX_ACTORS - 2) {
        act = actors + numActors;

        NewActorAtIndex(numActors, actor_type, x, y);

        if (actor_type == ACT_PARACHUTE_BALL) {
            act->forceactive = true;
        }

        numActors++;
    }
}

/*
Add sparkles to slippery areas of the map; add raindrops to empty areas of sky.
*/
void DrawRandomEffects(void)
{
    word x = random(SCROLLW) + scrollX;
    word y = random(SCROLLH) + scrollY;
    word maptile = GetMapTile(x, y);

    if (random(2U) != 0 && TILE_SLIPPERY(maptile)) {
        NewDecoration(SPR_SPARKLE_SLIPPERY, 5, x, y, DIR8_STATIONARY, 1);
    }

    if (hasRain) {
        y = scrollY + 1;

        if (GetMapTile(x, y) == TILE_EMPTY) {
            NewDecoration(SPR_RAINDROP, 1, x, y, DIR8_SOUTHWEST, 20);
        }
    }
}

static word numShards = MAX_SHARDS;

/*
Deactivate every element in the shards array, freeing them for re-use.
*/
void InitializeShards(void)
{
    word i;

    for (i = 0; i < numShards; i++) {
        shards[i].age = 0;
    }
}

/*
Insert the requested shard into the first free spot in the shards array.
*/
void NewShard(word sprite, word frame, word x, word y)
{
    /*
    INTERESTING: This never gets reset, so shard behavior is different for each
    run through the demo playback.
    */
    static word inclination = 0;
    word i;

    inclination++;
    if (inclination == 5) inclination = 0;

    for (i = 0; i < numShards; i++) {
        Shard *sh = shards + i;

        if (sh->age == 0) {
            sh->sprite = sprite;
            sh->x = x;
            sh->y = y;
            sh->frame = frame;
            sh->age = 1;
            sh->inclination = inclination;
            sh->bounced = false;

            break;
        }
    }
}

/*
Animate one frame for each active shard, expiring old ones in the process.
*/
void MoveAndDrawShards(void)
{
    word i;
    Shard *sh;

    for (i = 0; i < numShards; i++) {
        sh = shards + i;

        if (sh->age == 0) continue;

        if (sh->inclination == 0 || sh->inclination == 3) {
            if (TestSpriteMove(DIR4_EAST, sh->sprite, sh->frame, sh->x + 1, sh->y + 1) == MOVE_FREE) {
                sh->x++;

                if (sh->inclination == 3) {
                    sh->x++;
                }
            }
        } else if (sh->inclination == 1 || sh->inclination == 4) {
            if (TestSpriteMove(DIR4_WEST, sh->sprite, sh->frame, sh->x - 1, sh->y + 1) == MOVE_FREE) {
                sh->x--;

                if (sh->inclination == 4) {
                    sh->x--;
                }
            }
        }

restart:
        if (sh->age < 5) {
            sh->y -= 2;
        }

        if (sh->age == 5) {
            sh->y--;
        } else if (sh->age == 8) {
            if (TestSpriteMove(DIR4_SOUTH, sh->sprite, sh->frame, sh->x, sh->y + 1) != MOVE_FREE) {
                sh->age = 3;
                sh->y += 2;
                goto restart;
            }

            sh->y++;
        }

        if (sh->age >= 9) {
            if (sh->age > 16 && !IsSpriteVisible(sh->sprite, sh->frame, sh->x, sh->y)) {
                sh->age = 0;
                continue;
            }

            if (
                !sh->bounced &&
                TestSpriteMove(DIR4_SOUTH, sh->sprite, sh->frame, sh->x, sh->y + 1) != MOVE_FREE
            ) {
                sh->age = 3;
                sh->bounced = true;
                StartSound(SND_SHARD_BOUNCE);
                goto restart;
            }

            sh->y++;

            if (
                !sh->bounced &&
                TestSpriteMove(DIR4_SOUTH, sh->sprite, sh->frame, sh->x, sh->y + 1) != MOVE_FREE
            ) {
                sh->age = 3;
                sh->bounced = true;
                StartSound(SND_SHARD_BOUNCE);
                goto restart;
            }

            sh->y++;
        }

        if (sh->age == 1) {
            DrawSprite(sh->sprite, sh->frame, sh->x, sh->y, DRAWMODE_WHITE);
        } else {
            DrawSprite(sh->sprite, sh->frame, sh->x, sh->y, DRAWMODE_FLIPPED);
        }

        sh->age++;
        if (sh->age > 40) sh->age = 0;
    }
}

static word numExplosions = MAX_EXPLOSIONS;

/*
Deactivate every element in the explosions array, freeing them for re-use.
*/
void InitializeExplosions(void)
{
    word i;

    for (i = 0; i < numExplosions; i++) {
        explosions[i][0] = 0;
    }
}

/*
Insert the requested explosion into the first free spot in the explosions array.
*/
void NewExplosion(word x, word y)
{
    word i;

    for (i = 0; i < numExplosions; i++) {
        Explosion *ex = (Explosion *) explosions[i];

        if (ex->age != 0) continue;

        ex->age = 1;
        ex->x = x;
        ex->y = y + 2;

        StartSound(SND_EXPLOSION);

        break;
    }
}

/*
Animate one frame for each active explosion, expiring old ones in the process.
*/
void DrawExplosions(void)
{
    word i;

    for (i = 0; i < numExplosions; i++) {
        Explosion *ex = (Explosion *) explosions[i];

        if (ex->age == 0) continue;

#ifdef EXPLOSION_PALETTE
        if (paletteAnimationNum == PALANIM_EXPLOSIONS) {
            byte paletteColors[] = {
                MODE1_WHITE, MODE1_YELLOW, MODE1_WHITE, MODE1_BLACK, MODE1_YELLOW,
                MODE1_WHITE, MODE1_YELLOW, MODE1_BLACK, MODE1_BLACK
            };
            SetPaletteRegister(PALETTE_KEY_INDEX, paletteColors[ex->age - 1]);
        }
#endif  /* EXPLOSION_PALETTE */

        if (ex->age == 1) {
            NewDecoration(SPR_SPARKLE_LONG, 8, ex->x + 2, ex->y - 2, DIR8_STATIONARY, 1);
        }

        DrawSprite(SPR_EXPLOSION, (ex->age - 1) % 4, ex->x, ex->y, DRAWMODE_NORMAL);

        if (IsTouchingPlayer(SPR_EXPLOSION, (ex->age - 1) % 4, ex->x, ex->y)) {
            HurtPlayer();
        }

        ex->age++;
        if (ex->age == 9) {
            ex->age = 0;
            NewDecoration(SPR_SMOKE_LARGE, 6, ex->x + 1, ex->y - 1, DIR8_NORTH, 1);
        }
    }
}

/*
Return true if *any* explosion is touching the specified sprite.
*/
bool IsNearExplosion(word sprite, word frame, word x, word y)
{
    word i;

    for (i = 0; i < numExplosions; i++) {
        Explosion *ex;

        if (*explosions[i] == 0) continue;

        ex = (Explosion *) explosions[i];

        if (IsIntersecting(SPR_EXPLOSION, 0, ex->x, ex->y, sprite, frame, x, y)) {
            return true;
        }
    }

    return false;
}

static word numSpawners = MAX_SPAWNERS;

/*
Deactivate every element in the spawners array, freeing them for re-use.
*/
void InitializeSpawners(void)
{
    word i;

    for (i = 0; i < numSpawners; i++) {
        spawners[i].actor = ACT_BASKET_NULL;
    }
}

/*
Insert the requested spawner into the first free spot in the spawners array.
*/
void NewSpawner(word actor, word x, word y)
{
    word i;

    for (i = 0; i < numSpawners; i++) {
        Spawner *sp = spawners + i;

        if (sp->actor == ACT_BASKET_NULL) {
            sp->actor = actor;
            sp->x = x;
            sp->y = y;
            sp->age = 0;

            break;
        }
    }
}

/*
Animate one frame for each active spawner, expiring old ones in the process.

NOTE: This function only behaves correctly for actors whose sprite type number
matches the actor type number. While spawning, frame 0 of the sprite is used.
*/
void MoveAndDrawSpawners(void)
{
    word i;

    for (i = 0; i < numSpawners; i++) {
        Spawner *sp = spawners + i;

        if (sp->actor == ACT_BASKET_NULL) continue;

        sp->age++;

        if (
            (sp->y--, TestSpriteMove(DIR4_NORTH, sp->actor, 0, sp->x, sp->y)) != MOVE_FREE ||
            (
                sp->age < 9 &&
                (sp->y--, TestSpriteMove(DIR4_NORTH, sp->actor, 0, sp->x, sp->y)) != MOVE_FREE
            )
        ) {
            NewActor(sp->actor, sp->x, sp->y + 1);
            DrawSprite(sp->actor, 0, sp->x, sp->y + 1, DRAWMODE_NORMAL);
            sp->actor = ACT_BASKET_NULL;

        } else if (sp->age == 11) {
            NewActor(sp->actor, sp->x, sp->y);
            DrawSprite(sp->actor, 0, sp->x, sp->y, DRAWMODE_FLIPPED);
            sp->actor = ACT_BASKET_NULL;

        } else {
            DrawSprite(sp->actor, 0, sp->x, sp->y, DRAWMODE_FLIPPED);
        }
    }
}

static int numDecorations = MAX_DECORATIONS;

/*
Deactivate every element in the decorations array, freeing them for re-use.
*/
void InitializeDecorations(void)
{
    word i;

    for (i = 0; i < numDecorations; i++) {
        decorations[i].alive = false;
    }
}

/*
Insert the given decoration into the first free spot in the decorations array.
*/
void NewDecoration(
    word sprite, word numframes, word x, word y, word dir, word numtimes
) {
    word i;

    for (i = 0; i < numDecorations; i++) {
        Decoration *dec = decorations + i;

        if (!dec->alive) {
            dec->alive = true;
            dec->sprite = sprite;
            dec->numframes = numframes;
            dec->x = x;
            dec->y = y;
            dec->dir = dir;
            dec->numtimes = numtimes;

            decorationFrame[i] = 0;

            break;
        }
    }
}

/*
Animate one frame for each active decoration, expiring old ones in the process.
*/
void MoveAndDrawDecorations(void)
{
    int i;

    for (i = 0; i < numDecorations; i++) {
        Decoration *dec = decorations + i;

        if (!dec->alive) continue;

        /* Possible BUG: dec->numframes should be decorationFrame[i] instead. */
        if (IsSpriteVisible(dec->sprite, dec->numframes, dec->x, dec->y)) {
            if (dec->sprite != SPR_SPARKLE_SLIPPERY) {
                DrawSprite(dec->sprite, decorationFrame[i], dec->x, dec->y, DRAWMODE_NORMAL);
            } else {
                DrawSprite(dec->sprite, decorationFrame[i], dec->x, dec->y, DRAWMODE_IN_FRONT);
            }

            if (dec->sprite == SPR_RAINDROP) {
                dec->x--;
                dec->y += random(3);
            }

            dec->x += dir8X[dec->dir];
            dec->y += dir8Y[dec->dir];

            decorationFrame[i]++;
            if (decorationFrame[i] == dec->numframes) {
                decorationFrame[i] = 0;
                if (dec->numtimes != 0) {
                    dec->numtimes--;
                    if (dec->numtimes == 0) {
                        dec->alive = false;
                    }
                }
            }
        } else {
            dec->alive = false;
        }
    }
}

/*
Decide, somehow, if a pounce is valid. Not totally clear on this yet.
*/
bool PounceHelper(int recoil)
{
    if (playerDeadTime != 0 || playerDizzyLeft != 0) return false;

    if (
        /* 2nd `isPlayerRecoiling` test is pointless */
        (!isPlayerRecoiling || (isPlayerRecoiling && playerMomentumNorth < 2)) &&
        (((isPlayerFalling && playerFallTime >= 0) || playerJumpTime > 6) && isPounceReady)
    ) {
        playerMomentumSaved = playerMomentumNorth = recoil + 1;
        isPlayerRecoiling = true;

        ClearPlayerDizzy();

        if (recoil > 18) {
            isPlayerLongJumping = true;
        } else {
            isPlayerLongJumping = false;
        }

        pounceHintState = POUNCE_HINT_SEEN;

        if (recoil == 7) {
            pounceStreak++;
            if (pounceStreak == 10) {
                pounceStreak = 0;
                NewActor(ACT_SPEECH_WOW_50K, playerX - 1, playerY - 5);
            }
        } else {
            pounceStreak = 0;
        }

        return true;

    } else if (playerMomentumSaved - 2 < playerMomentumNorth && isPounceReady && isPlayerRecoiling) {
        ClearPlayerDizzy();

        if (playerMomentumNorth > 18) {
            isPlayerLongJumping = true;
        } else {
            isPlayerLongJumping = false;
        }

        pounceHintState = POUNCE_HINT_SEEN;

        return true;
    }

    return false;
}

/*
Cause the player pain, deduct health, and determine if the player becomes dead.
*/
void HurtPlayer(void)
{
    if (
        playerDeadTime != 0 || isGodMode || blockActionCmds || activeTransporter != 0 ||
        playerIsInvincible || isPlayerInPipe || playerHurtCooldown != 0
    ) return;

    playerClingDir = DIR4_NONE;

    if (!sawHurtBubble) {
        sawHurtBubble = true;

        NewActor(ACT_SPEECH_OUCH, playerX - 1, playerY - 5);

        if (pounceHintState == POUNCE_HINT_UNSEEN) {
            pounceHintState = POUNCE_HINT_QUEUED;
        }
    }

    playerHealth--;

    if (playerHealth == 0) {
        playerDeadTime = 1;
        scooterMounted = 0;
    } else {
        UpdateHealth();
        playerHurtCooldown = 44;
        StartSound(SND_PLAYER_HURT);
    }
}

/*
Add six pieces of pounce debris radiating outward from x,y.
*/
void NewPounceDecoration(word x, word y)
{
    NewDecoration(SPR_POUNCE_DEBRIS, 6, x + 1, y,     DIR8_SOUTHWEST, 2);
    NewDecoration(SPR_POUNCE_DEBRIS, 6, x + 3, y,     DIR8_SOUTHEAST, 2);
    NewDecoration(SPR_POUNCE_DEBRIS, 6, x + 4, y - 2, DIR8_EAST,      2);
    NewDecoration(SPR_POUNCE_DEBRIS, 6, x + 3, y - 4, DIR8_NORTHEAST, 2);
    NewDecoration(SPR_POUNCE_DEBRIS, 6, x + 1, y - 4, DIR8_NORTHWEST, 2);
    NewDecoration(SPR_POUNCE_DEBRIS, 6, x,     y - 2, DIR8_WEST,      2);
}

/*
Can the passed sprite/frame be destroyed by an explosion?

Return true if so, and handle special cases. Otherwise returns false. As a side
effect, adds a shard decoration and adds to the player's score if the sprite is
explodable.
*/
bool CanBeExploded(word sprite, word frame, word x, word y)
{
    switch (sprite) {
    case SPR_ARROW_PISTON_W:
    case SPR_ARROW_PISTON_E:
    case SPR_SPIKES_FLOOR:
    case SPR_SPIKES_FLOOR_RECIP:
    case SPR_SAW_BLADE:
    case SPR_CABBAGE:
    case SPR_SPEAR:
    case SPR_JUMPING_BULLET:
    case SPR_STONE_HEAD_CRUSHER:
    case SPR_GHOST:
    case SPR_MOON:
    case SPR_HEART_PLANT:
    case SPR_BABY_GHOST:
    case SPR_ROAMER_SLUG:
    case SPR_BABY_GHOST_EGG:
    case SPR_SHARP_ROBOT_FLOOR:
    case SPR_SHARP_ROBOT_CEIL:
    case SPR_CLAM_PLANT:
    case SPR_PARACHUTE_BALL:
    case SPR_SPIKES_E:
    case SPR_SPIKES_E_RECIP:
    case SPR_SPIKES_W:
    case SPR_SPARK:
    case SPR_EYE_PLANT:
    case SPR_RED_JUMPER:
    case SPR_SUCTION_WALKER:
    case SPR_SPIT_WALL_PLANT_E:
    case SPR_SPIT_WALL_PLANT_W:
    case SPR_SPITTING_TURRET:
    case SPR_RED_CHOMPER:
    case SPR_PINK_WORM:
    case SPR_HINT_GLOBE:
    case SPR_PUSHER_ROBOT:
    case SPR_SENTRY_ROBOT:
    case SPR_PINK_WORM_SLIME:
    case SPR_DRAGONFLY:
    case SPR_BIRD:
    case SPR_ROCKET:
    case SPR_74:  /* probably for ACT_BABY_GHOST_EGG_PROX; never happens */
    case SPR_84:  /* " " " ACT_CLAM_PLANT_CEIL " " " */
    case SPR_96:  /* " " " ACT_EYE_PLANT_CEIL " " " */
        if (sprite == SPR_HINT_GLOBE) {
            NewActor(ACT_SCORE_EFFECT_12800, x, y);
        }

        if (
            (sprite == SPR_SPIKES_FLOOR_RECIP || sprite == SPR_SPIKES_E_RECIP) &&
            frame == 2  /* retracted */
        ) return false;

        NewShard(sprite, frame, x, y);
        AddScoreForSprite(sprite);

        if (sprite == SPR_EYE_PLANT) {
            if (numEyePlants == 1) {
                NewActor(ACT_SPEECH_WOW_50K, playerX - 1, playerY - 5);
            }

            NewDecoration(SPR_SPARKLE_LONG, 8, x, y, DIR8_STATIONARY, 1);
            NewSpawner(ACT_BOMB_IDLE, x, y);

            numEyePlants--;
        }

        return true;
    }

    return false;
}

/*
Handle destruction of the passed barrel, and spawn an actor from its contents.
*/
void DestroyBarrel(word index)
{
    Actor *act = actors + index;

    act->dead = true;

    NewShard(act->data2, 0, act->x - 1, act->y);
    NewShard(act->data2, 1, act->x + 1, act->y - 1);
    NewShard(act->data2, 2, act->x + 3, act->y);
    NewShard(act->data2, 3, act->x + 2, act->y + 2);

    if (GameRand() % 2 != 0) {
        StartSound(SND_BARREL_DESTROY_1);
    } else {
        StartSound(SND_BARREL_DESTROY_2);
    }

    NewSpawner(act->data1, act->x + 1, act->y);

    if (numBarrels == 1) {
        NewActor(ACT_SPEECH_WOW_50K, playerX - 1, playerY - 5);
    }

    numBarrels--;
}

/*
Handle interactions between the player and the passed actor.

This handles the player pouncing on and damaging actors, actors touching and
damaging/affecting the player, and the player picking up prizes. Returns true if
the actor was destroyed/picked up or if this actor needs to be drawn in a non-
standard way.

It's not clear what benefit there is to passing sprite/frame/x/y instead of
reading it from the actor itself. Both methods are used interchangeably.
*/
bool TouchPlayer(word index, word sprite, word frame, word x, word y)
{
    Actor *act = actors + index;
    word width;
    register word height;
    register word offset;

    if (!IsSpriteVisible(sprite, frame, x, y)) return true;

    offset = *(actorInfoData + sprite) + (frame * 4);
    height = *(actorInfoData + offset);
    width = *(actorInfoData + offset + 1);

    isPounceReady = false;
    if (sprite == SPR_BOSS) {
        height = 7;
        if (
            (y - height) + 5 >= playerY &&
            y - height <= playerY && playerX + 2 >= x && (x + width) - 1 >= playerX
        ) {
            isPounceReady = true;
        }
    } else if (
        (playerFallTime > 3 ? 1 : 0) + (y - height) + 1 >= playerY &&
        y - height <= playerY && playerX + 2 >= x && (x + width) - 1 >= playerX &&
        scooterMounted == 0
    ) {
        isPounceReady = true;
    }

    switch (sprite) {
    case SPR_JUMP_PAD:
        if (act->data5 != 0) break;  /* ceiling-mounted */

        if (act->damagecooldown == 0 && PounceHelper(40)) {
            StartSound(SND_PLAYER_POUNCE);
            if (!sawJumpPadBubble) {
                sawJumpPadBubble = true;
                NewActor(ACT_SPEECH_WHOA, playerX - 1, playerY - 5);
            }
            act->data1 = 3;
        }
        return false;

    case SPR_JUMP_PAD_ROBOT:
        if (act->damagecooldown == 0 && PounceHelper(20)) {
            StartSound(SND_JUMP_PAD_ROBOT);
            act->data1 = 3;
        }
        return false;

    case SPR_CABBAGE:
        if (act->damagecooldown == 0 && PounceHelper(7)) {
            act->damagecooldown = 5;
            StartSound(SND_PLAYER_POUNCE);
            nextDrawMode = DRAWMODE_WHITE;
            act->data1--;
            if (act->data1 == 0) {
                act->dead = true;
                AddScoreForSprite(SPR_CABBAGE);
                NewPounceDecoration(act->x, act->y);
                return true;
            }
        } else if (act->damagecooldown == 0 && IsTouchingPlayer(sprite, frame, x, y)) {
            HurtPlayer();
        }
        return false;

    case SPR_BASKET:
    case SPR_BARREL:
        if (act->damagecooldown == 0 && PounceHelper(5)) {
            DestroyBarrel(index);
            AddScore(100);
            NewActor(ACT_SCORE_EFFECT_100, act->x, act->y);
            return true;
        }
        return false;

    case SPR_GHOST:
    case SPR_MOON:
        if (act->damagecooldown == 0 && PounceHelper(7)) {
            act->damagecooldown = 3;
            StartSound(SND_PLAYER_POUNCE);
            act->data5--;
            nextDrawMode = DRAWMODE_WHITE;
            if (act->data5 == 0) {
                act->dead = true;
                if (sprite == SPR_GHOST) {
                    NewActor(ACT_BABY_GHOST, act->x, act->y);
                }
                NewPounceDecoration(act->x - 1, act->y + 1);
                AddScoreForSprite(SPR_GHOST);
                return true;
            }
        } else if (act->damagecooldown == 0 && IsTouchingPlayer(sprite, frame, x, y)) {
            HurtPlayer();
        }
        return false;

    case SPR_BABY_GHOST:
    case SPR_SUCTION_WALKER:
    case SPR_BIRD:
        if (act->damagecooldown == 0 && PounceHelper(7)) {
            StartSound(SND_PLAYER_POUNCE);
            act->dead = true;
            NewPounceDecoration(act->x, act->y);
            AddScoreForSprite(act->sprite);
            return true;
        } else if (IsTouchingPlayer(sprite, frame, x, y)) {
            HurtPlayer();
        }
        return false;

    case SPR_BABY_GHOST_EGG:
    case SPR_74:  /* probably for ACT_BABY_GHOST_EGG_PROX; never happens */
        if (act->damagecooldown == 0 && PounceHelper(7)) {
            StartSound(SND_BGHOST_EGG_CRACK);
            if (act->data2 == 0) {
                act->data2 = 10;
            } else {
                act->data2 = 1;
            }
        }
        return false;

    case SPR_PARACHUTE_BALL:
        if (act->damagecooldown == 0 && PounceHelper(7)) {
            StartSound(SND_PLAYER_POUNCE);
            act->data3 = 0;
            act->damagecooldown = 3;
            act->data5--;
            if (act->data1 != 0 || act->fallspeed != 0) {
                act->data5 = 0;
            }
            if (act->data5 == 0) {
                NewPounceDecoration(act->x, act->y);
                act->dead = true;
                if (act->data1 > 0) {
                    AddScore(3200);
                    NewActor(ACT_SCORE_EFFECT_3200, act->x, act->y);
                } else if (act->fallspeed != 0) {
                    AddScore(12800);
                    NewActor(ACT_SCORE_EFFECT_12800, act->x, act->y);
                } else {
                    AddScore(800);
                }
            } else {
                nextDrawMode = DRAWMODE_WHITE;
                if (act->data1 == 0) {
                    act->data2 = 0;
                    act->data1 = (GameRand() % 2) + 1;
                }
            }
            return false;
        }
        /* Can't maintain parity with the original using an `else if` here. */
        if (act->damagecooldown == 0 && IsTouchingPlayer(sprite, frame, x, y)) {
            HurtPlayer();
        }
        return false;

    case SPR_RED_JUMPER:
        if (act->damagecooldown == 0 && PounceHelper(15)) {
            StartSound(SND_PLAYER_POUNCE);
            act->damagecooldown = 6;
            act->data5--;
            if (act->data5 == 0) {
                NewActor(ACT_STAR_FLOAT, act->x, act->y);
                NewPounceDecoration(act->x, act->y);
                act->dead = true;
                return true;
            }
            nextDrawMode = DRAWMODE_WHITE;
        } else if (act->damagecooldown == 0 && IsTouchingPlayer(sprite, frame, x, y)) {
            HurtPlayer();
        }
        return false;

    case SPR_SPITTING_TURRET:
    case SPR_RED_CHOMPER:
    case SPR_PUSHER_ROBOT:
        if (act->damagecooldown == 0 && PounceHelper(7)) {
            act->damagecooldown = 3;
            StartSound(SND_PLAYER_POUNCE);
            nextDrawMode = DRAWMODE_WHITE;
            if (sprite != SPR_RED_CHOMPER) {
                act->data5--;
            }
            if (act->data5 == 0 || sprite == SPR_RED_CHOMPER) {
                act->dead = true;
                AddScoreForSprite(act->sprite);
                NewPounceDecoration(act->x, act->y);
                return true;
            }
        } else if (act->damagecooldown == 0 && IsTouchingPlayer(sprite, frame, x, y)) {
            HurtPlayer();
        }
        return false;

    case SPR_PINK_WORM:
        if (act->damagecooldown == 0 && PounceHelper(7)) {
            AddScoreForSprite(SPR_PINK_WORM);
            StartSound(SND_PLAYER_POUNCE);
            NewPounceDecoration(act->x, act->y);
            act->dead = true;
            NewActor(ACT_PINK_WORM_SLIME, act->x, act->y);
            return true;
        }
        return false;

    case SPR_SENTRY_ROBOT:
        if (
            ((!areLightsActive && hasLightSwitch) || (areLightsActive && !hasLightSwitch)) &&
            act->damagecooldown == 0 && PounceHelper(15)
        ) {
            act->damagecooldown = 3;
            StartSound(SND_PLAYER_POUNCE);
            if (act->data1 != DIR2_WEST) {
                act->frame = 7;
            } else {
                act->frame = 8;
            }
        } else if (act->damagecooldown == 0 && IsTouchingPlayer(sprite, frame, x, y)) {
            HurtPlayer();
        }
        return false;

    case SPR_DRAGONFLY:
    case SPR_IVY_PLANT:
        if (act->damagecooldown == 0 && PounceHelper(7)) {
            pounceStreak = 0;
            StartSound(SND_PLAYER_POUNCE);
            act->damagecooldown = 5;
        } else if (act->damagecooldown == 0 && IsTouchingPlayer(sprite, frame, x, y)) {
            HurtPlayer();
        }
        return false;

    case SPR_ROCKET:
        if (act->x == playerX && act->damagecooldown == 0 && PounceHelper(5)) {
            StartSound(SND_PLAYER_POUNCE);
        }
        return false;

    case SPR_TULIP_LAUNCHER:
        if (act->private2 != 0) {
            act->private2--;
            if (act->private2 == 0) {
                isPlayerFalling = true;
                isPounceReady = true;
                act->damagecooldown == 0 && PounceHelper(20);
                StartSound(SND_PLAYER_POUNCE);
                blockMovementCmds = false;
                blockActionCmds = false;
                playerFallTime = 0;
                act->private1 = 1;
                act->data2 = 0;
                act->data1 = 1;
                playerY -= 2;
                if (!sawTulipLauncherBubble) {
                    sawTulipLauncherBubble = true;
                    NewActor(ACT_SPEECH_WHOA, playerX - 1, playerY - 5);
                }
            }
        } else if (
            act->private1 == 0 && act->x + 1 <= playerX && act->x + 5 >= playerX + 2 &&
            (act->y - 1 == playerY || act->y - 2 == playerY) && isPlayerFalling
        ) {
            act->private2 = 20;
            isPounceReady = false;
            playerMomentumNorth = 0;
            isPlayerFalling = false;
            blockMovementCmds = true;
            blockActionCmds = true;
            act->private1 = 1;
            act->data2 = 0;
            act->data1 = 1;
            StartSound(SND_TULIP_INGEST);
        }
        return false;

    case SPR_BOSS:
        if (
            act->private2 == 0
#ifdef HAS_ACT_BOSS
#   ifdef HARDER_BOSS
            && act->data5 != 20  /* why isn't this 18, like in ActBoss()? */
#   else
            && act->data5 != 12
#   endif  /* HARDER_BOSS */
#endif  /* ACT_BOSS */
        ) {
            if (act->damagecooldown == 0 && PounceHelper(7)) {
                StartSound(SND_PLAYER_POUNCE);
                act->data5++;
                act->private1 = 10;
                act->damagecooldown = 7;
                if (act->data1 != 2) {
                    act->data1 = 2;
                    act->data2 = 31;
                    act->data3 = 0;
                    act->data4 = 1;
                    act->weighted = false;
                    act->fallspeed = 0;
                }
                if (act->data5 == 4) {
                    NewShard(SPR_BOSS, 1, act->x, act->y - 4);
                    StartSound(SND_BOSS_DAMAGE);
                }
                NewDecoration(SPR_SMOKE, 6, act->x,     act->y, DIR8_NORTHWEST, 1);
                NewDecoration(SPR_SMOKE, 6, act->x + 3, act->y, DIR8_NORTHEAST, 1);
            } else if (act->damagecooldown == 0 && IsTouchingPlayer(sprite, frame, x, y)) {
                HurtPlayer();
            }
        }
        return true;
    }

    if (!IsTouchingPlayer(sprite, frame, x, y)) {
        return false;
    }

    switch (sprite) {
    case SPR_STAR:
        NewDecoration(SPR_SPARKLE_LONG, 8, x, y, DIR8_STATIONARY, 1);
        gameStars++;
        act->dead = true;
        StartSound(SND_BIG_PRIZE);
        AddScoreForSprite(sprite);
        NewActor(ACT_SCORE_EFFECT_200, x, y);
        UpdateStars();
        return true;

    case SPR_ARROW_PISTON_W:
    case SPR_ARROW_PISTON_E:
    case SPR_FIREBALL:
    case SPR_SAW_BLADE:
    case SPR_SPEAR:
    case SPR_FLYING_WISP:
    case SPR_TWO_TONS_CRUSHER:
    case SPR_JUMPING_BULLET:
    case SPR_STONE_HEAD_CRUSHER:
    case SPR_PYRAMID:
    case SPR_PROJECTILE:
    case SPR_SHARP_ROBOT_FLOOR:
    case SPR_SHARP_ROBOT_CEIL:
    case SPR_SPARK:
    case SPR_SMALL_FLAME:
    case SPR_6:  /* probably for ACT_FIREBALL_E; never happens */
    case SPR_48:  /* " " " ACT_PYRAMID_CEIL " " " */
    case SPR_50:  /* " " " ACT_PYRAMID_FLOOR " " " */
        HurtPlayer();
        if (act->sprite == SPR_PROJECTILE) {
            act->dead = true;
        }
        return false;

    case SPR_FLAME_PULSE_W:
    case SPR_FLAME_PULSE_E:
        if (act->frame > 1) {
            HurtPlayer();
        }
        return false;

    case SPR_GREEN_SLIME:
    case SPR_RED_SLIME:
        if (act->data5 != 0) {
            act->y = act->data2;
            act->data4 = 0;
            if (act->y > playerY - 4 || act->frame == 6) {
                HurtPlayer();
            }
            act->frame = 0;
            return false;
        }
        /* Can't maintain parity with the original using an `else if` here. */
        if (act->y > playerY - 4) {
            HurtPlayer();
        }
        return false;

    case SPR_CLAM_PLANT:
    case SPR_84:  /* probably for ACT_CLAM_PLANT_CEIL; never happens */
        if (act->frame != 0) {
            HurtPlayer();
        }
        return false;

    case SPR_HEAD_SWITCH_BLUE:
    case SPR_HEAD_SWITCH_RED:
    case SPR_HEAD_SWITCH_GREEN:
    case SPR_HEAD_SWITCH_YELLOW:
        if (act->frame == 0) {
            act->y--;
            act->frame = 1;
        }
        return false;

    case SPR_SPIKES_FLOOR:
    case SPR_SPIKES_FLOOR_RECIP:
    case SPR_SPIKES_E:
    case SPR_SPIKES_E_RECIP:
    case SPR_SPIKES_W:
        if (act->frame > 1) return true;
        HurtPlayer();
        return false;

    case SPR_POWER_UP:
        act->dead = true;
        StartSound(SND_BIG_PRIZE);
        NewDecoration(SPR_SPARKLE_SHORT, 4, act->x, act->y, DIR8_STATIONARY, 3);
        if (!sawHealthHint) {
            sawHealthHint = true;
            ShowHealthHint();
        }
        if (playerHealth <= playerMaxHealth) {
            playerHealth++;
            UpdateHealth();
            AddScore(100);
            NewActor(ACT_SCORE_EFFECT_100, act->x, act->y);
        } else {
            AddScore(12800);
            NewActor(ACT_SCORE_EFFECT_12800, act->x, act->y);
        }
        return true;

    case SPR_GRN_TOMATO:
    case SPR_RED_TOMATO:
    case SPR_YEL_PEAR:
    case SPR_ONION:
        act->dead = true;
        AddScore(200);
        NewActor(ACT_SCORE_EFFECT_200, x, y);
        NewDecoration(SPR_SPARKLE_SHORT, 4, act->x, act->y, DIR8_STATIONARY, 3);
        StartSound(SND_PRIZE);
        return true;

    case SPR_GRAPES:
    case SPR_DANCING_MUSHROOM:
    case SPR_BOTTLE_DRINK:
    case SPR_GRN_GOURD:
    case SPR_BLU_SPHERES:
    case SPR_POD:
    case SPR_PEA_PILE:
    case SPR_LUMPY_FRUIT:
    case SPR_HORN:
    case SPR_RED_BERRIES:
    case SPR_YEL_FRUIT_VINE:
    case SPR_HEADDRESS:
    case SPR_ROOT:
    case SPR_REDGRN_BERRIES:
    case SPR_RED_GOURD:
    case SPR_BANANAS:
    case SPR_RED_LEAFY:
    case SPR_BRN_PEAR:
    case SPR_CANDY_CORN:
        act->dead = true;
        if (
            sprite == SPR_YEL_FRUIT_VINE || sprite == SPR_BANANAS ||
            sprite == SPR_GRAPES || sprite == SPR_RED_BERRIES
        ) {
            AddScore(800);
            NewActor(ACT_SCORE_EFFECT_800, x, y);
        } else {
            AddScore(400);
            NewActor(ACT_SCORE_EFFECT_400, x, y);
        }
        NewDecoration(SPR_SPARKLE_SHORT, 4, act->x, act->y, DIR8_STATIONARY, 3);
        StartSound(SND_PRIZE);
        return true;

    case SPR_HAMBURGER:
        act->dead = true;
        AddScore(12800);
        NewActor(SPR_SCORE_EFFECT_12800, x, y);
        NewDecoration(SPR_SPARKLE_SHORT, 4, act->x, act->y, DIR8_STATIONARY, 3);
        StartSound(SND_PRIZE);
        if (playerMaxHealth < 5) playerMaxHealth++;
        if (!sawHamburgerBubble) {
            NewActor(ACT_SPEECH_WHOA, playerX - 1, playerY - 5);
            sawHamburgerBubble = true;
        }
        UpdateHealth();
        return true;

    case SPR_EXIT_SIGN:
        winLevel = true;
        return false;

    case SPR_HEART_PLANT:
        act->data1 = 1;
        HurtPlayer();
        return false;

    case SPR_BOMB_IDLE:
        if (playerBombs <= 8) {
            act->dead = true;
            playerBombs++;
            sawBombHint = true;
            AddScore(100);
            NewActor(ACT_SCORE_EFFECT_100, act->x, act->y);
            UpdateBombs();
            NewDecoration(SPR_SPARKLE_SHORT, 4, act->x, act->y, DIR8_STATIONARY, 3);
            StartSound(SND_PRIZE);
            return true;
        }
        return false;

    case SPR_FOOT_SWITCH:
        if (act->data1 < 4 && act->data4 == 0) {
            isPlayerFalling = true;
            ClearPlayerDizzy();
            PounceHelper(3);
            act->data1++;
            if (act->data2 == 0) {
                act->data3 = 64;
                act->data2 = 1;
            } else {
                act->data3 = 0;
            }
            act->data4 = 1;
        }
        return false;

    case SPR_ROAMER_SLUG:
        {  /* for scope */
            word i = GameRand() % 4;
            if (act->damagecooldown == 0) {
                word gifts[] = {ACT_RED_GOURD, ACT_RED_TOMATO, ACT_CLR_DIAMOND, ACT_GRN_EMERALD};
                act->damagecooldown = 10;
                if (PounceHelper(7)) {
                    StartSound(SND_PLAYER_POUNCE);
                } else {
                    playerClingDir = DIR4_NONE;
                }
                NewSpawner(gifts[i], act->x, act->y + 1);
                StartSound(SND_ROAMER_GIFT);
                nextDrawMode = DRAWMODE_WHITE;
                act->data2--;
                if (act->data2 == 0) {
                    act->dead = true;
                    NewPounceDecoration(act->x - 1, act->y + 1);
                }
            }
        }
        return false;

    case SPR_PIPE_CORNER_N:
    case SPR_PIPE_CORNER_S:
    case SPR_PIPE_CORNER_W:
    case SPR_PIPE_CORNER_E:
        if (isPlayerInPipe) {
            switch (sprite) {
            case SPR_PIPE_CORNER_N:
                SetPlayerPush(DIR8_NORTH, 100, 2, PLAYER_HIDDEN, false, false);
                break;

            case SPR_PIPE_CORNER_S:
                SetPlayerPush(DIR8_SOUTH, 100, 2, PLAYER_HIDDEN, false, false);
                break;

            case SPR_PIPE_CORNER_W:
                SetPlayerPush(DIR8_WEST, 100, 2, PLAYER_HIDDEN, false, false);
                break;

            case SPR_PIPE_CORNER_E:
                SetPlayerPush(DIR8_EAST, 100, 2, PLAYER_HIDDEN, false, false);
                break;
            }
            StartSound(SND_PIPE_CORNER_HIT);
        }
        return true;

    case SPR_PIPE_END:
        if (act->data2 == 0 && (act->y + 3 == playerY || act->y + 2 == playerY)) {
            if (isPlayerPushed) {
                playerX = act->x;
                SET_PLAYER_DIZZY();
                isPlayerInPipe = false;
                ClearPlayerPush();
                if (!sawPipeBubble) {
                    sawPipeBubble = true;
                    NewActor(ACT_SPEECH_WHOA, playerX - 1, playerY - 5);
                }
            }
        } else if (
            (!isPlayerFalling || isPlayerRecoiling) && (cmdJump || isPlayerRecoiling) &&
            act->x == playerX && (act->y + 3 == playerY || act->y + 2 == playerY)
        ) {
            isPlayerInPipe = true;
        }
        return false;

    case SPR_TRANSPORTER_108:
        if (transporterTimeLeft == 0) {
            if (act->x <= playerX && act->x + 4 >= playerX + 2 && act->y == playerY) {
                if (cmdNorth) {
                    activeTransporter = act->data5;
                    transporterTimeLeft = 15;
                    isPlayerFalling = false;
                }
                isPlayerNearTransporter = true;
            } else {
                isPlayerNearTransporter = false;
            }
        }
        return true;

    case SPR_SPIKES_FLOOR_BENT:
    case SPR_SPIT_WALL_PLANT_E:
    case SPR_SPIT_WALL_PLANT_W:
    case SPR_PINK_WORM_SLIME:
    case SPR_THRUSTER_JET:
        HurtPlayer();
        return false;

    case SPR_SCOOTER:
        if (isPlayerFalling && (act->y == playerY || act->y + 1 == playerY)) {
            scooterMounted = 4;
            StartSound(SND_PLAYER_LAND);
            ClearPlayerPush();
            isPlayerFalling = false;
            playerFallTime = 0;
            isPlayerRecoiling = false;
            isPounceReady = false;
            playerMomentumNorth = 0;
            pounceStreak = 0;
            if (!sawScooterBubble) {
                sawScooterBubble = true;
                NewActor(ACT_SPEECH_WHOA, playerX - 1, playerY - 5);
            }
        }
        return false;

    case SPR_EXIT_MONSTER_W:
        if (act->data4 != 0) {
            act->data4--;
            if (act->data4 == 0) {
                winLevel = true;
                act->frame = 0;
                return false;
            }
            act->frame = 0;
        } else if (act->data1 != 0 && act->y == playerY && act->x <= playerX) {
            act->frame = 0;
            act->data5 = 0;
            act->data4 = 5;
            blockActionCmds = true;
            blockMovementCmds = true;
            StartSound(SND_EXIT_MONSTER_INGEST);
        }
        return true;

    case SPR_ROTATING_ORNAMENT:
    case SPR_GRN_EMERALD:
    case SPR_CLR_DIAMOND:
        act->dead = true;
        NewDecoration(SPR_SPARKLE_SHORT, 4, act->x, act->y, DIR8_STATIONARY, 3);
        AddScore(3200);
        NewActor(ACT_SCORE_EFFECT_3200, x, y);
        StartSound(SND_PRIZE);
        return true;

    case SPR_BLU_CRYSTAL:
    case SPR_RED_CRYSTAL:
        act->dead = true;
        NewDecoration(SPR_SPARKLE_SHORT, 4, act->x, act->y, DIR8_STATIONARY, 3);
        AddScore(1600);
        NewActor(ACT_SCORE_EFFECT_1600, x, y);
        StartSound(SND_PRIZE);
        return true;

    case SPR_CYA_DIAMOND:
    case SPR_RED_DIAMOND:
    case SPR_GRY_OCTAHEDRON:
    case SPR_BLU_EMERALD:
    case SPR_HEADPHONES:
        act->dead = true;
        NewDecoration(SPR_SPARKLE_SHORT, 4, act->x, act->y, DIR8_STATIONARY, 3);
        AddScore(800);
        NewActor(ACT_SCORE_EFFECT_800, x, y);
        StartSound(SND_PRIZE);
        return true;

    case SPR_BEAR_TRAP:
        if (act->data2 == 0 && act->x == playerX && act->y == playerY) {
            act->data2 = 1;
            blockMovementCmds = true;
            if (!sawBearTrapBubble) {
                sawBearTrapBubble = true;
                NewActor(ACT_SPEECH_UMPH, playerX - 1, playerY - 5);
            }
            return false;
        }
        /* FALL THROUGH! */

    case SPR_EXIT_PLANT:
        if (
            act->frame == 0 && act->x < playerX && act->x + 5 > playerX &&
            act->y - 2 > playerY && act->y - 5 < playerY && isPlayerFalling
        ) {
            act->data5 = 1;
            blockMovementCmds = true;
            blockActionCmds = true;
            act->frame = 1;
            StartSound(SND_EXIT_MONSTER_INGEST);
        }
        return false;

    case SPR_INVINCIBILITY_CUBE:
        act->dead = true;
        NewActor(ACT_INVINCIBILITY_BUBB, playerX - 1, playerY + 1);
        NewDecoration(SPR_SPARKLE_LONG, 8, x, y, DIR8_STATIONARY, 1);
        /* BUG: score effect is spawned, but no score given */
        NewActor(ACT_SCORE_EFFECT_12800, x, y);
        StartSound(SND_BIG_PRIZE);
        return true;

    case SPR_MONUMENT:
        if (!sawMonumentBubble) {
            sawMonumentBubble = true;
            NewActor(ACT_SPEECH_UMPH, playerX - 1, playerY - 5);
        }
        if (act->x == playerX + 2) {
            SetPlayerPush(DIR8_WEST, 5, 2, PLAYER_BASE_EAST + PLAYER_PUSHED, false, true);
            StartSound(SND_PUSH_PLAYER);
        } else if (act->x + 2 == playerX) {
            SetPlayerPush(DIR8_EAST, 5, 2, PLAYER_BASE_WEST + PLAYER_PUSHED, false, true);
            StartSound(SND_PUSH_PLAYER);
        }
        return false;

    case SPR_JUMP_PAD:  /* ceiling-mounted only */
        if (
            act->data5 != 0 && act->damagecooldown == 0 && scooterMounted == 0 &&
            (!isPlayerFalling || isPlayerRecoiling)
        ) {
            act->damagecooldown = 2;
            StartSound(SND_PLAYER_POUNCE);
            act->data1 = 3;
            playerMomentumNorth = 0;
            isPlayerRecoiling = false;
            isPlayerFalling = true;
            playerFallTime = 4;
            playerJumpTime = 0;
        }
        return false;

#ifdef HAS_ACT_EXIT_MONSTER_N
    case SPR_EXIT_MONSTER_N:
        blockActionCmds = true;
        blockMovementCmds = true;
        act->data1++;
        if (act->frame != 0) {
            winLevel = true;
        } else if (act->data1 == 3) {
            act->frame++;
        }
        if (act->data1 > 1) {
            playerY = act->y;
            playerY = act->y;  /* why this again and never X? */
            isPlayerFalling = false;
        }
        return false;
#endif  /* ACT_E2L1_EXIT_MOUTH */
    }

    return false;
}

/*
Handle all common per-frame tasks for one actor.

Tasks include application of gravity, killing actors that fall off the map,
wakeup/cooldown management, interactions between the actor and the player/
explosions, calling the actor tick function, and typical-case sprite drawing.
*/
void ProcessActor(word index)
{
    Actor *act = actors + index;

    if (act->dead) return;

    if (act->y > mapHeight + SCROLLH + 3) {
        act->dead = true;
        return;
    }

    nextDrawMode = DRAWMODE_NORMAL;

    if (act->damagecooldown != 0) act->damagecooldown--;

    if (IsSpriteVisible(act->sprite, act->frame, act->x, act->y)) {
        if (act->stayactive) {
            act->forceactive = true;
        }
    } else if (!act->forceactive) {
        return;
    } else {
        nextDrawMode = DRAWMODE_HIDDEN;
    }

    if (act->weighted) {
        if (TestSpriteMove(DIR4_SOUTH, act->sprite, 0, act->x, act->y) != MOVE_FREE) {
            act->y--;
            act->fallspeed = 0;
        }

        if (TestSpriteMove(DIR4_SOUTH, act->sprite, 0, act->x, act->y + 1) == MOVE_FREE) {
            if (act->fallspeed < 5) act->fallspeed++;

            if (act->fallspeed > 1 && act->fallspeed < 6) {
                act->y++;
            }

            if (act->fallspeed == 5) {
                if (TestSpriteMove(DIR4_SOUTH, act->sprite, 0, act->x, act->y + 1) != MOVE_FREE) {
                    act->fallspeed = 0;
                } else {
                    act->y++;
                }
            }
        } else {
            act->fallspeed = 0;
        }
    }

    if (IsSpriteVisible(act->sprite, act->frame, act->x, act->y)) {
        nextDrawMode = DRAWMODE_NORMAL;
    }

    act->tickfunc(index);

    if (
        IsNearExplosion(act->sprite, act->frame, act->x, act->y) &&
        CanBeExploded(act->sprite, act->frame, act->x, act->y)
    ) {
        act->dead = true;
    } else if (
        !TouchPlayer(index, act->sprite, act->frame, act->x, act->y) &&
        nextDrawMode != DRAWMODE_HIDDEN
    ) {
        DrawSprite(act->sprite, act->frame, act->x, act->y, nextDrawMode);
    }
}

/*
Reset per-frame global actor variables, and process each actor in turn.
*/
void MoveAndDrawActors(void)
{
    word i;

    isPlayerNearHintGlobe = false;

    for (i = 0; i < numActors; i++) {
        ProcessActor(i);
    }

    if (mysteryWallTime != 0) mysteryWallTime = 0;
}

/*
Prepare the video hardware for the possibility of showing the in-game menu.

Eventually handles keyboard/joystick input, returning a status byte that
indicates if the game should end or if a level change is needed.
*/
byte ProcessGameInputHelper(word page, byte demo)
{
    byte status;

    EGA_MODE_LATCHED_WRITE();

    SelectDrawPage(page);

    status = ProcessGameInput(demo);

    SelectDrawPage(!page);

    return status;
}

/*
Fill the backdrop table (which is four 40x18 tables, in two planes, interlaced).
The exact magic here is not fully understood yet.
*/
void InitializeBackdropTable(void)
{
    int x, y;
    word val = 0;

    for (y = 0; y < 18; y++) {
        for (x = 0; x < 40; x++) {
            *(backdropTable + (y * 80) + x       ) =
            *(backdropTable + (y * 80) + x +   40) = val;

            *(backdropTable + (y * 80) + x + 1480) =
            *(backdropTable + (y * 80) + x + 1440) = val;

            val += 8;
        }
    }
}

/*
Respond to keyboard controller interrupts and update the global key states.
*/
void interrupt KeyboardInterruptService(void)
{
    lastScancode = inportb(0x0060);

    outportb(0x0061, inportb(0x0061) | 0x80);
    outportb(0x0061, inportb(0x0061) & ~0x80);

    if (lastScancode != SCANCODE_EXTENDED) {
        if ((lastScancode & 0x80) != 0) {
            isKeyDown[lastScancode & 0x7f] = false;
        } else {
            isKeyDown[lastScancode] = true;
        }
    }

    if (isKeyDown[SCANCODE_ALT] && isKeyDown[SCANCODE_C] && isDebugMode) {
        savedInt9();
    } else {
        outportb(0x0020, 0x20);
    }
}

/*
Update the programmable interval timer with the next PC speaker sound chunk.
This is also the central pacemaker for game clock ticks.
*/
void PCSpeakerService(void)
{
    static word soundCursor = 0;

    gameTickCount++;

    if (isNewSound) {
        isNewSound = false;
        soundCursor = 0;
        enableSpeaker = true;
    }

    if (*(soundDataPtr[activeSoundIndex] + soundCursor) == END_SOUND) {
        enableSpeaker = false;
        activeSoundPriority = 0;

        outportb(0x0061, inportb(0x0061) & ~0x02);
    }

    if (enableSpeaker) {
        word sample = *(soundDataPtr[activeSoundIndex] + soundCursor);

        if (sample == 0 && isSoundEnabled) {
            outportb(0x0061, inportb(0x0061) & ~0x03);
        } else if (isSoundEnabled) {
            outportb(0x0043, 0xb6);
            outportb(0x0042, (byte) (sample & 0x00ff));
            outportb(0x0042, (byte) (sample >> 8));
            outportb(0x0061, inportb(0x0061) | 0x03);
        }

        soundCursor++;
    } else {
        outportb(0x0061, inportb(0x0061) & ~0x02);
    }
}

/*
Write a page of "PC-ASCII" text and attributes to the screen, then emit a series
of newline characters to ensure the text cursor clears the bottom of what was
just written.
*/
void DrawFullscreenText(char *entry_name)
{
    FILE *fp = GroupEntryFp(entry_name);
    byte *dest = MK_FP(0xb800, 0);

    fread(backdropTable, 4000, 1, fp);
    movmem(backdropTable, dest, 4000);
    printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");  /* 22 of these */
}

/*
Exit the program cleanly.

Saves the configuration file, restores the keyboard interrupt handler, graphics,
and AdLib. Removes the temporary save file, displays a text page, and returns to
DOS.
*/
void ExitClean(void)
{
    SaveConfigurationData(JoinPath(writePath, FILENAME_BASE ".CFG"));

    disable();
    setvect(9, savedInt9);
    enable();

    FadeOut();

    textmode(C80);

    /* Silence PC speaker */
    outportb(0x0061, inportb(0x0061) & ~0x02);

    StopAdLib();

    /* BUG: `writePath` is not considered here! */
    remove(FILENAME_BASE ".SVT");

    DrawFullscreenText(EXIT_TEXT_PAGE);

    exit(EXIT_SUCCESS);
}

/*
Read the tile attribute data into the designated memory location.
*/
void LoadTileAttributeData(char *entry_name)
{
    FILE *fp = GroupEntryFp(entry_name);

    fread(tileAttributeData, 7000, 1, fp);
    fclose(fp);
}

/*
Read the masked tile data into the designated memory location.
*/
void LoadMaskedTileData(char *entry_name)
{
    FILE *fp = GroupEntryFp(entry_name);

    fread(maskedTileData, 40000U, 1, fp);
    fclose(fp);
}

/*
Ensure the system has an EGA adapter, and verify there's enough free memory. If
either are not true, exit back to DOS.

If execution got this far, one memory check already succeeded: DOS reads the
16-bit (little-endian) value at offset Ah in the EXE header, which is the number
of 16-byte paragraphs that need to be allocated in addition to the total size of
the executable's load image. The total size of both values averages around 141
KiB depending on the episode, which is already decucted from the amount reported
by coreleft() here.

The memory test in this function checks for the *additional* amount that will be
dynamically allocated during Startup(). There will be up to fifteen total calls
to malloc(), requesting a maximum total of 389,883 bytes of memory. Each
separate call for `malloc(bytes)` really subtracts `((bytes + 0x17) >> 4) << 4`
from what's reported by coreleft(), so the final amount ends up being 390,080.

NOTE: This function assumes the video mode has already been set to Dh.
*/
void ValidateSystem(void)
{
    union REGS x86regs;
    dword bytesfree;

    /* INT 10h,Fh: Get Video State - Puts current video mode number into AL. */
    x86regs.h.ah = 0x0f;
    int86(0x10, &x86regs, &x86regs);
    if (x86regs.h.al != 0x0d) {
        textmode(C80);
        printf("EGA Card not detected!\n");
        /* BUG: AdLib isn't shut down here; hoses DOSBox */
        exit(EXIT_SUCCESS);
    }

    bytesfree = coreleft();

    if (
        /* Empirically, real usage values are 383,072 and 7,008. */
        ( isAdLibPresent && bytesfree < 383792L + 7000) ||
        (!isAdLibPresent && bytesfree < 383792L)
    ) {
        StopAdLib();
        textmode(C80);
        DrawFullscreenText("NOMEMORY.mni");
        exit(EXIT_SUCCESS);
    }
}

/*
Start with a bang. Set video mode, initialize the AdLib, install the keyboard
service, initialize the PC speaker state, allocate enough memory, then show the
pre-title image.

While the pre-title image is up, load the config file, then allocate and
generate/load a whole slew of data to each arena. This takes a noticable amount
of time on a slower machine. At the end, move onto displaying the copyright
screen.

Nothing allocated here is ever explicitly freed. DOS gets it all back when the
program eventually exits.
*/
void Startup(void)
{
    /*
    Mode Dh is EGA/VGA 40x25 characters with an 8x8 pixel box.
    320x200 graphics resolution, 16 color, 8 pages, A000 screen address
    */
    SetVideoMode(0x0d);

    StartAdLib();

    ValidateSystem();

    totalMemFreeBefore = coreleft();

    disable();

    savedInt9 = getvect(9);
    setvect(9, KeyboardInterruptService);

    enableSpeaker = false;
    activeSoundPriority = 0;
    gameTickCount = 0;
    isSoundEnabled = true;

    enable();

    miscData = malloc(35000U);

    DrawFullscreenImage(IMAGE_PRETITLE);

    WaitSoft(200);

    LoadConfigurationData(JoinPath(writePath, FILENAME_BASE ".CFG"));

    SetBorderColorRegister(MODE1_BLACK);

    InitializeBackdropTable();

    maskedTileData = malloc(40000U);

    soundData1 = malloc((word)GroupEntryLength("SOUNDS.MNI"));
    soundData2 = malloc((word)GroupEntryLength("SOUNDS2.MNI"));
    soundData3 = malloc((word)GroupEntryLength("SOUNDS3.MNI"));

    LoadSoundData("SOUNDS.MNI",  soundData1, 0);
    LoadSoundData("SOUNDS2.MNI", soundData2, 23);
    LoadSoundData("SOUNDS3.MNI", soundData3, 46);

    playerTileData = malloc((word)GroupEntryLength("PLAYERS.MNI"));

    mapData.b = malloc(WORD_MAX);

    /*
    16-bit nightmare here. Each actor data chunk is limited to 65,535 bytes,
    the first two chunks are full, and the last one gets the low word remander
    plus the two bytes that didn't fit into the first two chunks. If you find
    yourself asking "hey, what happens if there aren't two-and-a-bit chunks
    worth of data in the file" you get a shiny gold star.
    */
    actorTileData[0] = malloc(WORD_MAX);
    actorTileData[1] = malloc(WORD_MAX);
    actorTileData[2] = malloc((word)GroupEntryLength("ACTORS.MNI") + 2);

    LoadGroupEntryData("STATUS.MNI", actorTileData[0], 7296);
    CopyTilesToEGA(actorTileData[0], 7296 / 4, 0x8000);

    LoadGroupEntryData("TILES.MNI", actorTileData[0], 64000U);
    CopyTilesToEGA(actorTileData[0], 64000U / 4, 0x4000);

    LoadActorTileData("ACTORS.MNI");

    LoadGroupEntryData("PLAYERS.MNI", playerTileData, (word)GroupEntryLength("PLAYERS.MNI"));

    actorInfoData = malloc((word)GroupEntryLength("ACTRINFO.MNI"));
    LoadInfoData("ACTRINFO.MNI", actorInfoData, (word)GroupEntryLength("ACTRINFO.MNI"));

    playerInfoData = malloc((word)GroupEntryLength("PLYRINFO.MNI"));
    LoadInfoData("PLYRINFO.MNI", playerInfoData, (word)GroupEntryLength("PLYRINFO.MNI"));

    cartoonInfoData = malloc((word)GroupEntryLength("CARTINFO.MNI"));
    LoadInfoData("CARTINFO.MNI", cartoonInfoData, (word)GroupEntryLength("CARTINFO.MNI"));

    fontTileData = malloc(4000);
    LoadFontTileData("FONTS.MNI", fontTileData, 4000);

    if (isAdLibPresent) {
        tileAttributeData = malloc(7000);
        LoadTileAttributeData("TILEATTR.MNI");
    }

    totalMemFreeAfter = coreleft();

    ClearScreen();

    ShowCopyright();

    isJoystickReady = false;
}

/*
Clear the screen, then redraw the in-game status bar onto both video pages.
*/
void ClearGameScreen(void)
{
    SelectDrawPage(0);
    RedrawStaticGameScreen();

    SelectDrawPage(1);
    RedrawStaticGameScreen();
}

/*
Cancel any active push the player may be experiencing.
*/
void ClearPlayerPush(void)
{
    isPlayerPushed = false;
    playerPushDir = DIR8_STATIONARY;
    playerPushMaxTime = 0;
    playerPushTime = 0;
    playerPushSpeed = 0;
    playerForceFrame = PLAYER_WALK_1;
    isPlayerRecoiling = false;
    playerMomentumNorth = 0;
    canCancelPlayerPush = false;
    isPlayerFalling = true;
    playerFallTime = 0;
}

/*
Push the player in a direction, for a maximum number of frames, at a certain
speed. The player sprite can be overridden, and the push can be configured to
pass through walls. The ability to "jump out" of a push is available, but this
is never used in the game.
*/
void SetPlayerPush(
    word dir, word max_count, word speed, word force_frame, bool can_cancel,
    bool stop_at_wall
) {
    playerPushDir = dir;
    playerPushMaxTime = max_count;
    playerPushTime = 0;
    playerPushSpeed = speed;
    playerForceFrame = force_frame;
    canCancelPlayerPush = can_cancel;
    isPlayerPushed = true;
    scooterMounted = 0;
    stopPlayerPushAtWall = stop_at_wall;
    isPlayerRecoiling = false;
    playerMomentumNorth = 0;

    ClearPlayerDizzy();
}

/*
Push the player for one frame. Stop if the player hits the edge of the map or a
wall (if enabled), or if the push expires.
*/
void MovePlayerPush(void)
{
    word i;
    bool wallhit = false;

    if (!isPlayerPushed) return;

    if (cmdJump && canCancelPlayerPush) {
        isPlayerPushed = false;
        return;
    }

    for (i = 0; i < playerPushSpeed; i++) {
        if (
            dir8X[playerPushDir] + playerX > 0 &&
            dir8X[playerPushDir] + playerX + 2 < mapWidth
        ) {
            playerX += dir8X[playerPushDir];
        }

        playerY += dir8Y[playerPushDir];

        if (
            dir8X[playerPushDir] + scrollX > 0 &&
            dir8X[playerPushDir] + scrollX < mapWidth - (SCROLLW - 1)
        ) {
            scrollX += dir8X[playerPushDir];
        }

        if (dir8Y[playerPushDir] + scrollY > 2) {
            scrollY += dir8Y[playerPushDir];
        }

        if (stopPlayerPushAtWall && (
            TestPlayerMove(DIR4_WEST,  playerX, playerY) != MOVE_FREE ||
            TestPlayerMove(DIR4_EAST,  playerX, playerY) != MOVE_FREE ||
            TestPlayerMove(DIR4_NORTH, playerX, playerY) != MOVE_FREE ||
            TestPlayerMove(DIR4_SOUTH, playerX, playerY) != MOVE_FREE
        )) {
            wallhit = true;
            break;
        }
    }

    if (wallhit) {
        /* Rather than precompute, move into the wall then back up one step. */
        playerX -= dir8X[playerPushDir];
        playerY -= dir8Y[playerPushDir];
        scrollX -= dir8X[playerPushDir];
        scrollY -= dir8Y[playerPushDir];

        ClearPlayerPush();
    } else {
        playerPushTime++;
        if (playerPushTime >= playerPushMaxTime) {
            ClearPlayerPush();
        }
    }
}

/*
This is the hairiest function in the entire game. Best of luck to you.
*/
void MovePlayer(void)
{
    static word idlecount = 0;
    static int jumptable[] = {-2, -1, -1, -1, -1, -1, -1, 0, 0, 0};
    static word movecount = 0;
    static word bombcooldown = 0;
    word horizmove;
    register word southmove = 0;
    register bool clingslip = false;

    canPlayerCling = false;

    if (
        playerDeadTime != 0 || activeTransporter != 0 || scooterMounted != 0 ||
        playerDizzyLeft != 0 || blockActionCmds
    ) return;

    movecount++;

    MovePlayerPush();

    if (isPlayerPushed) {
        playerClingDir = DIR4_NONE;
        return;
    }

    if (playerClingDir != DIR4_NONE) {
        word clingtarget;

        if (playerClingDir == DIR4_WEST) {
            clingtarget = GetMapTile(playerX - 1, playerY - 2);
        } else {
            clingtarget = GetMapTile(playerX + 3, playerY - 2);
        }

        if (TILE_SLIPPERY(clingtarget) && TILE_CAN_CLING(clingtarget)) {
            if (TestPlayerMove(DIR4_SOUTH, playerX, playerY + 1) != MOVE_FREE) {
                playerClingDir = DIR4_NONE;
            } else {
                playerY++;
                clingslip = true;

                if (playerClingDir == DIR4_WEST) {
                    clingtarget = GetMapTile(playerX - 1, playerY - 2);
                } else {
                    clingtarget = GetMapTile(playerX + 3, playerY - 2);
                }

                if (!TILE_SLIPPERY(clingtarget) && !TILE_CAN_CLING(clingtarget)) {
                    playerClingDir = DIR4_NONE;
                    clingslip = false;
                }
            }
        } else if (!TILE_CAN_CLING(clingtarget)) {
            playerClingDir = DIR4_NONE;
        }
    }

    if (playerClingDir == DIR4_NONE) {
        if (!cmdBomb) {
            bombcooldown = 0;
        }

        if (cmdBomb && bombcooldown == 0) {
            bombcooldown = 2;
        }

        if (bombcooldown != 0 && bombcooldown != 1) {
            bombcooldown--;
            if (bombcooldown == 1) {
                bool nearblocked, farblocked;
                if (playerBaseFrame == PLAYER_BASE_WEST) {
                    nearblocked = TILE_BLOCK_WEST(GetMapTile(playerX - 1, playerY - 2));
                    farblocked  = TILE_BLOCK_WEST(GetMapTile(playerX - 2, playerY - 2));
                    if (playerBombs == 0 && !sawBombHint) {
                        sawBombHint = true;
                        ShowBombHint();
                    } else if (!nearblocked && !farblocked && playerBombs > 0) {
                        NewActor(ACT_BOMB_ARMED, playerX - 2, playerY - 2);
                        playerBombs--;
                        UpdateBombs();
                        StartSound(SND_PLACE_BOMB);
                    } else {
                        StartSound(SND_NO_BOMBS);
                    }
                } else {
                    nearblocked = TILE_BLOCK_EAST(GetMapTile(playerX + 3, playerY - 2));
                    farblocked  = TILE_BLOCK_EAST(GetMapTile(playerX + 4, playerY - 2));
                    if (playerBombs == 0 && !sawBombHint) {
                        sawBombHint = true;
                        ShowBombHint();
                    }
                    /* INCONSISTENCY: This is not fused to the previous `if`, while
                    west is. This affects bomb sound after hint is dismissed. */
                    if (!nearblocked && !farblocked && playerBombs > 0) {
                        NewActor(ACT_BOMB_ARMED, playerX + 3, playerY - 2);
                        playerBombs--;
                        UpdateBombs();
                        StartSound(SND_PLACE_BOMB);
                    } else {
                        StartSound(SND_NO_BOMBS);
                    }
                }
            }
        } else {
            cmdBomb = false;
        }
    }

    if (
        playerJumpTime == 0 && cmdBomb && !isPlayerFalling && playerClingDir == DIR4_NONE &&
        (!cmdJump || cmdJumpLatch)
    ) {
        if (cmdWest) {
            playerFaceDir = DIR4_WEST;
            playerBombDir = DIR4_WEST;
            playerBaseFrame = PLAYER_BASE_WEST;
        } else if (cmdEast) {
            playerFaceDir = DIR4_EAST;
            playerBombDir = DIR4_EAST;
            playerBaseFrame = PLAYER_BASE_EAST;
        } else if (playerFaceDir == DIR4_WEST) {
            playerBombDir = DIR4_WEST;
        } else if (playerFaceDir == DIR4_EAST) {
            playerBombDir = DIR4_EAST;
        }
    } else {
        playerBombDir = DIR4_NONE;
        TestPlayerMove(DIR4_SOUTH, playerX, playerY + 1);  /* used for side effects */
        if (!isPlayerSlidingEast || !isPlayerSlidingWest) {
            if (isPlayerSlidingWest) {
                if (playerClingDir == DIR4_NONE) {
                    playerX--;
                }
                if (
                    TestPlayerMove(DIR4_SOUTH, playerX, playerY + 1) == MOVE_FREE &&
                    playerClingDir == DIR4_NONE
                ) {
                    playerY++;
                }
                if (playerY - scrollY > 14) {
                    scrollY++;
                }
                if (playerX - scrollX < 12 && scrollX > 0) {
                    scrollX--;
                }
                playerClingDir = DIR4_NONE;
            }
            if (isPlayerSlidingEast) {
                if (playerClingDir == DIR4_NONE) {
                    playerX++;
                }
                if (
                    TestPlayerMove(DIR4_SOUTH, playerX, playerY + 1) == MOVE_FREE &&
                    playerClingDir == DIR4_NONE
                ) {
                    playerY++;
                }
                if (playerY - scrollY > 14) {
                    scrollY++;
                }
                if (playerX - scrollX > 23 && mapWidth - SCROLLW > scrollX) {
                    scrollX++;
                }
                playerClingDir = DIR4_NONE;
            }
        }
        if (cmdWest && playerClingDir == DIR4_NONE && !cmdEast) {
            southmove = TestPlayerMove(DIR4_SOUTH, playerX, playerY + 1);
            if (playerFaceDir == DIR4_WEST) {
                playerX--;
            } else {
                playerFaceDir = DIR4_WEST;
            }
            playerBaseFrame = PLAYER_BASE_WEST;
            if (playerX < 1) {
                playerX++;
            } else {
                horizmove = TestPlayerMove(DIR4_WEST, playerX, playerY);
                if (horizmove == MOVE_BLOCKED) {
                    playerX++;
                    if (TestPlayerMove(DIR4_SOUTH, playerX, playerY + 1) == MOVE_FREE && canPlayerCling) {
                        playerClingDir = DIR4_WEST;
                        isPlayerRecoiling = false;
                        playerMomentumNorth = 0;
                        StartSound(SND_PLAYER_CLING);
                        isPlayerFalling = false;
                        playerJumpTime = 0;
                        playerFallTime = 0;
                        if (cmdJump) {
                            cmdJumpLatch = true;
                        } else {
                            cmdJumpLatch = false;
                        }
                    }
                }
            }
            if (horizmove == MOVE_SLOPED) {  /* WHOA uninitialized */
                playerY--;
            } else if (
                southmove == MOVE_SLOPED &&
                TestPlayerMove(DIR4_SOUTH, playerX, playerY + 1) == MOVE_FREE
            ) {
                isPlayerFalling = false;
                playerJumpTime = 0;
                playerY++;
            }
        }
        if (cmdEast && playerClingDir == DIR4_NONE && !cmdWest) {
            southmove = TestPlayerMove(DIR4_SOUTH, playerX, playerY + 1);
            if (playerFaceDir == DIR4_EAST) {
                playerX++;
            } else {
                playerFaceDir = DIR4_EAST;
            }
            playerBaseFrame = PLAYER_BASE_EAST;
            if (mapWidth - 4 < playerX) {
                playerX--;
            } else {
                horizmove = TestPlayerMove(DIR4_EAST, playerX, playerY);
                if (horizmove == MOVE_BLOCKED) {
                    playerX--;
                    if (TestPlayerMove(DIR4_SOUTH, playerX, playerY + 1) == MOVE_FREE && canPlayerCling) {
                        playerClingDir = DIR4_EAST;
                        isPlayerRecoiling = false;
                        playerMomentumNorth = 0;
                        StartSound(SND_PLAYER_CLING);
                        playerJumpTime = 0;
                        isPlayerFalling = false;
                        playerFallTime = 0;
                        if (cmdJump) {
                            cmdJumpLatch = true;
                        } else {
                            cmdJumpLatch = false;
                        }
                    }
                }
            }
            if (horizmove == MOVE_SLOPED) {  /* WHOA uninitialized */
                playerY--;
            } else if (
                southmove == MOVE_SLOPED &&
                TestPlayerMove(DIR4_SOUTH, playerX, playerY + 1) == MOVE_FREE
            ) {
                isPlayerFalling = false;
                playerFallTime = 0;
                playerY++;
            }
        }
        if (playerClingDir != DIR4_NONE && cmdJumpLatch && !cmdJump) {
            cmdJumpLatch = false;
        }
        if (
            playerMomentumNorth != 0 ||
            (cmdJump && !isPlayerFalling && !cmdJumpLatch) ||
            (playerClingDir != DIR4_NONE && cmdJump && !cmdJumpLatch)
        ) {
            bool newjump;

            if (isPlayerRecoiling && playerMomentumNorth > 0) {
                playerMomentumNorth--;
                if (playerMomentumNorth < 10) {
                    isPlayerLongJumping = false;
                }
                if (playerMomentumNorth > 1) {
                    playerY--;
                }
                if (playerMomentumNorth > 13) {
                    playerMomentumNorth--;
                    if (TestPlayerMove(DIR4_NORTH, playerX, playerY) == MOVE_FREE) {
                        playerY--;
                    } else {
                        isPlayerLongJumping = false;
                    }
                }
                newjump = false;
                if (playerMomentumNorth == 0) {
                    playerJumpTime = 0;
                    isPlayerRecoiling = false;
                    playerFallTime = 0;
                    isPlayerLongJumping = false;
                    cmdJumpLatch = true;
                }
            } else {
                if (playerClingDir == DIR4_WEST) {
                    if (cmdWest) {
                        playerClingDir = DIR4_NONE;
                    } else if (cmdEast) {
                        playerBaseFrame = PLAYER_BASE_EAST;
                    }
                }
                if (playerClingDir == DIR4_EAST) {
                    if (cmdEast) {
                        playerClingDir = DIR4_NONE;
                    } else if (cmdWest) {
                        playerBaseFrame = PLAYER_BASE_WEST;
                    }
                }
                if (playerClingDir == DIR4_NONE) {
                    playerY += jumptable[playerJumpTime];
                }
                if (playerJumpTime == 0 && TestPlayerMove(DIR4_NORTH, playerX, playerY + 1) != MOVE_FREE) {
                    playerY++;
                }
                isPlayerRecoiling = false;
                newjump = true;
            }

            playerClingDir = DIR4_NONE;

            if (TestPlayerMove(DIR4_NORTH, playerX, playerY) != MOVE_FREE) {
                if (playerJumpTime > 0 || isPlayerRecoiling) {
                    StartSound(SND_PLAYER_HIT_HEAD);
                }
                playerMomentumNorth = 0;
                isPlayerRecoiling = false;
                if (TestPlayerMove(DIR4_NORTH, playerX, playerY + 1) != MOVE_FREE) {
                    playerY++;
                }
                playerY++;
                isPlayerFalling = true;
                if (cmdJump) {
                    cmdJumpLatch = true;
                }
                playerFallTime = 0;
                isPlayerLongJumping = false;
            } else if (newjump && playerJumpTime == 0) {
                StartSound(SND_PLAYER_JUMP);
            }
            if (!isPlayerRecoiling && playerJumpTime++ > 6) {
                isPlayerFalling = true;
                if (cmdJump) {
                    cmdJumpLatch = true;
                }
                playerFallTime = 0;
            }
        }
        if (playerClingDir == DIR4_NONE) {
            if (isPlayerFalling && cmdJump) {
                cmdJumpLatch = true;
            }
            if ((!cmdJump || cmdJumpLatch) && !isPlayerFalling) {
                isPlayerFalling = true;
                playerFallTime = 0;
            }
            if (isPlayerFalling && !isPlayerRecoiling) {
                playerY++;
                if (TestPlayerMove(DIR4_SOUTH, playerX, playerY) != MOVE_FREE) {
                    if (playerFallTime != 0) {
                        StartSound(SND_PLAYER_LAND);
                    }
                    isPlayerFalling = false;
                    playerY--;
                    playerJumpTime = 0;
                    if (cmdJump) {
                        cmdJumpLatch = true;
                    } else {
                        cmdJumpLatch = false;
                    }
                    playerFallTime = 0;
                }
                if (playerFallTime > 3) {
                    playerY++;
                    scrollY++;
                    if (TestPlayerMove(DIR4_SOUTH, playerX, playerY) != MOVE_FREE) {
                        StartSound(SND_PLAYER_LAND);
                        isPlayerFalling = false;
                        playerY--;
                        scrollY--;
                        playerJumpTime = 0;
                        if (cmdJump) {
                            cmdJumpLatch = true;
                        } else {
                            cmdJumpLatch = false;
                        }
                        playerFallTime = 0;
                    }
                }
                if (playerFallTime < 25) playerFallTime++;
            }
            if (isPlayerFalling && playerFallTime == 1 && !isPlayerRecoiling) {
                playerY--;
            }
        }
    }

    if (playerBombDir != DIR4_NONE) {
        idlecount = 0;
        playerFrame = PLAYER_CROUCH;
    } else if ((cmdNorth || cmdSouth) && !cmdWest && !cmdEast && !isPlayerFalling && !cmdJump) {
        idlecount = 0;
        if (cmdNorth && !isPlayerNearTransporter && !isPlayerNearHintGlobe) {
            if (scrollY > 0 && playerY - scrollY < SCROLLH - 1) {
                scrollY--;
            }
            if (clingslip) {
                scrollY++;
            }
            if (playerClingDir != DIR4_NONE) {
                playerFrame = PLAYER_CLING_NORTH;
            } else {
                playerFrame = PLAYER_LOOK_NORTH;
            }
        } else if (cmdSouth) {
            if (scrollY + 3 < playerY) {
                scrollY++;
                if ((clingslip || isPlayerSlidingEast || isPlayerSlidingWest) && scrollY + 3 < playerY) {
                    scrollY++;
                }
            }
            if (playerClingDir != DIR4_NONE) {
                playerFrame = PLAYER_CLING_SOUTH;
            } else {
                playerFrame = PLAYER_LOOK_SOUTH;
            }
        }
        return;
    } else if (playerClingDir == DIR4_WEST) {
        idlecount = 0;
        if (cmdEast) {
            playerFrame = PLAYER_CLING_OPPOSITE;
        } else {
            playerFrame = PLAYER_CLING;
        }
    } else if (playerClingDir == DIR4_EAST) {
        idlecount = 0;
        if (cmdWest) {
            playerFrame = PLAYER_CLING_OPPOSITE;
        } else {
            playerFrame = PLAYER_CLING;
        }
    } else if ((isPlayerFalling && !isPlayerRecoiling) || (playerJumpTime > 6 && !isPlayerFalling)) {
        idlecount = 0;
        if (!isPlayerRecoiling && !isPlayerFalling && playerJumpTime > 6) {
            playerFrame = PLAYER_FALL;
        } else if (playerFallTime >= 10 && playerFallTime < 25) {
            playerFrame = PLAYER_FALL_LONG;
        } else if (playerFallTime == 25) {
            playerFrame = PLAYER_FALL_SEVERE;
            SET_PLAYER_DIZZY();
        } else if (!isPlayerFalling) {
            playerFrame = PLAYER_JUMP;
        } else {
            playerFrame = PLAYER_FALL;
        }
    } else if ((cmdJump && !cmdJumpLatch) || isPlayerRecoiling) {
        idlecount = 0;
        playerFrame = PLAYER_JUMP;
        if (isPlayerRecoiling && isPlayerLongJumping) {
            playerFrame = PLAYER_JUMP_LONG;
        }
        if (playerMomentumNorth < 3 && isPlayerRecoiling) {
            playerFrame = PLAYER_FALL;
        }
    } else if (cmdWest == cmdEast) {
        byte rnd = random(50);
        playerFrame = PLAYER_STAND;
        if (!cmdWest && !cmdEast && !isPlayerFalling) {
            idlecount++;
            if (idlecount > 100 && idlecount < 110) {
                playerFrame = PLAYER_LOOK_NORTH;
            } else if (idlecount > 139 && idlecount < 150) {
                playerFrame = PLAYER_LOOK_SOUTH;
            } else if (idlecount == 180) {
                playerFrame = PLAYER_SHAKE_1;
            } else if (idlecount == 181) {
                playerFrame = PLAYER_SHAKE_2;
            } else if (idlecount == 182) {
                playerFrame = PLAYER_SHAKE_3;
            } else if (idlecount == 183) {
                playerFrame = PLAYER_SHAKE_2;
            } else if (idlecount == 184) {
                playerFrame = PLAYER_SHAKE_1;
            } else if (idlecount == 185) {
                idlecount = 0;
            }
        }
        if (
            playerFrame != PLAYER_LOOK_NORTH &&
            playerFrame != PLAYER_LOOK_SOUTH &&
            (rnd == 0 || rnd == 31)
        ) {
            playerFrame = PLAYER_STAND_BLINK;
        }
    } else if (!isPlayerFalling) {
        idlecount = 0;
        if (movecount % 2 != 0) {
            if (playerFrame % 2 != 0) {
                StartSound(SND_PLAYER_FOOTSTEP);
            }
            playerFrame++;
        }
        if (playerFrame > PLAYER_WALK_4) playerFrame = PLAYER_WALK_1;
    }
    if (playerY - scrollY > 14) {
        scrollY++;
    }
    if (clingslip && playerY - scrollY > 14) {
        scrollY++;
    } else {
        if (playerMomentumNorth > 10 && playerY - scrollY < 7 && scrollY > 0) {
            scrollY--;
        }
        if (playerY - scrollY < 7 && scrollY > 0) {
            scrollY--;
        }
    }
    if (playerX - scrollX > 23 && mapWidth - SCROLLW > scrollX && mapYPower > 5) {
        scrollX++;
    } else if (playerX - scrollX < 12 && scrollX > 0) {
        scrollX--;
    }
}

/*
Handle player movement and bomb placement while the player is riding a scooter.
*/
void MovePlayerScooter(void)
{
    static word bombcooldown = 0;

    ClearPlayerDizzy();

    isPounceReady = false;
    playerMomentumNorth = 0;
    isPlayerFalling = false;

    if (playerDeadTime != 0) return;

    if (scooterMounted > 1) {
        cmdNorth = true;
        scooterMounted--;
    } else if (cmdJump) {
        cmdJumpLatch = true;
        scooterMounted = 0;
        isPlayerFalling = true;
        playerFallTime = 1;
        isPlayerRecoiling = false;
        isPounceReady = true;
        PounceHelper(9);
        playerMomentumNorth -= 2;
        StartSound(SND_PLAYER_JUMP);
        return;
    }

    if (cmdWest && !cmdEast) {
        if (playerBaseFrame == PLAYER_BASE_WEST) {
            playerX--;
        }

        playerBaseFrame = PLAYER_BASE_WEST;
        playerFrame = PLAYER_STAND;

        if (playerX < 1) playerX++;

        if (
            TestPlayerMove(DIR4_WEST, playerX, playerY) != MOVE_FREE ||
            TestPlayerMove(DIR4_WEST, playerX, playerY + 1) != MOVE_FREE
        ) {
            playerX++;
        }

        if (playerX % 2 != 0) {
            NewDecoration(SPR_SCOOTER_EXHAUST, 4, playerX + 3, playerY + 1, DIR8_EAST, 1);
            StartSound(SND_SCOOTER_PUTT);
        }
    }

    if (cmdEast && !cmdWest) {
        if (playerBaseFrame != PLAYER_BASE_WEST) {
            playerX++;
        }

        playerBaseFrame = PLAYER_BASE_EAST;
        playerFrame = PLAYER_STAND;

        if (mapWidth - 4 < playerX) playerX--;

        if (
            TestPlayerMove(DIR4_EAST, playerX, playerY) != MOVE_FREE ||
            TestPlayerMove(DIR4_EAST, playerX, playerY + 1) != MOVE_FREE
        ) {
            playerX--;
        }

        if (playerX % 2 != 0) {
            NewDecoration(SPR_SCOOTER_EXHAUST, 4, playerX - 1, playerY + 1, DIR8_WEST, 1);
            StartSound(SND_SCOOTER_PUTT);
        }
    }

    if (cmdNorth && !cmdSouth) {
        playerFrame = PLAYER_LOOK_NORTH;

        if (playerY > 4) playerY--;

        if (TestPlayerMove(DIR4_NORTH, playerX, playerY) != MOVE_FREE) {
            playerY++;
        }

        if (playerY % 2 != 0) {
            NewDecoration(SPR_SCOOTER_EXHAUST, 4, playerX + 1, playerY + 1, DIR8_SOUTH, 1);
            StartSound(SND_SCOOTER_PUTT);
        }
    } else if (cmdSouth && !cmdNorth) {
        playerFrame = PLAYER_LOOK_SOUTH;

        if (mapHeight + 17 > playerY) playerY++;

        if (TestPlayerMove(DIR4_SOUTH, playerX, playerY + 1) != MOVE_FREE) {
            playerY--;
        }
    } else {
        playerFrame = PLAYER_STAND;
    }

    if (!cmdBomb) {
        bombcooldown = 0;
    }

    if (cmdBomb && bombcooldown == 0) {
        bombcooldown = 1;
        playerFrame = PLAYER_CROUCH;
    }

    if (bombcooldown != 0 && bombcooldown != 2) {
        playerFrame = PLAYER_CROUCH;

        if (bombcooldown != 0) {  /* redundant; outer condition makes this always true */
            register bool nearblocked, farblocked;
            bombcooldown = 2;

            if (playerBaseFrame == PLAYER_BASE_WEST) {
                nearblocked = TILE_BLOCK_WEST(GetMapTile(playerX - 1, playerY - 2));
                farblocked  = TILE_BLOCK_WEST(GetMapTile(playerX - 2, playerY - 2));
                if (!nearblocked && !farblocked && playerBombs > 0) {
                    NewActor(ACT_BOMB_ARMED, playerX - 2, playerY - 2);
decrement:
                    playerBombs--;
                    UpdateBombs();
                    StartSound(SND_PLACE_BOMB);
                } else {
                    StartSound(SND_NO_BOMBS);
                }
            } else {
                nearblocked = TILE_BLOCK_EAST(GetMapTile(playerX + 3, playerY - 2));
                farblocked  = TILE_BLOCK_EAST(GetMapTile(playerX + 4, playerY - 2));
                if (!nearblocked && !farblocked && playerBombs > 0) {
                    NewActor(ACT_BOMB_ARMED, playerX + 3, playerY - 2);
                    /* HACK: For the life of me, I can't get the compiler's
                    deduplicator to preserve the target branch without this. */
                    goto decrement;
                } else {
                    StartSound(SND_NO_BOMBS);
                }
            }
        }
    } else {
        cmdBomb = false;
    }

    if (playerY - scrollY > 14) {
        scrollY++;
    } else {
        if (playerMomentumNorth > 10 && playerY - scrollY < 7 && scrollY > 0) {
            scrollY--;
        }

        if (playerY - scrollY < 7 && scrollY > 0) {
            scrollY--;
        }
    }

    if (playerX - scrollX > 23 && mapWidth - SCROLLW > scrollX) {
        scrollX++;
    } else if (playerX - scrollX < 12 && scrollX > 0) {
        scrollX--;
    }
}

/*
If the player has a head-shake queued up, perform it here.
*/
void ProcessPlayerDizzy(void)
{
    static word shakeframes[] = {
        PLAYER_SHAKE_1, PLAYER_SHAKE_2, PLAYER_SHAKE_3, PLAYER_SHAKE_2,
        PLAYER_SHAKE_1, PLAYER_SHAKE_2, PLAYER_SHAKE_3, PLAYER_SHAKE_2,
        PLAYER_SHAKE_1
    };

    if (playerClingDir != DIR4_NONE) {
        queuePlayerDizzy = false;
        playerDizzyLeft = 0;
    }

    if (queuePlayerDizzy && TestPlayerMove(DIR4_SOUTH, playerX, playerY + 1) != MOVE_FREE) {
        queuePlayerDizzy = false;
        playerDizzyLeft = 8;
        StartSound(SND_PLAYER_LAND);
    }

    if (playerDizzyLeft != 0) {
        playerFrame = shakeframes[playerDizzyLeft];
        playerDizzyLeft--;
        isPlayerFalling = false;

        if (playerDizzyLeft > 8) {
            /* There's no clear way how this can ever happen */
            ClearPlayerDizzy();
        }
    }
}

/*
Draw the player sprite, as well as any reactions to external factors. Also
responsible for determining if the player fell off the map, and drawing the
different player death animations.

Returns true if the level needs to restart due to player death, and false during
normal gameplay.
*/
bbool DrawPlayerHelper(void)
{
    static byte speechframe = 0;

    if (mapHeight + SCROLLH + 3 < playerY && playerDeadTime == 0) {
        playerFallDeadTime = 1;
        playerDeadTime = 1;

        if (mapHeight + SCROLLH + 4 == playerY) {
            playerY++;
        }

        speechframe++;
        if (speechframe == 5) speechframe = 0;
    }

    if (playerFallDeadTime != 0) {
        playerFallDeadTime++;

        if (playerFallDeadTime == 2) {
            StartSound(SND_PLAYER_HURT);
        }

        while (playerFallDeadTime < 12) {
            WaitHard(2);
            playerFallDeadTime++;
        }

        if (playerFallDeadTime == 13) {
            StartSound(SND_PLAYER_DEATH);
        }

        if (playerFallDeadTime > 12 && playerFallDeadTime < 19) {
            DrawSprite(
                SPR_SPEECH_MULTI, speechframe,
                playerX - 1, (playerY - playerFallDeadTime) + 13, DRAWMODE_IN_FRONT
            );
        }

        if (playerFallDeadTime > 18) {
            DrawSprite(
                SPR_SPEECH_MULTI, speechframe,
                playerX - 1, playerY - 6, DRAWMODE_IN_FRONT
            );
        }

        if (playerFallDeadTime > 30) {
            LoadGameState('T');
            SwitchLevel(levelNum);
            playerFallDeadTime = 0;
            return true;
        }

    } else if (playerDeadTime == 0) {
        if (playerHurtCooldown == 44) {
            DrawPlayer(playerBaseFrame + PLAYER_PAIN, playerX, playerY, DRAWMODE_WHITE);
        } else if (playerHurtCooldown > 40) {
            DrawPlayer(playerBaseFrame + PLAYER_PAIN, playerX, playerY, DRAWMODE_NORMAL);
        }

        if (playerHurtCooldown != 0) playerHurtCooldown--;

        if (playerHurtCooldown < 41) {
            if (!isPlayerPushed) {
                DrawPlayer(playerBaseFrame + playerFrame, playerX, playerY, DRAWMODE_NORMAL);
            } else {
                DrawPlayer(playerForceFrame, playerX, playerY, DRAWMODE_NORMAL);
            }
        }

    } else if (playerDeadTime < 10) {
        if (playerDeadTime == 1) {
            StartSound(SND_PLAYER_HURT);
        }

        playerDeadTime++;
        DrawPlayer((playerDeadTime % 2) + PLAYER_DEAD_1, playerX - 1, playerY, DRAWMODE_IN_FRONT);

    } else if (playerDeadTime > 9) {
        if (scrollY > 0 && playerDeadTime < 12) {
            scrollY--;
        }

        if (playerDeadTime == 10) {
            StartSound(SND_PLAYER_DEATH);
        }

        playerY--;
        playerDeadTime++;
        DrawPlayer((playerDeadTime % 2) + PLAYER_DEAD_1, playerX - 1, playerY, DRAWMODE_IN_FRONT);

        if (playerDeadTime > 36) {
            LoadGameState('T');
            SwitchLevel(levelNum);
            return true;
        }
    }

    return false;
}

/*
Wait indefinitely for any key to be pressed and released, then return the
scancode of that key.
*/
byte WaitForAnyKey(void)
{
    lastScancode = SCANCODE_NULL;  /* will get modified by the keyboard interrupt service */

    while ((lastScancode & 0x80) == 0)
        ;  /* VOID */

    return lastScancode & ~0x80;
}

/*
Return true if any key is currently pressed, regardless of which key it is.
*/
bbool IsAnyKeyDown(void)
{
    return !(inportb(0x0060) & 0x80);
}

/*
Append a relative `file` name to an absolute `dir` name, producing a complete
absolute file name. Length of the returned string is limited to 80 bytes max,
which is never a problem in DOS.
*/
char *JoinPath(char *dir, char *file)
{
    int dstoff;
    word srcoff;

    if (*dir == '\0') return file;

    for (dstoff = 0; *(dir + dstoff) != '\0'; dstoff++) {
        *(joinPathBuffer + dstoff) = *(dir + dstoff);
    }

    *(joinPathBuffer + dstoff++) = '\\';

    for (srcoff = 0; *(file + srcoff) != '\0'; srcoff++) {
        *(joinPathBuffer + dstoff++) = *(file + srcoff);
    }

    /* BUG: Output is not properly null-terminated. */

    return joinPathBuffer;
}

/*
Load game state from a save file. The slot number is a single character, '1'
through '9', or the letter 'T' for the temporary save file. Returns true if the
load was successful, or false if the specified save file didn't exist. Also
capable of exiting the game if a manipulated save file is loaded.
*/
bbool LoadGameState(char slot_char)
{
    static char *filename = FILENAME_BASE ".SV ";
    FILE *fp;
    word checksum;

    *(filename + SAVE_SLOT_INDEX) = slot_char;

    fp = fopen(JoinPath(writePath, filename), "rb");
    if (fp == NULL) {
        fclose(fp);  /* Nice. */

        return false;
    }

    playerHealth = getw(fp);
    fread(&gameScore, 4, 1, fp);
    gameStars = getw(fp);
    levelNum = getw(fp);
    playerBombs = getw(fp);
    playerMaxHealth = getw(fp);
    usedCheatCode = getw(fp);
    sawBombHint = getw(fp);
    pounceHintState = getw(fp);
    sawHealthHint = getw(fp);

    checksum = playerHealth + (word)gameStars + levelNum + playerBombs + playerMaxHealth;
    if (getw(fp) != checksum) {
        ShowAlteredFileError();
        ExitClean();
    }

    fclose(fp);

    return true;
}

/*
Save the current game state to a save file. The slot number is a single
character, '1' through '9', or the letter 'T' for the temporary save file.
*/
void SaveGameState(char slot_char)
{
    static char *filename = FILENAME_BASE ".SV ";
    FILE *fp;
    word checksum;

    *(filename + SAVE_SLOT_INDEX) = slot_char;

    fp = fopen(JoinPath(writePath, filename), "wb");

    putw(playerHealth, fp);
    fwrite(&gameScore, 4, 1, fp);
    putw((word)gameStars, fp);
    putw(levelNum, fp);
    putw(playerBombs, fp);
    putw(playerMaxHealth, fp);
    putw(usedCheatCode, fp);
    putw(true, fp);  /* bomb hint */
    putw(POUNCE_HINT_SEEN, fp);
    putw(true, fp);  /* health hint */
    checksum = playerHealth + (word)gameStars + levelNum + playerBombs + playerMaxHealth;
    putw(checksum, fp);

    fclose(fp);
}

/*
Present a UI for restoring a saved game, and return the result of the prompt.
*/
byte PromptRestoreGame(void)
{
    word x;
    byte lastkey;

    x = UnfoldTextFrame(11, 7, 28, "Restore a game.", "Press ESC to quit.");
    DrawTextLine(x, 14, " What game number (1-9)?");
    lastkey = WaitSpinner(x + 24, 14);

    if (lastkey == SCANCODE_ESC || lastkey == SCANCODE_SPACE || lastkey == SCANCODE_ENTER) {
        /* VOID */

    } else if (lastkey >= SCANCODE_1 && lastkey < SCANCODE_0) {
        DrawScancodeCharacter(x + 24, 14, lastkey);

        if (!LoadGameState(lastkey + 47)) {
            return RESTORE_GAME_NOT_FOUND;
        } else {
            return RESTORE_GAME_SUCCESS;
        }
    } else {
        x = UnfoldTextFrame(11, 4, 28, "Invalid game number!", "Press ANY key.");
        WaitSpinner(x + 25, 13);
    }

    return RESTORE_GAME_ABORT;
}

/*
Present a UI for saving the game. As the prompt says, the save file will be
written with the state of the game when the level was last started.
*/
void PromptSaveGame(void)
{
    word x;
    byte lastkey;
    word tmphealth, tmpbombs, tmplevel, tmpstars, tmpbars;
    dword tmpscore;

    x = UnfoldTextFrame(8, 10, 28, "Save a game.", "Press ESC to quit.");
    DrawTextLine(x, 11, " What game number (1-9)?");
    DrawTextLine(x, 13, " NOTE: Game is saved at");
    DrawTextLine(x, 14, " BEGINNING of level.");
    lastkey = WaitSpinner(x + 24, 11);

    if (lastkey == SCANCODE_ESC || lastkey == SCANCODE_SPACE || lastkey == SCANCODE_ENTER) {
        /* VOID */

    } else if (lastkey >= SCANCODE_1 && lastkey < SCANCODE_0) {
        DrawScancodeCharacter(x + 24, 11, lastkey);

        tmphealth = playerHealth;
        tmpbombs = playerBombs;
        tmpstars = (word)gameStars;
        tmplevel = levelNum;
        tmpbars = playerMaxHealth;
        tmpscore = gameScore;

        LoadGameState('T');
        SaveGameState(lastkey + 47);

        playerHealth = tmphealth;
        playerBombs = tmpbombs;
        gameStars = tmpstars;
        levelNum = tmplevel;
        gameScore = tmpscore;
        playerMaxHealth = tmpbars;

        x = UnfoldTextFrame(7, 4, 20, "Game Saved.", "Press ANY key.");
        WaitSpinner(x + 17, 9);
    } else {
        x = UnfoldTextFrame(11, 4, 28, "Invalid game number!", "Press ANY key.");
        WaitSpinner(x + 25, 13);
    }
}

/*
Present a UI for the "warp mode" debug feature. This abandons any progress made
in the current level, and jumps unconditionally to the new level. Returns true
if the request was acceptable, or false if the input was bad or Esc was pressed.
*/
bbool PromptLevelWarp(void)
{
    word levels[] = {
        0, 1, 4, 5, 8, 9, 12, 13, 16, 17,
#ifdef HAS_LEVEL_11
        20,
#endif  /* HAS_LEVEL_11 */
        2, 3
    };
    int x;
    char buffer[4];

    x = UnfoldTextFrame(2, 4, 28, "Warp Mode!", "Enter level (1-"
#ifdef HAS_LEVEL_11
    "13"
#else
    "12"
#endif  /* HAS_LEVEL_11 */
    "):");
    ReadAndEchoText(x + 21, 4, buffer, 2);

    /* Repurposing x here to hold the level index */
    x = atoi(buffer) - 1;

    if (
        x >= 0 &&
#ifdef HAS_LEVEL_11
        x <= 12
#else
        x <= 11
#endif  /* HAS_LEVEL_11 */
    ) {
        levelNum = x;  /* no effect, next two calls both clobber this */
        LoadGameState('T');
        SwitchLevel(levels[x]);

        return true;
    }

    return false;
}

/*
Display the main title screen, credits (if the user waits long enough), and
main menu options. Returns a status byte indicating which demo mode the game
loop should run under.
*/
byte TitleLoop(void)
{
    register word junk;  /* only here to tie up the SI register */
    word idlecount;
    byte result;
    byte lastkey;

    isNewGame = false;

title:
    StartMenuMusic(MUSIC_ZZTOP);
    DrawFullscreenImage(IMAGE_TITLE);
    idlecount = 0;
    gameTickCount = 0;

    while (!IsAnyKeyDown()) {
        WaitHard(3);
        idlecount++;

        if (idlecount == 600) {
            DrawFullscreenImage(IMAGE_CREDITS);
        }

        if (idlecount == 1200) {
            InitializeGame();
            return DEMOSTATE_PLAY;
        }
    }

    lastkey = WaitForAnyKey();
    if (lastkey == SCANCODE_Q || lastkey == SCANCODE_ESC) {
        if (PromptQuitConfirm()) ExitClean();
        goto title;
    }

    for (;;) {
        ShowMainMenu();

        for (;;) {
            lastkey = WaitSpinner(28,
#ifdef FOREIGN_ORDERS
            21
#else
            20
#endif  /* FOREIGN_ORDERS */
            );
            switch (lastkey) {
            case SCANCODE_B:
            case SCANCODE_ENTER:
            case SCANCODE_SPACE:
                InitializeGame();
                isNewGame = true;
                pounceHintState = POUNCE_HINT_UNSEEN;
                StartSound(SND_NEW_GAME);
                return DEMOSTATE_NONE;
            case SCANCODE_O:
                ShowOrderingInformation();
                goto bigbreak;
            case SCANCODE_I:
                ShowInstructions();
                goto bigbreak;
            case SCANCODE_A:
                ShowPublisherBBS();
                goto bigbreak;
            case SCANCODE_R:
                result = PromptRestoreGame();
                if (result == RESTORE_GAME_SUCCESS) {
                    return DEMOSTATE_NONE;
                } else if (result == RESTORE_GAME_NOT_FOUND) {
                    ShowRestoreGameError();
                }
                goto bigbreak;
            case SCANCODE_S:
                ShowStory();
                goto bigbreak;
            case SCANCODE_F11:
                if (isDebugMode) {
                    InitializeGame();
                    return DEMOSTATE_RECORD;
                }
                goto bigbreak;
            case SCANCODE_D:
                InitializeGame();
                return DEMOSTATE_PLAY;
            case SCANCODE_T:
                goto title;
            case SCANCODE_Q:
            case SCANCODE_ESC:
                if (PromptQuitConfirm()) ExitClean();
                goto bigbreak;
            case SCANCODE_C:
                DrawFullscreenImage(IMAGE_CREDITS);
                WaitForAnyKey();
                goto bigbreak;
            case SCANCODE_G:
                GameRedefineMenu();
                goto bigbreak;
#ifdef FOREIGN_ORDERS
            case SCANCODE_F:
                ShowForeignOrders();
                goto bigbreak;
#endif  /* FOREIGN_ORDERS */
            case SCANCODE_H:
                FadeOut();
                ClearScreen();
                ShowHighScoreTable();
                goto bigbreak;
            }
        }

bigbreak:
        DrawFullscreenImage(IMAGE_TITLE);
    }

#pragma warn -use
}
#pragma warn .use

/*
Display the slimmed-down in-game menu. Returns a status byte that indicates if
the game should continue, restart on a different level, or exit.
*/
byte ShowGameMenu(void)
{
    word x = UnfoldTextFrame(2, 12, 22, "HELP MENU", "Press ESC to quit.");
    DrawTextLine(x, 5,  " S)ave your game");
    DrawTextLine(x, 6,  " R)estore a game");
    DrawTextLine(x, 7,  " H)elp");
    DrawTextLine(x, 8,  " G)ame redefine");
    DrawTextLine(x, 9,  " V)iew High Scores");
    DrawTextLine(x, 10, " Q)uit Game");

    for (;;) {
        byte result;
        byte lastkey = WaitSpinner(29, 12);

        switch (lastkey) {
        case SCANCODE_G:
            GameRedefineMenu();
            return GAME_MENU_CONTINUE;
        case SCANCODE_S:
            PromptSaveGame();
            return GAME_MENU_CONTINUE;
        case SCANCODE_R:
            result = PromptRestoreGame();
            if (result == RESTORE_GAME_SUCCESS) {
                SwitchLevel(levelNum);
                return GAME_MENU_RESTART;
            } else if (result == RESTORE_GAME_NOT_FOUND) {
                ShowRestoreGameError();
            }
            return GAME_MENU_CONTINUE;
        case SCANCODE_V:
            ShowHighScoreTable();
            return GAME_MENU_CONTINUE;
        case SCANCODE_Q:
            return GAME_MENU_QUIT;
        case SCANCODE_H:
            ShowHintsAndKeys(1);
            return GAME_MENU_CONTINUE;
        case SCANCODE_ESC:
            return GAME_MENU_CONTINUE;
        }
    }
}

/*
Read the next byte of demo data into the global command variables. Return true
if the end of the demo data has been reached, otherwise return false.
*/
bbool ReadDemoFrame(void)
{
    cmdWest  = (bbool)(*(miscData + demoDataPos) & 0x01);
    cmdEast  = (bbool)(*(miscData + demoDataPos) & 0x02);
    cmdNorth = (bbool)(*(miscData + demoDataPos) & 0x04);
    cmdSouth = (bbool)(*(miscData + demoDataPos) & 0x08);
    cmdJump  = (bbool)(*(miscData + demoDataPos) & 0x10);
    cmdBomb  = (bbool)(*(miscData + demoDataPos) & 0x20);
    winLevel =  (bool)(*(miscData + demoDataPos) & 0x40);

    demoDataPos++;
    if (demoDataPos > demoDataLength) {
        return true;
    }

    return false;
}

/*
Pack the current state of all the global command variables into a byte, then
append that byte to the demo data. Return true if the demo data storage is full,
otherwise return false.
*/
bbool WriteDemoFrame(void)
{
    if (demoDataLength > 4998) return true;

    /*
    This function runs early enough in the game loop that this assignment
    doesn't change the behavior of any player/actor touch.
    */
    winLevel = isKeyDown[SCANCODE_X];

    *(miscData + demoDataPos) = cmdWest | (cmdEast  << 1) | (cmdNorth << 2) |
        (cmdSouth << 3) | (cmdJump  << 4) | (cmdBomb  << 5) | (winLevel << 6);

    demoDataPos++;
    demoDataLength++;

    return false;
}

/*
Flush the recorded demo data to disk.
*/
void SaveDemoData(void)
{
    FILE *fp = fopen("PREVDEMO.MNI", "wb");
    miscDataContents = IMAGE_DEMO;

    putw(demoDataLength, fp);
    fwrite(miscData, demoDataLength, 1, fp);

    fclose(fp);
}

/*
Read demo data into memory.
*/
void LoadDemoData(void)
{
    FILE *fp = GroupEntryFp("PREVDEMO.MNI");
    miscDataContents = IMAGE_DEMO;

    if (fp == NULL) {
        /* These were already set in InitializeGame() */
        demoDataLength = 0;
        demoDataPos = 0;
    } else {
        demoDataLength = getw(fp);
        fread(miscData, demoDataLength, 1, fp);
    }

    fclose(fp);
}

/*
Read the state of the keyboard/joystick for the next iteration of the game loop.

Returns a status byte that indicates if the game should end or if a level change
is needed.
*/
byte ProcessGameInput(byte demostate)
{
    if (demostate != DEMOSTATE_PLAY) {
        if (
            isKeyDown[SCANCODE_TAB] &&
            isKeyDown[SCANCODE_F12] &&
            isKeyDown[SCANCODE_KP_DOT]  /* Del */
        ) {
            isDebugMode = !isDebugMode;
            StartSound(SND_PAUSE_GAME);
            WaitHard(90);
        }

        if (isKeyDown[SCANCODE_F10] && isDebugMode) {
            if (isKeyDown[SCANCODE_G]) {
                GodModeToggle();
            }

            if (isKeyDown[SCANCODE_W] && PromptLevelWarp()) {
                return GAME_INPUT_RESTART;
            }

            if (isKeyDown[SCANCODE_P]) {
                StartSound(SND_PAUSE_GAME);
                while (isKeyDown[SCANCODE_P])
                    ;  /* VOID */
                while (!isKeyDown[SCANCODE_P])
                    ;  /* VOID */
                while (isKeyDown[SCANCODE_P])
                    ;  /* VOID */
            }

            if (isKeyDown[SCANCODE_M]) {
                MemoryUsage();
            }

            if (
                isKeyDown[SCANCODE_E] &&
                isKeyDown[SCANCODE_N] &&
                isKeyDown[SCANCODE_D]
            ) {
                winGame = true;
            }
        }

        if (
            isKeyDown[SCANCODE_C] &&
            isKeyDown[SCANCODE_0] &&
            isKeyDown[SCANCODE_F10] &&
            usedCheatCode == false
        ) {
            StartSound(SND_PAUSE_GAME);
            usedCheatCode = true;
            ShowCheatMessage();
            playerMaxHealth = 5;
            playerBombs = 9;
            sawBombHint = true;
            playerHealth = 6;
            UpdateBombs();
            UpdateHealth();
        }

        if (isKeyDown[SCANCODE_S]) {
            ToggleSound();
        } else if (isKeyDown[SCANCODE_M]) {
            ToggleMusic();
        } else if (isKeyDown[SCANCODE_ESC] || isKeyDown[SCANCODE_Q]) {
            if (PromptQuitConfirm()) return GAME_INPUT_QUIT;
        } else if (isKeyDown[SCANCODE_F1]) {
            byte result = ShowGameMenu();
            if (result == GAME_MENU_RESTART) {
                return GAME_INPUT_RESTART;
            } else if (result == GAME_MENU_QUIT) {
                if (PromptQuitConfirm()) return GAME_INPUT_QUIT;
            }
        } else if (isKeyDown[SCANCODE_P]) {
            StartSound(SND_PAUSE_GAME);
            PauseMessage();
        }
    } else if ((inportb(0x0060) & 0x80) == 0) {
        return GAME_INPUT_QUIT;
    }

    if (demostate != DEMOSTATE_PLAY) {
        if (!isJoystickReady) {
            cmdWest  = isKeyDown[scancodeWest] >> (byte)blockMovementCmds;
            cmdEast  = isKeyDown[scancodeEast] >> (byte)blockMovementCmds;
            cmdJump  = isKeyDown[scancodeJump] >> (byte)blockMovementCmds;
            cmdNorth = isKeyDown[scancodeNorth];
            cmdSouth = isKeyDown[scancodeSouth];
            cmdBomb  = isKeyDown[scancodeBomb];
        } else {
            /* Returned state is not important; all global cmds get set. */
            ReadJoystickState(JOYSTICK_A);
        }

        if (blockActionCmds) {
            cmdNorth = cmdSouth = cmdBomb = false;
        }

        if (demostate == DEMOSTATE_RECORD) {
            if (WriteDemoFrame()) {
                return GAME_INPUT_QUIT;
            }
        }
    } else if (ReadDemoFrame()) {
        return GAME_INPUT_QUIT;
    }

    return GAME_INPUT_CONTINUE;
}

/*
Draw the "Super Star Bonus" screen, deduct from the star count, and add to the
player's score.
*/
void ShowStarBonus(void)
{
    register word stars;
    register word x;
    word rank = 0;

    StopMusic();

    if (gameStars == 0) {
        FadeOut();
        return;
    }

    FadeToWhite(3);

    SelectDrawPage(0);
    SelectActivePage(0);
    ClearScreen();

    UnfoldTextFrame(2, 14, 30, "Super Star Bonus!!!!", "");
    DrawSprite(SPR_STAR, 2, 8, 8, DRAWMODE_ABSOLUTE);
    DrawTextLine(14, 7, "X 1000 =");
    DrawNumberFlushRight(27, 7, gameStars * 1000);
    WaitHard(50);
    DrawTextLine(10, 12, "YOUR SCORE =  ");
    DrawNumberFlushRight(29, 12, gameScore);
    FadeIn();
    WaitHard(100);

    for (stars = (word)gameStars; stars > 0; stars--) {
        gameScore += 1000;

        WaitHard(15);

        for (x = 0; x < 7; x++) {
            /* Clear score area -- not strictly needed, numbers grow and are opaque */
            DrawSpriteTile(fontTileData + FONT_BACKGROUND_GRAY, x + 23, 12);
        }

        StartSound(SND_BIG_PRIZE);
        DrawNumberFlushRight(29, 12, gameScore);

        if (rank / 6 < 13) rank++;

        for (x = 0; x < 16; x++) {
            if (x < 7) {
                /* Clear x1000 area -- needed as the number of digits shrinks */
                DrawSpriteTile(fontTileData + FONT_BACKGROUND_GRAY, x + 22, 7);
            }

            if (rank % 8 == 1) {
                /* Clear rank area */
                DrawSpriteTile(fontTileData + FONT_BACKGROUND_GRAY, x + 13, 14);
            }
        }

        DrawNumberFlushRight(27, 7, (stars - 1) * 1000L);

        /*
        BUG: Due to differences in division (6 vs. 8), "Radical!", "Incredible",
        and "Towering" never display in the game.
        */
        if (rank % 8 == 1) {
            DrawTextLine(13, 14, starBonusRanks[rank / 6]);
        }
    }

    WaitHard(400);

    gameStars = 0;
}

/*
Show the intermission screen that bookends every bonus stage.
*/
void ShowSectionIntermission(char *topline, char *bottomline)
{
    word x;

    FadeOut();
    SelectDrawPage(0);
    SelectActivePage(0);
    ClearScreen();

    x = UnfoldTextFrame(6, 4, 30, topline, bottomline);
    FadeIn();
    WaitSpinner(x + 27, 8);

    ShowStarBonus();

    FadeOut();
    ClearScreen();
}

/*
Handle progression to the next level.
*/
void NextLevel(void)
{
    word stars = (word)gameStars;

    if (demoState != DEMOSTATE_NONE) {
        switch (levelNum) {
        case 0:
            levelNum = 13;
            break;
        case 13:
            levelNum = 5;
            break;
        case 5:
            levelNum = 9;
            break;
        case 9:
            levelNum = 16;
            break;
        }
    } else {
        switch (levelNum) {
        case 2:
        case 6:
        case 10:
        case 14:
        case 18:
        case 22:
        case 26:
            levelNum++;
            /* fall through! */
        case 3:
        case 7:
        case 11:
        case 15:
        case 19:
        case 23:
        case 27:
            ShowSectionIntermission("Bonus Level Completed!!", "Press ANY key.");
            /* fall through! */
        case 0:
        case 4:
        case 8:
        case 12:
        case 16:
        case 20:
        case 24:
            levelNum++;
            break;
        case 1:
        case 5:
        case 9:
        case 13:
        case 17:
        case 21:
        case 25:
            ShowSectionIntermission("Section Completed!", "Press ANY key.");
            if (stars > 24) {
                FadeOutCustom(0);
                ClearScreen();
                DrawFullscreenImage(IMAGE_BONUS);
                StartSound(SND_BONUS_STAGE);
                if (stars > 49) {
                    levelNum++;
                }
                levelNum++;
                WaitHard(150);
            } else {
                levelNum += 3;
            }
            break;
        }
    }
}

/*
Run the game loop. This function does not return until the entire game has been
won or the player quits.
*/
void GameLoop(byte demostate)
{
    for (;;) {
        word result;

        while (gameTickCount < 13)
            ;  /* VOID */

        gameTickCount = 0;

        AnimatePalette();

        result = ProcessGameInputHelper(activePage, demostate);
        if (result == GAME_INPUT_QUIT) return;
        if (result == GAME_INPUT_RESTART) continue;

        MovePlayer();

        if (scooterMounted != 0) {
            MovePlayerScooter();
        }

        if (queuePlayerDizzy || playerDizzyLeft != 0) {
            ProcessPlayerDizzy();
        }

        MovePlatforms();
        MoveFountains();
        DrawMapRegion();

        if (DrawPlayerHelper()) continue;

        DrawFountains();
        MoveAndDrawActors();
        MoveAndDrawShards();
        MoveAndDrawSpawners();
        DrawRandomEffects();
        DrawExplosions();
        MoveAndDrawDecorations();
        DrawLights();

        if (demoState != DEMOSTATE_NONE) {
            DrawSprite(SPR_DEMO_OVERLAY, 0, 18, 4, DRAWMODE_ABSOLUTE);
        }

#ifdef DEBUG_BAR
        {
            char debugBar[41];
            word x, y;

            /* Dump variable contents into a bar at the edges of the screen. */
            for (x = 0; x < 40; x++) {
                DrawSpriteTile(fontTileData + FONT_BACKGROUND_GRAY, x, 0);
                for (y = 19; y < 25; y++) {
                    DrawSpriteTile(fontTileData + FONT_BACKGROUND_GRAY, x, y);
                }
            }

            sprintf(debugBar,
                "E%uL%02u! PX=%03u PY=%03u SX=%03u SY=%03u",
                EPISODE, levelNum, playerX, playerY, scrollX, scrollY);
            DrawTextLine(0, 0, debugBar);
            sprintf(debugBar,
                "Score=%07lu Health=%u:%u Bomb=%u Star=%02lu",
                gameScore, playerHealth - 1, playerMaxHealth, playerBombs, gameStars);
            DrawTextLine(0, 19, debugBar);
            sprintf(debugBar,
                "CJ=%d CJL=%d iF=%d FT=%02d iR=%u iLJ=%u MN=%02u",
                cmdJump, cmdJumpLatch, isPlayerFalling, playerFallTime,
                isPlayerRecoiling, isPlayerLongJumping, playerMomentumNorth);
            DrawTextLine(0, 20, debugBar);
            sprintf(debugBar,
                "JT=%d QD=%u DL=%u DT=%02u FDT=%02d HC=%02u",
                playerJumpTime, queuePlayerDizzy, playerDizzyLeft, playerDeadTime,
                playerFallDeadTime, playerHurtCooldown);
            DrawTextLine(0, 21, debugBar);
            TestPlayerMove(1, playerX, playerY + 1);
            sprintf(debugBar,
                "NSWE=%u%u%u%u PS=%u iSE=%u iSW=%u cC=%03u CD=%u",
                TestPlayerMove(DIR4_NORTH, playerX, playerY - 1),
                TestPlayerMove(DIR4_SOUTH, playerX, playerY + 1),
                TestPlayerMove(DIR4_WEST, playerX - 1, playerY),
                TestPlayerMove(DIR4_EAST, playerX + 1, playerY),
                pounceStreak, isPlayerSlidingEast, isPlayerSlidingWest,
                canPlayerCling, playerClingDir);
            DrawTextLine(0, 22, debugBar);
        }
#endif  /* DEBUG_BAR */

        SelectDrawPage(activePage);
        activePage = !activePage;
        SelectActivePage(activePage);

        if (pounceHintState == POUNCE_HINT_QUEUED) {
            pounceHintState = POUNCE_HINT_SEEN;
            ShowPounceHint();
        }

        if (winLevel) {
            winLevel = false;
            StartSound(SND_WIN_LEVEL);
            NextLevel();
            SwitchLevel(levelNum);
        } else if (winGame) {
            break;
        }
    }

    ShowEnding();
}

/*
Insert either a regular actor or a special actor into the world. The map format
uses types 0..31 for special actor types, and types 32+ become regular actor
types 1+. Due to an overlap in branch coverage, there is no way to refer to
regular actor type 0.

The caller must provide the index for regular actors, which is convoluted.
*/
void NewMapActor(word index, word map_actor, int x, int y)
{
    if (map_actor < 32) {
        switch (map_actor) {
        case SPA_PLAYER_START:
            if (x > mapWidth - 15) {
                scrollX = mapWidth - SCROLLW;
            } else if (x - 15 >= 0 && mapYPower > 5) {
                scrollX = x - 15;
            } else {
                scrollX = 0;
            }

            if (y - 10 >= 0) {
                scrollY = y - 10;
            } else {
                scrollY = 0;
            }

            playerX = x;
            playerY = y;

            break;

        case SPA_PLATFORM:
            platforms[numPlatforms].x = x;
            platforms[numPlatforms].y = y;
            numPlatforms++;

            break;

        case SPA_FOUNTAIN_SMALL:
        case SPA_FOUNTAIN_MEDIUM:
        case SPA_FOUNTAIN_LARGE:
        case SPA_FOUNTAIN_HUGE:
            fountains[numFountains].x = x - 1;
            fountains[numFountains].y = y - 1;
            fountains[numFountains].dir = DIR4_NORTH;
            fountains[numFountains].stepcount = 0;
            fountains[numFountains].height = 0;
            fountains[numFountains].stepmax = map_actor * 3;
            fountains[numFountains].delayleft = 0;
            numFountains++;

            break;

        case SPA_LIGHT_WEST:
        case SPA_LIGHT_MIDDLE:
        case SPA_LIGHT_EAST:
            if (numLights != MAX_LIGHTS - 1) {
                lights[numLights].side = map_actor - 6;
                lights[numLights].x = x;
                lights[numLights].y = y;
                numLights++;
            }

            break;
        }
    }

    if (map_actor >= 31 && NewActorAtIndex(index, map_actor - 31, x, y)) {
        numActors++;
    }
}

/*
Load data from a map file, initialize global state, and build all actors.
*/
void LoadMapData(word level_num)
{
    word i;
    word actorwords;
    word a;
    FILE *fp = GroupEntryFp(mapNames[level_num]);

    isCartoonDataLoaded = false;

    getw(fp);  /* skip over flags */
    mapWidth = getw(fp);

    switch (mapWidth) {
    case 1 << 5:
        mapYPower = 5;
        break;
    case 1 << 6:
        mapYPower = 6;
        break;
    case 1 << 7:
        mapYPower = 7;
        break;
    case 1 << 8:
        mapYPower = 8;
        break;
    case 1 << 9:
        mapYPower = 9;
        break;
    case 1 << 10:
        mapYPower = 10;
        break;
    case 1 << 11:
        mapYPower = 11;
        break;
    }

    actorwords = getw(fp);
    numActors = 0;
    numPlatforms = 0;
    numFountains = 0;
    numLights = 0;
    areLightsActive = true;
    hasLightSwitch = false;

    fread(mapData.w, actorwords, 2, fp);

    for (i = 0; i < actorwords; i += 3) {
        register word x;
        register word y;

        a = *(mapData.w + i);
        x = *(mapData.w + i + 1);
        y = *(mapData.w + i + 2);
        NewMapActor(numActors, a, x, y);

        if (numActors > MAX_ACTORS - 1) break;
    }

    fread(mapData.b, WORD_MAX, 1, fp);
    fclose(fp);

    for (i = 0; i < numPlatforms; i++) {
        for (a = 2; a < 7; a++) {
            *((word *)(platforms + i) + a) =
                *(mapData.w + platforms[i].x + (platforms[i].y << mapYPower) + a - 4);
        }
    }

    levelNum = level_num;
    mapHeight = (word)(0x10000L / (mapWidth * 2)) - (SCROLLH + 1);
}

/*
Track the backdrop parameters (backdrop number and horizontal/vertical scroll
flags) and return true if they have changed since the last call. Otherwise
return false.
*/
bbool IsNewBackdrop(word backdrop)
{
    static word lastbd = WORD_MAX;
    static word lasth = WORD_MAX;
    static word lastv = WORD_MAX;

    if (backdrop != lastbd || hasHScrollBackdrop != lasth || hasVScrollBackdrop != lastv) {
        lastbd = backdrop;
        lasth = hasHScrollBackdrop;
        lastv = hasVScrollBackdrop;

        return true;
    }

    return false;
}

/*
Load the specified backdrop image data into the video memory. Requires a scratch
buffer to perform this work.
*/
void LoadBackdropData(char *entry_name, byte *scratch)
{
    FILE *fp = GroupEntryFp(entry_name);

    EGA_MODE_DEFAULT();
    EGA_BIT_MASK_DEFAULT();

    miscDataContents = IMAGE_NONE;
    fread(scratch, 0x5a00, 1, fp);

    CopyTilesToEGA(scratch, 0x1680, 0xa300);

    if (hasHScrollBackdrop) {
        ShiftPixelsHorizontally(scratch, scratch + 0x5a00);
        CopyTilesToEGA(scratch + 0x5a00, 0x1680, 0xb980);
    }

    if (hasVScrollBackdrop) {
        ShiftPixelsVertically(scratch, miscData + 0x1388, scratch + 0xb400);
        CopyTilesToEGA(miscData + 0x1388, 0x1680, 0xd000);

        ShiftPixelsVertically(scratch + 0x5a00, miscData + 0x1388, scratch + 0xb400);
        CopyTilesToEGA(miscData + 0x1388, 0x1680, 0xe680);
    }

    fclose(fp);
}

/*
Set all variables that pertain to the state of the player. These reset at the
start of each level.
*/
void InitializePlayer(void)
{
    winGame = false;
    playerClingDir = DIR4_NONE;
    isPlayerFalling = true;
    cmdJumpLatch = true;
    playerJumpTime = 0;
    playerFallTime = 1;
    isPlayerRecoiling = false;
    playerMomentumNorth = 0;
    playerFaceDir = DIR4_EAST;
    playerFrame = PLAYER_WALK_1;
    playerBaseFrame = PLAYER_BASE_EAST;
    playerDeadTime = 0;
    winLevel = false;
    playerHurtCooldown = 40;
    transporterTimeLeft = 0;
    activeTransporter = 0;
    isPlayerInPipe = false;
    scooterMounted = 0;
    isPlayerNearTransporter = false;
    isPlayerNearHintGlobe = false;
    areForceFieldsActive = true;
    blockMovementCmds = false;

    ClearPlayerDizzy();

    blockActionCmds = false;
    arePlatformsActive = true;
    playerIsInvincible = false;
    paletteStepCount = 0;
    randStepCount = 0;
    playerFallDeadTime  = 0;
    sawHurtBubble  = false;
    sawAutoHintGlobe = false;
    numBarrels = 0;
    numEyePlants = 0;
    pounceStreak = 0;

    sawJumpPadBubble =
        sawMonumentBubble =
        sawScooterBubble =
        sawTransporterBubble =
        sawPipeBubble =
        sawBossBubble =
        sawPusherRobotBubble =
        sawBearTrapBubble =
        sawMysteryWallBubble =
        sawTulipLauncherBubble =
        sawHamburgerBubble = false;
}

/*
Switch to a new level and perform all related tasks.
*/
void SwitchLevel(word level_num)
{
    FILE *fp;
    word bdnum;

    if (level_num == 0 && isNewGame) {
        DrawFullscreenImage(IMAGE_ONE_MOMENT);
        /* All my childhood, I wondered what it was doing here. It's bupkis. */
        WaitSoft(300);
    } else {
        FadeOut();
    }

    fp = GroupEntryFp(mapNames[level_num]);
    mapFlags = getw(fp);
    fclose(fp);

    StopMusic();

    hasRain = (bool)(mapFlags & 0x0020);
    bdnum = mapFlags & 0x001f;
    hasHScrollBackdrop = (bool)(mapFlags & 0x0040);
    hasVScrollBackdrop = (bool)(mapFlags & 0x0080);
    paletteAnimationNum = (byte)(mapFlags >> 8) & 0x07;
    musicNum = (mapFlags >> 11) & 0x001f;

    InitializePlayer();

    if (IsNewBackdrop(bdnum)) {
        LoadBackdropData(backdropNames[bdnum], mapData.b);
    }

    LoadMapData(level_num);

    if (level_num == 0 && isNewGame) {
        FadeOut();
        isNewGame = false;
    }

    if (demoState == DEMOSTATE_NONE) {
        switch (level_num) {
        case 0:
        case 1:
        case 4:
        case 5:
        case 8:
        case 9:
        case 12:
        case 13:
        case 16:
        case 17:
            SelectDrawPage(0);
            SelectActivePage(0);
            ClearScreen();
            FadeIn();
            ShowLevelIntro(level_num);
            WaitSoft(150);
            FadeOut();
            break;
        }
    }

    InitializeShards();
    InitializeExplosions();
    InitializeDecorations();
    ClearPlayerPush();
    InitializeSpawners();
    ClearGameScreen();

    SelectDrawPage(activePage);
    activePage = !activePage;
    SelectActivePage(activePage);

    SaveGameState('T');
    StartGameMusic(musicNum);

    if (!isAdLibPresent) {
        tileAttributeData = miscData + 5000;
        miscDataContents = IMAGE_TILEATTR;
        LoadTileAttributeData("TILEATTR.MNI");
    }

    FadeIn();

#ifdef EXPLOSION_PALETTE
    if (paletteAnimationNum == PALANIM_EXPLOSIONS) {
        SetPaletteRegister(PALETTE_KEY_INDEX, MODE1_BLACK);
    }
#endif  /* EXPLOSION_PALETTE */
}

/*
Set all variables that pertain to the state of the game. These are set once at
the beginning of the game and retain their values across levels.
*/
void InitializeGame(void)
{
    gameScore = 0;
    playerHealth = 4;
    playerMaxHealth = 3;
    levelNum = 0;
    playerBombs = 0;
    gameStars = 0;
    demoDataPos = 0;
    demoDataLength = 0;
    usedCheatCode = false;
    sawBombHint = false;
    sawHealthHint = false;
}

/*
Main entry point for the game, after the 80286 processor test has passed. This
function never returns; the only way to end the program is for something within
the loop to call ExitClean() or a similar function to request termination.
*/
void InnerMain(int argc, char *argv[])
{
    if (argc == 2) {
        writePath = argv[1];
    } else {
        writePath = "\0";
    }

    Startup();

    for (;;) {
        demoState = TitleLoop();

        SwitchLevel(levelNum);
        LoadMaskedTileData("MASKTILE.MNI");

        if (demoState == DEMOSTATE_PLAY) {
            LoadDemoData();
        }

        isInGame = true;
        GameLoop(demoState);
        isInGame = false;

        StopMusic();

        if (demoState != DEMOSTATE_PLAY && demoState != DEMOSTATE_RECORD) {
            CheckHighScore();
        }

        if (demoState == DEMOSTATE_RECORD) {
            SaveDemoData();
        }
    }
}
