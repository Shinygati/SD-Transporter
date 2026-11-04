package io.github.sdtransporter.pokewalker;
import android.content.Context;import android.util.AtomicFile;
import java.io.*;import java.nio.file.*;import java.util.*;
public class ProfileTest {
 static void populate(WalkerStore s,String token,String key)throws Exception{s.data=new byte[65536];s.token=token;s.trainerKey=key;s.mode=2;s.returnPending=false;s.revoked=false;s.returnCompleted=false;s.save();}
 public static void main(String[] args)throws Exception{
  File dir=Files.createTempDirectory("walker-profiles-").toFile();Context ctx=new Context(dir);
  WalkerStore a=StoreTest.reopen(dir);populate(a,"11111111111111111111111111111111","111111111111111111111111111111111111");
  if(!a.file.getBaseFile().equals(new File(dir,"walker-state.bin")))throw new AssertionError("Legacy journal moved");
  WalkerStore b=WalkerStore.createProfile(ctx,"SoulSilver");populate(b,"22222222222222222222222222222222","222222222222222222222222222222222222");String bID=b.profileId;
  if(a.file.getBaseFile().equals(b.file.getBaseFile())||WalkerStore.all(ctx).size()!=2)throw new AssertionError("Profiles share a journal");
  for(WalkerStore s:WalkerStore.all(ctx))s.creditPhysical(100,1000,1);
  for(WalkerStore s:WalkerStore.all(ctx))s.creditPhysical(125,2000,1);
  if(Native.steps(a.data)!=25||Native.steps(b.data)!=25)throw new AssertionError("Not all active profiles earned steps");
  b.returnPending=true;b.save();for(WalkerStore s:WalkerStore.all(ctx))s.creditPhysical(130,3000,1);
  if(Native.steps(a.data)!=30||Native.steps(b.data)!=25)throw new AssertionError("Pending return earned steps or froze another profile");
  b.returnPending=false;b.mode=0;b.save();a.mode=1;a.save();for(WalkerStore s:WalkerStore.all(ctx))s.creditTimer(1000100);
  if(Native.steps(a.data)!=31||Native.steps(b.data)!=25)throw new AssertionError("Timer isolation failed");
  long op=b.beginSync();boolean refused=false;try{WalkerStore.selectProfile(ctx,"default");}catch(IOException expected){refused=true;}if(!refused)throw new AssertionError("Switched during a transfer");b.finishSync(op);
  WalkerStore.selectProfile(ctx,"default");a.deleteActive();
  if(!a.trainerKey.isEmpty()||a.active())throw new AssertionError("Delete retained trainer binding");
  // Simulate the old build's successful delete journal, which kept its binding.
  a.trainerKey="111111111111111111111111111111111111";a.save();
  if(!StoreTest.reopen(dir).trainerKey.isEmpty())throw new AssertionError("Legacy deleted profile stayed trainer-bound");
  if(!b.active()||!Files.exists(b.file.getBaseFile().toPath()))throw new AssertionError("Deleting one profile cleared another");
  WalkerStore.selectProfile(ctx,bID);WalkerStore loaded=StoreTest.reopen(dir);
  if(!loaded.profileId.equals(bID)||!loaded.active()||!loaded.trainerKey.equals("222222222222222222222222222222222222"))throw new AssertionError("Selection or trainer binding lost on restart");
  for(WalkerStore item:WalkerStore.all(ctx))if(item.profileId.equals("default")&&item.active())throw new AssertionError("Deleted profile reactivated");
  AtomicFile.silentDrop=true;refused=false;try{WalkerStore.selectProfile(ctx,"default");}catch(IOException expected){refused=true;}AtomicFile.silentDrop=false;
  if(!refused||!WalkerStore.get(ctx).profileId.equals(bID))throw new AssertionError("Failed catalogue write changed selected profile");
  System.out.println("Profiles: legacy migration, separate journals/trainer bindings, all-active physical/timer credit, pending-return isolation, deletion isolation, restart and failed-catalogue writes passed.");
 }
}
