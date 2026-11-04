package io.github.sdtransporter.pokewalker;
import android.content.Context;import android.util.AtomicFile;
import java.io.*;import java.nio.file.*;import java.util.*;
public final class ProfileVisitTransactionTest {
 static void sample(boolean silent,int fault)throws Exception{
  File root=Files.createTempDirectory("profile-pair-fault-").toFile();Context ctx=new Context(root);
  WalkerStore a=StoreTest.reopen(root);ProfileTest.populate(a,"11111111111111111111111111111111","111111111111111111111111111111111111");a.data[65530]=1;a.save();
  WalkerStore b=WalkerStore.createProfile(ctx,"Peer");ProfileTest.populate(b,"22222222222222222222222222222222","222222222222222222222222222222222222");b.data[65530]=2;b.save();
  WalkerProfiles profiles=new WalkerProfiles(root);
  AtomicFile.writes=0;if(silent)AtomicFile.dropAt=fault;else AtomicFile.failAt=fault;
  try{profiles.visit("default",b.profileId);}catch(IOException expected){}
  finally{AtomicFile.failAt=AtomicFile.dropAt=-1;}
  WalkerProfiles reopened=new WalkerProfiles(root);
  if(!reopened.error.isEmpty())throw new AssertionError("Rollforward blocked: "+reopened.error);
  byte[] aa=reopened.store("default").data,bb=reopened.store(b.profileId).data;
  if(aa[65531]!=bb[65531]||(aa[65531]!=0&&aa[65531]!=1))throw new AssertionError("Half-published visit at "+fault);
  if(aa[65530]!=1||bb[65530]!=2)throw new AssertionError("Partner replaced");
  // Battle accounting and pending rewards are part of the same verified journal.
  WalkerStore own=reopened.store("default");own.battleSession="abcdef0123456789abcdef0123456789";own.battlePeer="abcdef12";
  own.battlePaid=true;own.battleDone=true;own.battleWinner=1;own.battleRewarded=true;own.battleRewardChoice=0x8001;own.save();
  own=new WalkerProfiles(root).store("default");
  if(!own.battlePaid||!own.battleDone||!own.battleRewarded||own.battleWinner!=1||own.battleRewardChoice!=0x8001)throw new AssertionError("Battle receipt lost");
 }
 public static void main(String[] args)throws Exception{
  for(int n=1;n<=14;n++){sample(false,n);sample(true,n);}
  System.out.println("Profile visits: 28 interrupted/silently dropped writes recover both profiles together; partner identity and battle reward receipts survive restart.");
 }
}
