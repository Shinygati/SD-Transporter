/*
    NitroHax -- Cheat tool for the Nintendo DS
    Copyright (C) 2008  Michael "Chishm" Chisholm

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stddef.h>
#include <nds/card.h>
#include "nds_card.h"

void getHeader (u32* ndsHeader) {
	cardParamCommand (CARD_CMD_DUMMY, 0, 
		CARD_ACTIVATE | CARD_CLK_SLOW | CARD_BLK_SIZE(1) | CARD_DELAY1(0x1FFF) | CARD_DELAY2(0x3F), 
		NULL, 0);

	cardParamCommand(CARD_CMD_HEADER_READ, 0,
		CARD_ACTIVATE | CARD_nRESET | CARD_CLK_SLOW | CARD_BLK_SIZE(1) | CARD_DELAY1(0x1FFF) | CARD_DELAY2(0x3F),
		ndsHeader, 512);

}


#ifdef CARTWALKER
#include <nds/bios.h>

/* Use the same reset and header commands as the SDK, with bounded polling. */
static bool cardTransferChecked(u32 flags, u8 *header) {
    unsigned bytes = 0;
    REG_ROMCTRL = flags;
    for (unsigned polls = 0; polls < 4000000; ++polls) {
        u32 status = REG_ROMCTRL;
        if (status & CARD_DATA_READY) {
            u32 word = REG_CARD_DATA_RD;
            if (header && bytes < 512) {
                __builtin_memcpy(header + bytes, &word, sizeof(word));
                bytes += sizeof(word);
            }
        }
        if (!(status & CARD_BUSY))
            return !header || bytes == 512;
    }
    return false;
}

bool cartReadHeaderChecked(u8 *header) {
    const u8 reset[8] = {0, 0, 0, 0, 0, 0, 0, CARD_CMD_DUMMY};
    const u8 read[8] = {0};
    REG_ROMCTRL = 0;
    REG_AUXSPICNTH = 0;
    swiDelay(0x28e7e);
    REG_AUXSPICNTH = CARD_CR1_ENABLE | CARD_CR1_IRQ;
    REG_ROMCTRL = CARD_nRESET | CARD_SEC_SEED;
    cardWriteCommand(reset);
    if (!cardTransferChecked(CARD_ACTIVATE | CARD_nRESET | CARD_CLK_SLOW |
                             CARD_BLK_SIZE(5) | CARD_DELAY2(24), NULL))
        return false;
    cardWriteCommand(read);
    return cardTransferChecked(CARD_ACTIVATE | CARD_nRESET | CARD_CLK_SLOW |
                               CARD_BLK_SIZE(1) | CARD_DELAY1(0x1fff) |
                               CARD_DELAY2(0x3f), header);
}
#endif
