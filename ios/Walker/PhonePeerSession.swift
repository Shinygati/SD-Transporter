import Foundation
import Combine

// The peer image is read-only. Only this profile's entry, gifts and receipts
// can be committed; Bluetooth never transfers ownership of a Pokémon.
@MainActor final class PhonePeerSession: ObservableObject {
    @Published var message = "Connecting"
    @Published var remoteName = ""
    @Published var phones: [NearbyPhone] = []
    @Published var phase = "connecting"
    @Published var selected = 0
    @Published private(set) var visitPlayback: WalkerVisitPlayback?
    let battle: Bool
    private let model: WalkerModel
    private let radio = PeerBluetooth()
    private let packet: Data
    private let local: Data
    private let localKey: String
    private let localExpanded: Bool
    private var remoteExpanded = false
    private var remote: Data?
    private var remoteKey = ""
    private var session = UUID().uuidString.replacingOccurrences(of:"-",with:"").lowercased()
    private var host = false, closed = false, localReady = false, remoteReady = false, started = false
    private var board: [Int32]?
    private var localChoice = -1, remoteChoice = -1
    init(model: WalkerModel, battle: Bool) throws {
        self.model=model;self.battle=battle;localExpanded=model.state.expandedItems==true
        let bytes=try model.beginNearby()
        do { local=try WalkerCore.peerData(bytes) }
        catch { model.endNearby();throw error }
        packet=bytes;localKey=WalkerCore.peerKey(bytes)
        radio.onDevice = { [weak self] phone in self?.phones.append(phone) }
        radio.onStatus = { [weak self] text in self?.message=text }
        radio.onClose = { [weak self] in self?.close() }
        radio.onReady = { [weak self] host in self?.ready(host) }
        radio.onData = { [weak self] data in self?.receive(data) }
    }
    func start(host: Bool) { if host { radio.host() } else { radio.search() } }
    func connect(_ id: UUID) { radio.connect(id);phones=[] }
    private func send(_ kind: String,_ fields: [String:Any] = [:]) throws {
        var value=fields;value["v"]=1;value["k"]=kind;value["sid"]=session;value["purpose"]=battle ? "battle":"visit"
        try radio.send(JSONSerialization.data(withJSONObject:value))
    }
    private func ready(_ host: Bool) {
        guard !closed else { return };self.host=host
        do {
            if !host {
                try send("hello",["data":packet.base64EncodedString(),"expanded":localExpanded,"resume":model.state.battlePaid==true && model.state.battleDone != true ? model.state.battleSession ?? "":"","peer":model.state.battlePeer ?? ""])
            }
            message=host ? "Waiting for the other phone":"Exchanging Pokémon details"
        } catch { fail(error) }
    }
    private func peer(_ value: [String:Any]) throws {
        guard let text=value["data"] as? String,let bytes=Data(base64Encoded:text) else { throw problem("Invalid Pokémon details.") }
        remoteExpanded=value["expanded"] as? Bool==true
        remote=try WalkerCore.peerData(bytes);remoteKey=WalkerCore.peerKey(bytes)
        guard remoteKey != localKey else { throw problem("Choose two different Pokéwalkers.") }
        let info=WalkerCore.info(remote)
        remoteName="\(WalkerCore.species(info[6])) · Lv \(info[7]) · \(WalkerCore.text(remote!,3))"
    }
    private func problem(_ text: String) -> Error { WalkerError.message(text) }
    private func receive(_ data: Data) {
        guard !closed else { return }
        do {
            guard let value=try JSONSerialization.jsonObject(with:data) as? [String:Any],
                  value["v"] as? Int==1,value["purpose"] as? String==(battle ? "battle":"visit"),
                  let kind=value["k"] as? String,let sid=value["sid"] as? String,
                  sid.count==32,sid.allSatisfy({"0123456789abcdef".contains($0)}) else { throw problem("Both phones must choose the same activity.") }
            if kind=="hello" {
                guard host,phase=="connecting",remote==nil else { throw problem("Unexpected connection request.") }
                try peer(value);session=sid
                if battle,model.state.battlePeer==remoteKey,model.state.battlePaid==true {
                    if model.state.battleDone != true { session=model.state.battleSession ?? sid }
                    else if value["resume"] as? String==model.state.battleSession,value["peer"] as? String==localKey { session=model.state.battleSession ?? sid }
                }
                let completed=battle && model.state.battlePaid==true && model.state.battleDone==true && model.state.battleSession==session
                try send("offer",["data":packet.base64EncodedString(),"expanded":localExpanded,"completed":completed,"winner":completed ? winnerKey:""])
                phase=completed ? "complete":"offer";message=completed ? "Previous battle result restored":offerText;return
            }
            if kind=="offer" {
                guard !host,phase=="connecting",remote==nil else { throw problem("Unexpected connection response.") }
                try peer(value);session=sid
                if value["completed"] as? Bool==true {
                    guard model.state.battlePaid==true,model.state.battlePeer==remoteKey,model.state.battleSession==session,let winner=value["winner"] as? String,
                          [localKey,remoteKey,"draw"].contains(winner) else { throw problem("No matching paid battle is available to restore.") }
                    try model.nearbyFinish(session:session,result:winner==localKey ? 1:winner==remoteKey ? 2:3,expandedEligible:localExpanded && remoteExpanded)
                    phase="complete";message=resultText
                } else { phase="offer";message=offerText };return
            }
            guard sid==session,let remote else { throw problem("Bluetooth session does not match.") }
            switch kind {
            case "ready":
                guard phase=="offer" || phase=="waiting" else { throw problem("Unexpected activity confirmation.") }
                remoteReady=true;if host { try startIfReady() } else { message="Other player is ready" }
            case "visit":
                guard !host,!battle,localReady else { throw problem("Unexpected visit.") }
                visitPlayback = try model.nearbyVisit(remote)
                try send("visited");phase="complete";message=visitResultText
            case "visited":
                guard host,!battle,started else { throw problem("Unexpected visit confirmation.") }
                phase="complete";message=visitResultText
            case "start":
                guard !host,battle,localReady,!started,let seed=value["seed"] as? Int,let exact=Int32(exactly:seed) else { throw problem("Unexpected battle start.") }
                try model.nearbyPay(session:session,peer:remoteKey)
                board=WalkerCore.friendlyStart(remote,local,seed:exact);started=true;phase="waiting";message="Starting battle";try send("started")
            case "started":
                guard host,battle,started,let board,phase=="waiting" else { throw problem("Unexpected battle confirmation.") }
                try send("battle",["board":board.map(Int.init)])
            case "battle":
                guard !host,battle,let board,try integers(value["board"])==board else { throw problem("Battle state does not match.") }
                phase="battle";message="Select a move";try send("ack",["turn":Int(board[10])])
            case "move":
                guard battle,let board,board[12]==0,value["turn"] as? Int==Int(board[10]),let choice=value["choice"] as? Int,(0...3).contains(choice),remoteChoice<0 || remoteChoice==choice else { throw problem("Move belongs to another turn.") }
                remoteChoice=choice;if host { try resolve() } else { message="Other player selected a move" }
            case "turn":
                guard !host,let board,localChoice>=0,value["turn"] as? Int==Int(board[10]),let a=value["a"] as? Int,let b=value["b"] as? Int,
                      b==localChoice,remoteChoice<0 || remoteChoice==a else { throw problem("Unexpected battle turn.") }
                var predicted=board
                guard WalkerCore.friendlyTurn(remote,local,board:&predicted,aChoice:a,bChoice:b),try integers(value["board"])==predicted else { throw problem("Battle result failed verification.") }
                self.board=predicted;if predicted[12] != 0 { try finish() };try send("ack",["turn":Int(predicted[10])])
                localChoice = -1;remoteChoice = -1;phase=predicted[12] != 0 ? "complete":"battle";message=predicted[12] != 0 ? resultText:"Select a move"
            case "ack":
                guard host,let board,value["turn"] as? Int==Int(board[10]),phase=="waiting" else { throw problem("Battle acknowledgement does not match.") }
                localChoice = -1;remoteChoice = -1;phase=board[12] != 0 ? "complete":"battle";message=board[12] != 0 ? resultText:"Select a move"
            default: throw problem("Unsupported Bluetooth message.")
            }
        } catch { fail(error) }
    }
    private var offerText: String { battle ? ("Both players must accept the 30-watt entry cost. "+(localExpanded && remoteExpanded ? "Expanded rewards enabled on both phones.":"Course items only.")):"Ready for a visit" }
    private var visitResultText: String { visitPlayback == nil ? "This Pokéwalker already received today's gift.":"Visit complete. Gifts saved." }
    var rewardsDiffer: Bool { battle && localExpanded != remoteExpanded }
    var canAccept: Bool { !closed && phase=="offer" && !localReady }
    func accept() {
        guard canAccept else { return }
        do {
            guard !battle || model.info[5]>=30 || (model.state.battlePaid==true && model.state.battleDone != true && model.state.battlePeer==remoteKey) else { throw problem("Friendly battles need 30 watts.") }
            localReady=true;try send("ready");message="Waiting for the other player";if host { try startIfReady() }
        } catch { fail(error) }
    }
    private func startIfReady() throws {
        guard !started,localReady,remoteReady,let remote else { return };started=true
        if !battle { visitPlayback = try model.nearbyVisit(remote);phase="waiting";message="Saving both gifts";try send("visit");return }
        try model.nearbyPay(session:session,peer:remoteKey)
        let seed=Int32.random(in:Int32.min...Int32.max);board=WalkerCore.friendlyStart(local,remote,seed:seed)
        phase="waiting";message="Starting battle";try send("start",["seed":Int(seed)])
    }
    var canMove: Bool { !closed && phase=="battle" && board?[12]==0 && localChoice<0 }
    func select(_ direction: Int) { selected=(selected+direction+4)%4 }
    func choose() {
        guard canMove,let board else { return }
        do {
            let start=2+(host ? 0:4)
            guard !board[start..<(start+4)].contains(where:{$0>0}) || board[start+selected]>0 else { message="That move has no PP left";return }
            localChoice=selected;try send("move",["turn":Int(board[10]),"choice":selected]);message="Waiting for the other move";if host { try resolve() }
        } catch { fail(error) }
    }
    private func resolve() throws {
        guard localChoice>=0,remoteChoice>=0,let remote,let board else { return }
        var next=board
        guard WalkerCore.friendlyTurn(local,remote,board:&next,aChoice:localChoice,bChoice:remoteChoice) else { throw problem("The selected move is unavailable.") }
        self.board=next;if next[12] != 0 { try finish() };phase="waiting";message="Resolving turn"
        try send("turn",["turn":Int(board[10]),"a":localChoice,"b":remoteChoice,"board":next.map(Int.init)])
    }
    private func finish() throws {
        guard let board else { return }
        try model.nearbyFinish(session:session,result:board[12]==3 ? 3:board[12]==(host ? 1:2) ? 1:2,expandedEligible:localExpanded && remoteExpanded)
    }
    private var winnerKey: String { model.state.battleWinner==1 ? localKey:model.state.battleWinner==2 ? remoteKey:"draw" }
    private var resultText: String {
        model.state.battleWinner==1 ? ((model.state.battleRewardChoice ?? -1)>=0 ? "You won. Choose a slot for your item reward.":"You won. Your item reward is saved."):model.state.battleWinner==2 ? "Battle complete. Better luck next time.":"The battle ended in a draw."
    }
    func frame(ms: UInt32) -> [UInt8]? {
        guard let remote,let board else { return nil }
        return WalkerCore.friendlyFrame(host ? local:remote,host ? remote:local,board:board,side:host ? 0:1,selected:selected,ms:ms)
    }
    private func integers(_ value: Any?) throws -> [Int32] {
        guard let values=value as? [Int],values.count==24 else { throw problem("Invalid battle state.") }
        let result=values.compactMap(Int32.init(exactly:));guard result.count==24 else { throw problem("Invalid battle values.") };return result
    }
    private func fail(_ error: Error) { message=error.localizedDescription;close() }
    func close() { guard !closed else { return };closed=true;phase="disconnected";radio.close();model.endNearby() }
}
