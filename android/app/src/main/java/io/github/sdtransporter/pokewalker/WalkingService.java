package io.github.sdtransporter.pokewalker;
import android.Manifest;
import android.app.*;
import android.content.*;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.hardware.*;
import android.os.*;
import android.provider.Settings;
import java.util.*;
public final class WalkingService extends Service implements SensorEventListener {
    private SensorManager sensors;private int boot;private long lastNotice;
    private boolean physical,timers;
    private final HashMap<String,String> running=new HashMap<>();
    private final Handler handler=new Handler(Looper.getMainLooper());
    private final Runnable tick=new Runnable(){public void run(){
        synchronized(WalkerStore.LOCK){
            boolean any=false;
            for(WalkerStore s:WalkerStore.all(WalkingService.this))if(s.tracking()){
                any=true;s.creditTimer(Native.now());
            }
            if(!any){stopSelf();return;}
            if(SystemClock.elapsedRealtime()-lastNotice>=10000){notice(false);lastNotice=SystemClock.elapsedRealtime();}
        }
        handler.postDelayed(this,1000);
    }};
    @Override public void onCreate(){super.onCreate();sensors=(SensorManager)getSystemService(SENSOR_SERVICE);boot=Settings.Global.getInt(getContentResolver(),Settings.Global.BOOT_COUNT,-1);
        ((NotificationManager)getSystemService(NOTIFICATION_SERVICE)).createNotificationChannel(new NotificationChannel("walking","Walking",NotificationManager.IMPORTANCE_LOW));}
    @Override public int onStartCommand(Intent intent,int flags,int id){
        synchronized(WalkerStore.LOCK){
            handler.removeCallbacks(tick);sensors.unregisterListener(this);physical=timers=false;
            ArrayList<WalkerStore> all=WalkerStore.all(this);
            if(intent!=null&&"pause".equals(intent.getAction())){
                for(WalkerStore s:all){if(s.tracking()&&s.mode==1)Native.timer(s.data,Native.now(),s.rate);s.mode=0;s.changed();}
                stopForeground(STOP_FOREGROUND_REMOVE);stopSelf();return START_NOT_STICKY;
            }
            boolean sensorOK=Build.VERSION.SDK_INT<29||checkSelfPermission(Manifest.permission.ACTIVITY_RECOGNITION)==PackageManager.PERMISSION_GRANTED;
            Sensor sensor=sensors.getDefaultSensor(Sensor.TYPE_STEP_COUNTER);sensorOK&=sensor!=null;
            for(WalkerStore s:all){
                if(s.tracking()&&s.mode==2&&!sensorOK){s.error="Physical step counting is unavailable. Allow activity access or select a time-based mode.";continue;}
                if(!s.tracking()){running.remove(s.profileId);continue;}
                if(!s.token.equals(running.get(s.profileId))){Native.resume(s.data,Native.now(),s.rate);s.changed();running.put(s.profileId,s.token);}
                if(s.mode==2){if(s.boot!=boot||SystemClock.elapsedRealtime()<s.sensorElapsed)s.sensorLast=-1;s.boot=boot;physical=true;}else timers=true;
            }
            if(physical&&!sensors.registerListener(this,sensor,SensorManager.SENSOR_DELAY_NORMAL)){
                for(WalkerStore s:all)if(s.tracking()&&s.mode==2)s.error="The phone step counter could not be enabled.";physical=false;
            }
            if(!physical&&!timers){stopSelf();return START_NOT_STICKY;}
            try{notice(true);}catch(SecurityException ex){for(WalkerStore s:all)if(s.tracking())s.error="Tracking permission missing: "+ex.getMessage();stopSelf();return START_NOT_STICKY;}
            handler.post(tick);
        }
        return START_STICKY;
    }
    private void notice(boolean start){
        int count=0;for(WalkerStore s:WalkerStore.all(this))if(s.tracking())count++;
        PendingIntent open=PendingIntent.getActivity(this,0,new Intent(this,MainActivity.class),PendingIntent.FLAG_IMMUTABLE|PendingIntent.FLAG_UPDATE_CURRENT);
        PendingIntent pause=PendingIntent.getService(this,1,new Intent(this,WalkingService.class).setAction("pause"),PendingIntent.FLAG_IMMUTABLE|PendingIntent.FLAG_UPDATE_CURRENT);
        Notification n=new Notification.Builder(this,"walking").setSmallIcon(R.drawable.ic_stat_walker).setContentTitle("Pokewalker · "+count+" active profile"+(count==1?"":"s"))
            .setContentText("All active profiles earn steps. Tap to view your walkers.").setContentIntent(open).setOngoing(true).setOnlyAlertOnce(true)
            .addAction(new Notification.Action.Builder(android.R.drawable.ic_media_pause,"Pause all",pause).build()).build();
        if(start){if(Build.VERSION.SDK_INT>=34)startForeground(1,n,(physical?ServiceInfo.FOREGROUND_SERVICE_TYPE_HEALTH:0)|(timers?ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE:0));else startForeground(1,n);}
        else ((NotificationManager)getSystemService(NOTIFICATION_SERVICE)).notify(1,n);
    }
    @Override public void onSensorChanged(SensorEvent event){
        if(event.sensor.getType()!=Sensor.TYPE_STEP_COUNTER)return;
        synchronized(WalkerStore.LOCK){for(WalkerStore s:WalkerStore.all(this))if(s.tracking()&&s.mode==2){
            s.creditPhysical((long)event.values[0],SystemClock.elapsedRealtime(),boot);
        }}
    }
    @Override public void onAccuracyChanged(Sensor sensor,int accuracy){}
    @Override public void onDestroy(){handler.removeCallbacks(tick);if(sensors!=null)sensors.unregisterListener(this);synchronized(WalkerStore.LOCK){for(WalkerStore s:WalkerStore.all(this))if(s.data!=null)s.changed();}super.onDestroy();}
    @Override public IBinder onBind(Intent intent){return null;}
}
