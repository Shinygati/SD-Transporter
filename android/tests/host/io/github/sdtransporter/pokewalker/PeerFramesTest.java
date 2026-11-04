package io.github.sdtransporter.pokewalker;
import java.io.*;import java.util.*;
public final class PeerFramesTest {
 interface Bad {void run()throws Exception;}
 static void refuses(Bad bad)throws Exception{try{bad.run();}catch(IOException expected){return;}throw new AssertionError("Malformed frame accepted");}
 public static void main(String[] args)throws Exception{
  Random random=new Random(150);
  for(int length=1;length<=8192;length+=37){
   byte[] value=new byte[length];random.nextBytes(value);PeerFrames receiver=new PeerFrames();
   ArrayList<byte[]> frames=PeerFrames.encode(65535,value);byte[] result=null;
   for(byte[] frame:frames){result=receiver.accept(frame);if(result==null&&receiver.accept(frame)!=null)throw new AssertionError("Duplicate completed early");}
   if(!Arrays.equals(value,result)||receiver.accept(frames.get(frames.size()-1))!=null)throw new AssertionError("Roundtrip or duplicate changed payload");
  }
  byte[] value=new byte[8192];ArrayList<byte[]> frames=PeerFrames.encode(1,value);
  refuses(()->new PeerFrames().accept(frames.get(1)));
  refuses(()->{PeerFrames f=new PeerFrames();f.accept(frames.get(0));f.accept(frames.get(2));});
  refuses(()->{PeerFrames f=new PeerFrames();f.accept(frames.get(0));byte[] changed=frames.get(0).clone();changed[10]^=1;f.accept(changed);});
  refuses(()->{PeerFrames f=new PeerFrames();for(int i=0;i<frames.size();i++){byte[] changed=frames.get(i).clone();if(i==frames.size()-1)changed[changed.length-1]^=1;f.accept(changed);}});
  refuses(()->PeerFrames.encode(1,new byte[8193]));
  refuses(()->new PeerFrames().accept(new byte[7]));
  // Fixed vector used by the iOS codec cross-check.
  byte[] first=PeerFrames.encode(0x1234,new byte[]{1,2,3}).get(0);
  StringBuilder hex=new StringBuilder();for(byte b:first)hex.append(String.format("%02x",b&255));
  if(!hex.toString().equals("1234000000010d50575031000355bc801d010203"))throw new AssertionError(hex.toString());
  System.out.println("Bluetooth fragments: 222 payload sizes, retries, ordering, bounds, CRC corruption and fixed cross-platform vector passed.");
 }
}
