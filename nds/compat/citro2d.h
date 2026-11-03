#ifndef NDS_CITRO2D_COMPAT_H
#define NDS_CITRO2D_COMPAT_H
#include "3ds.h"
#include <stdlib.h>
#define GFX_TOP 0
#define GFX_BOTTOM 1
#define GFX_LEFT 0
#define C3D_DEFAULT_CMDBUF_SIZE 0
#define C3D_FRAME_SYNCDRAW 0
#define C2D_WithColor 0
typedef struct {int bottom;} C3D_RenderTarget;
typedef void *C2D_TextBuf;
typedef struct {const char *text;} C2D_Text;
int C3D_Init(unsigned);
void C3D_Fini(void);
void C3D_FrameBegin(unsigned);
void C3D_FrameEnd(unsigned);
int C2D_Init(unsigned);
void C2D_Fini(void);
void C2D_Prepare(void);
C3D_RenderTarget *C2D_CreateScreenTarget(int,int);
void C2D_TargetClear(C3D_RenderTarget *,uint32_t);
void C2D_SceneBegin(C3D_RenderTarget *);
void C2D_DrawRectSolid(float,float,float,float,float,uint32_t);
void C2D_DrawCircleSolid(float,float,float,float,uint32_t);
void C2D_DrawTriangle(float,float,uint32_t,float,float,uint32_t,float,float,uint32_t,float);
C2D_TextBuf C2D_TextBufNew(unsigned);
void C2D_TextBufDelete(C2D_TextBuf);
void C2D_TextBufClear(C2D_TextBuf);
void C2D_TextParse(C2D_Text *,C2D_TextBuf,const char *);
void C2D_TextOptimize(C2D_Text *);
void C2D_TextGetDimensions(const C2D_Text *,float,float,float *,float *);
void C2D_DrawText(const C2D_Text *,unsigned,float,float,float,float,float,uint32_t);
#endif
