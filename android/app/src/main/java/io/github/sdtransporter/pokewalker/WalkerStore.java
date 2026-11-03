package io.github.sdtransporter.pokewalker;

import android.content.Context;
import android.util.AtomicFile;
import java.io.*;

final class WalkerStore {
    static final Object LOCK=new Object();
    private static WalkerStore instance;
    final AtomicFile file;
    byte[] data;
    // pin is an unused legacy storage slot retained so existing app journals remain readable.
    String token="",host="",pin="",pendingToken="",error="";
    int mode=2,rate=10,boot=-1;
    long sensorLast=-1,sensorElapsed;
    boolean revoked;
    static WalkerStore get(Context context) {
        synchronized(LOCK) {
            if(instance==null)instance=new WalkerStore(context.getApplicationContext());
            return instance;
        }
    }
    private WalkerStore(Context context) {
        file=new AtomicFile(new File(context.getFilesDir(),"walker-state.bin"));
        try(DataInputStream in=new DataInputStream(file.openRead())) {
            if(in.readLong()!=0x5057414e44523101L)throw new IOException("Unknown state format");
            int length=in.readInt();
            if(length!=0 && length!=65536)throw new IOException("Invalid walker size");
            if(length!=0){data=new byte[length];in.readFully(data);}
            token=in.readUTF();host=in.readUTF();pin=in.readUTF();pendingToken=in.readUTF();
            mode=in.readInt();rate=in.readInt();boot=in.readInt();sensorLast=in.readLong();
            sensorElapsed=in.readLong();revoked=in.readBoolean();
            if(mode<0||mode>2||rate<1||rate>86400)throw new IOException("Invalid settings");
            if(data!=null && Native.info(data)[Native.HAS_POKEMON]==0)throw new IOException("Invalid walker contents");
        } catch(FileNotFoundException ignored) {
        } catch(Exception ex) {
            data=null;revoked=true;error="Saved state could not be opened: "+ex.getMessage();
        }
    }
    boolean active(){return data!=null && !revoked && !token.isEmpty();}
    void save() throws IOException {
        FileOutputStream stream=null;
        try {
            stream=file.startWrite();DataOutputStream out=new DataOutputStream(stream);
            out.writeLong(0x5057414e44523101L);out.writeInt(data==null?0:data.length);
            if(data!=null)out.write(data);
            out.writeUTF(token);out.writeUTF(host);out.writeUTF(pin);out.writeUTF(pendingToken);
            out.writeInt(mode);out.writeInt(rate);out.writeInt(boot);out.writeLong(sensorLast);
            out.writeLong(sensorElapsed);out.writeBoolean(revoked);out.flush();
            file.finishWrite(stream);error="";
        } catch(IOException ex) {
            if(stream!=null)file.failWrite(stream);
            error="Could not save app state: "+ex.getMessage();throw ex;
        }
    }
    void changed() {
        try{save();}catch(IOException ex){mode=0;error=ex.getMessage();}
    }
}
