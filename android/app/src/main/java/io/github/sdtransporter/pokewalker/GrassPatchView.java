package io.github.sdtransporter.pokewalker;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.os.SystemClock;
import android.view.View;

/** Original vector grass artwork matching the console companion's activity tiles. */
final class GrassPatchView extends View {
    private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path blade=new Path();
    private final int number;
    private final boolean rustling,searched;
    GrassPatchView(Context context,int number,boolean rustling,boolean searched) {
        super(context);this.number=number;this.rustling=rustling;this.searched=searched;
        setClickable(true);setFocusable(true);setMinimumHeight((int)dp(108));
        setContentDescription("Patch "+number+(searched?", searched":rustling?", rustling":" of grass"));
    }
    private float dp(float value){return value*getResources().getDisplayMetrics().density;}
    private void triangle(Canvas canvas,float x,float y,float width,float height,int color) {
        blade.reset();blade.moveTo(x-width/2,y+height);blade.lineTo(x,y);blade.lineTo(x+width/2,y+height);blade.close();
        paint.setColor(color);paint.setAlpha(searched?65:255);canvas.drawPath(blade,paint);
    }
    @Override public boolean performClick(){super.performClick();return true;}
    @Override protected void drawableStateChanged(){super.drawableStateChanged();invalidate();}
    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);float w=getWidth(),h=getHeight(),pad=dp(2);
        paint.setStyle(Paint.Style.FILL);paint.setColor(rustling?0xff5e6f3d:0xff17303b);
        canvas.drawRoundRect(pad,pad,w-pad,h-pad,dp(12),dp(12),paint);
        paint.setAlpha(searched?65:255);
        float sway=rustling?(float)Math.sin(SystemClock.uptimeMillis()/85.0)*dp(3):0;
        float cx=w/2+sway,top=dp(15),height=Math.min(dp(44),h*.44f);
        triangle(canvas,cx-height*.48f,top+height*.2f,height*.65f,height*.8f,rustling?0xfffcca66:0xff478767);
        triangle(canvas,cx+height*.14f,top,height*.8f,height,rustling?0xfffcca66:0xff589e73);
        triangle(canvas,cx+height*.58f,top+height*.35f,height*.58f,height*.65f,rustling?0xfffcca66:0xff478767);
        paint.setAlpha(255);paint.setTextAlign(Paint.Align.CENTER);paint.setTextSize(dp(16));paint.setColor(0xffe9f6f0);
        canvas.drawText("Patch "+number,w/2,h-dp(23),paint);
        paint.setTextSize(dp(12));paint.setColor(0xff9ebeb7);
        canvas.drawText(searched?"Searched":rustling?"Rustling!":"Grass",w/2,h-dp(7),paint);
        if(rustling){paint.setColor(0xfffcca66);paint.setTextSize(dp(25));canvas.drawText("!",w-dp(18),dp(30),paint);postInvalidateOnAnimation();}
        if(isPressed()||isFocused()) {
            paint.setColor(0xff44ddb3);canvas.drawRoundRect(dp(12),h-dp(4),w-dp(12),h-dp(2),dp(1),dp(1),paint);
        }
    }
}
