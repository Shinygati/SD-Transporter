import Foundation
import CoreBluetooth
import Combine

// PWBLE1, identical to Android PeerFrames. All GATT values fit the default
// 20-byte payload, so neither phone depends on MTU negotiation.
final class PeerFrames {
    static let limit = 8192
    private var id = -1, count = 0, next = 0, last = -1
    private var bytes = [UInt8]()
    private static func u16(_ p: [UInt8], _ n: Int) -> Int { Int(p[n]) << 8 | Int(p[n+1]) }
    static func encode(_ id: Int, _ payload: Data) throws -> [Data] {
        guard !payload.isEmpty, payload.count <= limit else { throw WalkerError.message("Bluetooth message is too large.") }
        let crc = UInt32(WalkerCore.crc(payload), radix:16)!
        var body: [UInt8] = [80,87,80,49,UInt8(payload.count >> 8),UInt8(payload.count & 255),UInt8(crc >> 24),UInt8((crc >> 16)&255),UInt8((crc >> 8)&255),UInt8(crc&255)]
        body.append(contentsOf:payload)
        let count = (body.count+12)/13
        return (0..<count).map { index in
            let length = min(13,body.count-index*13)
            var packet: [UInt8] = [UInt8((id >> 8)&255),UInt8(id&255),UInt8(index >> 8),UInt8(index&255),UInt8(count >> 8),UInt8(count&255),UInt8(length)]
            packet.append(contentsOf:body[(index*13)..<(index*13+length)]);return Data(packet)
        }
    }
    func accept(_ data: Data) throws -> Data? {
        let p = [UInt8](data)
        guard (8...20).contains(p.count) else { throw WalkerError.message("Invalid Bluetooth fragment.") }
        let message=Self.u16(p,0), index=Self.u16(p,2), total=Self.u16(p,4), length=Int(p[6])
        guard length==p.count-7, total>0, total<=(Self.limit+22)/13, index<total, index+1==total || length==13 else { throw WalkerError.message("Invalid Bluetooth fragment size.") }
        if message==last { return nil }
        if next==0 { guard index==0 else { throw WalkerError.message("Bluetooth message starts in the middle.") };id=message;count=total;bytes=[] }
        guard message==id,total==count else { throw WalkerError.message("Bluetooth message changed during transfer.") }
        if index<next {
            let at=index*13
            guard at+length<=bytes.count, Array(bytes[at..<(at+length)])==Array(p[7...]) else { throw WalkerError.message("Bluetooth retry differs.") }
            return nil
        }
        guard index==next,bytes.count+length<=Self.limit+10 else { throw WalkerError.message("Bluetooth fragment is out of order.") }
        bytes.append(contentsOf:p[7...]);next+=1
        if next<count { return nil }
        guard bytes.count>=11,Array(bytes[0..<4])==[80,87,80,49],Self.u16(bytes,4)==bytes.count-10 else { throw WalkerError.message("Incomplete Bluetooth message.") }
        var expected: UInt32=0;for byte in bytes[6..<10] { expected=(expected<<8)|UInt32(byte) }
        let result=Data(bytes[10...])
        guard UInt32(WalkerCore.crc(result),radix:16)==expected else { throw WalkerError.message("Bluetooth message checksum failed.") }
        last=id;next=0;count=0;bytes=[];return result
    }
}

struct NearbyPhone: Identifiable { let id: UUID; var name: String }
@MainActor final class PeerBluetooth: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate, CBPeripheralManagerDelegate {
    static let service=CBUUID(string:"78bcf034-78d1-4d38-a6d6-f04727081500")
    static let rx=CBUUID(string:"78bcf034-78d1-4d38-a6d6-f04727081501")
    static let tx=CBUUID(string:"78bcf034-78d1-4d38-a6d6-f04727081502")
    var onDevice: ((NearbyPhone)->Void)?
    var onReady: ((Bool)->Void)?
    var onData: ((Data)->Void)?
    var onStatus: ((String)->Void)?
    var onClose: (()->Void)?
    private var central: CBCentralManager?
    private var peripheral: CBPeripheralManager?
    private var peers: [UUID:CBPeripheral]=[:]
    private var client: CBPeripheral?
    private var receiver: CBCharacteristic?
    private var transmitter: CBCharacteristic?
    private var hostReceiver: CBMutableCharacteristic?
    private var hostTransmitter: CBMutableCharacteristic?
    private var subscriber: CBCentral?
    private var queue=[Data](), frames=PeerFrames()
    private var isHost=false, closed=false, ready=false, busy=false
    private var sequence=1
    private var activity=Date(), timer: Timer?
    override init() {
        super.init()
        timer=Timer.scheduledTimer(withTimeInterval:1,repeats:true) { [weak self] _ in
            Task { @MainActor in guard let self, !self.closed else { return };if self.ready && Date().timeIntervalSince(self.activity)>120 { self.fail("Bluetooth connection timed out.") } }
        }
    }
    func search() { isHost=false;central=CBCentralManager(delegate:self,queue:.main,options:[CBCentralManagerOptionShowPowerAlertKey:true]) }
    func host() { isHost=true;peripheral=CBPeripheralManager(delegate:self,queue:.main,options:[CBPeripheralManagerOptionShowPowerAlertKey:true]) }
    func connect(_ id: UUID) {
        guard !closed,let peer=peers[id] else { return }
        central?.stopScan();client=peer;peer.delegate=self;onStatus?("Connecting");central?.connect(peer)
    }
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        guard !closed else { return }
        guard central.state == .poweredOn else { if central.state != .unknown && central.state != .resetting { fail("Turn on Bluetooth and allow Pokewalker access in Settings.") };return }
        onStatus?("Searching for nearby Pokewalkers")
        central.scanForPeripherals(withServices:[Self.service],options:[CBCentralManagerScanOptionAllowDuplicatesKey:false])
    }
    func centralManager(_ central: CBCentralManager, didDiscover peer: CBPeripheral, advertisementData: [String:Any], rssi RSSI: NSNumber) {
        guard !closed,peers[peer.identifier]==nil else { return };peers[peer.identifier]=peer
        let serviceData=advertisementData[CBAdvertisementDataServiceDataKey] as? [CBUUID:Data]
        let name=serviceData?[Self.service].flatMap { String(data:$0,encoding:.utf8) } ?? (advertisementData[CBAdvertisementDataLocalNameKey] as? String) ?? peer.name ?? "Pokewalker"
        onDevice?(NearbyPhone(id:peer.identifier,name:name))
    }
    func centralManager(_ central: CBCentralManager, didConnect peer: CBPeripheral) { guard !closed,peer===client else { return };peer.discoverServices([Self.service]) }
    func centralManager(_ central: CBCentralManager, didFailToConnect peer: CBPeripheral,error: Error?) { if !closed { fail(error?.localizedDescription ?? "Bluetooth connection failed.") } }
    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peer: CBPeripheral,error: Error?) { if !closed,peer===client { fail("Bluetooth disconnected. Your Pokémon remains on this phone.") } }
    func peripheral(_ peer: CBPeripheral,didDiscoverServices error: Error?) {
        guard !closed,peer===client else { return }
        guard error==nil,let service=peer.services?.first(where:{$0.uuid==Self.service}) else { fail("The other phone is not running a compatible Pokewalker.");return }
        peer.discoverCharacteristics([Self.rx,Self.tx],for:service)
    }
    func peripheral(_ peer: CBPeripheral,didDiscoverCharacteristicsFor service: CBService,error: Error?) {
        guard !closed,peer===client else { return }
        receiver=service.characteristics?.first(where:{$0.uuid==Self.rx});transmitter=service.characteristics?.first(where:{$0.uuid==Self.tx})
        guard error==nil,receiver != nil,let transmitter else { fail("Bluetooth characteristics are unavailable.");return }
        peer.setNotifyValue(true,for:transmitter)
    }
    func peripheral(_ peer: CBPeripheral,didUpdateNotificationStateFor characteristic: CBCharacteristic,error: Error?) {
        guard !closed,peer===client,characteristic.uuid==Self.tx else { return }
        guard error==nil,characteristic.isNotifying else { fail("Bluetooth notification setup failed.");return }
        if !ready { ready=true;activity=Date();onReady?(false);pump() }
    }
    func peripheral(_ peer: CBPeripheral,didUpdateValueFor characteristic: CBCharacteristic,error: Error?) {
        guard !closed,peer===client,characteristic.uuid==Self.tx else { return }
        guard error==nil,let data=characteristic.value else { fail("Bluetooth read failed.");return };receive(data)
    }
    func peripheral(_ peer: CBPeripheral,didWriteValueFor characteristic: CBCharacteristic,error: Error?) {
        guard !closed,peer===client else { return }
        guard error==nil else { fail("Bluetooth write failed.");return }
        if busy { if !queue.isEmpty { queue.removeFirst() };busy=false;activity=Date();pump() }
    }
    func peripheralManagerDidUpdateState(_ manager: CBPeripheralManager) {
        guard !closed else { return }
        guard manager.state == .poweredOn else { if manager.state != .unknown && manager.state != .resetting { fail("Turn on Bluetooth and allow Pokewalker access in Settings.") };return }
        let rx=CBMutableCharacteristic(type:Self.rx,properties:[.write],value:nil,permissions:[.writeable])
        let tx=CBMutableCharacteristic(type:Self.tx,properties:[.notify],value:nil,permissions:[])
        hostReceiver=rx;hostTransmitter=tx
        let service=CBMutableService(type:Self.service,primary:true);service.characteristics=[rx,tx];manager.add(service)
    }
    func peripheralManager(_ manager: CBPeripheralManager,didAdd service: CBService,error: Error?) {
        guard !closed else { return };guard error==nil else { fail("Bluetooth service could not be published.");return }
        manager.startAdvertising([CBAdvertisementDataServiceUUIDsKey:[Self.service],CBAdvertisementDataLocalNameKey:"Pokewalker"]);onStatus?("Ready for a nearby phone")
    }
    func peripheralManagerDidStartAdvertising(_ peripheral: CBPeripheralManager,error: Error?) { if !closed,let error { fail(error.localizedDescription) } }
    func peripheralManager(_ manager: CBPeripheralManager,central: CBCentral,didSubscribeTo characteristic: CBCharacteristic) {
        guard !closed,characteristic.uuid==Self.tx,subscriber==nil || subscriber?.identifier==central.identifier else { return }
        subscriber=central;manager.stopAdvertising()
        if !ready { ready=true;activity=Date();onReady?(true);pump() }
    }
    func peripheralManager(_ manager: CBPeripheralManager,central: CBCentral,didUnsubscribeFrom characteristic: CBCharacteristic) { if !closed,subscriber?.identifier==central.identifier { fail("Bluetooth disconnected. Your Pokémon remains on this phone.") } }
    func peripheralManager(_ manager: CBPeripheralManager,didReceiveWrite requests: [CBATTRequest]) {
        for request in requests {
            guard !closed,ready,request.central.identifier==subscriber?.identifier,request.characteristic.uuid==Self.rx,request.offset==0,let data=request.value,data.count<=20 else { manager.respond(to:request,withResult:.requestNotSupported);continue }
            manager.respond(to:request,withResult:.success);receive(data)
        }
    }
    func peripheralManagerIsReady(toUpdateSubscribers peripheral: CBPeripheralManager) { if !closed { pump() } }
    func send(_ data: Data) throws {
        guard !closed,ready else { throw WalkerError.message("Bluetooth is not connected.") }
        queue.append(contentsOf:try PeerFrames.encode(sequence,data));sequence=(sequence+1)&65535;pump()
    }
    private func pump() {
        guard !closed,ready,!busy else { return }
        if isHost {
            guard let manager=peripheral,let characteristic=hostTransmitter,let subscriber else { return }
            while let data=queue.first {
                guard manager.updateValue(data,for:characteristic,onSubscribedCentrals:[subscriber]) else { return }
                queue.removeFirst();activity=Date()
            }
        } else if let data=queue.first,let client,let receiver {
            busy=true;activity=Date();client.writeValue(data,for:receiver,type:.withResponse)
        }
    }
    private func receive(_ data: Data) { do { activity=Date();if let message=try frames.accept(data) { onData?(message) } } catch { fail(error.localizedDescription) } }
    private func fail(_ message: String) { guard !closed else { return };onStatus?(message);close();onClose?() }
    func close() {
        guard !closed else { return };closed=true;ready=false;timer?.invalidate();timer=nil
        central?.stopScan();if let client { central?.cancelPeripheralConnection(client) }
        peripheral?.stopAdvertising();peripheral?.removeAllServices();queue=[]
    }
}
