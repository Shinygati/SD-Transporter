/* Original handoff progress display. MIT license. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
#define R16(a) (*(volatile u16*)(a))
#define R32(a) (*(volatile u32*)(a))
#ifndef TWLSDK
static const u8 font[26][7]={
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
 {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
 {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
 {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31}
};
static void line(int x,int y,const char *s,u16 color) {
 volatile u16 *pixels=(volatile u16 *)0x06800000;
 while(*s) {
  char c=*s++;
  if(c>='A'&&c<='Z')for(int r=0;r<7;r++)for(int col=0;col<5;col++)
   if(font[c-'A'][r]&(1u<<(4-col)))pixels[(y+r)*256+x+col]=color;
  x+=6;
 }
}

void gen5HandoffDisplay(u32 stage) {
 const char *message="WAIT ARM SEVEN";
 if(stage==1)message="COPY RECEIVED CLIENT";
 if(stage==2)message="DECOMPRESS CLIENT";
 if(stage==3)message="PATCH CLIENT";
 if(stage==4)message="INSTALL SD ADAPTER";
 if(stage==5)message="WRITE BOOT IMAGE";
 if(stage==6)message="START TRANSFER CLIENT";
 if(stage==0xdead0001)message="CLIENT CODE MISMATCH";
 if(stage==0xdead0002)message="SD ADAPTER PATCH FAILED";
 if(stage==0xdead0003)message="IRQ HOOK FAILED";
 if(stage==0xdead0004)message="ARM SEVEN PATCH FAILED";
 int error=(stage&0xffff0000u)==0xdead0000u;
 R16(0x04000304)=0x8203;R16(0x0400006c)=0;
 *(volatile u8*)0x04000240=0x80;R32(0x04000000)=0x20000;
 volatile u16 *pixels=(volatile u16*)0x06800000;
 for(u32 i=0;i<256*192;i++)pixels[i]=error?0x0848:0x1082;
 line(14,28,"POKE TRANSFER SD",0x7fff);
 line(14,66,message,error?0x001f:0x03e0);
 if(error)line(14,112,"REPORT THIS MESSAGE",0x7fff);
}
#endif
