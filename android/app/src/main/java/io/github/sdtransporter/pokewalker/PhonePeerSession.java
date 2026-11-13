package io.github.sdtransporter.pokewalker;
import android.content.Context;
import android.os.SystemClock;
import android.util.Base64;
import org.json.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.security.SecureRandom;
import java.util.*;
import java.util.zip.CRC32;
/* Nearby visits/battles never import a remote Pokemon into an active profile.
 * Only local gifts, a local entry cost and the local battle receipt are saved. */
final class PhonePeerSession implements AutoCloseable,PeerBluetooth.Listener {
    interface Listener {void changed();void device(String id,String name);void closed();}
    final boolean battle;final WalkerStore owner;private final Listener listener;private final PeerBluetooth radio;
    private final long operation;private final String ownerToken;private final byte[] localPacket,local;
    private final boolean localExpanded;private boolean remoteExpanded;
    private byte[] remote;private String localKey,remoteKey="",session=UUID.randomUUID().toString().replace("-","");
    String phase="connecting",message="Connecting",remoteName="";private boolean host,closed,localReady,remoteReady,started;
    int selected;private int localChoice=-1,remoteChoice=-1;private int[] board;
    PhonePeerSession(Context context,WalkerStore owner,boolean battle,Listener listener)throws IOException{
        this.owner=owner;this.battle=battle;this.listener=listener;
        synchronized(WalkerStore.LOCK){
            if(!owner.active()||owner.returnPending||owner.syncing||!owner.error.isEmpty())throw new IOException("Select an active Pokémon and finish syncing first");
            if(owner.battleRewardChoice>=0)throw new IOException("Claim or leave your previous battle reward first");
            if(owner.mode==1)Native.timer(owner.data,Native.now(),owner.rate);owner.save();
            localPacket=Native.peerExport(owner.data);local=localPacket==null?null:Native.peerImport(localPacket);
            if(local==null)throw new IOException("This Pokémon has incomplete partner or move data");
            localExpanded=owner.expandedItems;localKey=key(localPacket);ownerToken=owner.token;operation=owner.beginSync();
        }
        PeerBluetooth transport;try{transport=new PeerBluetooth(context,this);}catch(IOException|RuntimeException ex){owner.finishSync(operation);throw ex;}radio=transport;
    }
    static String key(byte[] packet){CRC32 crc=new CRC32();crc.update(packet,12,104);crc.update(packet,246,16);crc.update(packet,140,6);return String.format(Locale.ROOT,"%08x",crc.getValue());}
    void host(){radio.host("Pokewalker");}void search(){radio.search();}void connect(String id){radio.connect(id);}
    public void device(String id,String name){if(!closed)listener.device(id,name);}
    public void status(String message){if(!closed){this.message=message;listener.changed();}}
    public void disconnected(){if(!closed){phase="disconnected";close();listener.changed();}}
    private void current()throws IOException{
        if(closed||!owner.currentSync(operation)||!owner.syncing||owner.data==null||owner.revoked||owner.returnCompleted||!owner.token.equals(ownerToken))throw new IOException("The selected Pokémon changed. Reconnect from its current profile.");
        if(!owner.error.isEmpty())throw new IOException(owner.error);
    }
    private JSONObject object(String kind)throws JSONException{return new JSONObject().put("v",1).put("k",kind).put("sid",session).put("purpose",battle?"battle":"visit");}
    private void send(JSONObject message)throws IOException,JSONException{radio.send(message.toString().getBytes(StandardCharsets.UTF_8));}
    public void ready(boolean host){
        if(closed)return;this.host=host;
        try{current();message=host?"Waiting for the other phone":"Exchanging Pokémon details";
            if(!host)send(object("hello").put("data",Base64.encodeToString(localPacket,Base64.NO_WRAP)).put("expanded",localExpanded).put("resume",owner.battlePaid&&!owner.battleDone?owner.battleSession:"").put("peer",owner.battlePeer));
            listener.changed();
        }catch(Exception ex){fail(ex);}
    }
    private void peer(JSONObject input)throws IOException,JSONException{
        byte[] packet=Base64.decode(input.getString("data"),Base64.DEFAULT);remote=Native.peerImport(packet);
        remoteExpanded=Boolean.TRUE.equals(input.opt("expanded"));
        if(remote==null)throw new IOException("The other phone sent invalid Pokémon details");remoteKey=key(packet);
        if(remoteKey.equals(localKey))throw new IOException("Choose two different Pokewalkers");
        long[] info=Native.info(remote);remoteName=Native.species((int)info[Native.SPECIES])+" · Lv "+info[Native.LEVEL]+" · "+Native.text(remote,3);
    }
    public void message(byte[] bytes){
        if(closed)return;
        try{synchronized(WalkerStore.LOCK){
            current();JSONObject input=new JSONObject(new String(bytes,StandardCharsets.UTF_8));
            if(input.getInt("v")!=1||!input.getString("purpose").equals(battle?"battle":"visit"))throw new IOException("Both phones must choose the same activity");
            String kind=input.getString("k"),sid=input.getString("sid");if(!sid.matches("[a-f0-9]{32}"))throw new IOException("Invalid Bluetooth session");
            if(kind.equals("hello")){
                if(!host||!phase.equals("connecting")||remote!=null)throw new IOException("Unexpected connection request");peer(input);session=sid;
                String resume=input.optString("resume",""),peer=input.optString("peer","");
                if(battle&&owner.battlePeer.equals(remoteKey)&&owner.battlePaid&&!owner.battleDone)session=owner.battleSession;
                else if(battle&&owner.battlePeer.equals(remoteKey)&&owner.battlePaid&&resume.equals(owner.battleSession)&&peer.equals(localKey))session=resume;
                boolean completed=battle&&owner.battlePaid&&owner.battleDone&&owner.battleSession.equals(session);
                send(object("offer").put("data",Base64.encodeToString(localPacket,Base64.NO_WRAP)).put("expanded",localExpanded).put("completed",completed).put("winner",completed?winnerKey():""));
                phase=completed?"complete":"offer";message=completed?"Previous battle result restored":battle?("Both players must accept the 30-watt entry cost. "+(localExpanded&&remoteExpanded?"Expanded rewards enabled on both phones.":"Course items only.")):"Ready for a visit";listener.changed();return;
            }
            if(kind.equals("offer")){
                if(host||!phase.equals("connecting")||remote!=null)throw new IOException("Unexpected connection response");peer(input);session=sid;
                if(input.optBoolean("completed",false)){restoreResult(input.getString("winner"));phase="complete";message=resultText();listener.changed();return;}
                phase="offer";message=battle?("Both players must accept the 30-watt entry cost. "+(localExpanded&&remoteExpanded?"Expanded rewards enabled on both phones.":"Course items only.")):"Ready for a visit";listener.changed();return;
            }
            if(!session.equals(sid)||remote==null)throw new IOException("Bluetooth session does not match");
            switch(kind){
            case "ready":remoteReady=true;if(host)startIfReady();else message="Other player is ready";break;
            case "visit":
                if(host||battle||!localReady)throw new IOException("Unexpected visit");visit();send(object("visited"));phase="complete";message=visitResultText();break;
            case "visited":
                if(!host||battle||!started)throw new IOException("Unexpected visit confirmation");phase="complete";message=visitResultText();break;
            case "start":
                if(host||!battle||!localReady||started)throw new IOException("Unexpected battle start");pay();board=Native.friendlyStart(remote,local,input.getInt("seed"));started=true;phase="waiting";message="Starting battle";send(object("started"));break;
            case "started":
                if(!host||!battle||!started||board==null)throw new IOException("Unexpected battle confirmation");send(object("battle").put("board",array(board)));phase="waiting";message="Starting battle";break;
            case "battle":
                if(host||!battle||board==null||!Arrays.equals(board,integers(input.getJSONArray("board"))))throw new IOException("Battle state does not match");phase="battle";message="Select a move";send(object("ack").put("turn",board[10]));break;
            case "move":{
                if(!battle||board==null||board[12]!=0||input.getInt("turn")!=board[10])throw new IOException("Move belongs to another turn");
                int choice=input.getInt("choice");if(choice<0||choice>3||(remoteChoice>=0&&remoteChoice!=choice))throw new IOException("Invalid move");remoteChoice=choice;if(host)resolve();else message="Other player selected a move";break;}
            case "turn":{
                if(host||board==null||localChoice<0||input.getInt("turn")!=board[10])throw new IOException("Unexpected battle turn");
                int ca=input.getInt("a"),cb=input.getInt("b");if(cb!=localChoice||(remoteChoice>=0&&remoteChoice!=ca))throw new IOException("Selected move does not match");
                int[] predicted=board.clone();if(!Native.friendlyTurn(remote,local,predicted,ca,cb)||!Arrays.equals(predicted,integers(input.getJSONArray("board"))))throw new IOException("Battle result failed verification");
                board=predicted;if(board[12]!=0)finish();send(object("ack").put("turn",board[10]));localChoice=remoteChoice=-1;phase=board[12]!=0?"complete":"battle";message=board[12]!=0?resultText():"Select a move";break;}
            case "ack":
                if(!host||board==null||input.getInt("turn")!=board[10])throw new IOException("Battle acknowledgement does not match");localChoice=remoteChoice=-1;phase=board[12]!=0?"complete":"battle";message=board[12]!=0?resultText():"Select a move";break;
            default:throw new IOException("Unsupported Bluetooth message");
            }
            listener.changed();
        }}catch(Exception ex){fail(ex);}
    }
    boolean rewardsDiffer(){return battle&&localExpanded!=remoteExpanded;}
    boolean canAccept(){return !closed&&phase.equals("offer")&&!localReady;}
    void accept(){
        try{synchronized(WalkerStore.LOCK){current();if(!canAccept())return;
            if(battle&&Native.info(owner.data)[Native.WATTS]<30&&!(owner.battlePaid&&!owner.battleDone&&owner.battlePeer.equals(remoteKey)))throw new IOException("Friendly battles need 30 watts");
            localReady=true;send(object("ready"));message="Waiting for the other player";if(host)startIfReady();listener.changed();
        }}catch(Exception ex){fail(ex);}
    }
    private void startIfReady()throws IOException,JSONException{
        if(started||!localReady||!remoteReady)return;started=true;
        if(!battle){visit();phase="waiting";message="Saving both gifts";send(object("visit"));return;}
        pay();int seed=new SecureRandom().nextInt();board=Native.friendlyStart(local,remote,seed);phase="waiting";message="Starting battle";send(object("start").put("seed",seed));
    }
    private void visit()throws IOException{
        byte[] changed=owner.data.clone();
        int[] reward=new int[3];
        int result=Native.visit(changed,remote,Native.now(),reward);
        if(result==0)throw new IOException("These Pokémon cannot visit");if(result==-1)return;
        owner.archiveCurrent("visiting");owner.data=changed;owner.archiveCurrent("visited");owner.save();
        visitReward=reward;
    }
    private void pay()throws IOException{
        if(owner.battlePaid&&!owner.battleDone&&owner.battlePeer.equals(remoteKey)){owner.battleSession=session;owner.save();return;}
        if(owner.battlePaid&&owner.battleDone&&owner.battleSession.equals(session))throw new IOException("This battle is already complete");
        if(owner.battleRewardChoice>=0)throw new IOException("Claim your previous battle reward first");
        if(Native.info(owner.data)[Native.WATTS]<30)throw new IOException("Friendly battles need 30 watts");
        owner.archiveCurrent("battle-entry");byte[] next=owner.data.clone();if(Native.command(next,1,30)!=1)throw new IOException("Could not reserve the entry cost");
        owner.data=next;owner.battleSession=session;owner.battlePeer=remoteKey;owner.battlePaid=true;owner.battleDone=owner.battleRewarded=false;owner.battleWinner=0;owner.battleRewardChoice=-1;owner.save();
    }
    boolean canMove(){return !closed&&phase.equals("battle")&&board!=null&&board[12]==0&&localChoice<0;}
    void select(int direction){selected=(selected+direction+4)%4;listener.changed();}
    void choose(){
        try{synchronized(WalkerStore.LOCK){current();if(!canMove())return;
            int side=host?0:1;boolean available=false;for(int n=0;n<4;n++)if(board[2+side*4+n]>0)available=true;
            if(available&&board[2+side*4+selected]<=0){message="That move has no PP left";listener.changed();return;}
            localChoice=selected;send(object("move").put("turn",board[10]).put("choice",selected));message="Waiting for the other move";if(host)resolve();listener.changed();
        }}catch(Exception ex){fail(ex);}
    }
    private void resolve()throws IOException,JSONException{
        if(localChoice<0||remoteChoice<0)return;int turn=board[10];int[] next=board.clone();
        if(!Native.friendlyTurn(local,remote,next,localChoice,remoteChoice))throw new IOException("The selected move is unavailable");
        board=next;if(board[12]!=0)finish();phase="waiting";message="Resolving turn";send(object("turn").put("turn",turn).put("a",localChoice).put("b",remoteChoice).put("board",array(board)));
    }
    private String winnerKey(){return owner.battleWinner==1?localKey:owner.battleWinner==2?remoteKey:"draw";}
    private void restoreResult(String winner)throws IOException{
        if(!owner.battlePaid||!owner.battlePeer.equals(remoteKey)||!owner.battleSession.equals(session))throw new IOException("No matching paid battle is available to restore");
        complete(winner.equals(localKey)?1:winner.equals(remoteKey)?2:3);
    }
    private void finish()throws IOException{int own=host?1:2;complete(board[12]==3?3:board[12]==own?1:2);}
    private void complete(int result)throws IOException{
        if(!owner.battlePaid||!owner.battleSession.equals(session))throw new IOException("Battle entry was not saved");
        if(owner.battleDone){if(owner.battleWinner!=result)throw new IOException("Saved battle result differs");return;}
        byte[] next=owner.data.clone();int pending=-1;
        if(result==1&&!owner.battleRewarded){int choice=Native.command(next,12,localExpanded&&remoteExpanded?1:0);if(choice<0)throw new IOException("No valid reward data is available");int saved=Native.command(next,10,choice);if(saved==0)throw new IOException("Battle reward could not be stored");pending=saved<0?choice:-1;}
        owner.archiveCurrent("before-battle-result");owner.data=next;owner.battleDone=true;owner.battleWinner=result;owner.battleRewardChoice=pending;owner.battleRewarded=true;owner.archiveCurrent("battle-result");owner.save();
    }
    private String resultText(){return owner.battleWinner==1?owner.battleRewardChoice>=0?"You won. Choose a slot for your item reward.":"You won. Your item reward is saved.":owner.battleWinner==2?"Battle complete. Better luck next time.":"The battle ended in a draw.";}
    int[] visitReward;
    private String visitResultText(){return visitReward==null?"This Pokéwalker already received today's gift.":"Visit complete. Gifts saved.";}
    byte[] frame(){return board==null||remote==null?null:Native.friendlyFrame(host?local:remote,host?remote:local,board,host?0:1,selected,SystemClock.elapsedRealtime());}
    private static JSONArray array(int[] values){JSONArray result=new JSONArray();for(int v:values)result.put(v);return result;}
    private static int[] integers(JSONArray values)throws IOException,JSONException{if(values.length()!=24)throw new IOException("Invalid battle state");int[] out=new int[24];for(int n=0;n<24;n++)out[n]=values.getInt(n);return out;}
    private void fail(Exception ex){if(ex instanceof IOException){synchronized(WalkerStore.LOCK){if(owner.error.isEmpty()&&(owner.battlePaid||started))owner.error="Activity interrupted. Reopen saved state before continuing.";}}message=ex.getMessage()==null?"Bluetooth activity stopped":ex.getMessage();phase="disconnected";close();listener.changed();}
    public void close(){if(closed)return;closed=true;radio.close();synchronized(WalkerStore.LOCK){owner.finishSync(operation);}listener.closed();}
}
