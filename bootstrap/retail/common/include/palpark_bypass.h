/* Optional Pal Park history-check bypass, GPL-3.0-or-later.
 * Called ONLY after full retail-ROM verification and explicit opt-in.
 * Find the complete Thumb IsPalParkTransferMacAddressUnset routine by its
 * instructions, never by a game/revision-specific address. Returning true
 * skips the migration-only MAC/RTC/daily history checks. Box eligibility and
 * existing Pal Park occupants are still checked by the game's own code. */
#ifndef PALPARK_BYPASS_H
#define PALPARK_BYPASS_H
#include <stdint.h>
#include <string.h>

static inline int palParkPatchArm9(void *arm9, unsigned size) {
	static const uint16_t pattern[] = {
		0xb418, 0x2400, 0x211a, 0x1c23, 0x0189, 0x18c2, 0x5c52,
		0x1c5b, 0x4314, 0x2b06, 0xdbf9, 0x2c00, 0xd102, 0x2001,
		0xbc18, 0x4770, 0x2000, 0xbc18, 0x4770
	};
	unsigned char *match = 0, *code = (unsigned char *)arm9;
	unsigned i;
	if (size < sizeof(pattern)) return 0;
	for (i = 0; i <= size - sizeof(pattern); i += 2) {
		if (memcmp(code + i, pattern, sizeof(pattern))) continue;
		if (match) return 0; /* ambiguous: leave the game untouched */
		match = code + i;
	}
	if (!match) return 0;
	/* movs r0,#1; bx lr. No save bytes or global RTC values are changed. */
	match[0] = 0x01; match[1] = 0x20; match[2] = 0x70; match[3] = 0x47;
	return 1;
}
#endif
