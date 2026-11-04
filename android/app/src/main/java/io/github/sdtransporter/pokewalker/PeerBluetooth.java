package io.github.sdtransporter.pokewalker;
import android.annotation.SuppressLint;
import android.bluetooth.*;
import android.bluetooth.le.*;
import android.content.Context;
import android.os.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
@SuppressLint("MissingPermission")
final class PeerBluetooth implements AutoCloseable {
    static final UUID SERVICE=UUID.fromString("78bcf034-78d1-4d38-a6d6-f04727081500");
    static final UUID RX=UUID.fromString("78bcf034-78d1-4d38-a6d6-f04727081501"),TX=UUID.fromString("78bcf034-78d1-4d38-a6d6-f04727081502");
    static final UUID CCCD=UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");
    interface Listener{void device(String id,String name);void ready(boolean host);void message(byte[] data);void status(String message);void disconnected();}
    private final Context context;private final Listener listener;private final Handler main=new Handler(Looper.getMainLooper());
    private final BluetoothManager manager;private final BluetoothAdapter adapter;private final HashMap<String,BluetoothDevice> devices=new HashMap<>();
    private BluetoothLeScanner scanner;private BluetoothLeAdvertiser advertiser;private BluetoothGatt client;private BluetoothGattServer server;private BluetoothDevice remote;
    private BluetoothGattCharacteristic receive,transmit;private final ArrayDeque<byte[]> queue=new ArrayDeque<>();private final PeerFrames frames=new PeerFrames();
    private boolean closed,host,busy,ready;private int sequence=1;private long lastActivity=SystemClock.elapsedRealtime();
    private final Runnable timeout=new Runnable(){public void run(){if(closed)return;if(ready&&SystemClock.elapsedRealtime()-lastActivity>120000){error("Bluetooth connection timed out");return;}main.postDelayed(this,1000);}};
    PeerBluetooth(Context context,Listener listener)throws IOException{this.context=context;this.listener=listener;manager=(BluetoothManager)context.getSystemService(Context.BLUETOOTH_SERVICE);adapter=manager==null?null:manager.getAdapter();if(adapter==null||!adapter.isEnabled())throw new IOException("Turn on Bluetooth first");main.post(timeout);}
    void search(){host=false;scanner=adapter.getBluetoothLeScanner();if(scanner==null){error("Bluetooth discovery is unavailable");return;}listener.status("Searching for nearby Pokewalkers");scanner.startScan(Collections.singletonList(new ScanFilter.Builder().setServiceUuid(new ParcelUuid(SERVICE)).build()),new ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(),scan);}
    private final ScanCallback scan=new ScanCallback(){
        public void onScanResult(int type,ScanResult result){main.post(()->{if(closed)return;BluetoothDevice device=result.getDevice();String id=device.getAddress();if(devices.containsKey(id))return;devices.put(id,device);ScanRecord record=result.getScanRecord();byte[] data=record==null?null:record.getServiceData(new ParcelUuid(SERVICE));String name=data!=null?new String(data,StandardCharsets.UTF_8):record!=null?record.getDeviceName():null;listener.device(id,name==null?"Pokewalker":name);});}
        public void onScanFailed(int code){main.post(()->error("Bluetooth search could not start ("+code+")"));}
    };
    void connect(String id){BluetoothDevice device=devices.get(id);if(device==null||closed)return;if(scanner!=null)scanner.stopScan(scan);listener.status("Connecting");client=device.connectGatt(context,false,clientCallbacks,BluetoothDevice.TRANSPORT_LE);}
    void host(String name){host=true;advertiser=adapter.getBluetoothLeAdvertiser();if(advertiser==null){error("This phone cannot host Bluetooth. Choose Search and host on the other phone.");return;}
        server=manager.openGattServer(context,serverCallbacks);if(server==null){error("Bluetooth service could not start");return;}
        BluetoothGattService service=new BluetoothGattService(SERVICE,BluetoothGattService.SERVICE_TYPE_PRIMARY);
        receive=new BluetoothGattCharacteristic(RX,BluetoothGattCharacteristic.PROPERTY_WRITE,BluetoothGattCharacteristic.PERMISSION_WRITE);
        transmit=new BluetoothGattCharacteristic(TX,BluetoothGattCharacteristic.PROPERTY_NOTIFY,BluetoothGattCharacteristic.PERMISSION_READ);
        transmit.addDescriptor(new BluetoothGattDescriptor(CCCD,BluetoothGattDescriptor.PERMISSION_READ|BluetoothGattDescriptor.PERMISSION_WRITE));service.addCharacteristic(receive);service.addCharacteristic(transmit);
        advertisedName=name.getBytes(StandardCharsets.UTF_8);if(advertisedName.length>10)advertisedName=Arrays.copyOf(advertisedName,10);
        if(!server.addService(service))error("Bluetooth service could not be published");
    }
    private byte[] advertisedName;
    private final AdvertiseCallback advertising=new AdvertiseCallback(){public void onStartFailure(int code){main.post(()->error("Bluetooth hosting could not start ("+code+")"));}};
    private void advertise(){if(closed||advertiser==null)return;advertiser.startAdvertising(new AdvertiseSettings.Builder().setConnectable(true).setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_LOW_LATENCY).setTimeout(0).build(),new AdvertiseData.Builder().addServiceUuid(new ParcelUuid(SERVICE)).build(),new AdvertiseData.Builder().addServiceData(new ParcelUuid(SERVICE),advertisedName).build(),advertising);listener.status("Ready for a nearby phone");}
    private final BluetoothGattCallback clientCallbacks=new BluetoothGattCallback(){
        public void onConnectionStateChange(BluetoothGatt g,int status,int state){main.post(()->{if(closed||g!=client)return;if(status!=BluetoothGatt.GATT_SUCCESS||state==BluetoothProfile.STATE_DISCONNECTED){drop();return;}if(state==BluetoothProfile.STATE_CONNECTED&&!g.discoverServices())error("Bluetooth service discovery failed");});}
        public void onServicesDiscovered(BluetoothGatt g,int status){main.post(()->{if(closed||g!=client)return;BluetoothGattService service=g.getService(SERVICE);receive=service==null?null:service.getCharacteristic(RX);transmit=service==null?null:service.getCharacteristic(TX);if(status!=0||receive==null||transmit==null||!g.setCharacteristicNotification(transmit,true)){error("The other phone is not running a compatible Pokewalker");return;}BluetoothGattDescriptor descriptor=transmit.getDescriptor(CCCD);if(descriptor==null){error("Bluetooth notifications are unavailable");return;}descriptor.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);if(!g.writeDescriptor(descriptor))error("Bluetooth notification setup failed");});}
        public void onDescriptorWrite(BluetoothGatt g,BluetoothGattDescriptor descriptor,int status){main.post(()->{if(closed||g!=client)return;if(status!=0){error("Bluetooth notification setup failed");return;}ready=true;lastActivity=SystemClock.elapsedRealtime();listener.ready(false);pump();});}
        public void onCharacteristicWrite(BluetoothGatt g,BluetoothGattCharacteristic c,int status){main.post(()->{if(closed||g!=client)return;if(status!=0){error("Bluetooth write failed");return;}sent();});}
        public void onCharacteristicChanged(BluetoothGatt g,BluetoothGattCharacteristic c){byte[] data=c.getValue();if(data!=null){byte[] copy=data.clone();main.post(()->{if(!closed&&g==client)receive(copy);});}}
        public void onCharacteristicChanged(BluetoothGatt g,BluetoothGattCharacteristic c,byte[] value){byte[] copy=value.clone();main.post(()->{if(!closed&&g==client)receive(copy);});}
    };
    private final BluetoothGattServerCallback serverCallbacks=new BluetoothGattServerCallback(){
        public void onServiceAdded(int status,BluetoothGattService s){main.post(()->{if(status==0)advertise();else error("Bluetooth service could not be published");});}
        public void onConnectionStateChange(BluetoothDevice device,int status,int state){main.post(()->{if(closed)return;if(state==BluetoothProfile.STATE_DISCONNECTED&&device.equals(remote))drop();});}
        public void onDescriptorWriteRequest(BluetoothDevice device,int requestId,BluetoothGattDescriptor descriptor,boolean prepared,boolean response,int offset,byte[] value){main.post(()->{
            if(closed||server==null)return;boolean accept=!prepared&&offset==0&&CCCD.equals(descriptor.getUuid())&&Arrays.equals(value,BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)&&(remote==null||remote.equals(device));
            if(response)server.sendResponse(device,requestId,accept?BluetoothGatt.GATT_SUCCESS:BluetoothGatt.GATT_FAILURE,offset,null);
            if(accept&&!ready){remote=device;ready=true;lastActivity=SystemClock.elapsedRealtime();if(advertiser!=null)advertiser.stopAdvertising(advertising);listener.ready(true);pump();}
        });}
        public void onCharacteristicWriteRequest(BluetoothDevice device,int requestId,BluetoothGattCharacteristic c,boolean prepared,boolean response,int offset,byte[] value){byte[] copy=value==null?new byte[0]:value.clone();main.post(()->{
            if(closed||server==null)return;boolean accept=ready&&device.equals(remote)&&!prepared&&offset==0&&RX.equals(c.getUuid())&&copy.length<=20;
            if(response)server.sendResponse(device,requestId,accept?BluetoothGatt.GATT_SUCCESS:BluetoothGatt.GATT_FAILURE,offset,null);
            if(accept)receive(copy);
        });}
        public void onNotificationSent(BluetoothDevice device,int status){main.post(()->{if(closed||!device.equals(remote))return;if(status!=0)error("Bluetooth notification failed");else sent();});}
    };
    void send(byte[] payload)throws IOException{if(closed||!ready)throw new IOException("Bluetooth is not connected");queue.addAll(PeerFrames.encode(sequence++&65535,payload));pump();}
    private void pump(){if(closed||!ready||busy||queue.isEmpty())return;byte[] packet=queue.peek();busy=true;lastActivity=SystemClock.elapsedRealtime();boolean ok;
        if(host){transmit.setValue(packet);ok=server.notifyCharacteristicChanged(remote,transmit,false);}
        else{receive.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);receive.setValue(packet);ok=client.writeCharacteristic(receive);}
        if(!ok)error("Bluetooth could not queue data");
    }
    private void sent(){if(!busy)return;queue.poll();busy=false;lastActivity=SystemClock.elapsedRealtime();pump();}
    private void receive(byte[] packet){try{lastActivity=SystemClock.elapsedRealtime();byte[] data=frames.accept(packet);if(data!=null)listener.message(data);}catch(IOException ex){error(ex.getMessage());}}
    private void error(String message){if(closed)return;listener.status(message);close();listener.disconnected();}
    private void drop(){if(closed)return;listener.status("Bluetooth disconnected. Your Pokémon remains on this phone.");close();listener.disconnected();}
    public void close(){if(closed)return;closed=true;ready=false;main.removeCallbacks(timeout);try{if(scanner!=null)scanner.stopScan(scan);if(advertiser!=null)advertiser.stopAdvertising(advertising);if(client!=null){client.disconnect();client.close();}if(server!=null){if(remote!=null)server.cancelConnection(remote);server.close();}}catch(RuntimeException ignored){}queue.clear();}
}
