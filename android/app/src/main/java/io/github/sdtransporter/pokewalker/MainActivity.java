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
    private File recoveryExport;
    private final Handler handler=new Handler(Looper.getMainLooper());
    private int generation,encounter,round,rounds,target=-1,attempts,hidden,empty=-1,item;
    private int[] battle;
    private boolean battleAnimating;
    private int battleVisualAction=-1,previousPlayerHp=4,previousWildHp=4;
    private long battleAnimationStart;
    private long radarDeadline;
    private ProgressBar radarProgress;
    private final Runnable redraw=new Runnable(){public void run(){if(syncBusy&&!state.syncing){syncBusy=false;home();startTracking(false);}updateHeader();handler.postDelayed(this,500);}};
    @Override public void onCreate(Bundle bundle){
        super.onCreate(bundle);state=WalkerStore.get(this);
        getWindow().setStatusBarColor(0xff0b1a28);getWindow().setNavigationBarColor(0xff0b1a28);
        ScrollView scroll=new ScrollView(this);scroll.setFillViewport(true);scroll.setBackgroundColor(0xff0b1a28);
        LinearLayout root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);root.setPadding(dp(16),dp(12),dp(16),dp(20));scroll.addView(root);
        root.addView(text("Pokewalker",27,INK));picture=new WalkerPicture();root.addView(picture,new LinearLayout.LayoutParams(-1,dp(250)));
        stats=text("",18,MINT);root.addView(stats);
        panel=new LinearLayout(this);panel.setOrientation(LinearLayout.VERTICAL);root.addView(panel);setContentView(scroll);
        // Keep controls clear of gesture navigation, system bars and cutouts.
        scroll.setOnApplyWindowInsetsListener((view,insets)->{
            if(Build.VERSION.SDK_INT>=30){
                android.graphics.Insets safe=insets.getInsets(WindowInsets.Type.systemBars()|WindowInsets.Type.displayCutout());
                view.setPadding(safe.left,safe.top,safe.right,safe.bottom);
            }else view.setPadding(insets.getSystemWindowInsetLeft(),insets.getSystemWindowInsetTop(),insets.getSystemWindowInsetRight(),insets.getSystemWindowInsetBottom());
            return insets;
        });
        scroll.requestApplyInsets();
        home();handler.post(redraw);
        startTracking(false);
    }
    @Override protected void onResume(){super.onResume();WalkerStore selected=WalkerStore.get(this);boolean changed=state!=selected;state=selected;syncBusy=state.syncing;if(changed){syncFailure="";home();}}
    @Override protected void onDestroy(){handler.removeCallbacks(redraw);generation++;super.onDestroy();}
    private int dp(float x){return (int)(x*getResources().getDisplayMetrics().density+.5f);}
    private TextView text(String s,int size,int color){TextView v=new TextView(this);v.setText(s);v.setTextSize(size);v.setTextColor(color);v.setPadding(0,dp(5),0,dp(5));return v;}
    private void heading(String title){picture.getLayoutParams().height=dp(page.equals("radar")||page.equals("dowsing")?150:250);picture.requestLayout();panel.removeAllViews();panel.addView(text(title,23,INK));}
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
    private View grassPatch(LinearLayout row,int number,boolean rustling,boolean searched,Runnable action){
        GrassPatchView view=new GrassPatchView(this,number,rustling,searched);
        LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(0,dp(108),1);lp.setMargins(dp(3),dp(4),dp(3),dp(4));row.addView(view,lp);
        view.setOnClickListener(v->action.run());view.setEnabled(!searched);return view;
    }
    private LinearLayout patchRow(){LinearLayout row=new LinearLayout(this);row.setOrientation(LinearLayout.HORIZONTAL);panel.addView(row);return row;}
    private void toast(String message){Toast.makeText(this,message,Toast.LENGTH_LONG).show();}
    private boolean owned(){if(!state.error.isEmpty()){home();toast("Retry saving before using activities.");return false;}if(syncBusy||state.syncing){toast("Please wait for sync to finish.");return false;}if(!state.active()){toast("Receive a walker from a console first.");return false;}if(state.returnPending){toast("Finish returning your walker before using activities.");return false;}return true;}
    private int random(int n){return Native.command(state.data,0,0)%n;}
    private void save(){state.changed();if(!state.error.isEmpty())toast(state.error);}
    private void updateHeader(){
        synchronized(WalkerStore.LOCK){
            if(state.returnPending)stats.setText("Return awaiting confirmation\nPhone copy locked · retry on the sync screen");
            else if(state.data==null)stats.setText("Sync with a 3DS or NDS console over local Wi-Fi");
            else {
                long[] i=Native.info(state.data);
                String mode=state.revoked?"Session revoked":state.returnPending?"Return pending · phone copy locked":state.mode==2?"Phone pedometer":state.mode==1?"Time-based":"Walking paused";
                stats.setText((i[Native.HAS_POKEMON]!=0?Native.species((int)i[Native.SPECIES])+" · Lv "+i[Native.LEVEL]:"Walking solo · a wild Pokémon may join")+"\n"+i[Native.STROLL]+" steps · "+i[Native.WATTS]+" watts\n"+mode);
            }
            picture.invalidate();
        }
    }
    private void home(){
        generation++;battleAnimating=false;page="home";heading("A little adventure");
        button("Walker profiles · "+WalkerStore.profileName(this),this::profilesScreen);
        synchronized(WalkerStore.LOCK){
            if(!state.error.isEmpty()){note(state.error);button("Retry saving",()->{synchronized(WalkerStore.LOCK){save();home();}});}
            if(state.returnPending)note("Return pending. Walking and activities are paused until the console confirms the return.");
            if(state.revoked)note("This phone session cannot sync back. Recover on the console, then perform a fresh handoff.");
            if(state.active()&&!state.returnPending){
                long[] info=Native.info(state.data);
                button(info[Native.VISITOR]>0?"Wild visitor · free battle":"Poké Radar · 10 watts",this::radarStart);
                button("Dowsing · 3 watts",this::dowseStart);
                button("Your pack",this::pack);button("Trainer card",this::card);
            }
        }
        button("Settings",this::settings);button("Console Wi-Fi sync",this::syncScreen);updateHeader();
    }
    private boolean profilesBusy(){if(syncBusy)return true;for(WalkerStore s:WalkerStore.all(this))if(s.syncing)return true;return false;}
    private void profilesScreen(){
        if(profilesBusy()){toast("Wait for the transfer to finish.");return;}
        generation++;page="profiles";heading("Walker profiles");note("All active profiles earn steps. Each profile keeps its own Pokémon, trainer pairing and backups.");
        synchronized(WalkerStore.LOCK){
            for(WalkerProfiles.Entry e:WalkerStore.profileEntries(this)){
                WalkerStore candidate=null;for(WalkerStore item:WalkerStore.all(this))if(item.profileId.equals(e.id))candidate=item;
                final String id=e.id;
                button((id.equals(state.profileId)?"Selected · ":"")+e.name,()->{
                    synchronized(WalkerStore.LOCK){if(profilesBusy()){toast("Wait for sync to finish.");return;}try{state.save();state=WalkerStore.selectProfile(this,id);syncFailure="";home();startTracking(false);}catch(IOException ex){toast(ex.getMessage());}}
                });
                if(candidate!=null&&!candidate.error.isEmpty())note(candidate.error);
                if(candidate!=null)note(candidate.data==null?"No active Pokémon":Native.text(candidate.data,3)+(candidate.returnPending?" · Return pending":candidate.revoked?" · Revoked":" · On phone"));
            }
        }
        button("Add walker profile",()->profileNamePrompt(true));button("Rename selected profile",()->profileNamePrompt(false));button("Back",this::home);
    }
    private void profileNamePrompt(boolean create){
        if(profilesBusy()){toast("Wait for the transfer to finish.");return;}
        EditText name=new EditText(this);name.setSingleLine(true);name.setHint("For example: HeartGold");if(!create)name.setText(WalkerStore.profileName(this));
        new AlertDialog.Builder(this).setTitle(create?"Add walker profile":"Rename profile").setView(name).setPositiveButton("Save",(dialog,which)->{
            synchronized(WalkerStore.LOCK){if(profilesBusy()){toast("Wait for sync to finish.");return;}try{state.save();if(create)state=WalkerStore.createProfile(this,name.getText().toString());else WalkerStore.renameProfile(this,name.getText().toString());syncFailure="";profilesScreen();startTracking(false);}catch(IOException ex){toast(ex.getMessage());}}
        }).setNegativeButton("Cancel",null).show();
    }
    private void result(String title,String detail){generation++;page="result";heading(title);note(detail);button("Continue",this::home);}
    private void settings(){
        generation++;page="settings";heading("Settings");note("Build 1.4-test");
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
                synchronized(WalkerStore.LOCK){if(state.returnPending||state.syncing){toast("Finish the pending sync before changing the rate.");return;}if(state.active()&&state.mode==1)Native.timer(state.data,Native.now(),state.rate);state.rate=rates[n];if(state.active())Native.resume(state.data,Native.now(),state.rate);save();}
                dialog.dismiss();settings();
            }).setNegativeButton("Cancel",null).show();
        });
        note("Tracking continues with an ongoing notification while the screen is off. Pause tracking here to stop it.");button("Recovery copies",this::recoveryCopies);if(state.data!=null)button("Delete active walker",this::deleteActive);button("Privacy policy",()->startActivity(new Intent(this,PrivacyActivity.class)));button("About and licences",()->startActivity(new Intent(this,PrivacyActivity.class).putExtra("credits",true)));button("Back",this::home);
    }
    private void recoveryCopies(){
        if(syncBusy){toast("Please wait for sync to finish.");return;}
        page="recovery";generation++;heading("Recovery copies");
        note("These snapshots preserve earlier walkers. Exporting a copy does not reactivate a revoked session or replace your active walker.");
        File[] files=state.recoveryDirectory().listFiles((dir,name)->name.endsWith(".walker"));
        if(files==null||files.length==0)note("No recovery copies yet.");
        else {
            java.util.Arrays.sort(files,(a,b)->Long.compare(b.lastModified(),a.lastModified()));
            for(File file:files){
                String label=java.text.DateFormat.getDateTimeInstance(java.text.DateFormat.SHORT,java.text.DateFormat.SHORT).format(new java.util.Date(file.lastModified()));
                try{byte[] copy=java.nio.file.Files.readAllBytes(file.toPath());if(copy.length==65536&&Native.walkable(copy))note(Native.text(copy,3));}catch(IOException ignored){}
                button("Export copy · "+label,()->{
                    recoveryExport=file;Intent intent=new Intent(Intent.ACTION_CREATE_DOCUMENT);intent.setType("application/octet-stream");intent.addCategory(Intent.CATEGORY_OPENABLE);intent.putExtra(Intent.EXTRA_TITLE,"Pokewalker-recovery.walker");startActivityForResult(intent,70);
                });
                button("Delete copy · "+label,()->new AlertDialog.Builder(this).setTitle("Delete recovery copy?").setMessage("Permanently delete this snapshot? Your active walker and session records are kept.")
                    .setPositiveButton("Delete",(dialog,which)->{synchronized(WalkerStore.LOCK){if(state.syncing){toast("Wait for sync to finish.");return;}try{state.deleteRecovery(file);toast("Recovery copy deleted.");}catch(IOException ex){toast(ex.getMessage());}}recoveryCopies();}).setNegativeButton("Cancel",null).show());
            }
        }
        button("Back",this::settings);
    }
    private void deleteActive(){
        if(syncBusy||state.syncing){toast("Wait for sync to finish.");return;}
        new AlertDialog.Builder(this).setTitle("Delete active walker?")
            .setMessage("Remove this phone's active walker? A recovery copy is kept first. This phone session will stay inactive. If the console is still locked, use Recover on the console; deleting locally does not notify it.")
            .setPositiveButton("Delete",(dialog,which)->{
                synchronized(WalkerStore.LOCK){try{state.deleteActive();startTracking(false);home();toast("Local walker deleted. Recovery copy kept.");}catch(IOException ex){toast(ex.getMessage());}}
            }).setNegativeButton("Cancel",null).show();
    }
    @Override protected void onActivityResult(int request,int result,Intent data){
        super.onActivityResult(request,result,data);
        if(request!=70||result!=RESULT_OK||data==null||data.getData()==null||recoveryExport==null)return;
        try(InputStream in=new FileInputStream(recoveryExport);OutputStream out=getContentResolver().openOutputStream(data.getData(),"w")){
            if(out==null)throw new IOException("Cannot open export destination");byte[] block=new byte[4096];int n;
            while((n=in.read(block))!=-1)out.write(block,0,n);out.flush();toast("Recovery copy exported.");
        }catch(IOException ex){toast("Export failed: "+ex.getMessage());}
        recoveryExport=null;
    }
    private void chooseMode(int mode){
        synchronized(WalkerStore.LOCK){
            if(state.returnPending||state.syncing){toast("Finish the pending sync before changing walking mode.");return;}
            if(state.active()&&state.mode==1)Native.timer(state.data,Native.now(),state.rate);
            if(mode!=state.mode){state.sensorLast=-1;state.sensorElapsed=0;}
            state.mode=mode;if(state.active())Native.resume(state.data,Native.now(),state.rate);save();
        }
        startTracking(true);
    }
    private void startTracking(boolean ask){
        boolean any=false,needsSensor=false,hasTimer=false;
        for(WalkerStore item:WalkerStore.all(this))if(item.active()&&!item.returnPending&&!item.syncing&&item.mode!=0){any=true;needsSensor|=item.mode==2;hasTimer|=item.mode==1;}
        if(!any){stopService(new Intent(this,WalkingService.class));return;}
        java.util.ArrayList<String> needed=new java.util.ArrayList<>();
        if(needsSensor && Build.VERSION.SDK_INT>=29 && checkSelfPermission(Manifest.permission.ACTIVITY_RECOGNITION)!=PackageManager.PERMISSION_GRANTED)
            needed.add(Manifest.permission.ACTIVITY_RECOGNITION);
        if(ask && Build.VERSION.SDK_INT>=33 && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS)!=PackageManager.PERMISSION_GRANTED)
            needed.add(Manifest.permission.POST_NOTIFICATIONS);
        boolean consent=getSharedPreferences("walking",MODE_PRIVATE).getBoolean("trackingConsent",false);
        if(!needed.isEmpty()||!consent){
            if(ask) new AlertDialog.Builder(this).setTitle("Walking permissions")
                .setMessage((state.mode==2?"Physical activity access lets the app count and store your walking steps, including while the app is in the background with its tracking notification active.":"The optional timer creates and stores virtual steps while its tracking notification is active, including in the background.")+" Steps stay on this phone unless you choose to return the walker to a console. Location is not accessed. The notification provides a Pause control.")
                .setPositiveButton("Continue",(dialog,which)->{getSharedPreferences("walking",MODE_PRIVATE).edit().putBoolean("trackingConsent",true).apply();if(needed.isEmpty())startTracking(false);else requestPermissions(needed.toArray(new String[0]),20);}).setNegativeButton("Cancel",null).show();
            else toast("Open Settings and enable a walking mode to allow tracking.");
            if(ask||!consent||!hasTimer)return;
        }
        try{startForegroundService(new Intent(this,WalkingService.class));}catch(Exception ex){toast("Could not start tracking: "+ex.getMessage());}
    }
    @Override public void onRequestPermissionsResult(int request,String[] permissions,int[] grants){super.onRequestPermissionsResult(request,permissions,grants);if(request==20)startTracking(false);}
    private void radarStart(){
        synchronized(WalkerStore.LOCK){
            if(!owned())return;long[] info=Native.info(state.data);if(info[Native.HAS_POKEMON]==0){toast("Keep walking until a Pokémon joins, or send one from HGSS.");return;}int visitor=(int)info[Native.VISITOR];
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
        for(int n=0;n<4;n++){if(n%2==0)row=patchRow();final int index=n;grassPatch(row,n+1,index==target,false,()->{
            if(target<0)return;
            if(SystemClock.elapsedRealtime()>radarDeadline){result("It got away","Try another Radar search.");return;}
            if(index!=target){if(round==0){generation++;nextRustle();}else result("It got away","The Pokémon escaped.");return;}
            generation++;round++;if(round>=rounds)startBattle(encounter);else nextRustle();
        });}
        button("Leave search",this::home);
    }
    private void startBattle(int which){generation++;battleAnimating=false;battleVisualAction=-1;battleAnimationStart=SystemClock.elapsedRealtime();previousPlayerHp=previousWildHp=4;battle=new int[]{4,4,0,0,0,which,0};battleScreen("Attack, evade or catch.");}
    private void battleScreen(String message){
        page="battle";heading("Wild "+Native.species(Native.command(state.data,9,battle[5])));note(message);note("Your HP: "+battle[0]+" / 4     Wild HP: "+battle[1]+" / 4");
        button("Attack",()->battleAction(0)).setEnabled(!battleAnimating);button("Evade",()->battleAction(1)).setEnabled(!battleAnimating);button("Throw Poké Ball",()->battleAction(2)).setEnabled(!battleAnimating);button("Leave",this::home);
    }
    private void battleAction(int action){
        if(battleAnimating)return;
        previousPlayerHp=battle[0];previousWildHp=battle[1];
        synchronized(WalkerStore.LOCK){if(!owned())return;Native.battle(state.data,battle,action);save();if(!state.error.isEmpty()){home();return;}}
        final int savedReward=battle[4]!=0?saveReward(false,battle[5],-1):-99;
        if(savedReward==-2)return;
        final int epoch=++generation;final String profile=state.profileId;
        battleVisualAction=action;battleAnimationStart=SystemClock.elapsedRealtime();battleAnimating=true;battleScreen(action==2?"The Poké Ball is flying…":action==1?"Evading…":"Attacking…");
        handler.postDelayed(()->{
            if(epoch!=generation||!state.profileId.equals(profile)||!page.equals("battle"))return;
            battleAnimating=false;
            String[] messages={"You attacked; the wild Pokémon replied.","It dodged and attacked!","A critical hit, then a reply!","A stare-down…","It fled!","Caught!","It escaped from the ball and fled.","You evaded and counter-attacked!","Your partner was defeated; up to 10 watts were lost."};
            if(battle[4]!=0){showReward(savedReward,false,battle[5]);return;}
            if(battle[3]!=0){result(messages[battle[6]],"Your walking partner is safe.");return;}battleScreen(messages[battle[6]]);
        },action==2?1800:1200);
    }
    private void dowseStart(){
        synchronized(WalkerStore.LOCK){
            if(!owned())return;if(Native.info(state.data)[Native.HAS_POKEMON]==0){toast("Dowsing needs a walking companion.");return;}if(Native.info(state.data)[Native.WATTS]<3){toast("Dowsing needs 3 watts.");return;}
            item=Native.command(state.data,2,0);if(item<0){toast("No usable route item data.");return;}
            Native.command(state.data,1,3);hidden=random(6);attempts=2;empty=-1;save();
        }
        generation++;dowseScreen("An item is hidden in one of six patches.");
    }
    private void dowseScreen(String message){
        page="dowsing";heading("Dowsing");note(message);note(attempts+" attempts left");
        LinearLayout row=null;
        for(int n=0;n<6;n++){if(n%3==0)row=patchRow();final int index=n;View b=grassPatch(row,n+1,false,n==empty,()->{
            if(index==hidden){storeReward(true,item,-1);return;}
            empty=index;attempts--;if(attempts==0){result("Nothing found","Try again on your next search.");return;}
            int distance=Math.abs(index%3-hidden%3)+Math.abs(index/3-hidden/3);dowseScreen(distance<=1?"It's near!":"It's far away…");
        });b.setEnabled(n!=empty);}
        button("Leave search",this::home);
    }
    private int saveReward(boolean isItem,int choice,int slot){
        final int result;
        synchronized(WalkerStore.LOCK){if(!owned())return -2;result=Native.command(state.data,isItem?4:3,((slot+1)<<8)|choice);
            try{if(result>0)state.archiveCurrent("reward");state.save();}
            catch(IOException ex){state.mode=0;state.error="Save failed. Keep the app open and retry saving. "+ex.getMessage();home();return -2;}}
        return result;
    }
    private void storeReward(boolean isItem,int choice,int slot){int saved=saveReward(isItem,choice,slot);if(saved!=-2)showReward(saved,isItem,choice);}
    private void showReward(int result,boolean isItem,int choice){
        if(result<0){
            page="replace";heading("Pack full");note((isItem?"Found "+Native.item(Native.command(state.data,8,choice)):"Caught "+Native.species(Native.command(state.data,9,choice)))+". Choose a slot to replace, or leave the new reward.");
            for(int n=0;n<3;n++){final int target=n;int id=Native.command(state.data,isItem?7:6,n);button("Replace "+(isItem?Native.item(id):(id>0&&id<494?Native.species(id):"Empty")),()->storeReward(isItem,choice,target));}
            button("Keep current pack",this::home);
        } else result(result==1?(isItem?"You found "+Native.item(Native.command(state.data,8,choice))+"!":"You caught "+Native.species(Native.command(state.data,9,choice))+"!"):"Unusable reward",result==1?"Saved. It is ready to return to HGSS.":"No reward was added.");
    }
    private void pack(){generation++;page="pack";heading("Your pack");if(!owned()){home();return;}
        synchronized(WalkerStore.LOCK){for(int n=0;n<3;n++){int id=Native.command(state.data,6,n);note("Pokémon "+(n+1)+": "+(id>0&&id<494?Native.species(id):"Empty"));}
            for(int n=0;n<3;n++){int id=Native.command(state.data,7,n);note("Item "+(n+1)+": "+(Native.item(id)));}}
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
        button("Receive Pokémon from console",()->{
            if(state.active()){toast("This phone already owns a walker. Return it before receiving another.");return;}
            Runnable receive=()->transfer(false,host.getText().toString().trim());
            if(state.data!=null)new AlertDialog.Builder(this).setTitle("Receive a fresh walker?").setMessage("The old phone copy will be preserved in recovery history before a fresh handoff. Its progress will not be merged into the new walker.").setPositiveButton("Preserve and receive",(d,w)->receive.run()).setNegativeButton("Cancel",null).show();else receive.run();
        });
        button("Return Pokémon",()->transfer(true,host.getText().toString().trim()));button("Back",this::home);
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
        final WalkerStore transferState=state;
        if(syncBusy||transferState.syncing)return;
        if(!validHost(host)){toast("Enter the console IP address.");return;}
        if(!getPreferences(MODE_PRIVATE).getBoolean("localSyncConsent",false)) {
            new AlertDialog.Builder(this).setTitle("Sync over local Wi-Fi")
                .setMessage("Walker data includes trainer information, Pokémon and step totals. It is sent directly to the console address you enter. This local connection is not encrypted; use a network you trust. No developer server receives it.")
                .setPositiveButton("Continue",(dialog,which)->{getPreferences(MODE_PRIVATE).edit().putBoolean("localSyncConsent",true).apply();transfer(returning,host);}).setNegativeButton("Cancel",null).show();return;
        }
        final String session;final byte[] payload;final long operation;
        synchronized(WalkerStore.LOCK){
            if(transferState.syncing){toast("A sync is already in progress.");return;}
            if(returning&&transferState.active()&&transferState.mode==1)Native.timer(transferState.data,Native.now(),transferState.rate);
            if(returning&&!transferState.active()){toast("No active walker to return.");return;}
            if(!returning&&transferState.active()){toast("Return the active walker before receiving another.");return;}
            try{if(!returning&&!transferState.pendingToken.isEmpty()&&transferState.completedSession(transferState.pendingToken))transferState.pendingToken="";transferState.archiveCurrent(returning?"returning":"replaced");}catch(IOException ex){toast("Sync stopped: "+ex.getMessage());return;}
            if(transferState.trainerKey.isEmpty()&&transferState.data!=null)transferState.trainerKey=Native.text(transferState.data,4);
            transferState.host=host;
            if(returning){transferState.mode=0;transferState.returnPending=true;payload=transferState.data.clone();session=transferState.token;}
            else {payload=null;if(transferState.pendingToken.isEmpty()){byte[] random=new byte[16];new SecureRandom().nextBytes(random);StringBuilder b=new StringBuilder();for(byte value:random)b.append(String.format(java.util.Locale.ROOT,"%02x",value&255));transferState.pendingToken=b.toString();}session=transferState.pendingToken;}
            try{transferState.save();operation=transferState.beginSync();}catch(IOException ex){toast(ex.getMessage());return;}
        }
        syncBusy=true;syncFailure="";generation++;startTracking(false);heading(returning?"Returning Pokémon…":"Receiving Pokémon…");note("Keep both devices connected. Interrupted transfers retry automatically using the same session.");
        new Thread(()->{
            try {
                final byte[] incoming;
                try {incoming=WalkerSync.exchange(returning,host,session,payload,transferState.trainerKey,message->runOnUiThread(()->{if(state!=transferState||!transferState.currentSync(operation)||isDestroyed())return;heading(message);note("Keep the console sync screen open. Your recovery snapshot is preserved.");}));}
                catch(WalkerSync.Rejected ex){
                    synchronized(WalkerStore.LOCK){
                        if(!transferState.currentSync(operation))throw new IOException("This sync has been superseded");
                        if(!returning&&ex.status==410)transferState.pendingToken="";
                        if(returning&&ex.status==410){transferState.revoked=true;transferState.mode=0;transferState.returnPending=false;}
                        transferState.save();
                    }
                    throw ex;
                }
                synchronized(WalkerStore.LOCK){
                    if(!transferState.currentSync(operation))throw new IOException("This sync has been superseded");
                    if(returning){transferState.completeReturn(session,payload);transferState.data=null;transferState.token="";transferState.pendingToken="";transferState.revoked=false;transferState.returnPending=false;transferState.sensorLast=-1;}
                    else {if(incoming.length!=65536||!Native.walkable(incoming))throw new IOException("Invalid walker received");String receivedKey=Native.text(incoming,4);if(!receivedKey.matches("[a-f0-9]{36}"))throw new IOException("Trainer details are unavailable");if(!transferState.trainerKey.isEmpty()&&!transferState.trainerKey.equals(receivedKey))throw new IOException("Trainer name or ID does not match this profile. Recover the console handoff; choose the matching profile.");transferState.trainerKey=receivedKey;if(transferState.completedSession(session))throw new IOException("This handoff was already returned. Start a fresh receive.");transferState.returnCompleted=false;transferState.data=incoming;transferState.token=session;transferState.pendingToken="";transferState.revoked=false;transferState.returnPending=false;transferState.sensorLast=-1;transferState.boot=-1;transferState.mode=2;Native.resume(transferState.data,Native.now(),transferState.rate);transferState.archiveCurrent("received");}
                    transferState.save();transferState.finishSync(operation);
                }
                runOnUiThread(()->{if(state!=transferState||!transferState.currentSync(operation)||isDestroyed())return;syncBusy=false;home();toast(returning?"Returned. Console unlocked.":"Received. Console copy is locked.");startTracking(!returning);});
            } catch(Exception ex){runOnUiThread(()->{if(state!=transferState||!transferState.currentSync(operation)||isDestroyed())return;syncBusy=false;syncFailure="Sync failed: "+ex.getMessage()+"\nRetry here with the same console. You can also choose Recover on the console to cancel the handoff and restore its snapshot.";syncScreen();startTracking(false);});}
            finally{synchronized(WalkerStore.LOCK){transferState.finishSync(operation);}}
        },"walker-sync").start();
    }
    @Override public void onBackPressed(){if(syncBusy){toast("Please wait for sync to finish.");return;}if(!page.equals("home"))home();else super.onBackPressed();}
    private final class WalkerPicture extends View {
        final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
        WalkerPicture(){super(MainActivity.this);}
        private void bitmap(Canvas c,int offset,int width,int height,float x,float y,float scale){
            int[] colors={0xffc6dba8,0xff99b484,0xff5c7e5b,0xff2b4539};paint.setAntiAlias(false);
            for(int py=0;py<height;py++)for(int px=0;px<width;px++){int at=offset+((py/8)*width+px)*2,bit=py&7,color=((state.data[at]>>bit)&1)*2+((state.data[at+1]>>bit)&1);if(color!=0){paint.setColor(colors[color]);c.drawRect(x+px*scale,y+py*scale,x+(px+1)*scale,y+(py+1)*scale,paint);}}
            paint.setAntiAlias(true);
        }
        private void drawBattle(Canvas c,float left,float top,float right,float bottom){
            long elapsed=Math.max(0,SystemClock.elapsedRealtime()-battleAnimationStart);float scale=Math.max(1,(float)Math.floor(Math.min((right-left)/96,(bottom-top)/64)));
            float x=(left+right-96*scale)/2,y=(top+bottom-64*scale)/2;
            int frame=(int)(elapsed/160)&1;float player=56,wild=8;boolean showWild=true,showPlayer=true;
            float t=Math.min(1,elapsed/(battleVisualAction==2?1800f:1200f));
            if(battleVisualAction<0)wild=8-Math.max(0,1-elapsed/450f)*40;
            else if(battleAnimating&&battleVisualAction==0){if(t<.45f)player-=16*(float)Math.sin(t/.45*Math.PI);else wild+=16*(float)Math.sin((t-.45)/.55*Math.PI);if(t>.3f&&t<.48f)showWild=frame==0;if(t>.7f&&t<.86f)showPlayer=frame==0;}
            else if(battleAnimating&&battleVisualAction==1){wild+=16*(float)Math.sin(Math.min(1,t*2)*Math.PI);player+=Math.min(8,t*20);if(battle[6]==7&&t>.5f)player=56-16*(float)Math.sin((t-.5)*2*Math.PI);if(battle[6]==4)wild-=t*42;}
            boolean ball=battleAnimating&&battleVisualAction==2;
            if(ball&&t>.28f)showWild=false;
            if(showWild)bitmap(c,0x9a7e+battle[5]*384+frame*192,32,24,x+wild*scale,y+22*scale,scale);
            if(showPlayer)bitmap(c,0x91be+frame*192,32,24,x+player*scale,y+22*scale,scale);
            if(ball){float q=Math.min(1,t/.3f),bx=x+(70-46*q)*scale,by=y+(34-15*(float)Math.sin(q*Math.PI))*scale;if(t>.3f){bx=x+(24+(t<.8f?(float)Math.sin(t*40)*2:0))*scale;by=y+38*scale;}
                paint.setColor(0xff2b4539);c.drawCircle(bx,by,5*scale,paint);paint.setColor(0xffc6dba8);c.drawCircle(bx,by,3*scale,paint);paint.setColor(0xff2b4539);c.drawRect(bx-4*scale,by-scale/2,bx+4*scale,by+scale/2,paint);
                if(t>.8f&&battle[4]!=0)for(int n=-1;n<=1;n++){float sx=bx+n*9*scale,sy=by-(n==0?13:8)*scale;c.drawRect(sx-scale,sy-3*scale,sx+scale,sy+3*scale,paint);c.drawRect(sx-3*scale,sy-scale,sx+3*scale,sy+scale,paint);}
                if(t>.8f&&battle[4]==0){paint.setColor(0xff2b4539);c.drawLine(bx-9*scale,by-8*scale,bx-3*scale,by-3*scale,paint);c.drawLine(bx+9*scale,by-8*scale,bx+3*scale,by-3*scale,paint);}
            }
            int hpPlayer=battleAnimating&&t<.8f?previousPlayerHp:battle[0],hpWild=battleAnimating&&t<.4f?previousWildHp:battle[1];
            for(int n=0;n<4;n++){paint.setColor(n<hpWild?0xff2b4539:0xff99b484);c.drawRect(x+(8+n*6)*scale,y+54*scale,x+(12+n*6)*scale,y+57*scale,paint);paint.setColor(n<hpPlayer?0xff2b4539:0xff99b484);c.drawRect(x+(56+n*6)*scale,y+54*scale,x+(60+n*6)*scale,y+57*scale,paint);}
        }
        @Override protected void onDraw(Canvas canvas){
            super.onDraw(canvas);float w=getWidth(),h=getHeight(),cx=w/2,cy=h/2,r=Math.min(w*.43f,h*.46f);
            boolean compact=page.equals("radar")||page.equals("dowsing");
            float lcdRadius=Math.min(w*.38f,dp(compact?100:185)*.46f);
            paint.setShader(new LinearGradient(0,0,0,h,0xff0b1a28,0xff194240,Shader.TileMode.CLAMP));canvas.drawRect(0,0,w,h,paint);paint.setShader(null);
            paint.setColor(0xffdce7db);canvas.drawCircle(cx,cy,r,paint);paint.setColor(0xffcd5657);canvas.drawArc(cx-r+dp(4),cy-r+dp(4),cx+r-dp(4),cy+r-dp(4),180,180,true,paint);
            float left=cx-lcdRadius*.87f,top=cy-lcdRadius*.53f,right=cx+lcdRadius*.87f,bottom=cy+lcdRadius*.53f;
            paint.setColor(0xff2b4539);canvas.drawRoundRect(left-dp(4),top-dp(4),right+dp(4),bottom+dp(4),dp(9),dp(9),paint);
            paint.setColor(0xffc6dba8);canvas.drawRoundRect(left,top,right,bottom,dp(6),dp(6),paint);
            synchronized(WalkerStore.LOCK){if(state.data!=null&&!state.returnPending&&!state.revoked&&!state.returnCompleted&&Native.info(state.data)[Native.HAS_POKEMON]!=0){
                if(page.equals("battle")&&battle!=null){drawBattle(canvas,left,top,right,bottom);postInvalidateDelayed(80);}else{
                int frame=(int)((SystemClock.elapsedRealtime()/400)&1),offset=0x933e+frame*768;float scale=Math.min((right-left-12)/64,(bottom-top-8)/48);
                float x0=cx-32*scale,y0=cy-24*scale;int[] colors={0xffc6dba8,0xff99b484,0xff5c7e5b,0xff2b4539};
                paint.setAntiAlias(false);
                for(int y=0;y<48;y++)for(int x=0;x<64;x++){int pos=offset+((y/8)*64+x)*2,bit=y&7,c=((state.data[pos]>>bit)&1)*2+((state.data[pos+1]>>bit)&1);if(c!=0){paint.setColor(colors[c]);canvas.drawRect(x0+x*scale,y0+y*scale,x0+(x+1)*scale,y0+(y+1)*scale,paint);}}
                paint.setAntiAlias(true);
                }
            }}
            for(int n=-1;n<=1;n++){paint.setColor(0xff577c70);canvas.drawCircle(cx+n*r*.38f,cy+r*.76f,r*.08f,paint);}
        }
    }
}
