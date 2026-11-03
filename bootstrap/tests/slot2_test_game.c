/* Test "game" for the slot-2 blob: drives the virtual cartridge the way
 * NitroSDK's CTRDG library does (byte-wide flash commands, polling, header
 * reads of every width, block copies). Compiled for ARM and for Thumb. */
typedef unsigned int u32; typedef unsigned short u16; typedef unsigned char u8;
#define ROM8(a)  (*(volatile u8 *)(0x08000000u + (a)))
#define ROM16(a) (*(volatile u16 *)(0x08000000u + (a)))
#define ROM32(a) (*(volatile u32 *)(0x08000000u + (a)))
#define FL(a)    (*(volatile u8 *)(0x0A000000u + (a)))
static void cmd(u8 c) { FL(0x5555) = 0xAA; FL(0x2AAA) = 0x55; FL(0x5555) = c; }
static u32 readId(void) { cmd(0x90); u32 id = FL(0) | (FL(1) << 8); cmd(0xF0); return id; }
static void bank(u32 b) { cmd(0xB0); FL(0) = (u8)b; }
static int eraseSector(u32 a) { cmd(0x80); FL(0x5555) = 0xAA; FL(0x2AAA) = 0x55; FL(a) = 0x30;
	for (int t = 0; t < 1000; t++) if (FL(a) == 0xFF) return 1; return 0; }
static int program(u32 a, u8 v) { cmd(0xA0); FL(a) = v;
	for (int t = 0; t < 1000; t++) if (FL(a) == v) return 1; return 0; }
static void copy32(const volatile u32 *src, u32 *dst, u32 words) {
	/* ldmia/stmia like MI_CpuCopy32 */
	while (words >= 4) {
		register u32 a __asm__("r4"), b __asm__("r5"), c __asm__("r6"), d __asm__("r7");
		register const volatile u32 *s __asm__("r3") = src;
		__asm__ volatile("ldmia %4!, {%0, %1, %2, %3}" : "=r"(a), "=r"(b), "=r"(c), "=r"(d), "+r"(s) :: "memory");
		src = s;
		dst[0] = a; dst[1] = b; dst[2] = c; dst[3] = d; dst += 4; words -= 4;
	}
}
u32 results[64];
u32 header[16];
u8 sector[0x1000];
void gameMain(void) {
	u32 *r = results;
	r[0] = ROM8(0xB2);                    /* fixed 0x96 */
	r[1] = ROM32(0xAC);                   /* game code */
	r[2] = ROM16(0xB0);                   /* maker */
	r[3] = ROM16(0xBE);                   /* module ID */
	r[4] = ROM16(0x1FFFE);                /* module ID image */
	r[5] = ROM16(0x123456);               /* open bus */
	{ u32 v; __asm__ volatile("ldr %0, [%1]" : "=r"(v) : "r"(0x080000ADu)); r[6] = v; } /* unaligned LDR rotates */
	copy32((const volatile u32 *)0x080000A0, header, 8);
	r[7] = header[3];                     /* == game code via ldmia */
	r[8] = readId();
	bank(1);
	r[9] = eraseSector(0x3000);
	u32 ok = 1;
	for (u32 i = 0; i < 0x1000; i++) ok &= program(0x3000 + i, (u8)(i * 7 + 3));
	r[10] = ok;
	for (u32 i = 0; i < 0x1000; i++) sector[i] = FL(0x3000 + i);
	u32 sum = 0; for (u32 i = 0; i < 0x1000; i++) sum = sum * 31 + sector[i];
	r[11] = sum;
	bank(0);
	r[12] = FL(0x3000);                   /* bank 0 untouched */
	r[13] = *(volatile u16 *)0x0A000010;  /* wide backup read repeats the byte */
	FL(0x5555) = 0xAA; FL(0x2AAA) = 0x11; /* broken command must not program */
	FL(0x0004) = 0x00;
	r[14] = FL(0x0004);
	r[16] = *(volatile u8 *)0x0B123456;   /* unused slot space: open bus */
	*(volatile u32 *)0x0B000000 = 0x12345678; /* ignored */
	r[15] = 0xC0FFEE;
}
