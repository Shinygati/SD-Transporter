package io.github.sdtransporter.pokewalker;
import android.content.Context;
import android.graphics.*;
import android.graphics.drawable.Drawable;
import java.util.Locale;
final class PixelMenuIcon extends Drawable {
    private final int[] rows;private final Paint paint=new Paint();private final int size;
    PixelMenuIcon(Context context,int kind){rows=Native.menuIcon(kind);size=(int)(32*context.getResources().getDisplayMetrics().density+.5f);}
    public int getIntrinsicWidth(){return size;}public int getIntrinsicHeight(){return size;}
    public void draw(Canvas c){Rect b=getBounds();paint.setAntiAlias(true);paint.setColor(0xff244a46);c.drawRoundRect(new RectF(b),6,6,paint);paint.setAntiAlias(false);paint.setColor(0xff44ddb3);float u=Math.min(b.width(),b.height())/20f,x=b.centerX()-8*u,y=b.centerY()-8*u;for(int py=0;py<16;py++)for(int px=0;px<16;px++)if((rows[py]&(1<<px))!=0)c.drawRect(x+px*u,y+py*u,x+(px+1)*u,y+(py+1)*u,paint);}
    public void setAlpha(int value){paint.setAlpha(value);}public void setColorFilter(ColorFilter filter){paint.setColorFilter(filter);}public int getOpacity(){return PixelFormat.TRANSLUCENT;}
    static int kind(String title){String t=title.toLowerCase(Locale.ROOT);
        if(t.contains("profile"))return 6;if(t.contains("radar")||t.contains("visitor"))return 1;if(t.contains("dows")||t.contains("expanded"))return 2;if(t.contains("pack"))return 3;if(t.contains("trainer"))return 4;
        if(t.contains("bluetooth")||t.contains("nearby"))return 7;if(t.contains("sync")||t.contains("console")||t.contains("receive"))return 8;if(t.contains("backup")||t.contains("recovery"))return 9;
        if(t.contains("rate")||t.contains("per step"))return 12;if(t.contains("pedometer")||t.contains("physical"))return 10;if(t.contains("time-based")||t.contains("timer"))return 11;
        if(t.contains("delete")||t.contains("remove"))return 13;if(t.contains("privacy"))return 14;if(t.contains("credit")||t.contains("licence")||t.contains("about"))return 15;
        if(t.contains("back")||t.contains("return")||t.contains("leave"))return 16;if(t.contains("save")||t.contains("retry"))return 17;if(t.contains("connect"))return 18;if(t.contains("setting"))return 5;return 0;
    }
}
