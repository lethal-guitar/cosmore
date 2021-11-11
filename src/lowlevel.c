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

#include "glue.h"

static word drawPageNumber;
static word drawPageSegment = 0xa000;

void SetVideoMode(word mode_num)
{
    _AX = mode_num;
    _AH = 0x00;
    geninterrupt(0x10);

    outport(0x03ce, (0x00 << 8) | 0x07);  /* high byte goes to 0x3cf */

    outportb(0x03c4, 0x02);
}

void SetBorderColorRegister(word color_value)
{
    _AH = 0x10;
    _AL = 0x01;
    _BX = color_value;
    _BH = _BL;
    geninterrupt(0x10);
}

void SetPaletteRegister(word palette_index, word color_value)
{
    _AX = (0x10 << 8) | 0x00;  /* high byte goes to AH */
    _BL = palette_index;
    _BH = color_value;
    geninterrupt(0x10);
}

void DrawSolidTile(word src_offset, word dst_offset)
{
    word row;
    byte *src = MK_FP(0xa400, src_offset);
    byte *dst = MK_FP(drawPageSegment, dst_offset);

    for (row = 0; row < 8; row++) {
        *dst = *(src++);

        dst += 40;
    }
}

void UpdateDrawPageSegment(void)
{
    drawPageSegment = 0xa000 + (drawPageNumber * 0x0200);
}

void SelectDrawPage(word page_num)
{
    drawPageNumber = page_num;

    UpdateDrawPageSegment();
}

void DrawSpriteTileTranslucent(byte *src, word x, word y)
{
    word row;
    byte *dst = MK_FP(drawPageSegment, x + (y * 320));

    outportb(0x03c4, 0x02);

    for (row = 0; row < 8; row++) {
        byte setlatch;

        outport(0x03ce, (~(*src) << 8) | 0x08);  /* high byte goes to 0x3cf */

        outportb(0x03c5, 0x08);

        setlatch = *dst;
        *dst = 0xff;

        src += 5;
        dst += 40;

        (void) setlatch;
    }
}

void LightenScreenTileWest(word x, word y)
{
    word row;
    byte *dst = MK_FP(drawPageSegment, x + yOffsetTable[y]);
    byte mask = 0x01;

    outportb(0x03c5, 0x08);

    for (row = 0; row < 8; row++) {
        byte setlatch;

        outport(0x03ce, (mask << 8) | 0x08);  /* high byte goes to 0x3cf */

        setlatch = *dst;
        *dst = mask;

        dst += 40;
        mask = (mask << 1) | 0x01;

        (void) setlatch;
    }
}

void LightenScreenTile(word x, word y)
{
    word row;
    byte *dst = MK_FP(drawPageSegment, x + yOffsetTable[y]);

    EGA_BIT_MASK_DEFAULT();
    outportb(0x03c5, 0x08);

    for (row = 0; row < 8; row++) {
        *dst = 0xff;

        dst += 40;
    }
}

void LightenScreenTileEast(word x, word y)
{
    word row;
    byte *dst = MK_FP(drawPageSegment, x + yOffsetTable[y]);
    byte mask = 0x80;

    outportb(0x03c4, 0x02);

    for (row = 0; row < 8; row++) {
        byte setlatch;

        outport(0x03ce, (mask << 8) | 0x08);  /* high byte goes to 0x3cf */

        setlatch = *dst;
        *dst = mask;

        dst += 40;
        mask = (mask >> 1) | 0x80;

        (void) setlatch;
    }
}

void SelectActivePage(word page_num)
{
    _AX = page_num;
    _AH = 0x05;
    geninterrupt(0x10);
}

void DrawSpriteTile(byte *src, word x, word y)
{
    word plane;
    byte *dst = MK_FP(drawPageSegment, x + yOffsetTable[y]);

    for (plane = 0; plane < 4; plane++) {
        byte *localsrc = src;
        byte *localdst = dst;
        byte planemask = 1 << plane;
        word row;

        outport(0x03c4, (planemask << 8) | 0x02);  /* high byte goes to 0x3c5 */
        outport(0x03ce, (plane << 8) | 0x04);  /* high byte goes to 0x3cf */
        for (row = 0; row < 8; row++) {
            *localdst = (*localdst & *localsrc) | *(localsrc + plane + 1);

            localsrc += 5;
            localdst += 40;
        }
    }
}

void DrawMaskedTile(byte *src, word x, word y)
{
    word plane;
    byte *dst = MK_FP(drawPageSegment, x + yOffsetTable[y]);

    EGA_MODE_DEFAULT();

    for (plane = 0; plane < 4; plane++) {
        byte *localsrc = src - 16000;
        byte *localdst = dst;
        byte planemask = 1 << plane;
        word row;

        outport(0x03c4, (planemask << 8) | 0x02);  /* high byte goes to 0x3c5 */
        outport(0x03ce, (plane << 8) | 0x04);  /* high byte goes to 0x3cf */
        for (row = 0; row < 8; row++) {
            *localdst = (*localdst & *localsrc) | *(localsrc + plane + 1);

            localsrc += 5;
            localdst += 40;
        }
    }

    EGA_MODE_LATCHED_WRITE();
}

void DrawSpriteTileFlipped(byte *src, word x, word y)
{
    word plane;
    byte *dst = MK_FP(drawPageSegment, x + yOffsetTable[y]);

    for (plane = 0; plane < 4; plane++) {
        byte *localsrc = src;
        byte *localdst = dst + 280;
        byte planemask = 1 << plane;
        word row;

        outport(0x03c4, (planemask << 8) | 0x02);  /* high byte goes to 0x3c5 */
        outport(0x03ce, (plane << 8) | 0x04);  /* high byte goes to 0x3cf */
        for (row = 0; row < 8; row++) {
            *localdst = (*localdst & *localsrc) | *(localsrc + plane + 1);

            localsrc += 5;
            localdst -= 40;
        }
    }
}

void DrawSpriteTileWhite(byte *src, word x, word y)
{
    word row;
    byte *dst = MK_FP(drawPageSegment, x + yOffsetTable[y]);

    outportb(0x03c5, 0x0f);

    outport(0x03ce, (0x10 << 8) | 0x03);  /* high byte goes to 0x3cf */
    outport(0x03ce, (0x08 << 8) | 0x05);  /* high byte goes to 0x3cf */

    for (row = 0; row < 8; row++) {
        *dst &= ~(*src);

        src += 5;
        dst += 40;
    }

    outport(0x03ce, (0x00 << 8) | 0x03);  /* high byte goes to 0x3cf */
    EGA_MODE_DEFAULT();
}

word GetProcessorType(void)
{
    return CPUTYPE_80386;  /* eh, screw it */
}
