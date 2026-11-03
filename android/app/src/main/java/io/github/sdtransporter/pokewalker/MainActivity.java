package io.github.sdtransporter.pokewalker;

import android.Manifest;
import android.app.*;
import android.content.*;
import android.content.pm.PackageManager;
import android.graphics.*;
import android.graphics.drawable.GradientDrawable;
import android.hardware.*;
import android.os.*;
import android.view.*;
import android.widget.*;
import java.io.*;
import java.net.*;
import java.security.SecureRandom;

public final class MainActivity extends Activity {
    private static final int INK=0xffe9f6f0,MUTED=0xff9ebeb7,MINT=0xff44ddb3,GOLD=0xfffcca66,PANEL=0xff17303b;
    private WalkerStore state;
    private LinearLayout panel;
    private TextView stats;
    private WalkerPicture picture;
    private String page="home";
    private boolean syncBusy;
    private String syncFailure="";
    private final Handler handler=new Handler(Looper.getMainLooper());
    private int generation,encounter,round,rounds,target=-1,attempts,hidden,empty=-1,item;
    private int[] battle;
    private long radarDeadline;
    private ProgressBar radarProgress;
    private final Runnable redraw=new Runnable(){public void run(){updateHeader();handler.postDelayed(this,500);}};

    @Override public void onCreate(Bundle bundle){
        super.onCreate(bundle);state=WalkerStore.get(this);
        getWindow().setStatusBarColor(0xff0b1a28);getWindow().setNavigationBarColor(0xff0b1a28);
        ScrollView scroll=new ScrollView(this);scroll.setFillViewport(true);scroll.setBackgroundColor(0xff0b1a28);
        LinearLayout root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);root.setPadding(dp(16),dp(12),dp(16),dp(20));scroll.addView(root);
        root.addView(text("Pokewalker",27,INK));picture=new WalkerPicture();root.addView(picture,new LinearLayout.LayoutParams(-1,dp(185)));
        stats=text("",18,MINT);root.addView(stats);
        panel=new LinearLayout(this);panel.setOrientation(LinearLayout.VERTICAL);root.addView(panel);setContentView(scroll);
        home();handler.post(redraw);
        if(state.active()&&state.mode!=0)startTracking(false);
    }
    @Override protected void onDestroy(){handler.removeCallbacks(redraw);generation++;super.onDestroy();}
    private int dp(float x){return (int)(x*getResources().getDisplayMetrics().density+.5f);}
    private TextView text(String s,int size,int color){TextView v=new TextView(this);v.setText(s);v.setTextSize(size);v.setTextColor(color);v.setPadding(0,dp(5),0,dp(5));return v;}
    private void heading(String title){picture.getLayoutParams().height=dp(page.equals("radar")||page.equals("dowsing")||page.equals("battle")?100:185);picture.requestLayout();panel.removeAllViews();panel.addView(text(title,23,INK));}
    private void note(String message){panel.addView(text(message,17,MUTED));}
    private Button button(String label,Runnable action){
        Button b=new Button(this);b.setText(label);b.setTextSize(17);b.setTextColor(INK);b.setAllCaps(false);
        GradientDrawable background=new GradientDrawable();background.setColor(PANEL);background.setCornerRadius(dp(12));b.setBackground(background);
        LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,dp(55));lp.topMargin=dp(8);panel.addView(b,lp);
        b.setOnClickListener(v->action.run());return b;
    }
    private Button patch(LinearLayout row,String label,Runnable action){
        Button b=button(label,action);panel.removeView(b);LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(0,dp(60),1);lp.setMargins(dp(3),dp(4),dp(3),dp(4));row.addView(b,lp);return b;
    }
    private LinearLayout patchRow(){LinearLayout row=new LinearLayout(this);row.setOrientation(LinearLayout.HORIZONTAL);panel.addView(row);return row;}
    private void toast(String message){Toast.makeText(this,message,Toast.LENGTH_LONG).show();}
    private boolean owned(){if(syncBusy){toast("Please wait for sync to finish.");return false;}if(!state.active()){toast("Receive a walker from a console first.");return false;}return true;}
    private int random(int n){return Native.command(state.data,0,0)%n;}
    private void save(){state.changed();if(!state.error.isEmpty())toast(state.error);}
    private void updateHeader(){
        synchronized(WalkerStore.LOCK){
            if(state.data==null)stats.setText("Sync with a 3DS or NDS console over local Wi-Fi");
            else {
                long[] i=Native.info(state.data);
                String mode=state.revoked?"Session revoked":state.mode==2?"Phone pedometer":state.mode==1?"Time-based":"Walking paused";
                stats.setText(Native.species((int)i[Native.SPECIES])+" · Lv "+i[Native.LEVEL]+"\n"+i[Native.STROLL]+" steps · "+i[Native.WATTS]+" watts\n"+mode);
            }
            picture.invalidate();
        }
    }
    private void home(){
        generation++;page="home";heading("A little adventure");
        synchronized(WalkerStore.LOCK){
            if(!state.error.isEmpty())note(state.error);
            if(state.revoked)note("This phone session cannot sync back. Recover on the console, then perform a fresh handoff.");
            if(state.active()){
                long[] info=Native.info(state.data);
                button(info[Native.VISITOR]>0?"Wild visitor · free battle":"Poké Radar · 10 watts",this::radarStart);
                button("Dowsing · 3 watts",this::dowseStart);
                button("Your pack",this::pack);button("Trainer card",this::card);
            }
        }
        button("Settings",this::settings);button("Console Wi-Fi sync",this::syncScreen);updateHeader();
    }
    private void result(String title,String detail){generation++;page="result";heading(title);note(detail);button("Continue",this::home);}
    private void settings(){
        generation++;page="settings";heading("Settings");
        boolean sensor=((SensorManager)getSystemService(SENSOR_SERVICE)).getDefaultSensor(Sensor.TYPE_STEP_COUNTER)!=null;
        note("One step source is active at a time. Physical steps are the default; no automatic timer fallback is used.");
        button("Phone pedometer: "+(state.mode==2?"On":"Off")+(sensor?"":" (unavailable)"),()->{
            if(!sensor){toast("This phone has no step counter sensor.");return;}
            chooseMode(state.mode==2?0:2);settings();
        });
        button("Time-based counter: "+(state.mode==1?"On":"Off"),()->{chooseMode(state.mode==1?0:1);settings();});
        button("Time-based rate: one step / "+state.rate+" seconds",()->{
            final int[] rates={1,2,5,10,30,60,300,3600,86400};String[] labels=new String[rates.length];int checked=0;
            for(int n=0;n<rates.length;n++){labels[n]=rates[n]+" seconds";if(rates[n]==state.rate)checked=n;}
            new AlertDialog.Builder(this).setTitle("Time per step").setSingleChoiceItems(labels,checked,(dialog,n)->{
                synchronized(WalkerStore.LOCK){if(state.active()&&state.mode==1)Native.timer(state.data,Native.now(),state.rate);state.rate=rates[n];if(state.active())Native.resume(state.data,Native.now(),state.rate);save();}
                dialog.dismiss();settings();
            }).setNegativeButton("Cancel",null).show();
        });
        note("Tracking continues with an ongoing notification while the screen is off. Pause tracking here to stop it.");button("Back",this::home);
    }
    private void chooseMode(int mode){
        synchronized(WalkerStore.LOCK){
            if(state.active()&&state.mode==1)Native.timer(state.data,Native.now(),state.rate);
            if(mode!=state.mode){state.sensorLast=-1;state.sensorElapsed=0;}
            state.mode=mode;if(state.active())Native.resume(state.data,Native.now(),state.rate);save();
        }
        if(mode==0)stopService(new Intent(this,WalkingService.class));else if(state.active())startTracking(true);
    }
    private void startTracking(boolean ask){
        if(!state.active()||state.mode==0)return;
        java.util.ArrayList<String> needed=new java.util.ArrayList<>();
        if(state.mode==2 && Build.VERSION.SDK_INT>=29 && checkSelfPermission(Manifest.permission.ACTIVITY_RECOGNITION)!=PackageManager.PERMISSION_GRANTED)
            needed.add(Manifest.permission.ACTIVITY_RECOGNITION);
        if(ask && Build.VERSION.SDK_INT>=33 && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS)!=PackageManager.PERMISSION_GRANTED)
            needed.add(Manifest.permission.POST_NOTIFICATIONS);
        if(!needed.isEmpty()){
            if(ask)requestPermissions(needed.toArray(new String[0]),20);
            else toast("Enable physical activity permission in Settings to start tracking.");
            return;
        }
        try{startForegroundService(new Intent(this,WalkingService.class));}catch(Exception ex){toast("Could not start tracking: "+ex.getMessage());}
    }
    @Override public void onRequestPermissionsResult(int request,String[] permissions,int[] grants){super.onRequestPermissionsResult(request,permissions,grants);if(request==20)startTracking(false);}
    private void radarStart(){
        synchronized(WalkerStore.LOCK){
            if(!owned())return;long[] info=Native.info(state.data);int visitor=(int)info[Native.VISITOR];
            if(visitor>0){Native.command(state.data,5,0);save();startBattle(visitor-1);return;}
            if(info[Native.WATTS]<10){toast("Poké Radar needs 10 watts.");return;}
            int[] choice=Native.encounter(state.data);if(choice[0]<0){toast("No usable route encounters.");return;}
            Native.command(state.data,1,10);encounter=choice[0];round=0;rounds=3-encounter+random(2);save();
        }
        page="radar";generation++;nextRustle();
    }
    private void nextRustle(){
        page="radar";target=-1;heading("Poké Radar");note("Wait for a rustling patch. Search "+(round+1)+" / "+rounds);radarButtons();
        final int epoch=generation;final int delay;
        synchronized(WalkerStore.LOCK){delay=500+random(900);save();}
        handler.postDelayed(()->{
            if(epoch!=generation||!page.equals("radar"))return;
            synchronized(WalkerStore.LOCK){target=random(4);save();}
            int[] windows={64,48,32,24};int ms=windows[Math.min(round,3)]*1000/30;radarDeadline=SystemClock.elapsedRealtime()+ms;
            heading("Follow the rustle!");note("Tap the patch marked !");radarButtons();
            handler.postDelayed(()->{if(epoch==generation&&page.equals("radar")&&SystemClock.elapsedRealtime()>=radarDeadline)result("It got away","The rustling stopped.");},ms);
        },delay);
    }
    private void radarButtons(){
        radarProgress=new ProgressBar(this,null,android.R.attr.progressBarStyleHorizontal);radarProgress.setMax(1000);radarProgress.setProgress(target<0?0:1000);panel.addView(radarProgress,new LinearLayout.LayoutParams(-1,dp(12)));
        final int progressEpoch=generation;final ProgressBar current=radarProgress;final long duration=Math.max(1,radarDeadline-SystemClock.elapsedRealtime());
        if(target>=0)handler.post(new Runnable(){public void run(){if(progressEpoch!=generation||!page.equals("radar")||radarProgress!=current)return;long left=radarDeadline-SystemClock.elapsedRealtime();current.setProgress((int)Math.max(0,Math.min(1000,left*1000/duration)));if(left>0)handler.postDelayed(this,40);}});

        LinearLayout row=null;
        for(int n=0;n<4;n++){if(n%2==0)row=patchRow();final int index=n;patch(row,index==target?"! Rustling patch":"Grass patch "+(n+1),()->{
            if(target<0)return;
            if(SystemClock.elapsedRealtime()>radarDeadline){result("It got away","Try another Radar search.");return;}
            if(index!=target){if(round==0){generation++;nextRustle();}else result("It got away","The Pokémon escaped.");return;}
            generation++;round++;if(round>=rounds)startBattle(encounter);else nextRustle();
        });}
        button("Leave search",this::home);
    }
    private void startBattle(int which){generation++;battle=new int[]{4,4,0,0,0,which,0};battleScreen("Attack, evade or catch.");}
    private void battleScreen(String message){
        page="battle";heading("Wild encounter");note(message);note("Your HP: "+battle[0]+" / 4     Wild HP: "+battle[1]+" / 4");
        button("Attack",()->battleAction(0));button("Evade",()->battleAction(1));button("Throw Poké Ball",()->battleAction(2));button("Leave",this::home);
    }
    private void battleAction(int action){
        synchronized(WalkerStore.LOCK){if(!owned())return;Native.battle(state.data,battle,action);save();}
        String[] messages={"You attacked; the wild Pokémon replied.","It dodged and attacked!","A critical hit, then a reply!","A stare-down…","It fled!","Caught!","It escaped from the ball and fled.","You evaded and counter-attacked!","Your partner was defeated; up to 10 watts were lost."};
        if(battle[4]!=0){storeReward(false,battle[5],-1);return;}
        if(battle[3]!=0){result(messages[battle[6]],"Your walking partner is safe.");return;}battleScreen(messages[battle[6]]);
    }
    private void dowseStart(){
        synchronized(WalkerStore.LOCK){
            if(!owned())return;if(Native.info(state.data)[Native.WATTS]<3){toast("Dowsing needs 3 watts.");return;}
            item=Native.command(state.data,2,0);if(item<0){toast("No usable route item data.");return;}
            Native.command(state.data,1,3);hidden=random(6);attempts=2;empty=-1;save();
        }
        generation++;dowseScreen("An item is hidden in one of six patches.");
    }
    private void dowseScreen(String message){
        page="dowsing";heading("Dowsing");note(message);note(attempts+" attempts left");
        LinearLayout row=null;
        for(int n=0;n<6;n++){if(n%3==0)row=patchRow();final int index=n;Button b=patch(row,"Patch "+(n+1),()->{
            if(index==hidden){storeReward(true,item,-1);return;}
            empty=index;attempts--;if(attempts==0){result("Nothing found","Try again on your next search.");return;}
            int distance=Math.abs(index%3-hidden%3)+Math.abs(index/3-hidden/3);dowseScreen(distance<=1?"It's near!":"It's far away…");
        });b.setEnabled(n!=empty);}
        button("Leave search",this::home);
    }
    private void storeReward(boolean isItem,int choice,int slot){
        final int result;
        synchronized(WalkerStore.LOCK){if(!owned())return;result=Native.command(state.data,isItem?4:3,((slot+1)<<8)|choice);save();}
        if(result<0){
            page="replace";heading("Pack full");note("Choose one slot to replace, or leave the new reward.");
            for(int n=0;n<3;n++){final int target=n;int id=Native.command(state.data,isItem?7:6,n);button("Replace "+(isItem?"item #"+id:Native.species(id)),()->storeReward(isItem,choice,target));}
            button("Keep current pack",this::home);
        } else result(result==1?(isItem?"Item found!":"Pokémon caught!"):"Unusable reward",result==1?"It is ready to return to HGSS.":"No reward was added.");
    }
    private void pack(){generation++;page="pack";heading("Your pack");if(!owned()){home();return;}
        synchronized(WalkerStore.LOCK){for(int n=0;n<3;n++){int id=Native.command(state.data,6,n);note("Pokémon "+(n+1)+": "+(id>0&&id<494?Native.species(id):"Empty"));}
            for(int n=0;n<3;n++){int id=Native.command(state.data,7,n);note("Item "+(n+1)+": "+(id>0&&id<537?"#"+id:"Empty"));}}
        button("Back",this::home);
    }
    private void card(){generation++;page="card";heading("Trainer card");if(!owned()){home();return;}
        synchronized(WalkerStore.LOCK){long[] i=Native.info(state.data);note("Trainer: "+Native.text(state.data,0));note("Route: "+Native.text(state.data,2));note("Bond: "+i[Native.BOND]+" / 255");note("Today: "+i[Native.TODAY]+" steps");note("Lifetime: "+i[Native.TOTAL]+" steps");}
        button("Back",this::home);
    }
    private boolean validHost(String host){return host.matches("[A-Za-z0-9.:-]+");}
    private void syncScreen(){
        generation++;page="sync";heading("Console Wi-Fi sync");
        note("On the console, open Settings → Phone sync. Enter its IP address below. Keep the console app open on the same local Wi-Fi.");
        if(!syncFailure.isEmpty())note(syncFailure);
        if(!state.pendingToken.isEmpty()&&!state.active())note("An unfinished receive can be retried with the same console. If you force-unlocked it, retry once more after the old session is refused.");
        EditText host=new EditText(this);host.setSingleLine(true);host.setTextColor(INK);host.setHintTextColor(MUTED);host.setHint("Console IP, e.g. 192.168.1.20");host.setText(state.host);panel.addView(host);
        button("Saved consoles",()->showSavedConsoles(host));
        button("Save this console",()->saveConsole(host));
        button("Receive walker from console",()->{
            if(state.active()){toast("This phone already owns a walker. Return it before receiving another.");return;}
            Runnable receive=()->transfer(false,host.getText().toString().trim());
            if(state.data!=null)new AlertDialog.Builder(this).setTitle("Replace old phone copy?").setMessage("The existing phone copy will be replaced by a fresh handoff. Old phone progress will not be merged.").setPositiveButton("Receive fresh copy",(d,w)->receive.run()).setNegativeButton("Cancel",null).show();else receive.run();
        });
        button("Return walker and unlock on console",()->transfer(true,host.getText().toString().trim()));button("Back",this::home);
    }
    private void saveConsole(EditText hostField){
        String host=hostField.getText().toString().trim();
        if(!validHost(host)){toast("Enter the console IP first.");return;}
        EditText name=new EditText(this);name.setSingleLine(true);name.setHint("Console name");name.setText("My console");
        new AlertDialog.Builder(this).setTitle("Save console").setMessage(host).setView(name).setPositiveButton("Save",(d,w)->{
            String label=name.getText().toString().trim();if(label.isEmpty())label=host;
            toast(SavedConsoles.save(this,label,host)?"Console saved.":"Could not save console.");
        }).setNegativeButton("Cancel",null).show();
    }
    private void showSavedConsoles(EditText hostField){
        java.util.ArrayList<SavedConsoles.Entry> list=SavedConsoles.load(this);
        if(list.isEmpty()){toast("No saved consoles yet. Enter an IP and choose Save this console.");return;}
        String[] labels=new String[list.size()];for(int n=0;n<labels.length;n++)labels[n]=list.get(n).label();
        new AlertDialog.Builder(this).setTitle("Saved consoles").setItems(labels,(d,n)->hostField.setText(list.get(n).host))
            .setNeutralButton("Remove a console",(d,w)->new AlertDialog.Builder(this).setTitle("Remove saved console").setItems(labels,(dialog,n)->
                new AlertDialog.Builder(this).setTitle("Remove "+list.get(n).name+"?").setMessage("This only removes the saved address.").setPositiveButton("Remove",(confirm,which)->toast(SavedConsoles.remove(this,n)?"Console removed.":"Could not remove console.")).setNegativeButton("Cancel",null).show()
            ).setNegativeButton("Cancel",null).show()).setNegativeButton("Cancel",null).show();
    }
    private void transfer(boolean returning,String host){
        if(syncBusy)return;
        if(!validHost(host)){toast("Enter the console IP address.");return;}
        final String session;final byte[] payload;
        synchronized(WalkerStore.LOCK){
            if(returning&&!state.active()){toast("No active walker to return.");return;}
            state.host=host;
            if(returning){state.mode=0;payload=state.data.clone();session=state.token;}
            else {payload=null;if(state.pendingToken.isEmpty()){byte[] random=new byte[16];new SecureRandom().nextBytes(random);StringBuilder b=new StringBuilder();for(byte value:random)b.append(String.format(java.util.Locale.ROOT,"%02x",value&255));state.pendingToken=b.toString();}session=state.pendingToken;}
            try{state.save();}catch(IOException ex){toast(ex.getMessage());return;}
        }
        syncBusy=true;syncFailure="";generation++;stopService(new Intent(this,WalkingService.class));heading(returning?"Returning walker…":"Receiving walker…");note("Keep both devices connected. Interrupted transfers retry automatically using the same session.");
        new Thread(()->{
            try {
                final byte[] incoming;
                try {incoming=WalkerSync.exchange(returning,host,session,payload,message->runOnUiThread(()->{heading(message);note("Keep the console sync screen open. Your recovery snapshot is preserved.");}));}
                catch(WalkerSync.Rejected ex){
                    synchronized(WalkerStore.LOCK){
                        if(!returning&&ex.status==410)state.pendingToken="";
                        if(returning&&ex.status==410){state.revoked=true;state.mode=0;}
                        state.save();
                    }
                    throw ex;
                }
                synchronized(WalkerStore.LOCK){
                    if(returning){state.data=null;state.token="";state.pendingToken="";state.revoked=false;state.sensorLast=-1;}
                    else {if(incoming.length!=65536||Native.info(incoming)[Native.HAS_POKEMON]==0)throw new IOException("Invalid walker received");state.data=incoming;state.token=session;state.pendingToken="";state.revoked=false;state.sensorLast=-1;state.boot=-1;state.mode=2;Native.resume(state.data,Native.now(),state.rate);}
                    state.save();
                }
                runOnUiThread(()->{syncBusy=false;home();toast(returning?"Returned. Console unlocked.":"Received. Console copy is locked.");if(!returning)startTracking(true);});
            } catch(Exception ex){runOnUiThread(()->{syncBusy=false;syncFailure="Sync failed: "+ex.getMessage()+"\nRetry here with the same console. You can also choose Recover on the console to cancel the handoff and restore its snapshot.";syncScreen();});}
        },"walker-sync").start();
    }
    @Override public void onBackPressed(){if(syncBusy){toast("Please wait for sync to finish.");return;}if(!page.equals("home"))home();else super.onBackPressed();}
    private final class WalkerPicture extends View {
        final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
        WalkerPicture(){super(MainActivity.this);}
        @Override protected void onDraw(Canvas canvas){
            super.onDraw(canvas);float w=getWidth(),h=getHeight(),cx=w/2,cy=h/2,r=Math.min(w*.38f,h*.46f);
            paint.setShader(new LinearGradient(0,0,0,h,0xff0b1a28,0xff194240,Shader.TileMode.CLAMP));canvas.drawRect(0,0,w,h,paint);paint.setShader(null);
            paint.setColor(0xffdce7db);canvas.drawCircle(cx,cy,r,paint);paint.setColor(0xffcd5657);canvas.drawArc(cx-r+dp(4),cy-r+dp(4),cx+r-dp(4),cy+r-dp(4),180,180,true,paint);
            float left=cx-r*.87f,top=cy-r*.53f,right=cx+r*.87f,bottom=cy+r*.53f;
            paint.setColor(0xff2b4539);canvas.drawRoundRect(left-dp(4),top-dp(4),right+dp(4),bottom+dp(4),dp(9),dp(9),paint);
            paint.setColor(0xffc6dba8);canvas.drawRoundRect(left,top,right,bottom,dp(6),dp(6),paint);
            synchronized(WalkerStore.LOCK){if(state.data!=null){
                int frame=(int)((SystemClock.elapsedRealtime()/400)&1),offset=0x933e+frame*768;float scale=Math.min((right-left-12)/64,(bottom-top-8)/48);
                float x0=cx-32*scale,y0=cy-24*scale;int[] colors={0xffc6dba8,0xff99b484,0xff5c7e5b,0xff2b4539};
                paint.setAntiAlias(false);
                for(int y=0;y<48;y++)for(int x=0;x<64;x++){int pos=offset+((y/8)*64+x)*2,bit=y&7,c=((state.data[pos]>>bit)&1)*2+((state.data[pos+1]>>bit)&1);if(c!=0){paint.setColor(colors[c]);canvas.drawRect(x0+x*scale,y0+y*scale,x0+(x+1)*scale,y0+(y+1)*scale,paint);}}
                paint.setAntiAlias(true);
            }}
            for(int n=-1;n<=1;n++){paint.setColor(0xff577c70);canvas.drawCircle(cx+n*r*.38f,cy+r*.76f,r*.08f,paint);}
        }
    }
}
