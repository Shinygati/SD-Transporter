package io.github.sdtransporter.pokewalker;

import android.content.Context;
import android.util.AtomicFile;
import java.io.*;
import java.util.Arrays;
import java.util.zip.CRC32;

final class WalkerStore {
    static final Object LOCK=new Object();
    private static WalkerStore instance;
    private static WalkerProfiles profiles;
    final String profileId;
    String trainerKey="";
    final AtomicFile file;
    byte[] data;
    // pin is an unused legacy storage slot retained so existing app journals remain readable.
    String token="",host="",pin="",pendingToken="",error="";
    int mode=2,rate=10,boot=-1;
    long sensorLast=-1,sensorElapsed;
    boolean revoked,returnPending,expandedItems,visitBlocked;
    int classicVolume=2,classicContrast=1;
    String battleSession="",battlePeer="";
    boolean battlePaid,battleDone,battleRewarded;
    int battleRewardChoice=-1,battleWinner;
    boolean returnCompleted;
    volatile boolean syncing;
    private volatile long syncEpoch;
    long beginSync() throws IOException {
        if(syncing)throw new IOException("A sync is already in progress");
        syncing=true;return ++syncEpoch;
    }
    boolean currentSync(long operation){return operation==syncEpoch;}
    void finishSync(long operation){if(currentSync(operation))syncing=false;}
    static WalkerStore get(Context context) {
        synchronized(LOCK) {
            File root=context.getApplicationContext().getFilesDir();
            if(instance==null || profiles==null || !profiles.root.equals(root)){
                profiles=new WalkerProfiles(root);instance=profiles.store(profiles.selected);
            }
            return instance;
        }
    }
    static java.util.ArrayList<WalkerStore> all(Context context){synchronized(LOCK){get(context);return profiles.all();}}
    static java.util.ArrayList<WalkerProfiles.Entry> profileEntries(Context context){synchronized(LOCK){get(context);return new java.util.ArrayList<>(profiles.entries);}}
    static String profileName(Context context){synchronized(LOCK){get(context);return profiles.name(instance.profileId);}}
    static WalkerStore selectProfile(Context context,String id)throws IOException{synchronized(LOCK){get(context);profiles.select(id);instance=profiles.store(id);return instance;}}
    static WalkerStore createProfile(Context context,String name)throws IOException{synchronized(LOCK){get(context);String id=profiles.create(name);instance=profiles.store(id);return instance;}}
    static int[] visit(Context context,String id)throws IOException{synchronized(LOCK){get(context);return profiles.visit(instance.profileId,id);}}
    static WalkerStore recoverVisits(Context context){synchronized(LOCK){instance=null;profiles=null;return get(context);}}
    static void renameProfile(Context context,String name)throws IOException{synchronized(LOCK){get(context);profiles.rename(instance.profileId,name);}}
    boolean tracking(){return active()&&mode!=0&&!returnPending&&!syncing&&error.isEmpty();}
    WalkerStore(File directory,String profileId) {
        this.profileId=profileId;
        file=new AtomicFile(new File(directory,"walker-state.bin"));
        if(!directory.isDirectory()&&!directory.mkdirs()){error="Cannot open profile storage";revoked=true;return;}
        try(DataInputStream in=new DataInputStream(file.openRead())) {
            if(in.readLong()!=0x5057414e44523101L)throw new IOException("Unknown state format");
            int length=in.readInt();
            if(length!=0 && length!=65536)throw new IOException("Invalid walker size");
            if(length!=0){data=new byte[length];in.readFully(data);}
            token=in.readUTF();host=in.readUTF();pin=in.readUTF();pendingToken=in.readUTF();
            mode=in.readInt();rate=in.readInt();boot=in.readInt();sensorLast=in.readLong();
            sensorElapsed=in.readLong();revoked=in.readBoolean();
            try{returnPending=in.readBoolean();}catch(EOFException legacy){returnPending=false;}
            if(in.available()>0)trainerKey=in.readUTF();
            if(in.available()>0)expandedItems=in.readBoolean();
            if(in.available()>0){classicVolume=in.readInt();classicContrast=in.readInt();if(classicVolume<0||classicVolume>2||classicContrast<0||classicContrast>2)throw new IOException("Invalid display settings");}
            if(in.available()>0){if(in.readInt()!=0x50425731)throw new IOException("Invalid battle record");battleSession=in.readUTF();battlePeer=in.readUTF();battlePaid=in.readBoolean();battleDone=in.readBoolean();battleRewarded=in.readBoolean();battleRewardChoice=in.readInt();battleWinner=in.readInt();if(!battleSession.isEmpty()&&!battleSession.matches("[a-f0-9]{32}"))throw new IOException("Invalid battle session");if(battleRewardChoice < -1 || battleRewardChoice>0x8218||battleWinner<0||battleWinner>3)throw new IOException("Invalid battle result");}
            if(!trainerKey.isEmpty()&&!trainerKey.matches("[a-f0-9]{36}"))throw new IOException("Invalid trainer binding");
            if(mode<0||mode>2||rate<1||rate>86400)throw new IOException("Invalid settings");
            if(data!=null && !Native.walkable(data))throw new IOException("Invalid or unregistered walker contents");
            if(!token.isEmpty() && completedSession(token)) {
                if(deletedSession(token))trainerKey="";
                data=null;token="";pendingToken="";returnPending=false;revoked=false;mode=0;returnCompleted=true;
            }
            if(data==null && token.isEmpty() && !trainerKey.isEmpty() && latestSessionDeleted())trainerKey="";
        } catch(FileNotFoundException ignored) {
        } catch(Exception ex) {
            data=null;revoked=true;error="Saved state could not be opened: "+ex.getMessage();
        }
    }
    boolean active(){return data!=null && !revoked && !returnCompleted && !token.isEmpty();}
    void creditPhysical(long total,long elapsed,int bootCount) {
        if(!tracking()||mode!=2)return;
        if(boot!=bootCount||elapsed<sensorElapsed)sensorLast=-1;
        if(sensorLast>=0&&total>=sensorLast)Native.physical(data,Native.now(),total-sensorLast);
        sensorLast=total;sensorElapsed=elapsed;boot=bootCount;changed();
    }
    void creditTimer(long now){if(tracking()&&mode==1){int earned=Native.timer(data,now,rate);if(earned>0)changed();}}
    private static String crc(byte[] data){CRC32 crc=new CRC32();crc.update(data);return String.format(java.util.Locale.ROOT,"%08x",crc.getValue());}
    private File receiptFile(String session) throws IOException {
        if(!session.matches("[a-fA-F0-9]{32}"))throw new IOException("Invalid handoff session");
        File dir=new File(file.getBaseFile().getParentFile(),"sessions");
        if(!dir.isDirectory()&&!dir.mkdirs())throw new IOException("Cannot preserve completed handoff");
        return new File(dir,session.toLowerCase(java.util.Locale.ROOT)+".returned");
    }
    boolean completedSession(String session) throws IOException {
        File receipt=receiptFile(session);if(!receipt.exists()&&!new File(receipt+".bak").exists()&&!new File(receipt+".new").exists())return false;
        try(InputStream in=new AtomicFile(receipt).openRead()) {
            ByteArrayOutputStream bytes=new ByteArrayOutputStream();int value;
            while((value=in.read())!=-1){if(bytes.size()>=256)throw new IOException("Invalid completed handoff receipt");bytes.write(value);}
            String[] fields=new String(bytes.toByteArray(),java.nio.charset.StandardCharsets.US_ASCII).split("\n");
            if(fields.length!=4||(!fields[0].equals("PWRETURN1")&&!fields[0].equals("PWDELETE1"))||!fields[1].equals(session)||!fields[2].matches("[a-f0-9]{8}"))throw new IOException("Completed handoff receipt is damaged; session remains blocked");
            byte[] prefix=(fields[0]+"\n"+fields[1]+"\n"+fields[2]+"\n").getBytes(java.nio.charset.StandardCharsets.US_ASCII);
            if(!fields[3].equals(crc(prefix)))throw new IOException("Completed handoff receipt failed verification");
            return true;
        }
    }
    private boolean deletedSession(String session)throws IOException {
        if(!completedSession(session))return false;
        try(DataInputStream in=new DataInputStream(new AtomicFile(receiptFile(session)).openRead())){
            byte[] magic=new byte[9];in.readFully(magic);return new String(magic,java.nio.charset.StandardCharsets.US_ASCII).equals("PWDELETE1");
        }
    }
    private boolean latestSessionDeleted()throws IOException {
        File dir=new File(file.getBaseFile().getParentFile(),"sessions");
        File[] files=dir.listFiles(f->f.getName().matches("[a-f0-9]{32}\\.returned"));
        if(files==null||files.length==0)return false;
        // Ambiguous equal timestamps must not silently unbind a returned profile.
        long newest=0;for(File f:files)newest=Math.max(newest,f.lastModified());
        boolean deleted=false;
        for(File f:files)if(f.lastModified()==newest){if(!deletedSession(f.getName().substring(0,32)))return false;deleted=true;}
        return deleted;
    }
    void completeReturn(String session,byte[] returned) throws IOException {
        retireSession(session,returned,false);
    }
    private void retireSession(String session,byte[] returned,boolean deleted) throws IOException {
        File destination=receiptFile(session);
        String prefix=(deleted?"PWDELETE1\n":"PWRETURN1\n")+session+"\n"+crc(returned)+"\n";
        byte[] record=(prefix+crc(prefix.getBytes(java.nio.charset.StandardCharsets.US_ASCII))+"\n").getBytes(java.nio.charset.StandardCharsets.US_ASCII);
        AtomicFile receipt=new AtomicFile(destination);
        if(!destination.exists())writeVerified(receipt,record);
        else if(!matches(receipt,record))throw new IOException("A different completed return already uses this session");
        if(!completedSession(session))throw new IOException("Completed return could not be preserved");
        returnCompleted=true;
    }
    void deleteRecovery(File selected) throws IOException {
        File directory=recoveryDirectory().getCanonicalFile();File actual=selected.getCanonicalFile();
        if(!actual.getParentFile().equals(directory)||!actual.getName().endsWith(".walker")||!actual.isFile())throw new IOException("Only recovery snapshots can be removed here");
        if(!selected.delete())throw new IOException("Could not delete this recovery copy");
    }
    void deleteActive() throws IOException {
        if(syncing)throw new IOException("Wait for sync to finish");
        if(data==null)throw new IOException("No active walker to delete");
        archiveCurrent("deleted");retireSession(token,data,true);
        data=null;token="";pendingToken="";trainerKey="";returnPending=false;revoked=false;returnCompleted=false;mode=0;sensorLast=-1;boot=-1;
        save();
    }
    private static boolean matches(AtomicFile file,byte[] expected)throws IOException {
        try(InputStream in=file.openRead()) {
            byte[] block=new byte[4096];int at=0;
            while(at<expected.length){int n=in.read(block,0,Math.min(block.length,expected.length-at));if(n<=0)return false;for(int i=0;i<n;i++)if(block[i]!=expected[at+i])return false;at+=n;}
            return in.read()==-1;
        }
    }
    static void writeVerified(AtomicFile file,byte[] data)throws IOException {
        FileOutputStream out=null;
        try {out=file.startWrite();out.write(data);file.finishWrite(out);out=null;if(!matches(file,data))throw new IOException("Saved file verification failed");}
        catch(IOException ex){if(out!=null)file.failWrite(out);throw ex;}
    }
    static String recoveryKey(byte[] bytes) {
        int base=0xed;
        for(int candidate:new int[]{0xed,0x1ed}){int sum=1;for(int n=0;n<104;n++)sum+=(bytes[candidate+n]&255);if((sum&255)==(bytes[candidate+104]&255)){base=candidate;break;}}
        StringBuilder key=new StringBuilder();
        for(int n=0;n<56;n++)if(n<4||(n>=8&&n<10)||n>=12)key.append(String.format(java.util.Locale.ROOT,"%02x",bytes[base+n]&255));
        return key.toString();
    }
    File recoveryDirectory(){return new File(file.getBaseFile().getParentFile(),"recovery");}
    File archiveCurrent(String reason) throws IOException {
        if(data==null)return null;
        if(data.length!=65536)throw new IOException("Recovery copy has an invalid size");
        File directory=recoveryDirectory();
        if(!directory.isDirectory()&&!directory.mkdirs())throw new IOException("Cannot create recovery folder");
        CRC32 crc=new CRC32();crc.update(data);
        String id=token.matches("[a-fA-F0-9]{32}")?token:"local";
        File destination=new File(directory,reason+"-"+id+"-"+Long.toHexString(crc.getValue())+".walker");
        AtomicFile archive=new AtomicFile(destination);
        if(!destination.exists()) {
            FileOutputStream stream=null;
            try{stream=archive.startWrite();stream.write(data);archive.finishWrite(stream);}
            catch(IOException ex){if(stream!=null)archive.failWrite(stream);throw ex;}
        }
        try(InputStream in=archive.openRead()) {
            byte[] check=new byte[65536];int used=0;
            while(used<check.length){int n=in.read(check,used,check.length-used);if(n<0)break;used+=n;}
            if(used!=65536||in.read()!=-1||!Arrays.equals(check,data))throw new IOException("Recovery verification failed; existing copy was kept");
        }
        File[] copies=directory.listFiles(f->f.isFile()&&f.getName().endsWith(".walker"));
        if(copies==null)throw new IOException("Cannot check recovery retention");
        Arrays.sort(copies,(a,b)->{int time=Long.compare(a.lastModified(),b.lastModified());return time!=0?time:a.getName().compareTo(b.getName());});
        java.util.ArrayList<File> sameWalker=new java.util.ArrayList<>();String key=recoveryKey(data);
        for(File copy:copies){try(FileInputStream in=new FileInputStream(copy)){byte[] candidate=new byte[65536];int used=0;while(used<candidate.length){int n=in.read(candidate,used,candidate.length-used);if(n<0)break;used+=n;}if(used==65536&&in.read()==-1&&recoveryKey(candidate).equals(key))sameWalker.add(copy);}}
        int remaining=sameWalker.size();
        for(File copy:sameWalker)if(remaining>10&&!copy.equals(destination)){if(!copy.delete())throw new IOException("Cannot remove oldest recovery copy");remaining--;}
        return destination;
    }
    byte[] encoded()throws IOException {
            ByteArrayOutputStream bytes=new ByteArrayOutputStream();DataOutputStream out=new DataOutputStream(bytes);
            out.writeLong(0x5057414e44523101L);out.writeInt(data==null?0:data.length);
            if(data!=null)out.write(data);
            out.writeUTF(token);out.writeUTF(host);out.writeUTF(pin);out.writeUTF(pendingToken);
            out.writeInt(mode);out.writeInt(rate);out.writeInt(boot);out.writeLong(sensorLast);
            out.writeLong(sensorElapsed);out.writeBoolean(revoked);out.writeBoolean(returnPending);out.writeUTF(trainerKey);out.writeBoolean(expandedItems);out.writeInt(classicVolume);out.writeInt(classicContrast);out.writeInt(0x50425731);out.writeUTF(battleSession);out.writeUTF(battlePeer);out.writeBoolean(battlePaid);out.writeBoolean(battleDone);out.writeBoolean(battleRewarded);out.writeInt(battleRewardChoice);out.writeInt(battleWinner);out.flush();
            return bytes.toByteArray();
    }
    void save() throws IOException {
        if(visitBlocked)throw new IOException("A profile connection needs recovery. Reopen saved state before continuing.");
        /* Same pre-save check as the consoles; refusal leaves the saved file untouched. */
        if(data!=null&&(data.length!=65536||!Native.check(data)))
            throw new IOException("Walker data check failed. Your last saved progress was kept.");
        try {
            writeVerified(file,encoded());error="";
        } catch(IOException ex) {
            error="Could not save app state: "+ex.getMessage();throw ex;
        }
    }
    void changed() {
        try{save();}catch(IOException ex){mode=0;error=ex.getMessage();}
    }
}
