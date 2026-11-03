package io.github.sdtransporter.pokewalker;

import java.io.*;
import java.net.*;
import java.util.Locale;
import java.util.zip.CRC32;

/* HTTP transport kept independent of Android so reset/retry behavior can be exercised on the host JVM. */
final class WalkerSync {
    interface Progress {void update(String message);}
    static final class Rejected extends IOException {
        final int status;
        Rejected(int status,String message){super(message);this.status=status;}
    }
    static byte[] exchange(boolean returning,String host,String session,byte[] payload,Progress progress)throws IOException {
        IOException last=null;
        for(int attempt=1;attempt<=3;attempt++){
            HttpURLConnection connection=null;
            try {
                progress.update(returning?"Returning walker…":"Receiving walker…");
                String endpoint=host.contains(":")?host:host+":30820";
                connection=(HttpURLConnection)new URL("http://"+endpoint+"/walker").openConnection();
                connection.setConnectTimeout(10000);connection.setReadTimeout(30000);connection.setInstanceFollowRedirects(false);
                connection.setRequestProperty("Connection","close");connection.setRequestProperty("Accept-Encoding","identity");
                connection.setRequestProperty("X-Walker-Session",session);
                if(returning){
                    connection.setRequestMethod("POST");connection.setDoOutput(true);connection.setFixedLengthStreamingMode(payload.length);
                    try(OutputStream out=connection.getOutputStream()){for(int n=0;n<payload.length;n+=4096)out.write(payload,n,Math.min(4096,payload.length-n));}
                }
                int status=connection.getResponseCode(),length=connection.getContentLength();
                if(length<0||length>65536)throw new IOException("Invalid response length");
                if(status==200 && !returning && length!=65536)throw new IOException("Incomplete walker response");
                InputStream stream=status==200?connection.getInputStream():connection.getErrorStream();
                byte[] body=new byte[length];int received=0;
                if(stream!=null){try {
                    InputStream in=stream;
                    /* Stop at Content-Length: do not depend on an extra read during TCP close. */
                    while(received<length){int n=in.read(body,received,Math.min(4096,length-received));if(n<0)throw new EOFException("Transfer interrupted at "+received+" / "+length+" bytes");received+=n;}
                } finally {try{stream.close();}catch(IOException ignored){/* Complete framed data is verified below. */}}
                }
                if(received!=length)throw new EOFException("Missing sync response");
                if(status!=200)throw new Rejected(status,new String(body,java.nio.charset.StandardCharsets.UTF_8));
                if(!session.equals(connection.getHeaderField("X-Walker-Session")))throw new IOException("Sync session does not match");
                if(!returning){
                    CRC32 crc=new CRC32();crc.update(body);String expected=connection.getHeaderField("X-Walker-CRC32");
                    if(expected==null || !expected.equalsIgnoreCase(String.format(Locale.ROOT,"%08x",crc.getValue())))throw new IOException("Walker checksum does not match; update the console app and retry");
                }
                return body;
            } catch(Rejected rejected){throw rejected;}
            catch(IOException ex){last=ex;if(attempt==3)throw ex;}
            finally {if(connection!=null)connection.disconnect();}
            try{Thread.sleep(750);}catch(InterruptedException ex){Thread.currentThread().interrupt();throw new IOException("Sync interrupted",ex);}
        }
        throw last;
    }
}
