package io.github.sdtransporter.pokewalker;
import java.io.*;import java.net.*;import java.util.*;import java.util.zip.CRC32;
/** Exercises production return acknowledgement checks without network access. */
public final class SyncAckTest {
 static final String token="11111111111111111111111111111111";
 static String stored;
 static String receipt;
 static byte[] payload=new byte[65536];
 static class Reply extends HttpURLConnection {
  Reply(URL url){super(url);}
  public void disconnect(){} public boolean usingProxy(){return false;} public void connect(){}
  public OutputStream getOutputStream(){return new ByteArrayOutputStream();}
  public int getResponseCode(){return 200;}
  private byte[] body(){return receipt==null?new byte[]{'O','K'}:receipt.getBytes(java.nio.charset.StandardCharsets.US_ASCII);}
  public int getContentLength(){return body().length;}
  public InputStream getInputStream(){return new ByteArrayInputStream(body());}
  public String getHeaderField(String key){return key.equals("X-Walker-Session")?token:key.equals("X-Walker-Stored-CRC32")?stored:null;}
 }
 public static void main(String[] args)throws Exception {
  URL.setURLStreamHandlerFactory(protocol->protocol.equals("http")?new URLStreamHandler(){protected URLConnection openConnection(URL u){return new Reply(u);}}:null);
  for(int n=0;n<payload.length;n++)payload[n]=(byte)(n*31+7);
  for(String invalid:new String[]{null,"00000000"}){
   stored=invalid;boolean refused=false;
   try{WalkerSync.exchange(true,"127.0.0.1",token,payload,message->{});}catch(IOException expected){refused=true;}
   if(!refused)throw new AssertionError("Phone accepted an unconfirmed return");
  }
  CRC32 crc=new CRC32();crc.update(payload);stored=String.format(Locale.ROOT,"%08x",crc.getValue());
  if(WalkerSync.exchange(true,"127.0.0.1",token,payload,message->{}).length!=2)throw new AssertionError();
  String checksum=stored;stored=null;receipt="PWRETURN1 "+token+" "+checksum+"\n";
  WalkerSync.exchange(true,"127.0.0.1",token,payload,message->{});
  receipt="PWRETURN1 22222222222222222222222222222222 "+checksum+"\n";
  boolean refused=false;try{WalkerSync.exchange(true,"127.0.0.1",token,payload,message->{});}catch(IOException expected){refused=true;}
  if(!refused)throw new AssertionError("Wrong-session body receipt accepted");
  System.out.println("Matching structured saved-file receipt works without custom headers; wrong-session receipts refused.");
  System.out.println("Android production transport: missing/wrong stored checksum refuses success; matching persisted-return checksum accepted.");
 }
}
