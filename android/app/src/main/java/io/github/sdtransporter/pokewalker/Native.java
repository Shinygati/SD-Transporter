package io.github.sdtransporter.pokewalker;

final class Native {
    static { System.loadLibrary("walker"); }
    static final int REGISTERED=0,HAS_POKEMON=1,TOTAL=2,STROLL=3,TODAY=4,WATTS=5,SPECIES=6,LEVEL=7,BOND=8,POKEMON_COUNT=9,ITEM_COUNT=10,VISITOR=11;
    static native long[] info(byte[] ee);
    static native String text(byte[] ee,int kind);
    static native String species(int id);
    static native int resume(byte[] ee,long now,int rate);
    static native int timer(byte[] ee,long now,int rate);
    static native int physical(byte[] ee,long now,long steps);
    static native int command(byte[] ee,int op,int arg);
    static native int[] encounter(byte[] ee);
    static native void battle(byte[] ee,int[] state,int action);
    static long now() {return Math.max(0,System.currentTimeMillis()/1000L-946684800L);}
    private Native() {}
}
