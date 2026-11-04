package io.github.sdtransporter.pokewalker;
import android.util.AtomicFile;
import java.io.*;
import java.util.*;
import java.util.zip.CRC32;
/* Profile catalogue is separate from each walker's verified ownership journal. */
final class WalkerProfiles {
    static final class Entry {final String id;String name;Entry(String id,String name){this.id=id;this.name=name;}}
    final File root;final AtomicFile file;final ArrayList<Entry> entries=new ArrayList<>();
    final LinkedHashMap<String,WalkerStore> stores=new LinkedHashMap<>();
    String selected="default",error="";
    WalkerProfiles(File root){
        this.root=root;file=new AtomicFile(new File(root,"walker-profiles.bin"));
        entries.add(new Entry("default","Walker 1"));
        try(InputStream in=file.openRead()){
            ByteArrayOutputStream all=new ByteArrayOutputStream();byte[] block=new byte[1024];int n;
            while((n=in.read(block))!=-1){if(all.size()+n>65536)throw new IOException("Profile catalogue too large");all.write(block,0,n);}
            byte[] bytes=all.toByteArray();if(bytes.length<12)throw new IOException("Incomplete profile catalogue");
            CRC32 crc=new CRC32();crc.update(bytes,0,bytes.length-8);
            DataInputStream tail=new DataInputStream(new ByteArrayInputStream(bytes,bytes.length-8,8));if(tail.readLong()!=crc.getValue())throw new IOException("Profile catalogue checksum failed");
            DataInputStream data=new DataInputStream(new ByteArrayInputStream(bytes,0,bytes.length-8));
            if(data.readInt()!=0x50575031)throw new IOException("Unknown profile catalogue");
            String chosen=data.readUTF();int count=data.readInt();if(count<1||count>128)throw new IOException("Invalid profile count");
            ArrayList<Entry> loaded=new ArrayList<>();HashSet<String> ids=new HashSet<>();
            for(int i=0;i<count;i++){String id=data.readUTF(),name=data.readUTF();if(!validID(id)||!ids.add(id)||name.length()>100)throw new IOException("Invalid profile entry");loaded.add(new Entry(id,name));}
            if(data.available()!=0||!ids.contains(chosen))throw new IOException("Invalid selected profile");
            entries.clear();entries.addAll(loaded);selected=chosen;
        }catch(FileNotFoundException ignored){}catch(IOException ex){error="Profiles could not be opened: "+ex.getMessage();}
        if(error.isEmpty())try{recoverVisit();}catch(IOException ex){error="Profile connection needs recovery: "+ex.getMessage();}
    }
    static boolean validID(String id){return id.equals("default")||id.matches("[a-f0-9]{32}");}
    WalkerStore store(String id){
        if(!stores.containsKey(id)){
            File directory=id.equals("default")?root:new File(new File(root,"profiles"),id);
            WalkerStore value=new WalkerStore(directory,id);if(!error.isEmpty()){value.error=error;value.visitBlocked=true;}
            stores.put(id,value);
        }
        return stores.get(id);
    }
    ArrayList<WalkerStore> all(){ArrayList<WalkerStore> list=new ArrayList<>();for(Entry e:entries)list.add(store(e.id));return list;}
    String name(String id){for(Entry e:entries)if(e.id.equals(id))return e.name;return "Walker";}
    private File directory(String id)throws IOException {
        boolean found=false;for(Entry e:entries)if(e.id.equals(id))found=true;
        if(!validID(id)||!found)throw new IOException("Unknown connection profile");
        return id.equals("default")?root:new File(new File(root,"profiles"),id);
    }
    private AtomicFile visitFile(){return new AtomicFile(new File(root,"profile-connection.bin"));}
    private static byte[] read(AtomicFile file)throws IOException {
        try(InputStream in=file.openRead();ByteArrayOutputStream out=new ByteArrayOutputStream()){
            byte[] block=new byte[4096];int n;while((n=in.read(block))!=-1){if(out.size()+n>600000)throw new IOException("Connection journal too large");out.write(block,0,n);}return out.toByteArray();
        }
    }
    private void recoverVisit()throws IOException {
        byte[] record;try{record=read(visitFile());}catch(FileNotFoundException absent){return;}
        DataInputStream in=new DataInputStream(new ByteArrayInputStream(record));int magic=in.readInt();
        if(magic==0x50575630 && record.length==4)return;
        if(magic!=0x50575631 || record.length<12)throw new IOException("Invalid connection journal");
        CRC32 crc=new CRC32();crc.update(record,0,record.length-8);
        if(new DataInputStream(new ByteArrayInputStream(record,record.length-8,8)).readLong()!=crc.getValue())throw new IOException("Connection journal checksum failed");
        String[] ids=new String[2];byte[][] before=new byte[2][],after=new byte[2][];AtomicFile[] files=new AtomicFile[2];
        for(int i=0;i<2;i++){
            ids[i]=in.readUTF();if(i==1&&ids[0].equals(ids[1]))throw new IOException("Connection targets repeat");files[i]=new AtomicFile(new File(directory(ids[i]),"walker-state.bin"));
            int size=in.readInt();if(size<1||size>150000)throw new IOException("Invalid connection baseline");before[i]=new byte[size];in.readFully(before[i]);
            size=in.readInt();if(size<1||size>150000)throw new IOException("Invalid connection result");after[i]=new byte[size];in.readFully(after[i]);
            byte[] current=read(files[i]);if(!Arrays.equals(current,before[i])&&!Arrays.equals(current,after[i]))throw new IOException("Profile changed during interrupted connection. Copies preserved.");
        }
        if(in.available()!=8)throw new IOException("Trailing connection data");
        for(int i=0;i<2;i++)WalkerStore.writeVerified(files[i],after[i]);
        WalkerStore.writeVerified(visitFile(),new byte[]{0x50,0x57,0x56,0x30});
    }
    String visit(String first,String second)throws IOException {
        ready();if(first.equals(second))throw new IOException("Choose another profile");
        WalkerStore a=store(first),b=store(second);
        if(!a.active()||!b.active()||a.returnPending||b.returnPending||!a.error.isEmpty()||!b.error.isEmpty())throw new IOException("Both Pokemon must be active on this phone and not syncing");
        a.save();b.save();a.archiveCurrent("connecting");b.archiveCurrent("connecting");
        byte[] aa=a.data.clone(),bb=b.data.clone();int ar=Native.visit(aa,b.data,Native.now()),br=Native.visit(bb,a.data,Native.now());
        if(ar==-1||br==-1)throw new IOException("These Pokewalkers already met today");
        if(ar<=0||br<=0)throw new IOException("Two different registered Pokewalkers with partners are required");
        byte[] beforeA=read(a.file),beforeB=read(b.file),afterA,afterB;
        byte[] oldA=a.data,oldB=b.data;
        try{a.data=aa;b.data=bb;afterA=a.encoded();afterB=b.encoded();}finally{a.data=oldA;b.data=oldB;}
        ByteArrayOutputStream bytes=new ByteArrayOutputStream();DataOutputStream out=new DataOutputStream(bytes);out.writeInt(0x50575631);
        for(int i=0;i<2;i++){byte[] before=i==0?beforeA:beforeB,after=i==0?afterA:afterB;out.writeUTF(i==0?first:second);out.writeInt(before.length);out.write(before);out.writeInt(after.length);out.write(after);}out.flush();CRC32 crc=new CRC32();crc.update(bytes.toByteArray());out.writeLong(crc.getValue());out.flush();
        try{
            WalkerStore.writeVerified(visitFile(),bytes.toByteArray());recoverVisit();
            a.data=aa;b.data=bb;a.archiveCurrent("connected");b.archiveCurrent("connected");
        }catch(IOException ex){error="Profile connection needs recovery: "+ex.getMessage();for(WalkerStore s:all()){s.error=error;s.visitBlocked=true;}throw ex;}
        return "The Pokemon played together. Both gifts were saved. Meet again tomorrow.";
    }
    void ready()throws IOException{if(!error.isEmpty())throw new IOException(error);for(WalkerStore s:all())if(s.syncing)throw new IOException("Wait for the transfer to finish before changing profiles");}
    void save()throws IOException{
        ByteArrayOutputStream payload=new ByteArrayOutputStream();DataOutputStream out=new DataOutputStream(payload);
        out.writeInt(0x50575031);out.writeUTF(selected);out.writeInt(entries.size());for(Entry e:entries){out.writeUTF(e.id);out.writeUTF(e.name);}out.flush();
        CRC32 crc=new CRC32();crc.update(payload.toByteArray());out.writeLong(crc.getValue());out.flush();WalkerStore.writeVerified(file,payload.toByteArray());
    }
    String create(String name)throws IOException{
        ready();if(entries.size()>=128)throw new IOException("Profile limit reached");String id=UUID.randomUUID().toString().replace("-","");
        Entry e=new Entry(id,name.trim().isEmpty()?"Walker "+(entries.size()+1):name.trim());if(e.name.length()>100)throw new IOException("Profile name too long");
        entries.add(e);String old=selected;selected=id;
        try{save();}catch(IOException ex){entries.remove(e);selected=old;throw ex;}return id;
    }
    void select(String id)throws IOException{ready();boolean found=false;for(Entry e:entries)if(e.id.equals(id))found=true;if(!found)throw new IOException("Unknown profile");String old=selected;selected=id;try{save();}catch(IOException ex){selected=old;throw ex;}}
    void rename(String id,String name)throws IOException{ready();name=name.trim();if(name.isEmpty()||name.length()>100)throw new IOException("Enter a profile name (1-100 characters)");for(Entry e:entries)if(e.id.equals(id)){String old=e.name;e.name=name;try{save();}catch(IOException ex){e.name=old;throw ex;}return;}}
}
