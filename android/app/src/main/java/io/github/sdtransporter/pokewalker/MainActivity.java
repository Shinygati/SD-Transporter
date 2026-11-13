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
    private TextView stats, appTitle;
    private WalkerPicture picture;
    private String page="home";
    private PhonePeerSession peerSession;
    private Runnable pendingPeerPermission;
    private int[] classic=new int[40];
    private byte[] classicPixels;
    private int[] visitReward;
    private long visitStarted;
    private String classicProfile="";
    private boolean classicOnly=true;
    private int pressedImageButton=-1;
    private boolean imageHeld;
    private final Runnable imageHold=()->{if(page.equals("classic")&&pressedImageButton==1){imageHeld=true;classicPress(3);}};
    private android.media.AudioTrack classicAudio;
    private final Runnable classicClock=new Runnable(){public void run(){if(page.equals("classic")){classicPress(-1);handler.postDelayed(this,100);}}};
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
        appTitle=text("Pokewalker",27,INK);root.addView(appTitle);picture=new WalkerPicture();root.addView(picture,new LinearLayout.LayoutParams(-1,dp(250)));
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
        picture.setOnTouchListener((view,event)->{
            if(event.getActionMasked()==MotionEvent.ACTION_DOWN){
                handler.removeCallbacks(imageHold);imageHeld=false;pressedImageButton=picture.imageButton(event.getX(),event.getY());
                if(page.equals("classic")&&pressedImageButton==1)handler.postDelayed(imageHold,menuHoldMillis());
                view.getParent().requestDisallowInterceptTouchEvent(pressedImageButton>=0);return true;
            }
            if(event.getActionMasked()==MotionEvent.ACTION_CANCEL){handler.removeCallbacks(imageHold);pressedImageButton=-1;return true;}
            if(event.getActionMasked()==MotionEvent.ACTION_MOVE){if(picture.imageButton(event.getX(),event.getY())!=pressedImageButton){handler.removeCallbacks(imageHold);pressedImageButton=-1;}return true;}
            if(event.getActionMasked()!=MotionEvent.ACTION_UP)return true;
            handler.removeCallbacks(imageHold);
            if(page.equals("peer")&&peerSession!=null){if(event.getY()>view.getHeight()*.65f){float x=event.getX()/view.getWidth();if(x<.4f)peerSession.select(-1);else if(x>.6f)peerSession.select(1);else peerSession.choose();}return true;}
            if(!page.equals("classic")){openClassic();return true;}
            int button=picture.imageButton(event.getX(),event.getY());if(!imageHeld&&button>=0&&button==pressedImageButton)classicPress(button);pressedImageButton=-1;
            return true;
        });
        picture.setContentDescription("Pokewalker. Tap to open original-style controls.");
        home();handler.post(redraw);
        startTracking(false);
    }
    @Override protected void onResume(){super.onResume();if(page.equals("classic")){handler.removeCallbacks(classicClock);handler.post(classicClock);}WalkerStore selected=WalkerStore.get(this);boolean changed=state!=selected;state=selected;syncBusy=peerSession==null&&state.syncing;if(changed){syncFailure="";home();}}
    @Override protected void onPause(){handler.removeCallbacks(imageHold);pressedImageButton=-1;handler.removeCallbacks(classicClock);stopClassicAudio();super.onPause();}
    @Override protected void onDestroy(){handler.removeCallbacks(redraw);handler.removeCallbacks(classicClock);stopClassicAudio();if(peerSession!=null)peerSession.close();pendingPeerPermission=null;generation++;super.onDestroy();}
    private int dp(float x){return (int)(x*getResources().getDisplayMetrics().density+.5f);}
    private TextView text(String s,int size,int color){TextView v=new TextView(this);v.setText(s);v.setTextSize(size);v.setTextColor(color);v.setPadding(0,dp(5),0,dp(5));return v;}
    private void heading(String title){appTitle.setVisibility(View.VISIBLE);stats.setVisibility(View.VISIBLE);picture.getLayoutParams().height=dp(page.equals("radar")||page.equals("dowsing")?260:320);picture.requestLayout();panel.removeAllViews();panel.addView(text(title,23,INK));}
    private void note(String message){panel.addView(text(message,17,MUTED));}
    private Button button(String label,Runnable action){
        Button b=new Button(this);b.setText(label);b.setTextSize(17);b.setTextColor(INK);b.setAllCaps(false);
        GradientDrawable background=new GradientDrawable();background.setColor(PANEL);background.setCornerRadius(dp(12));b.setBackground(background);
        LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,dp(55));lp.topMargin=dp(8);panel.addView(b,lp);
        if(label.length()>2){b.setCompoundDrawablesWithIntrinsicBounds(new PixelMenuIcon(this,PixelMenuIcon.kind(label)),null,null,null);b.setCompoundDrawablePadding(dp(12));b.setPadding(dp(14),0,dp(14),0);b.setGravity(Gravity.CENTER_VERTICAL|Gravity.LEFT);}
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
    private boolean owned(){if(!state.error.isEmpty()){home();toast("Retry saving before using activities.");return false;}if(syncBusy||state.syncing){toast("Please wait for sync to finish.");return false;}if(!state.active()){toast("Receive a Pokémon from your console first.");return false;}if(state.returnPending){toast("Finish returning your Pokémon before using activities.");return false;}return true;}
    private int random(int n){return Native.command(state.data,0,0)%n;}
    private void save(){state.changed();if(!state.error.isEmpty())toast(state.error);}
    private String packSummary(boolean item){
        StringBuilder names=new StringBuilder();
        for(int n=0;n<3;n++){int id=Native.command(state.data,item?7:6,n);if(id>0&&id<=(item?536:493)){if(names.length()>0)names.append(", ");names.append(item?Native.item(id):Native.species(id));}}
        return names.length()==0?"None":names.toString();
    }
    private void updateHeader(){
        synchronized(WalkerStore.LOCK){
            if(state.returnPending)stats.setText("Return awaiting confirmation\nPhone copy locked · retry on the sync screen");
            else if(state.data==null)stats.setText("Sync with a 3DS or NDS console over local Wi-Fi");
            else {
                long[] i=Native.info(state.data);
                String mode=state.revoked?"Session revoked":state.returnPending?"Return pending · phone copy locked":state.mode==2?"Phone pedometer":state.mode==1?"Time-based":"Walking paused";
                stats.setText((i[Native.HAS_POKEMON]!=0?Native.species((int)i[Native.SPECIES])+" · Lv "+i[Native.LEVEL]:"No partner · send a Pokémon from HGSS")+"\n"+i[Native.STROLL]+" steps · "+i[Native.WATTS]+" watts\n"+Native.text(state.data,2)+"\nItems: "+packSummary(true)+"\nCaught: "+packSummary(false)+"\n"+mode);
            }
            picture.invalidate();
        }
    }
    private void stopClassicAudio(){if(classicAudio!=null){classicAudio.release();classicAudio=null;}}
    private void playClassicScore(int score,int volume){
        if(volume<=0)return;
        short[] pcm=Native.classicSound(state.data,score);stopClassicAudio();
        if(pcm!=null&&pcm.length>0)try{
            classicAudio=new android.media.AudioTrack.Builder().setAudioAttributes(new android.media.AudioAttributes.Builder().setUsage(android.media.AudioAttributes.USAGE_GAME).setContentType(android.media.AudioAttributes.CONTENT_TYPE_SONIFICATION).build()).setAudioFormat(new android.media.AudioFormat.Builder().setSampleRate(22050).setChannelMask(android.media.AudioFormat.CHANNEL_OUT_MONO).setEncoding(android.media.AudioFormat.ENCODING_PCM_16BIT).build()).setTransferMode(android.media.AudioTrack.MODE_STATIC).setBufferSizeInBytes(pcm.length*2).build();
            classicAudio.setVolume(volume==1?.25f:.5f);classicAudio.write(pcm,0,pcm.length);classicAudio.play();
        }catch(RuntimeException ex){stopClassicAudio();}
    }
    private void showVisit(int[] reward){
        if(reward==null||visitReward==reward)return;
        visitReward=reward;
        visitStarted=SystemClock.elapsedRealtime();
        picture.invalidate();
        handler.postDelayed(()->playVisitScore(reward,9),4000);
        handler.postDelayed(()->playVisitScore(reward,6),8000);
    }
    private void playVisitScore(int[] reward,int score){
        synchronized(WalkerStore.LOCK){
            if(!isDestroyed()&&visitReward==reward&&(page.equals("visit")||page.equals("peer")))
                playClassicScore(score,state.classicVolume);
        }
    }
    private void openClassic(){openClassic(true);}
    private void openClassic(boolean only){
        synchronized(WalkerStore.LOCK){if(!owned())return;classicProfile=state.profileId;classic=new int[40];classic[19]=state.classicVolume;classic[20]=state.classicContrast;}
        classicOnly=only;generation++;page="classic";renderClassicControls();
        handler.removeCallbacks(classicClock);handler.post(classicClock);
    }
    private void renderClassicControls(){
        panel.removeAllViews();appTitle.setVisibility(classicOnly?View.GONE:View.VISIBLE);stats.setVisibility(classicOnly?View.GONE:View.VISIBLE);
        picture.getLayoutParams().height=dp(360);picture.requestLayout();
        Switch toggle=new Switch(this);toggle.setText("Classic mode");toggle.setTextColor(INK);toggle.setTextSize(18);toggle.setChecked(classicOnly);
        panel.addView(toggle,new LinearLayout.LayoutParams(-1,dp(56)));
        toggle.setOnCheckedChangeListener((view,enabled)->{classicOnly=enabled;renderClassicControls();});
        if(!classicOnly){
            LinearLayout controls=patchRow();patch(controls,"◀",()->classicPress(0));Button centre=patch(controls,"●",()->classicPress(1));patch(controls,"▶",()->classicPress(2));
            final boolean[] held={false};
            final Runnable back=()->{if(page.equals("classic")&&centre.isAttachedToWindow()){held[0]=true;classicPress(3);}};
            centre.setOnTouchListener((view,event)->{
                switch(event.getActionMasked()){
                case MotionEvent.ACTION_DOWN:held[0]=false;view.getParent().requestDisallowInterceptTouchEvent(true);handler.postDelayed(back,menuHoldMillis());return true;
                case MotionEvent.ACTION_MOVE:if(event.getX()<0||event.getY()<0||event.getX()>view.getWidth()||event.getY()>view.getHeight()){handler.removeCallbacks(back);held[0]=true;}return true;
                case MotionEvent.ACTION_UP:handler.removeCallbacks(back);if(!held[0])view.performClick();return true;
                case MotionEvent.ACTION_CANCEL:handler.removeCallbacks(back);return true;
                default:return true;
                }
            });
            button("Connect",this::connectMenu);button("Back to app",this::home);
        }
    }
    private void classicPress(int button){
        if(!page.equals("classic"))return;
        synchronized(WalkerStore.LOCK){
            if(!state.profileId.equals(classicProfile)||!state.active()||state.returnPending||state.syncing||!state.error.isEmpty()){handler.removeCallbacks(classicClock);stopClassicAudio();home();return;}
            byte[] before=state.data.clone();
            Native.classicInput(state.data,classic,button,Native.now(),SystemClock.elapsedRealtime(),state.expandedItems);
            boolean controlsChanged=state.classicVolume!=classic[19]||state.classicContrast!=classic[20];
            state.classicVolume=classic[19];state.classicContrast=classic[20];
            if(controlsChanged||!java.util.Arrays.equals(before,state.data)){
                try{
                    boolean reward=false;for(int n=0xce8c;n<0xcef0;n++)if(before[n]!=state.data[n]){reward=true;break;}
                    if(reward)state.archiveCurrent("reward");state.save();
                }catch(IOException ex){state.error="Save failed. Keep the app open and retry. "+ex.getMessage();state.mode=0;handler.removeCallbacks(classicClock);home();return;}
            }
            if(classic[10]>0){
                int score=classic[10]-1;classic[10]=0;
                playClassicScore(score,classic[19]);
            }
            classicPixels=Native.classicFrame(state.data,classic,SystemClock.elapsedRealtime());picture.invalidate();
            if(classic[0]==10){handler.removeCallbacks(classicClock);connectMenu();}
            else if(classic[0]==8&&classic[2]==1){handler.removeCallbacks(classicClock);syncScreen();}
        }
    }
    private void home(){
        handler.removeCallbacks(classicClock);stopClassicAudio();
        visitReward=null;
        if(peerSession!=null){peerSession.close();peerSession=null;}
        generation++;battleAnimating=false;page="home";heading("A little adventure");
        LinearLayout controls=patchRow();for(int n=0;n<3;n++){final int key=n;patch(controls,n==0?"◀":n==1?"●":"▶",()->{openClassic(false);classicPress(key);});}
        button("Walker profiles · "+WalkerStore.profileName(this),this::profilesScreen);
        synchronized(WalkerStore.LOCK){
            if(!state.error.isEmpty()){note(state.error);button("Retry saving",()->{synchronized(WalkerStore.LOCK){if(state.visitBlocked)state=WalkerStore.recoverVisits(this);else save();home();}});}
            if(state.returnPending)note("Return pending. Walking and activities are paused until the console confirms the return.");
            if(state.revoked)note("This session was revoked. To recover this phone copy into a missing console target, use Receive phone copy on the console and Return Pokémon here.");
            if(state.active()&&!state.returnPending){
                if(state.battleRewardChoice>=0)button("Claim battle reward",this::claimBattleReward);
            }
        }
        button("Settings",this::settings);button("Console Wi-Fi sync",this::syncScreen);updateHeader();
    }
    private boolean profilesBusy(){if(syncBusy)return true;for(WalkerStore s:WalkerStore.all(this))if(s.syncing)return true;return false;}
    private void connectMenu(){
        if(!owned())return;handler.removeCallbacks(classicClock);stopClassicAudio();generation++;page="connect";heading("Connect");
        button("Another profile on this phone",this::visitProfiles);button("Nearby phone · Bluetooth",this::nearbyPurpose);button("Back",this::home);
    }
    private void nearbyPurpose(){
        if(!owned())return;generation++;page="nearby";heading("Nearby phone");
        note("Keep both phones open in Pokewalker. Choose the same activity on each phone.");
        button("Visit another Pokewalker",()->nearbyRoles(false));button("Friendly battle · 30 watts",()->nearbyRoles(true));button("Back",this::connectMenu);
    }
    private void nearbyRoles(boolean battle){
        if(!owned())return;generation++;page="nearby";heading(battle?"Friendly battle":"Visit a Pokewalker");
        note(battle?"Each player pays 30 watts once both accept. Winner receives a rare item reward. Expanded items must be enabled on both phones; otherwise rewards use course items. Battles use retained moves, power, accuracy and PP; status moves brace against the next attack. Type matchups and other special effects are not simulated.":"Both Pokémon remain in their own profiles. Each Pokewalker receives its normal daily visit gift.");
        button("Host a connection",()->peerPermission(()->beginPeer(battle,true)));button("Search nearby phones",()->peerPermission(()->beginPeer(battle,false)));button("Back",this::nearbyPurpose);
    }
    private void peerPermission(Runnable action){
        String[] wanted=Build.VERSION.SDK_INT>=31?new String[]{Manifest.permission.BLUETOOTH_SCAN,Manifest.permission.BLUETOOTH_CONNECT,Manifest.permission.BLUETOOTH_ADVERTISE}:new String[]{Manifest.permission.ACCESS_FINE_LOCATION};
        java.util.ArrayList<String> missing=new java.util.ArrayList<>();for(String name:wanted)if(checkSelfPermission(name)!=PackageManager.PERMISSION_GRANTED)missing.add(name);
        if(!missing.isEmpty()){pendingPeerPermission=action;requestPermissions(missing.toArray(new String[0]),30);return;}action.run();
    }
    private void beginPeer(boolean battle,boolean host){
        if(!owned())return;generation++;classicPixels=null;page="peer";heading(battle?"Friendly battle":"Nearby visit");
        try{
            peerSession=new PhonePeerSession(this,state,battle,new PhonePeerSession.Listener(){
                public void changed(){if(!isDestroyed()&&page.equals("peer"))peerScreen();}
                public void device(String id,String name){if(isDestroyed()||!page.equals("peer")||peerSession==null)return;button(name,()->{if(peerSession!=null)peerSession.connect(id);});}
                public void closed(){startTracking(false);}
            });
            peerScreen();if(host)peerSession.host();else peerSession.search();
        }catch(IOException|SecurityException ex){if(peerSession!=null){peerSession.close();peerSession=null;}result("Bluetooth unavailable",ex.getMessage());}
    }
    private void peerScreen(){
        if(peerSession==null||!page.equals("peer"))return;
        heading(peerSession.battle?"Friendly battle":"Nearby visit");if(!peerSession.remoteName.isEmpty())note(peerSession.remoteName);note(peerSession.message);
        showVisit(peerSession.visitReward);
        byte[] frame=peerSession.frame();if(frame!=null){classicPixels=frame;picture.invalidate();}
        if(peerSession.canAccept())button(peerSession.battle?"Accept · 30 watts":"Start visit",()->{
            if(peerSession==null)return;
            if(peerSession.rewardsDiffer())new AlertDialog.Builder(this).setTitle("Reward settings differ")
                .setMessage("One phone has Expanded Dowsing items disabled. This battle will use only original course items, with rarer finds weighted more heavily. Each player still pays 30 watts.")
                .setPositiveButton("Continue · 30 watts",(d,w)->{if(peerSession!=null&&peerSession.canAccept())peerSession.accept();})
                .setNegativeButton("Cancel",null).show();
            else peerSession.accept();
        });
        if(peerSession.canMove()){
            LinearLayout row=patchRow();patch(row,"◀",()->peerSession.select(-1));patch(row,"●",()->peerSession.choose());patch(row,"▶",()->peerSession.select(1));
        }
        button("Disconnect",this::home);
    }
    private void claimBattleReward(){
        if(!owned()||state.battleRewardChoice<0)return;generation++;page="claimReward";heading("Battle reward");note(Native.item(Native.command(state.data,8,state.battleRewardChoice)));
        for(int n=0;n<3;n++){final int slot=n;button("Replace slot "+(n+1)+" · "+Native.item(Native.command(state.data,7,n)),()->new AlertDialog.Builder(this).setTitle("Replace this item?").setMessage("The battle reward will replace the item in this slot.").setPositiveButton("Replace",(d,w)->finishBattleReward(slot)).setNegativeButton("Cancel",null).show());}
        button("Leave reward",()->new AlertDialog.Builder(this).setTitle("Leave this reward?").setMessage("Your current pack is kept. This battle reward cannot be claimed again.").setPositiveButton("Leave",(d,w)->finishBattleReward(-1)).setNegativeButton("Cancel",null).show());button("Back",this::home);
    }
    private void finishBattleReward(int slot){
        synchronized(WalkerStore.LOCK){if(!owned()||state.battleRewardChoice<0)return;try{
            int choice=state.battleRewardChoice;if(slot>=0&&Native.command(state.data,10,((slot+1)<<16)|choice)!=1)throw new IOException("Reward could not be stored");
            state.battleRewardChoice=-1;state.archiveCurrent("battle-reward");state.save();home();
        }catch(IOException ex){state.error=ex.getMessage();home();}}
    }
    private void visitProfiles(){
        if(!owned())return;handler.removeCallbacks(classicClock);stopClassicAudio();
        generation++;page="connectProfiles";heading("Connect another Pokewalker");
        note("The Pokémon play together and each Pokewalker receives a gift. The same two devices can meet once per day. Their partners stay in their own profiles.");
        for(WalkerProfiles.Entry entry:WalkerStore.profileEntries(this))if(!entry.id.equals(state.profileId)){
            final String id=entry.id;
            button(entry.name,()->new AlertDialog.Builder(this).setTitle("Connect with "+entry.name+"?").setMessage("Both profiles are saved together before gifts are confirmed.").setPositiveButton("Connect",(d,w)->{
                synchronized(WalkerStore.LOCK){
                    try{
                        int[] reward=WalkerStore.visit(this,id);
                        generation++;page="visit";heading("A friendly meeting");
                        showVisit(reward);
                        note("Both gifts were saved. Meet again tomorrow.");
                        button("Continue",this::home);
                    }catch(IOException ex){toast(ex.getMessage());home();}
                }
            }).setNegativeButton("Cancel",null).show());
        }
        button("Back",this::home);
    }
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
    private int menuHoldMillis(){
        int value=getSharedPreferences("controls",MODE_PRIVATE).getInt("menu_hold_ms",1000);
        return value==250||value==500||value==1000||value==1500||value==2000||value==3000?value:1000;
    }
    private String menuHoldLabel(int milliseconds){return milliseconds==250?"0.25 seconds":milliseconds==500?"0.5 seconds":milliseconds==1000?"1 second":milliseconds==1500?"1.5 seconds":(milliseconds/1000)+" seconds";}
    private void settings(){
        generation++;page="settings";heading("Settings");
        button("Menu button hold: "+menuHoldLabel(menuHoldMillis()),()->{
            final int[] durations={250,500,1000,1500,2000,3000};String[] labels=new String[durations.length];int checked=0;
            for(int n=0;n<durations.length;n++){labels[n]=menuHoldLabel(durations[n]);if(durations[n]==menuHoldMillis())checked=n;}
            new AlertDialog.Builder(this).setTitle("Menu button hold").setSingleChoiceItems(labels,checked,(dialog,n)->{
                getSharedPreferences("controls",MODE_PRIVATE).edit().putInt("menu_hold_ms",durations[n]).apply();dialog.dismiss();settings();
            }).setNegativeButton("Cancel",null).show();
        });
        note("Hold the centre button for this long to return to the main LCD while browsing. Applies to every profile.");
        button("Expanded Dowsing rewards: "+(state.expandedItems?"On":"Off"),()->{
            if(state.syncing||state.returnPending){toast("Finish syncing first.");return;}
            if(state.expandedItems){synchronized(WalkerStore.LOCK){state.expandedItems=false;save();}settings();return;}
            new AlertDialog.Builder(this).setTitle("Enable expanded rewards?").setMessage("Adds Master Ball, PP items, TMs, vitamins, Rare Candy, EV-training items, evolution items, held items and Heart Scales. 92% of Dowsing finds still use the course. Off by default. Battle rewards expand only when both phones enable this option.")
                .setPositiveButton("Opt in",(dialog,which)->{synchronized(WalkerStore.LOCK){if(state.syncing||state.returnPending)return;state.expandedItems=true;save();}settings();}).setNegativeButton("Cancel",null).show();
        });
        button("Nearby phone · Bluetooth",this::nearbyPurpose);
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
                .setMessage((state.mode==2?"Physical activity access lets the app count and store your walking steps, including while the app is in the background with its tracking notification active.":"The optional timer creates and stores virtual steps while its tracking notification is active, including in the background.")+" Steps stay on this phone unless you choose to return the Pokémon to your console. Location is not accessed. The notification provides a Pause control.")
                .setPositiveButton("Continue",(dialog,which)->{getSharedPreferences("walking",MODE_PRIVATE).edit().putBoolean("trackingConsent",true).apply();if(needed.isEmpty())startTracking(false);else requestPermissions(needed.toArray(new String[0]),20);}).setNegativeButton("Cancel",null).show();
            else toast("Open Settings and enable a walking mode to allow tracking.");
            if(ask||!consent||!hasTimer)return;
        }
        try{startForegroundService(new Intent(this,WalkingService.class));}catch(Exception ex){toast("Could not start tracking: "+ex.getMessage());}
    }
    @Override public void onRequestPermissionsResult(int request,String[] permissions,int[] grants){super.onRequestPermissionsResult(request,permissions,grants);if(request==20)startTracking(false);if(request==30){Runnable action=pendingPeerPermission;pendingPeerPermission=null;boolean allowed=grants.length>0;for(int value:grants)allowed&=value==PackageManager.PERMISSION_GRANTED;if(allowed&&action!=null)action.run();else toast("Bluetooth permission is needed to find and connect nearby phones.");}}
    private void radarStart(){
        synchronized(WalkerStore.LOCK){
            if(!owned())return;long[] info=Native.info(state.data);if(info[Native.HAS_POKEMON]==0){toast("Send a Pokémon on a stroll from HGSS first.");return;}int visitor=(int)info[Native.VISITOR];
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
            item=Native.command(state.data,2,state.expandedItems?1:0);if(item<0){toast("No usable route item data.");return;}
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
        synchronized(WalkerStore.LOCK){if(!owned())return -2;result=Native.command(state.data,isItem?10:3,((slot+1)<<(isItem?16:8))|choice);
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
            for(int n=0;n<3;n++){int id=Native.command(state.data,7,n);note("Item "+(n+1)+": "+(Native.item(id)));}for(int n=0;n<10;n++){int id=Native.command(state.data,11,n);if(id>0)note("Friend gift "+(n+1)+": "+Native.item(id));}}
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
            if(state.active()){toast("Return your current Pokémon before receiving another.");return;}
            Runnable receive=()->transfer(false,host.getText().toString().trim());
            if(state.data!=null)new AlertDialog.Builder(this).setTitle("Receive a new Pokémon?").setMessage("The old phone copy will be preserved in recovery history before a fresh handoff. Its progress will not be merged into the incoming Pokémon.").setPositiveButton("Preserve and receive",(d,w)->receive.run()).setNegativeButton("Cancel",null).show();else receive.run();
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
        if(returning&&state.revoked&&state.data!=null){
            new AlertDialog.Builder(this).setTitle("Recover phone copy?").setMessage("On the console walker list press Y, select the HGSS destination and confirm Receive phone copy. This only restores a missing target; an active console walker cannot be overwritten.")
                .setPositiveButton("Return phone copy",(d,w)->transfer(returning,host,true)).setNegativeButton("Cancel",null).show();return;
        }
        transfer(returning,host,false);
    }
    private void transfer(boolean returning,String host,boolean recovery){
        final WalkerStore transferState=state;
        if(syncBusy||transferState.syncing)return;
        if(!validHost(host)){toast("Enter the console IP address.");return;}
        if(!getPreferences(MODE_PRIVATE).getBoolean("localSyncConsent",false)) {
            new AlertDialog.Builder(this).setTitle("Sync over local Wi-Fi")
                .setMessage("Walker data includes trainer information, Pokémon and step totals. It is sent directly to the console address you enter. This local connection is not encrypted; use a network you trust. No developer server receives it.")
                .setPositiveButton("Continue",(dialog,which)->{getPreferences(MODE_PRIVATE).edit().putBoolean("localSyncConsent",true).apply();transfer(returning,host,recovery);}).setNegativeButton("Cancel",null).show();return;
        }
        final String session;final byte[] payload;final long operation;
        synchronized(WalkerStore.LOCK){
            if(transferState.syncing){toast("A sync is already in progress.");return;}
            if(returning&&transferState.battleRewardChoice>=0){toast("Claim or leave your battle reward before returning.");return;}
            if(returning&&transferState.active()&&transferState.mode==1)Native.timer(transferState.data,Native.now(),transferState.rate);
            if(returning&&!transferState.active()&&!(recovery&&transferState.data!=null&&!transferState.token.isEmpty()&&!transferState.returnCompleted)){toast("No Pokémon is available to return.");return;}
            if(returning)try{if(transferState.completedSession(transferState.token)){toast("This phone session was already returned or deleted.");return;}}catch(IOException ex){toast(ex.getMessage());return;}
            if(!returning&&transferState.active()){toast("Return your Pokémon before receiving another.");return;}
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
                    else {if(incoming.length!=65536||!Native.walkable(incoming))throw new IOException("Invalid walker received");String receivedKey=Native.text(incoming,4);if(!receivedKey.matches("[a-f0-9]{36}"))throw new IOException("Trainer details are unavailable");if(!transferState.trainerKey.isEmpty()&&!transferState.trainerKey.equals(receivedKey))throw new IOException("Trainer name or ID does not match this profile. Recover the console handoff; choose the matching profile.");transferState.trainerKey=receivedKey;if(transferState.completedSession(session))throw new IOException("This handoff was already returned. Start a fresh receive.");transferState.returnCompleted=false;transferState.battlePaid=transferState.battleDone=transferState.battleRewarded=false;transferState.battleSession=transferState.battlePeer="";transferState.battleWinner=0;transferState.battleRewardChoice=-1;transferState.data=incoming;transferState.token=session;transferState.pendingToken="";transferState.revoked=false;transferState.returnPending=false;transferState.sensorLast=-1;transferState.boot=-1;transferState.mode=2;Native.resume(transferState.data,Native.now(),transferState.rate);transferState.archiveCurrent("received");}
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
        final Bitmap shell=BitmapFactory.decodeResource(getResources(),R.drawable.walker_shell);
        WalkerPicture(){super(MainActivity.this);}
        int imageButton(float x,float y){
            float r=Math.min(getWidth()*.46f,getHeight()*.48f),cx=getWidth()/2f,cy=getHeight()/2f;
            for(int button=0;button<3;button++){
                float bx=cx+(button-1)*r*.45f,by=cy+r*(button==1?.61f:.55f);
                float radius=Math.max(dp(24),r*(button==1?.16f:.13f));
                if((x-bx)*(x-bx)+(y-by)*(y-by)<=radius*radius)return button;
            }
            return -1;
        }
        private void bitmap(Canvas c,int offset,int width,int height,float x,float y,float scale,boolean flip){
            int[] colors={0xffb4b7aa,0xff8d9383,0xff555e4c,0xff232b1d};paint.setAntiAlias(false);
            for(int py=0;py<height;py++)for(int px=0;px<width;px++){int at=offset+((py/8)*width+px)*2,bit=py&7,color=((state.data[at]>>bit)&1)*2+((state.data[at+1]>>bit)&1);if(color!=0){paint.setColor(colors[color]);int dx=flip?width-1-px:px;c.drawRect(x+dx*scale,y+py*scale,x+(dx+1)*scale,y+(py+1)*scale,paint);}}
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
            if(showWild)bitmap(c,0x9a7e+battle[5]*384+frame*192,32,24,x+wild*scale,y+22*scale,scale,(state.data[0x8f52+battle[5]*16+14]&1)==0);
            if(showPlayer)bitmap(c,0x91be+frame*192,32,24,x+player*scale,y+22*scale,scale,false);
            if(ball){float q=Math.min(1,t/.3f),bx=x+(70-46*q)*scale,by=y+(34-15*(float)Math.sin(q*Math.PI))*scale;if(t>.3f){bx=x+(24+(t<.8f?(float)Math.sin(t*40)*2:0))*scale;by=y+38*scale;}
                paint.setColor(0xff232b1d);c.drawCircle(bx,by,5*scale,paint);paint.setColor(0xffb4b7aa);c.drawCircle(bx,by,3*scale,paint);paint.setColor(0xff232b1d);c.drawRect(bx-4*scale,by-scale/2,bx+4*scale,by+scale/2,paint);
                if(t>.8f&&battle[4]!=0)for(int n=-1;n<=1;n++){float sx=bx+n*9*scale,sy=by-(n==0?13:8)*scale;c.drawRect(sx-scale,sy-3*scale,sx+scale,sy+3*scale,paint);c.drawRect(sx-3*scale,sy-scale,sx+3*scale,sy+scale,paint);}
                if(t>.8f&&battle[4]==0){paint.setColor(0xff232b1d);c.drawLine(bx-9*scale,by-8*scale,bx-3*scale,by-3*scale,paint);c.drawLine(bx+9*scale,by-8*scale,bx+3*scale,by-3*scale,paint);}
            }
            int hpPlayer=battleAnimating&&t<.8f?previousPlayerHp:battle[0],hpWild=battleAnimating&&t<.4f?previousWildHp:battle[1];
            for(int n=0;n<4;n++){paint.setColor(n<hpWild?0xff232b1d:0xff99b484);c.drawRect(x+(8+n*6)*scale,y+54*scale,x+(12+n*6)*scale,y+57*scale,paint);paint.setColor(n<hpPlayer?0xff232b1d:0xff99b484);c.drawRect(x+(56+n*6)*scale,y+54*scale,x+(60+n*6)*scale,y+57*scale,paint);}
        }
        private RectF drawShell(Canvas canvas,float cx,float cy,float r){
            paint.setAntiAlias(true);paint.setFilterBitmap(true);
            canvas.drawBitmap(shell,null,new RectF(cx-r,cy-r,cx+r,cy+r),paint);
            RectF lcd=new RectF(cx-r+2*r*230/1005f,cy-r+2*r*261/1003f,cx-r+2*r*783/1005f,cy-r+2*r*644/1003f);
            Path mask=new Path();mask.addRoundRect(lcd,r*.035f,r*.035f,Path.Direction.CW);
            canvas.save();canvas.clipPath(mask);canvas.drawColor(0xffb4b7aa);return lcd;
        }
        @Override protected void onDraw(Canvas canvas){
            super.onDraw(canvas);float w=getWidth(),h=getHeight(),cx=w/2,cy=h/2,r=Math.min(w*.46f,h*.48f);
            RectF lcd=drawShell(canvas,cx,cy,r);
            synchronized(WalkerStore.LOCK){
                byte[] pixels=null;
                if(visitReward!=null&&(page.equals("visit")||page.equals("peer")))
                    pixels=Native.visitFrame(state.data,visitReward,(int)((SystemClock.elapsedRealtime()-visitStarted)/250));
                else if(page.equals("peer")&&peerSession!=null)pixels=peerSession.frame();
                else if(page.equals("classic"))pixels=classicPixels;
                if(pixels==null&&state.data!=null&&!state.returnPending&&!state.revoked&&!state.returnCompleted)
                    pixels=Native.classicFrame(state.data,new int[40],SystemClock.elapsedRealtime());
                if(page.equals("battle")&&battle!=null&&state.data!=null)drawBattle(canvas,lcd.left,lcd.top,lcd.right,lcd.bottom);
                else if(pixels!=null){
                    float scale=Math.min(lcd.width()/96,lcd.height()/64),x=lcd.centerX()-48*scale,y=lcd.centerY()-32*scale;
                    int[][] shades={{0xffb4b7aa,0xff9a9f91,0xff787e6f,0xff535b4b},{0xffb4b7aa,0xff8d9383,0xff555e4c,0xff232b1d},{0xffb4b7aa,0xff7f8675,0xff3e4835,0xff10180d}};
                    int[] colors=shades[Math.max(0,Math.min(2,state.classicContrast))];paint.setAntiAlias(false);paint.setFilterBitmap(false);
                    for(int py=0;py<64;py++)for(int px=0;px<96;px++){
                        paint.setColor(colors[pixels[py*96+px]&3]);
                        canvas.drawRect(Math.round(x+px*scale),Math.round(y+py*scale),Math.round(x+(px+1)*scale),Math.round(y+(py+1)*scale),paint);
                    }
                }
            }
            canvas.restore();paint.setAntiAlias(true);
            if(page.equals("peer")||page.equals("classic")||
                    (page.equals("visit")&&SystemClock.elapsedRealtime()-visitStarted<10000))
                postInvalidateDelayed(100);
        }
    }
}
