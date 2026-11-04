package io.github.sdtransporter.pokewalker;
final class Native {
 static boolean walkable(byte[] data){return data!=null&&data.length==65536;}
 static final int HAS_POKEMON=1;
 static long[] info(byte[] d){return new long[]{1,1};}
 static long now(){return 1000000;}
 static long steps(byte[] d){return java.nio.ByteBuffer.wrap(d).getLong();}
 static int physical(byte[] d,long now,long delta){java.nio.ByteBuffer.wrap(d).putLong(steps(d)+delta);return (int)delta;}
 static int timer(byte[] d,long now,int rate){physical(d,now,1);return 1;}
 static int visit(byte[] own,byte[] peer,long now){if(own[65530]==peer[65530])return 0;own[65531]++;return 1;}
}
