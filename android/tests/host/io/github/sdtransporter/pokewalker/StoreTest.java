package io.github.sdtransporter.pokewalker;
import java.io.*;import java.nio.file.*;import java.util.*;import android.content.Context;import android.util.AtomicFile;
public class StoreTest {
 static WalkerStore reopen(File dir)throws Exception {
  java.lang.reflect.Field singleton=WalkerStore.class.getDeclaredField("instance");singleton.setAccessible(true);singleton.set(null,null);
  return WalkerStore.get(new Context(dir));
 }
 public static void main(String[] args)throws Exception {
  File dir=Files.createTempDirectory("walker-state-test-").toFile();WalkerStore s=WalkerStore.get(new Context(dir));
  long firstOperation=s.beginSync();s.finishSync(firstOperation);long nextOperation=s.beginSync();s.finishSync(firstOperation);
  if(!s.syncing||s.currentSync(firstOperation)||!s.currentSync(nextOperation))throw new AssertionError("Finishing old Activity worker unlocked a newer sync");
  s.finishSync(nextOperation);if(s.syncing)throw new AssertionError("Current operation did not release its lock");
  byte[] original=new byte[65536];for(int i=0;i<original.length;i++)original[i]=(byte)(i*7);
  s.data=original.clone();s.token="11111111111111111111111111111111";s.save();if(!s.active())throw new AssertionError();
  File first=s.archiveCurrent("returning");if(!Arrays.equals(Files.readAllBytes(first.toPath()),original))throw new AssertionError();
  if(!s.archiveCurrent("returning").equals(first))throw new AssertionError("retry duplicates archive");
  s.revoked=true;s.archiveCurrent("replaced");s.data=original.clone();s.data[123]^=2;s.token="22222222222222222222222222222222";s.revoked=false;
  File second=s.archiveCurrent("received");if(first.equals(second)||!Arrays.equals(Files.readAllBytes(first.toPath()),original))throw new AssertionError("old Pokemon overwritten");
  byte[] current=s.data.clone();AtomicFile.fail=true;s.data[124]^=1;
  boolean refused=false;try{s.archiveCurrent("new");}catch(IOException expected){refused=true;}AtomicFile.fail=false;
  if(!refused||!Arrays.equals(Files.readAllBytes(second.toPath()),current))throw new AssertionError("failed archive destroyed previous copy");
  s.data=current;Files.write(second.toPath(),new byte[]{1});refused=false;try{s.archiveCurrent("received");}catch(IOException expected){refused=true;}
  if(!refused||Files.size(second.toPath())!=1)throw new AssertionError("corrupt archive silently overwritten");
  s.returnPending=true;s.mode=0;s.pendingToken="33333333333333333333333333333333";s.save();
  WalkerStore restored=reopen(dir);
  if(!restored.returnPending||restored.mode!=0||!restored.pendingToken.equals(s.pendingToken)||!Arrays.equals(restored.data,current))throw new AssertionError("pending return lost after process restart");
  File journal=new File(dir,"walker-state.bin");byte[] latest=Files.readAllBytes(journal.toPath());
  Files.write(journal.toPath(),Arrays.copyOf(latest,latest.length-31)); // legacy format ended after revoked: no pending flag, empty UTF binding, expanded flag or 2 display settings
  restored=reopen(dir);
  if(restored.returnPending||restored.revoked||!Arrays.equals(restored.data,current)||!restored.active())throw new AssertionError("legacy journal no longer readable");
  restored.returnPending=false;restored.save();
  WalkerStore again=reopen(dir);if(again.returnPending||!again.active())throw new AssertionError("acknowledged return flag not cleared");
  byte[] oldActive=Files.readAllBytes(journal.toPath());
  again.returnPending=true;again.mode=0;again.save();again.archiveCurrent("returning");again.completeReturn(again.token,again.data);
  again.data=null;again.token="";again.returnPending=false;AtomicFile.silentDrop=true;refused=false;
  try{again.save();}catch(IOException expected){refused=true;}AtomicFile.silentDrop=false;
  if(!refused)throw new AssertionError("Silent failed journal publication was not detected");
  if(reopen(dir).active())throw new AssertionError("Returned walker reactivated after failed clearing write");
  Files.write(journal.toPath(),oldActive);
  if(reopen(dir).active())throw new AssertionError("Returned walker reactivated from pre-return journal");
  again=reopen(dir);again.data=current.clone();again.token="44444444444444444444444444444444";again.returnCompleted=false;again.revoked=false;again.returnPending=false;again.save();
  if(!reopen(dir).active())throw new AssertionError("Retired old session blocked fresh handoff");
  byte[] beforeDelete=Files.readAllBytes(journal.toPath());again=reopen(dir);again.deleteActive();
  Files.write(journal.toPath(),beforeDelete);if(reopen(dir).active())throw new AssertionError("Deleted session reactivated on restart");
  again=reopen(dir);refused=false;try{again.deleteRecovery(journal);}catch(IOException expected){refused=true;}
  if(!refused||!journal.exists())throw new AssertionError("Backup deletion reached active journal");
  again.deleteRecovery(first);if(first.exists())throw new AssertionError("Selected history copy not deleted");
  File retentionDir=Files.createTempDirectory("walker-retention-").toFile();WalkerStore history=reopen(retentionDir);
  for(int device=1;device<=2;device++)for(int n=0;n<14;n++){history.data=new byte[65536];history.data[0xed+16]=(byte)device;history.data[0x933e]=(byte)n;history.token=String.format("%032d",device);history.archiveCurrent("reward");}
  File[] histories=history.recoveryDirectory().listFiles((d,n)->n.endsWith(".walker"));
  if(histories.length!=20)throw new AssertionError("Retention must keep 10 per walker, not 10 globally: "+histories.length);
  System.out.println("Two distinct walkers retain ten snapshots each.");
  System.out.println("Returned/deleted sessions remain retired after restart or old-journal replay; silent writes detected; fresh handoffs allowed; deletion confined to recovery history.");
  System.out.println("Android production store: immutable histories, retry deduplication, changed Pokemon, archive failures, corrupt-copy refusal, pending-return persistence and legacy journal loading passed");
 }
}
