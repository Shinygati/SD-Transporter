package io.github.sdtransporter.pokewalker;

import android.Manifest;
import android.app.*;
import android.content.*;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.hardware.*;
import android.os.*;
import android.provider.Settings;

public final class WalkingService extends Service implements SensorEventListener {
    private WalkerStore state;
    private SensorManager sensors;
    private final Handler handler=new Handler(Looper.getMainLooper());
    private int boot;
    private long lastNotice;
    private final Runnable tick=new Runnable(){public void run(){
        synchronized(WalkerStore.LOCK) {
            if(!state.active()||state.mode==0){stopSelf();return;}
            if(state.mode==1){int earned=Native.timer(state.data,Native.now(),state.rate);if(earned>0)state.changed();}
            if(SystemClock.elapsedRealtime()-lastNotice>=10000){notice(false);lastNotice=SystemClock.elapsedRealtime();}
        }
        handler.postDelayed(this,1000);
    }};
    @Override public void onCreate(){
        super.onCreate();state=WalkerStore.get(this);
        sensors=(SensorManager)getSystemService(SENSOR_SERVICE);
        boot=Settings.Global.getInt(getContentResolver(),Settings.Global.BOOT_COUNT,-1);
        NotificationManager manager=(NotificationManager)getSystemService(NOTIFICATION_SERVICE);
        manager.createNotificationChannel(new NotificationChannel("walking","Walking",NotificationManager.IMPORTANCE_LOW));
    }
    @Override public int onStartCommand(Intent intent,int flags,int id){
        synchronized(WalkerStore.LOCK) {
            handler.removeCallbacks(tick);sensors.unregisterListener(this);
            if(!state.active()||state.mode==0){stopSelf();return START_NOT_STICKY;}
            if(state.mode==2) {
                if(Build.VERSION.SDK_INT>=29 && checkSelfPermission(Manifest.permission.ACTIVITY_RECOGNITION)!=PackageManager.PERMISSION_GRANTED) {
                    state.error="Allow physical activity access to use the phone pedometer.";stopSelf();return START_NOT_STICKY;
                }
                Sensor sensor=sensors.getDefaultSensor(Sensor.TYPE_STEP_COUNTER);
                if(sensor==null){state.error="This phone has no step counter sensor. No automatic timer fallback is used.";stopSelf();return START_NOT_STICKY;}
                if(state.boot!=boot || SystemClock.elapsedRealtime()<state.sensorElapsed)state.sensorLast=-1;
                state.boot=boot;
                if(!sensors.registerListener(this,sensor,SensorManager.SENSOR_DELAY_NORMAL)) {
                    state.error="The phone step counter could not be enabled.";stopSelf();return START_NOT_STICKY;
                }
            }
            Native.resume(state.data,Native.now(),state.rate);state.changed();
            try{notice(true);}catch(SecurityException ex){state.error="Tracking permission is missing: "+ex.getMessage();stopSelf();return START_NOT_STICKY;}
            handler.post(tick);
        }
        return START_STICKY;
    }
    private void notice(boolean start){
        long[] info=Native.info(state.data);
        PendingIntent open=PendingIntent.getActivity(this,0,new Intent(this,MainActivity.class),PendingIntent.FLAG_IMMUTABLE|PendingIntent.FLAG_UPDATE_CURRENT);
        Notification notification=new Notification.Builder(this,"walking").setSmallIcon(R.drawable.ic_stat_walker)
            .setContentTitle("Pokewalker · "+(state.mode==2?"Phone pedometer":"Time-based walking"))
            .setContentText(info[Native.STROLL]+" steps · "+info[Native.WATTS]+" watts").setContentIntent(open).setOngoing(true).setOnlyAlertOnce(true).build();
        if(start) {
            if(Build.VERSION.SDK_INT>=34)startForeground(1,notification,state.mode==2?ServiceInfo.FOREGROUND_SERVICE_TYPE_HEALTH:ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE);
            else startForeground(1,notification);
        } else ((NotificationManager)getSystemService(NOTIFICATION_SERVICE)).notify(1,notification);
    }
    @Override public void onSensorChanged(SensorEvent event){
        if(event.sensor.getType()!=Sensor.TYPE_STEP_COUNTER)return;
        synchronized(WalkerStore.LOCK) {
            if(!state.active()||state.mode!=2)return;
            long total=(long)event.values[0];
            if(state.sensorLast>=0 && total>=state.sensorLast)Native.physical(state.data,Native.now(),total-state.sensorLast);
            state.sensorLast=total;state.sensorElapsed=SystemClock.elapsedRealtime();state.boot=boot;
            /* The EEPROM and sensor baseline commit together: a restart cannot
             * award an already-saved sensor interval for a second time. */
            state.changed();
            if(state.mode==0)stopSelf();
        }
    }
    @Override public void onAccuracyChanged(Sensor sensor,int accuracy){}
    @Override public void onDestroy(){handler.removeCallbacks(tick);if(sensors!=null)sensors.unregisterListener(this);synchronized(WalkerStore.LOCK){if(state!=null&&state.data!=null)state.changed();}super.onDestroy();}
    @Override public IBinder onBind(Intent intent){return null;}
}
