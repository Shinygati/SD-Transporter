package io.github.sdtransporter.pokewalker;
import java.io.*;
import java.util.*;
import java.util.zip.CRC32;
/* PWBLE1: 20-byte-safe GATT fragments. BE u16 message/index/count, u8 length,
 * up to 13 payload bytes. Envelope: PWP1, BE u16 length, BE CRC32, payload.
 * No EEPROM is imported by this transport. */
final class PeerFrames {
    static final int LIMIT=8192;
    private int id=-1,count,next,used,last=-1;private final byte[] buffer=new byte[LIMIT+10];
    static int u16(byte[] p,int n){return (p[n]&255)*256+(p[n+1]&255);}
    static ArrayList<byte[]> encode(int id,byte[] payload)throws IOException{
        if(payload.length<1||payload.length>LIMIT)throw new IOException("Bluetooth message is too large");
        ByteArrayOutputStream bytes=new ByteArrayOutputStream();DataOutputStream out=new DataOutputStream(bytes);
        out.writeInt(0x50575031);out.writeShort(payload.length);CRC32 crc=new CRC32();crc.update(payload);out.writeInt((int)crc.getValue());out.write(payload);byte[] all=bytes.toByteArray();
        int count=(all.length+12)/13;ArrayList<byte[]> packets=new ArrayList<>();
        for(int n=0;n<count;n++){int length=Math.min(13,all.length-n*13);byte[] packet=new byte[length+7];packet[0]=(byte)(id>>8);packet[1]=(byte)id;packet[2]=(byte)(n>>8);packet[3]=(byte)n;packet[4]=(byte)(count>>8);packet[5]=(byte)count;packet[6]=(byte)length;System.arraycopy(all,n*13,packet,7,length);packets.add(packet);}
        return packets;
    }
    byte[] accept(byte[] packet)throws IOException{
        if(packet.length<8||packet.length>20)throw new IOException("Invalid Bluetooth fragment");
        int message=u16(packet,0),index=u16(packet,2),total=u16(packet,4),length=packet[6]&255;
        if(length!=packet.length-7||total<1||total>(LIMIT+22)/13||index>=total||(index+1<total&&length!=13))throw new IOException("Invalid Bluetooth fragment size");
        if(message==last)return null;
        if(next==0){if(index!=0)throw new IOException("Bluetooth message starts in the middle");id=message;count=total;used=0;}
        if(message!=id||total!=count)throw new IOException("Bluetooth message changed during transfer");
        if(index<next){int at=index*13;if(at+length>used)throw new IOException("Invalid duplicate Bluetooth fragment");for(int n=0;n<length;n++)if(buffer[at+n]!=packet[7+n])throw new IOException("Bluetooth retry differs");return null;}
        if(index!=next||used+length>buffer.length)throw new IOException("Bluetooth fragment is out of order");
        System.arraycopy(packet,7,buffer,used,length);used+=length;next++;
        if(next<count)return null;
        if(used<11||buffer[0]!='P'||buffer[1]!='W'||buffer[2]!='P'||buffer[3]!='1'||u16(buffer,4)!=used-10)throw new IOException("Incomplete Bluetooth message");
        long expected=0;for(int n=6;n<10;n++)expected=(expected<<8)|(buffer[n]&255);CRC32 crc=new CRC32();crc.update(buffer,10,used-10);
        if(crc.getValue()!=expected)throw new IOException("Bluetooth message checksum failed");
        byte[] result=Arrays.copyOfRange(buffer,10,used);last=id;next=count=used=0;return result;
    }
}
