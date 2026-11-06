// GPL-3.0-or-later. No retail artwork or firmware is bundled.
import Foundation

enum WalkerCore {
    static func walkable(_ data: Data) -> Bool {
        guard data.count == 65536 else { return false }
        return data.withUnsafeBytes { pw_walkable($0.bindMemory(to: UInt8.self).baseAddress) != 0 }
    }
    /// Same pre-save check as the consoles: repairs a damaged mirrored record
    /// copy and returns nil if any Pokémon summary or record is malformed.
    static func checked(_ data: Data) -> Data? {
        guard data.count == 65536 else { return nil }
        var copy = data
        let ok = copy.withUnsafeMutableBytes { pw_check($0.bindMemory(to: UInt8.self).baseAddress) != 0 }
        return ok ? copy : nil
    }
    static var now: UInt32 { UInt32(clamping: Int64(Date().timeIntervalSince1970) - 946684800) }
    static func info(_ data: Data?) -> [Int64] {
        guard let data, data.count == 65536 else { return Array(repeating: 0, count: 14) }
        var values = [Int64](repeating: 0, count: 14)
        data.withUnsafeBytes { raw in _ = pw_info(raw.bindMemory(to: UInt8.self).baseAddress, &values) }
        return values
    }
    static func item(_ id: Int64) -> String { String(cString: pw_item(UInt32(clamping: id))) }
    static func species(_ id: Int64) -> String { String(cString: pw_species(UInt32(clamping: id))) }
    static func text(_ data: Data, _ kind: Int32) -> String {
        var output = [CChar](repeating: 0, count: 128)
        data.withUnsafeBytes { pw_text($0.bindMemory(to: UInt8.self).baseAddress, kind, &output) }
        return String(cString: output)
    }
    static func crc(_ data: Data) -> String {
        let result = data.withUnsafeBytes { pw_crc($0.bindMemory(to: UInt8.self).baseAddress, UInt32(data.count)) }
        return String(format: "%08x", result)
    }
    static func homeFrame(_ data: Data?,ms: UInt32) -> [UInt8] {
        var pixels=[UInt8](repeating:0,count:6144),state=[Int32](repeating:0,count:40)
        guard let data,data.count==65536 else { return pixels }
        data.withUnsafeBytes { walkerClassicFrame($0.bindMemory(to:UInt8.self).baseAddress,&state,&pixels,ms) }
        return pixels
    }
    static func peerPacket(_ data: Data) throws -> Data {
        guard data.count==65536 else { throw WalkerError.message("Invalid partner data.") }
        var packet=[UInt8](repeating:0,count:1688)
        let ok=data.withUnsafeBytes { walkerPeerExport($0.bindMemory(to:UInt8.self).baseAddress,&packet) }
        guard ok != 0 else { throw WalkerError.message("Partner details are incomplete.") };return Data(packet)
    }
    static func peerData(_ packet: Data) throws -> Data {
        guard packet.count==1688 else { throw WalkerError.message("Invalid Bluetooth partner data.") }
        var bytes=[UInt8](repeating:0,count:65536)
        let ok=packet.withUnsafeBytes { walkerPeerImport($0.bindMemory(to:UInt8.self).baseAddress,1688,&bytes) }
        guard ok != 0 else { throw WalkerError.message("The other phone sent invalid Pokémon details.") };return Data(bytes)
    }
    static func peerKey(_ packet: Data) -> String { crc(packet.subdata(in:12..<116)+packet.subdata(in:246..<262)+packet.subdata(in:140..<146)) }
    static func friendlyStart(_ a: Data,_ b: Data,seed: Int32) -> [Int32] {
        var board=[Int32](repeating:0,count:24)
        a.withUnsafeBytes { aa in b.withUnsafeBytes { bb in walkerFriendlyStart(aa.bindMemory(to:UInt8.self).baseAddress,bb.bindMemory(to:UInt8.self).baseAddress,&board,UInt32(bitPattern:seed)) } };return board
    }
    static func friendlyTurn(_ a: Data,_ b: Data,board: inout [Int32],aChoice: Int,bChoice: Int) -> Bool {
        guard board.count==24,(0...3).contains(aChoice),(0...3).contains(bChoice) else { return false }
        return a.withUnsafeBytes { aa in b.withUnsafeBytes { bb in walkerFriendlyTurn(aa.bindMemory(to:UInt8.self).baseAddress,bb.bindMemory(to:UInt8.self).baseAddress,&board,UInt32(aChoice),UInt32(bChoice)) != 0 } }
    }
    static func friendlyFrame(_ a: Data,_ b: Data,board: [Int32],side: Int,selected: Int,ms: UInt32) -> [UInt8] {
        var pixels=[UInt8](repeating:0,count:6144)
        guard board.count==24,(0...1).contains(side),(0...3).contains(selected) else { return pixels }
        a.withUnsafeBytes { aa in b.withUnsafeBytes { bb in board.withUnsafeBufferPointer { state in walkerFriendlyFrame(aa.bindMemory(to:UInt8.self).baseAddress,bb.bindMemory(to:UInt8.self).baseAddress,state.baseAddress,UInt32(side),UInt32(selected),&pixels,ms) } } };return pixels
    }
    static func mutate<T>(_ data: inout Data, _ action: (UnsafeMutablePointer<UInt8>) -> T) -> T {
        precondition(data.count == 65536)
        return data.withUnsafeMutableBytes { action($0.bindMemory(to: UInt8.self).baseAddress!) }
    }
}

struct SavedConsole: Codable, Identifiable {
    var id = UUID()
    var name: String
    var host: String
}
struct WalkerJournal: Codable {
    var data: Data?
    var token = ""
    var pendingToken = ""
    var trainerKey: String? = nil
    var expandedItems: Bool? = nil
    var classicVolume: Int? = nil
    var classicContrast: Int? = nil
    var battleSession: String? = nil
    var battlePeer: String? = nil
    var battlePaid: Bool? = nil
    var battleDone: Bool? = nil
    var battleRewarded: Bool? = nil
    var battleRewardChoice: Int? = nil
    var battleWinner: Int? = nil
    var host = ""
    var consoles: [SavedConsole] = []
    var revoked = false
    var returnPending = false
    var mode = 2 // 0 paused, 1 foreground timer, 2 Core Motion
    var rate = 10
    var motionAnchor = Date()
    var motionCredited: Int64 = 0
    var syncConsent = false
}
private struct JournalEnvelope: Codable { var payload: Data; var crc: String }
private struct SessionRetirement: Codable { var token: String; var kind: String; var payloadCRC: String; var checksum: String }

final class WalkerDisk {
    let root: URL
    let journal: URL
    init(profileID: String = "default") throws {
        guard profileID == "default" || (profileID.count == 32 && profileID.allSatisfy({ "0123456789abcdef".contains($0) })) else { throw WalkerError.message("Invalid profile identifier.") }
        let base = try FileManager.default.url(for: .applicationSupportDirectory, in: .userDomainMask, appropriateFor: nil, create: true).appendingPathComponent("Walker", isDirectory: true)
        root = profileID == "default" ? base : base.appendingPathComponent("profiles", isDirectory: true).appendingPathComponent(profileID, isDirectory: true)
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        var protectedRoot = root
        var values = URLResourceValues(); values.isExcludedFromBackup = true
        try protectedRoot.setResourceValues(values)
        journal = root.appendingPathComponent("state.json")
    }
    func load() throws -> WalkerJournal {
        guard FileManager.default.fileExists(atPath: journal.path) else { return WalkerJournal() }
        let envelope = try JSONDecoder().decode(JournalEnvelope.self, from: Data(contentsOf: journal))
        guard WalkerCore.crc(envelope.payload) == envelope.crc else { throw WalkerError.message("Saved state failed verification. Existing files were kept for recovery.") }
        var state = try JSONDecoder().decode(WalkerJournal.self, from: envelope.payload)
        guard (0...2).contains(state.mode), (1...86400).contains(state.rate), state.data == nil || WalkerCore.walkable(state.data!) else { throw WalkerError.message("Invalid saved walker. Existing files were kept.") }
        if let key = state.trainerKey, !key.isEmpty, (key.count != 36 || !key.allSatisfy({ "0123456789abcdef".contains($0) })) { throw WalkerError.message("Saved trainer binding is damaged. Existing files were kept.") }
        if let session=state.battleSession,!session.isEmpty,(session.count != 32 || !session.allSatisfy({ "0123456789abcdef".contains($0) })) { throw WalkerError.message("Saved battle session is damaged.") }
        if let choice=state.battleRewardChoice,choice < -1 || choice > 0x8218 { throw WalkerError.message("Saved reward is damaged.") }
        if !state.token.isEmpty, try retired(state.token) {
            if try retiredKind(state.token) == "deleted" { state.trainerKey = nil }
            state.data = nil; state.token = ""; state.pendingToken = ""; state.mode = 0; state.returnPending = false; state.revoked = false
        }
        if state.data == nil, state.token.isEmpty, try latestSessionDeleted() { state.trainerKey = nil }
        return state
    }
    static func encoded(_ state: WalkerJournal) throws -> Data {
        let payload = try JSONEncoder().encode(state)
        let bytes = try JSONEncoder().encode(JournalEnvelope(payload: payload, crc: WalkerCore.crc(payload)))
        return bytes
    }
    func save(_ state: WalkerJournal) throws {
        let bytes = try Self.encoded(state)
        try bytes.write(to: journal, options: [.atomic, .completeFileProtectionUntilFirstUserAuthentication])
        guard try Data(contentsOf: journal) == bytes else { throw WalkerError.message("Saved state could not be verified.") }
    }
    static func recoveryKey(_ data: Data) -> Data {
        let bytes = [UInt8](data); var base = 0xed
        for candidate in [0xed, 0x1ed] {
            let sum = (0..<104).reduce(1) { $0 + Int(bytes[candidate + $1]) }
            if UInt8(truncatingIfNeeded: sum) == bytes[candidate + 104] { base = candidate; break }
        }
        return Data((0..<56).filter { $0 < 4 || (8..<10).contains($0) || $0 >= 12 }.map { bytes[base + $0] })
    }
    @discardableResult func archive(_ state: WalkerJournal, reason: String) throws -> URL? {
        guard let data = state.data else { return nil }
        guard data.count == 65536 else { throw WalkerError.message("Invalid recovery size.") }
        let directory = root.appendingPathComponent("recovery", isDirectory: true)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        let id = state.token.count == 32 && state.token.allSatisfy({ $0.isHexDigit }) ? state.token : "local"
        let url = directory.appendingPathComponent("\(reason)-\(id)-\(WalkerCore.crc(data)).walker")
        if !FileManager.default.fileExists(atPath: url.path) {
            try data.write(to: url, options: [.atomic, .completeFileProtectionUntilFirstUserAuthentication])
        }
        guard try Data(contentsOf: url) == data else { throw WalkerError.message("Recovery copy failed verification. The existing copy was kept.") }
        let copies = try FileManager.default.contentsOfDirectory(at: directory, includingPropertiesForKeys: [.contentModificationDateKey, .isRegularFileKey]).filter { $0.pathExtension == "walker" && ((try? $0.resourceValues(forKeys: [.isRegularFileKey]).isRegularFile) == true) }.sorted {
            let a = (try? $0.resourceValues(forKeys: [.contentModificationDateKey]).contentModificationDate) ?? .distantPast
            let b = (try? $1.resourceValues(forKeys: [.contentModificationDateKey]).contentModificationDate) ?? .distantPast
            return a == b ? $0.lastPathComponent < $1.lastPathComponent : a < b
        }
        let key = Self.recoveryKey(data)
        let sameWalker = copies.filter { copy in guard let candidate = try? Data(contentsOf: copy), candidate.count == 65536 else { return false }; return Self.recoveryKey(candidate) == key }
        var remaining = sameWalker.count
        for copy in sameWalker where remaining > 10 && copy != url { try FileManager.default.removeItem(at: copy); remaining -= 1 }
        return url
    }
    func histories() -> [URL] {
        let dir = root.appendingPathComponent("recovery", isDirectory: true)
        return ((try? FileManager.default.contentsOfDirectory(at: dir, includingPropertiesForKeys: [.contentModificationDateKey])) ?? [])
            .filter { $0.pathExtension == "walker" }.sorted { $0.lastPathComponent < $1.lastPathComponent }
    }
    private func receiptURL(_ token: String) throws -> URL {
        guard token.count == 32, token.allSatisfy({ $0.isHexDigit }) else { throw WalkerError.message("Invalid handoff session.") }
        let dir = root.appendingPathComponent("sessions", isDirectory: true)
        try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        return dir.appendingPathComponent(token + ".retired")
    }
    private func latestSessionDeleted() throws -> Bool {
        let directory = root.appendingPathComponent("sessions", isDirectory: true)
        guard FileManager.default.fileExists(atPath: directory.path) else { return false }
        let files = try FileManager.default.contentsOfDirectory(at: directory, includingPropertiesForKeys: [.contentModificationDateKey]).filter { $0.pathExtension == "retired" }
        let dated = try files.map { ($0, try $0.resourceValues(forKeys: [.contentModificationDateKey]).contentModificationDate ?? .distantPast) }
        guard let newest = dated.map({ $0.1 }).max() else { return false }
        for (url, date) in dated where date == newest {
            if try retiredKind(url.deletingPathExtension().lastPathComponent) != "deleted" { return false }
        }
        return true
    }
    func retired(_ token: String) throws -> Bool { try retiredKind(token) != nil }
    private func retiredKind(_ token: String) throws -> String? {
        let url = try receiptURL(token)
        guard FileManager.default.fileExists(atPath: url.path) else { return nil }
        let record = try JSONDecoder().decode(SessionRetirement.self, from: Data(contentsOf: url))
        let key = Data("\(record.token)|\(record.kind)|\(record.payloadCRC)".utf8)
        guard record.token == token, ["returned", "deleted"].contains(record.kind), record.checksum == WalkerCore.crc(key) else { throw WalkerError.message("A retired session record is damaged. It remains blocked.") }
        return record.kind
    }
    func retire(_ state: WalkerJournal, kind: String) throws {
        guard let data = state.data else { return }
        let url = try receiptURL(state.token)
        let crc = WalkerCore.crc(data)
        let record = SessionRetirement(token: state.token, kind: kind, payloadCRC: crc, checksum: WalkerCore.crc(Data("\(state.token)|\(kind)|\(crc)".utf8)))
        let bytes = try JSONEncoder().encode(record)
        if FileManager.default.fileExists(atPath: url.path) {
            guard try retired(state.token) else { throw WalkerError.message("Could not verify retired session.") }
        } else {
            try bytes.write(to: url, options: [.atomic, .completeFileProtectionUntilFirstUserAuthentication])
            guard try Data(contentsOf: url) == bytes else { throw WalkerError.message("Could not preserve retired session.") }
        }
    }
    func deleteHistory(_ url: URL) throws {
        let directory = root.appendingPathComponent("recovery", isDirectory: true).resolvingSymlinksInPath()
        let actual = url.resolvingSymlinksInPath()
        guard actual.deletingLastPathComponent() == directory, actual.pathExtension == "walker" else { throw WalkerError.message("Only recovery snapshots can be deleted here.") }
        try FileManager.default.removeItem(at: url)
    }
}

enum WalkerError: LocalizedError {
    case message(String)
    case rejected(Int, String)
    var errorDescription: String? {
        switch self { case .message(let s): return s; case .rejected(_, let s): return s }
    }
}

struct WalkerProfile: Codable, Identifiable {
    let id: String
    var name: String
}
private struct ProfileCatalogue: Codable { var selected: String; var entries: [WalkerProfile] }
final class WalkerCatalog {
    private let url: URL
    var selected = "default"
    var entries = [WalkerProfile(id: "default", name: "Walker 1")]
    init() throws {
        url = try WalkerDisk().root.appendingPathComponent("profiles.json")
        if FileManager.default.fileExists(atPath: url.path) {
            let record = try JSONDecoder().decode(JournalEnvelope.self, from: Data(contentsOf: url))
            guard WalkerCore.crc(record.payload) == record.crc else { throw WalkerError.message("Profile catalogue failed verification.") }
            let list = try JSONDecoder().decode(ProfileCatalogue.self, from: record.payload)
            let ids = list.entries.map(\.id)
            guard !ids.isEmpty, ids.count <= 128, Set(ids).count == ids.count, ids.contains(list.selected), ids.allSatisfy({ $0 == "default" || ($0.count == 32 && $0.allSatisfy { "0123456789abcdef".contains($0) }) }), list.entries.allSatisfy({ !$0.name.isEmpty && $0.name.count <= 100 }) else { throw WalkerError.message("Invalid profile catalogue. Existing files were kept.") }
            selected = list.selected; entries = list.entries
        }
    }
    func save() throws {
        let payload = try JSONEncoder().encode(ProfileCatalogue(selected: selected, entries: entries))
        let bytes = try JSONEncoder().encode(JournalEnvelope(payload: payload, crc: WalkerCore.crc(payload)))
        try bytes.write(to: url, options: [.atomic, .completeFileProtectionUntilFirstUserAuthentication])
        guard try Data(contentsOf: url) == bytes else { throw WalkerError.message("Profile catalogue was not saved.") }
    }
    func create(_ name: String) throws -> String {
        guard entries.count < 128 else { throw WalkerError.message("Profile limit reached.") }
        let id = UUID().uuidString.replacingOccurrences(of: "-", with: "").lowercased()
        let value = name.trimmingCharacters(in: .whitespacesAndNewlines)
        guard value.count <= 100 else { throw WalkerError.message("Profile name is too long.") }
        let old = selected
        entries.append(WalkerProfile(id: id, name: value.isEmpty ? "Walker \(entries.count + 1)" : value)); selected = id
        do { try save() } catch { entries.removeLast(); selected = old; throw error }; return id
    }
    func select(_ id: String) throws {
        guard entries.contains(where: { $0.id == id }) else { throw WalkerError.message("Unknown profile.") }
        let old = selected; selected = id; do { try save() } catch { selected = old; throw error }
    }
    func rename(_ name: String) throws {
        let value = name.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !value.isEmpty, value.count <= 100, let index = entries.firstIndex(where: { $0.id == selected }) else { throw WalkerError.message("Enter a profile name (1-100 characters).") }
        let old = entries[index].name; entries[index].name = value
        do { try save() } catch { entries[index].name = old; throw error }
    }
}

// A verified write-ahead journal publishes both profile results before success.
// Recovery only accepts the exact before/after journal images; a foreign change
// is preserved and blocks the connection rather than being overwritten.
private struct PairEntry: Codable { var id: String; var before: Data; var after: Data }
private struct PairPlan: Codable { var entries: [PairEntry] }
enum WalkerPairTransaction {
    private static func url(_ root: URL) -> URL { root.appendingPathComponent("profile-connection.json") }
    private static func write(_ bytes: Data, to target: URL) throws {
        try bytes.write(to: target, options: [.atomic, .completeFileProtectionUntilFirstUserAuthentication])
        guard try Data(contentsOf: target) == bytes else { throw WalkerError.message("Connection save could not be verified.") }
    }
    private static func encoded(_ plan: PairPlan) throws -> Data {
        let payload = try JSONEncoder().encode(plan)
        return try JSONEncoder().encode(JournalEnvelope(payload: payload, crc: WalkerCore.crc(payload)))
    }
    static func recover(root: URL, profiles: Set<String>) throws {
        let file = url(root)
        guard FileManager.default.fileExists(atPath: file.path) else { return }
        let bytes = try Data(contentsOf: file)
        guard bytes.count < 900000 else { throw WalkerError.message("Connection journal is too large.") }
        let envelope = try JSONDecoder().decode(JournalEnvelope.self, from: bytes)
        guard WalkerCore.crc(envelope.payload) == envelope.crc else { throw WalkerError.message("Connection journal checksum failed.") }
        let plan = try JSONDecoder().decode(PairPlan.self, from: envelope.payload)
        if plan.entries.isEmpty { return }
        guard plan.entries.count == 2, plan.entries[0].id != plan.entries[1].id else { throw WalkerError.message("Invalid connection targets.") }
        var targets: [(URL, Data)] = []
        for entry in plan.entries {
            guard profiles.contains(entry.id) else { throw WalkerError.message("Connection profile is unavailable.") }
            let disk = try WalkerDisk(profileID: entry.id)
            let current = try Data(contentsOf: disk.journal)
            guard current == entry.before || current == entry.after else { throw WalkerError.message("Profile changed during an interrupted connection. All copies were preserved.") }
            targets.append((disk.journal, entry.after))
        }
        for (target, after) in targets { try write(after, to: target) }
        try write(try encoded(PairPlan(entries: [])), to: file)
    }
    static func commit(root: URL, first: (String, WalkerDisk, WalkerJournal), second: (String, WalkerDisk, WalkerJournal), profiles: Set<String>) throws {
        let plan = try PairPlan(entries: [first,second].map { PairEntry(id: $0.0, before: try Data(contentsOf: $0.1.journal), after: try WalkerDisk.encoded($0.2)) })
        try write(try encoded(plan), to: url(root))
        try recover(root: root, profiles: profiles)
    }
}
