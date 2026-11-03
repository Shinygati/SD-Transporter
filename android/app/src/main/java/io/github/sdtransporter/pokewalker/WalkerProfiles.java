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
    }
    static boolean validID(String id){return id.equals("default")||id.matches("[a-f0-9]{32}");}
    WalkerStore store(String id){
        if(!stores.containsKey(id)){
            File directory=id.equals("default")?root:new File(new File(root,"profiles"),id);
            WalkerStore value=new WalkerStore(directory,id);if(!error.isEmpty())value.error=error;
            stores.put(id,value);
        }
        return stores.get(id);
    }
    ArrayList<WalkerStore> all(){ArrayList<WalkerStore> list=new ArrayList<>();for(Entry e:entries)list.add(store(e.id));return list;}
    String name(String id){for(Entry e:entries)if(e.id.equals(id))return e.name;return "Walker";}
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
