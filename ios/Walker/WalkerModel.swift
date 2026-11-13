import Foundation
import CoreMotion
import SwiftUI

@MainActor final class WalkerModel: ObservableObject {
    @Published var state = WalkerJournal()
    @Published var message = ""
    @Published var busy = false
    @Published var storageBlocked = false
    @Published var foreground = false
    private var peerOwnerToken="",peerOwnerProfile=""
    private var disk: WalkerDisk?
    private var catalog: WalkerCatalog?
    private var profileStates: [String: WalkerJournal] = [:]
    private var profileDisks: [String: WalkerDisk] = [:]
    private var blockedProfiles: Set<String> = []
    @Published var profiles: [WalkerProfile] = []
    @Published var selectedProfileID = "default"
    private let pedometer = CMPedometer()
    private var querying: Set<String> = []
    var profileName: String { profiles.first(where: { $0.id == selectedProfileID })?.name ?? "Walker" }
    var active: Bool { state.data != nil && !state.revoked && !state.token.isEmpty }
    var usable: Bool { active && !state.returnPending && !busy && !storageBlocked }
    var info: [Int64] { WalkerCore.info(state.data) }
    init() {
        do {
            let c = try WalkerCatalog(); catalog = c; profiles = c.entries; selectedProfileID = c.selected
            try WalkerPairTransaction.recover(root: WalkerDisk().root, profiles: Set(c.entries.map(\.id)))
            for profile in profiles {
                do { let d = try WalkerDisk(profileID: profile.id); profileDisks[profile.id] = d; profileStates[profile.id] = try d.load() }
                catch { blockedProfiles.insert(profile.id); message = error.localizedDescription }
            }
            disk = profileDisks[selectedProfileID]; state = profileStates[selectedProfileID] ?? WalkerJournal(); storageBlocked = blockedProfiles.contains(selectedProfileID)
        } catch { storageBlocked = true; message = error.localizedDescription }
    }
    private func canTrack(_ id: String, _ value: WalkerJournal) -> Bool {
        value.data != nil && !value.revoked && !value.token.isEmpty && !value.returnPending && !blockedProfiles.contains(id) && !(busy && id == selectedProfileID)
    }
    private func persistProfile(_ proposed: WalkerJournal, id: String) throws {
        guard let d = profileDisks[id], !blockedProfiles.contains(id) else { throw WalkerError.message("Storage needs attention. Existing files were kept.") }
        var next = proposed
        if let data = next.data {
            /* A data-check refusal is not a storage fault: keep the profile usable. */
            guard let checked = WalkerCore.checked(data) else { throw WalkerError.message("Walker data check failed. Your last saved progress was kept.") }
            next.data = checked
        }
        do { try d.save(next); profileStates[id] = next; if id == selectedProfileID { state = next }; objectWillChange.send() }
        catch { blockedProfiles.insert(id); if id == selectedProfileID { storageBlocked = true }; throw error }
    }
    private func persist(_ next: WalkerJournal) throws { try persistProfile(next, id: selectedProfileID) }
    func retryStorage() {
        do {
            guard let disk else { throw WalkerError.message("Restart after restoring storage access.") }
            try WalkerPairTransaction.recover(root: WalkerDisk().root, profiles: Set(profiles.map(\.id)))
            for profile in profiles {
                let d = try WalkerDisk(profileID: profile.id); profileDisks[profile.id] = d; profileStates[profile.id] = try d.load(); blockedProfiles.remove(profile.id)
            }
            if let saved = profileStates[selectedProfileID] { state = saved } else { state = try disk.load() }
            storageBlocked = false; message = "Saved state reopened."
        } catch { message = error.localizedDescription }
    }
    func fail(_ error: Error) { message = error.localizedDescription }
    func selectProfile(_ id: String) {
        guard !busy, let catalog else { return }; tick()
        do {
            if !storageBlocked { try persist(state) }; try catalog.select(id)
            selectedProfileID = id; disk = profileDisks[id]; state = profileStates[id] ?? WalkerJournal(); storageBlocked = blockedProfiles.contains(id); message = "Selected \(profileName)."; refreshMotion()
        } catch { fail(error) }
    }
    func createProfile(_ name: String) {
        guard !busy, let catalog else { return }; tick()
        do {
            if !storageBlocked { try persist(state) }
            let id = try catalog.create(name); let d = try WalkerDisk(profileID: id); let empty = WalkerJournal(); try d.save(empty)
            profileDisks[id] = d; profileStates[id] = empty; profiles = catalog.entries
            selectedProfileID = id; state = empty; disk = d; storageBlocked = false; message = "Profile created. Receive a Pokémon from your console."
        } catch { profiles = catalog.entries; for profile in profiles where profileDisks[profile.id] == nil { blockedProfiles.insert(profile.id) }; fail(error) }
    }
    func renameProfile(_ name: String) {
        guard !busy, let catalog else { return }; do { try catalog.rename(name); profiles = catalog.entries } catch { fail(error) }
    }
    func profileSummary(_ id: String) -> String {
        guard let value = profileStates[id] else { return "Storage needs attention" }
        guard let data = value.data else { return "No active Pokémon" }
        return WalkerCore.text(data, 3) + (value.returnPending ? " · Return pending" : value.revoked ? " · Revoked" : " · On phone")
    }
    func beginNearby() throws -> Data {
        guard usable,let data=state.data else { throw WalkerError.message("Select an active Pokémon and finish syncing first.") }
        guard (state.battleRewardChoice ?? -1)<0 else { throw WalkerError.message("Claim or leave your previous battle reward first.") }
        tick();try persist(state)
        let packet=try WalkerCore.peerPacket(state.data ?? data)
        peerOwnerToken=state.token;peerOwnerProfile=selectedProfileID;busy=true;return packet
    }
    func endNearby() { peerOwnerToken="";peerOwnerProfile="";busy=false;refreshMotion() }
    private func currentNearby() throws {
        guard busy,!storageBlocked,peerOwnerProfile==selectedProfileID,peerOwnerToken==state.token,state.data != nil,!state.revoked,!state.token.isEmpty else { throw WalkerError.message("The selected Pokémon changed. Reconnect from its current profile.") }
    }
    private func commitNearby(_ next: WalkerJournal,reason: String) throws {
        do { try disk?.archive(next,reason:reason);try persist(next) }
        catch { blockedProfiles.insert(selectedProfileID);storageBlocked=true;throw error }
    }
    func nearbyVisit(_ peer: Data) throws -> WalkerVisitPlayback? {
        try currentNearby()
        var next=state
        var reward = WalkerVisitReward()
        let result = WalkerCore.visit(&next.data!, peer: peer, now: WalkerCore.now, reward: &reward)
        guard result != 0 else { throw WalkerError.message("These Pokémon cannot visit.") }
        if result == -1 { return nil }
        try commitNearby(next,reason:"visited")
        return WalkerVisitPlayback(data: next.data!, reward: reward)
    }
    func nearbyPay(session: String,peer: String) throws {
        try currentNearby()
        var next=state
        if next.battlePaid==true,next.battleDone != true,next.battlePeer==peer { next.battleSession=session;try persist(next);return }
        if next.battlePaid==true,next.battleDone==true,next.battleSession==session { throw WalkerError.message("This battle is already complete.") }
        guard (next.battleRewardChoice ?? -1)<0,WalkerCore.info(next.data)[5]>=30 else { throw WalkerError.message("Friendly battles need 30 watts. Claim any previous reward first.") }
        guard WalkerCore.mutate(&next.data!,{pw_command($0,1,30,WalkerCore.now)}) == 1 else { throw WalkerError.message("Could not reserve the entry cost.") }
        next.battleSession=session;next.battlePeer=peer;next.battlePaid=true;next.battleDone=false;next.battleRewarded=false;next.battleWinner=0;next.battleRewardChoice = -1
        try commitNearby(next,reason:"battle-entry")
    }
    func nearbyFinish(session: String,result: Int,expandedEligible: Bool = false) throws {
        try currentNearby()
        guard state.battlePaid==true,state.battleSession==session else { throw WalkerError.message("Battle entry was not saved.") }
        if state.battleDone==true { guard state.battleWinner==result else { throw WalkerError.message("Saved battle result differs.") };return }
        var next=state
        if result==1,next.battleRewarded != true {
            let expanded: Int32=next.expandedItems==true && expandedEligible ? 1 : 0
            let choice=WalkerCore.mutate(&next.data!){pw_command($0,12,expanded,WalkerCore.now)}
            guard choice>=0 else { throw WalkerError.message("No valid reward data is available.") }
            let saved=WalkerCore.mutate(&next.data!){pw_command($0,10,choice,WalkerCore.now)}
            guard saved != 0 else { throw WalkerError.message("Battle reward could not be stored.") }
            next.battleRewardChoice = saved<0 ? Int(choice) : -1
        }
        next.battleDone=true;next.battleWinner=result;next.battleRewarded=true;try commitNearby(next,reason:"battle-result")
    }
    func claimBattleReward(_ slot: Int?) {
        guard usable,let choice=state.battleRewardChoice,choice>=0 else { return }
        do {
            var next=state
            if let slot {
                guard (0...2).contains(slot),WalkerCore.mutate(&next.data!,{pw_command($0,10,Int32(((slot+1)<<16)|choice),WalkerCore.now)})==1 else { throw WalkerError.message("Reward could not be stored.") }
            }
            next.battleRewardChoice = -1;try disk?.archive(next,reason:"battle-reward");try persist(next);message="Battle reward saved."
        } catch { fail(error) }
    }
    var battleRewardName: String { guard let choice=state.battleRewardChoice,choice>=0 else { return "" };return rewardName(item:true,choice:choice) }
    func classicInput(_ button: Int32, values: inout [Int32], milliseconds: UInt32) {
        guard usable, values.count == 40, state.data != nil else { return }
        do {
            var next = state
            let expanded: Int32 = next.expandedItems == true ? 1 : 0
            WalkerCore.mutate(&next.data!) { _ = walkerClassicInput($0, &values, button, WalkerCore.now, milliseconds, expanded) }
            let controls = next.classicVolume != Int(values[19]) || next.classicContrast != Int(values[20])
            next.classicVolume = Int(values[19]); next.classicContrast = Int(values[20])
            if next.data != state.data || controls {
                if next.data?.subdata(in: 0xce8c..<0xcef0) != state.data?.subdata(in: 0xce8c..<0xcef0) { try disk?.archive(next, reason: "reward") }
                try persist(next)
            }
        } catch { fail(error) }
    }
    func classicFrame(_ values: [Int32], milliseconds: UInt32) -> [UInt8] {
        var pixels = [UInt8](repeating: 0, count: 6144)
        guard let data = state.data, usable, values.count == 40 else { return pixels }
        data.withUnsafeBytes { bytes in values.withUnsafeBufferPointer { input in walkerClassicFrame(bytes.bindMemory(to: UInt8.self).baseAddress, input.baseAddress, &pixels, milliseconds) } }
        return pixels
    }
    func classicSound(_ index: Int) -> Data {
        guard let data = state.data, !storageBlocked, !state.revoked, !state.returnPending else { return Data() }
        var pcm = [Int16](repeating: 0, count: 220500)
        let capacity = UInt32(pcm.count)
        let count = data.withUnsafeBytes { walkerClassicSound($0.bindMemory(to: UInt8.self).baseAddress, UInt32(clamping: index), &pcm, capacity) }
        guard count > 0 else { return Data() }
        var wav = Data()
        func word<T: FixedWidthInteger>(_ value: T) { var little = value.littleEndian; withUnsafeBytes(of: &little) { wav.append(contentsOf: $0) } }
        wav.append(contentsOf: "RIFF".utf8);word(UInt32(count * 2 + 36));wav.append(contentsOf: "WAVEfmt ".utf8);word(UInt32(16));word(UInt16(1));word(UInt16(1));word(UInt32(22050));word(UInt32(44100));word(UInt16(2));word(UInt16(16));wav.append(contentsOf: "data".utf8);word(UInt32(count * 2));pcm.withUnsafeBytes { wav.append(contentsOf: $0.prefix(Int(count) * 2)) }
        return wav
    }
    func visit(_ id: String) -> WalkerVisitPlayback? {
        guard usable, id != selectedProfileID, let other = profileStates[id], canTrackForVisit(other, id), let ownDisk = disk, let peerDisk = profileDisks[id] else { message = "Both Pokemon must be active on this phone and not syncing."; return nil }
        tick()
        do {
            var own = state
            guard let peer = profileStates[id] else { return nil }
            var otherNext = peer
            try ownDisk.save(own); try peerDisk.save(peer)
            try ownDisk.archive(own, reason: "connecting"); try peerDisk.archive(peer, reason: "connecting")
            let now = WalkerCore.now
            var reward = WalkerVisitReward(), peerReward = WalkerVisitReward()
            let a = WalkerCore.visit(&own.data!, peer: peer.data!, now: now, reward: &reward)
            let b = WalkerCore.visit(&otherNext.data!, peer: state.data!, now: now, reward: &peerReward)
            guard a != -1, b != -1 else { message = "These Pokewalkers already met today."; return nil }
            guard a > 0, b > 0 else { message = "Two different registered Pokewalkers with partners are required."; return nil }
            try WalkerPairTransaction.commit(root: WalkerDisk().root, first: (selectedProfileID, ownDisk, own), second: (id, peerDisk, otherNext), profiles: Set(profiles.map(\.id)))
            profileStates[selectedProfileID] = own; state = own; profileStates[id] = otherNext
            try ownDisk.archive(own, reason: "connected"); try peerDisk.archive(otherNext, reason: "connected")
            objectWillChange.send(); message = "The Pokemon played together. Both gifts were saved. Meet again tomorrow."
            return WalkerVisitPlayback(data: own.data!, reward: reward)
        } catch { blockedProfiles.formUnion(profiles.map(\.id)); storageBlocked = true; fail(error); return nil }
    }
    private func canTrackForVisit(_ value: WalkerJournal, _ id: String) -> Bool { value.data != nil && !value.revoked && !value.returnPending && !value.token.isEmpty && !blockedProfiles.contains(id) }
    func friendItem(_ slot: Int) -> String {
        guard var data = state.data else { return "Empty" }
        let id = WalkerCore.mutate(&data) { pw_command($0,11,Int32(slot),WalkerCore.now) }
        return WalkerCore.item(Int64(id))
    }
    func setExpandedItems(_ enabled: Bool) {
        guard !busy, !state.returnPending, !storageBlocked else { return }
        do { var next = state; next.expandedItems = enabled; try persist(next) } catch { fail(error) }
    }
    func setForeground(_ value: Bool) {
        if !value { tick() }; foreground = value
        if value {
            for id in profiles.map(\.id) {
                guard var next = profileStates[id], canTrack(id, next), next.mode == 1, next.data != nil else { continue }
                let rate = UInt32(next.rate); _ = WalkerCore.mutate(&next.data!) { pw_resume($0, WalkerCore.now, rate) }
                do { try persistProfile(next, id: id) } catch { fail(error) }
            }
            refreshMotion()
        }
    }
    func tick() {
        guard foreground else { return }
        for id in profiles.map(\.id) {
            guard var next = profileStates[id], canTrack(id, next), next.mode == 1, next.data != nil else { continue }
            let rate = UInt32(next.rate); _ = WalkerCore.mutate(&next.data!) { pw_timer($0, WalkerCore.now, rate) }
            do { try persistProfile(next, id: id) } catch { fail(error) }
        }
    }
    func setMode(_ mode: Int) {
        guard !busy, !state.returnPending, !storageBlocked else { return }
        if mode == 2 && !CMPedometer.isStepCountingAvailable() { message = "This iPhone does not provide step history."; return }
        tick()
        do {
            var next = state; next.mode = mode; next.motionAnchor = Date(); next.motionCredited = 0; let rate = UInt32(next.rate)
            if next.data != nil { _ = WalkerCore.mutate(&next.data!) { pw_resume($0, WalkerCore.now, rate) } }; try persist(next)
            if mode == 2 { refreshMotion() }
        } catch { fail(error) }
    }
    func setRate(_ rate: Int) {
        guard !busy, !state.returnPending, !storageBlocked, (1...86400).contains(rate) else { return }; tick()
        do { var next = state; next.rate = rate; if next.data != nil { _ = WalkerCore.mutate(&next.data!) { pw_resume($0, WalkerCore.now, UInt32(rate)) } }; try persist(next) } catch { fail(error) }
    }
    func refreshMotion() {
        guard CMPedometer.isStepCountingAvailable() else { return }
        for id in profiles.map(\.id) {
            guard let value = profileStates[id], canTrack(id, value), value.mode == 2, !querying.contains(id) else { continue }
            querying.insert(id); let token = value.token; let oldAnchor = value.motionAnchor; let end = Date()
            let start = max(oldAnchor, end.addingTimeInterval(-7 * 86400 + 60))
            pedometer.queryPedometerData(from: start, to: end) { [weak self] result, error in
                Task { @MainActor in
                    guard let self else { return }; self.querying.remove(id)
                    guard var next = self.profileStates[id], self.canTrack(id, next), next.mode == 2, next.token == token, next.motionAnchor == oldAnchor else { return }
                    if let error { self.fail(error); return }; guard let result else { return }
                    do {
                        let total = result.numberOfSteps.int64Value; let credited = start == oldAnchor ? next.motionCredited : 0
                        guard total >= credited else { throw WalkerError.message("Motion history changed. No steps were subtracted; pause and restart this profile's step source.") }
                        let delta = UInt32(clamping: total - credited)
                        if delta > 0 { _ = WalkerCore.mutate(&next.data!) { pw_steps($0, WalkerCore.now, delta) } }
                        next.motionAnchor = start; next.motionCredited = total; try self.persistProfile(next, id: id)
                    } catch { self.fail(error) }
                }
            }
        }
    }
    @discardableResult func command(_ op: Int32, _ arg: Int32 = 0) -> Int {
        guard usable else { message = storageBlocked ? "Storage needs attention. Open Settings and Recovery copies." : "Finish syncing before using activities."; return -3 }
        do {
            var next = state
            let parameter: Int32 = op == 2 ? (next.expandedItems == true ? 1 : 0) : arg
            let result = WalkerCore.mutate(&next.data!) { pw_command($0, op, parameter, WalkerCore.now) }
            if (op == 3 || op == 4 || op == 10) && result > 0 { try disk?.archive(next, reason: "reward") }
            try persist(next); return Int(result)
        } catch { fail(error); return -3 }
    }
    func encounter() -> [Int32]? {
        guard usable else { return nil }
        do {
            var next = state; var values = [Int32](repeating: 0, count: 3)
            WalkerCore.mutate(&next.data!) { pw_encounter($0, &values) }
            try persist(next); return values[0] < 0 ? nil : values
        } catch { fail(error); return nil }
    }
    func battle(_ values: [Int32], action: Int32) -> [Int32] {
        guard usable else { return values }
        do {
            var next = state; var output = values
            WalkerCore.mutate(&next.data!) { pw_battle($0, &output, action) }
            try persist(next); return output
        } catch { fail(error); return values }
    }
    func historyTrainer(_ url: URL) -> String {
        guard let data = try? Data(contentsOf: url), data.count == 65536 else { return "Trainer unavailable" }
        return WalkerCore.text(data, 3)
    }
    func rewardName(item: Bool, choice: Int) -> String {
        guard var copy = state.data else { return item ? "Unknown item" : "Unknown Pokémon" }
        let id = WalkerCore.mutate(&copy) { pw_command($0, item ? 8 : 9, Int32(choice), WalkerCore.now) }
        return item ? WalkerCore.item(Int64(id)) : WalkerCore.species(Int64(id))
    }
    func text(_ kind: Int32) -> String { state.data.map { WalkerCore.text($0, kind) } ?? "" }
    func packID(item: Bool, slot: Int) -> Int {
        guard var copy = state.data else { return 0 }
        return Int(WalkerCore.mutate(&copy) { pw_command($0, item ? 7 : 6, Int32(slot), WalkerCore.now) })
    }
    func saveConsole(_ name: String, host: String) {
        guard ConsoleSync.validIP(host), !busy else { message = "Enter a valid console IPv4 address."; return }
        do {
            var next = state
            next.consoles.removeAll { $0.host == host }
            next.consoles.append(SavedConsole(name: name.isEmpty ? host : name, host: host))
            next.host = host; try persist(next)
        } catch { fail(error) }
    }
    func removeConsole(_ id: UUID) {
        guard !busy else { return }
        do { var next = state; next.consoles.removeAll { $0.id == id }; try persist(next) } catch { fail(error) }
    }
    func consent() {
        do { var next = state; next.syncConsent = true; try persist(next) } catch { fail(error) }
    }
    var histories: [URL] { disk?.histories() ?? [] }
    func deleteHistory(_ url: URL) {
        guard !busy else { return }
        do { try disk?.deleteHistory(url); objectWillChange.send(); message = "Recovery copy deleted." } catch { fail(error) }
    }
    func deleteActive() {
        guard !busy, let disk, state.data != nil else { return }
        do {
            try disk.archive(state, reason: "deleted")
            try disk.retire(state, kind: "deleted")
            var next = state; next.data = nil; next.token = ""; next.pendingToken = ""; next.trainerKey = nil; next.mode = 0; next.returnPending = false; next.revoked = false
            try persist(next)
            message = "Local walker deleted. Recovery copy kept. Use Recover on the console if it is still locked."
        } catch { storageBlocked = true; fail(error) }
    }
    func sync(returning: Bool, host: String, recovery: Bool = false) async {
        guard !busy, !storageBlocked, ConsoleSync.validIP(host), state.syncConsent else { message = "Enter the console IP and accept the local-sync disclosure."; return }
        guard returning ? (active || (recovery && state.data != nil && !state.token.isEmpty)) : !active else { message = returning ? "No Pokémon is available to return." : "Return your current Pokémon before receiving another."; return }
        if returning,(state.battleRewardChoice ?? -1)>=0 { message="Claim or leave your battle reward before returning.";return }
        tick()
        busy = true
        defer { busy = false }
        do {
            guard let disk else { return }
            if returning, try disk.retired(state.token) { throw WalkerError.message("This phone session was already returned or deleted.") }
            try disk.archive(state, reason: returning ? "returning" : "replaced")
            var prepared = state; prepared.host = host
            if prepared.trainerKey == nil, let data = prepared.data { prepared.trainerKey = WalkerCore.text(data, 4) }
            if returning { prepared.mode = 0; prepared.returnPending = true }
            else if prepared.pendingToken.isEmpty { prepared.pendingToken = UUID().uuidString.replacingOccurrences(of: "-", with: "").lowercased() }
            try persist(prepared)
            let token = returning ? state.token : state.pendingToken
            message = returning ? "Returning Pokémon…" : "Receiving Pokémon…"
            let received: Data
            do { received = try await ConsoleSync.exchange(host: host, token: token, payload: returning ? state.data : nil, trainerKey: state.trainerKey) }
            catch WalkerError.rejected(let status, let detail) {
                if status == 410 {
                    var rejected = state
                    if returning { rejected.revoked = true; rejected.returnPending = false; rejected.mode = 0 }
                    else { rejected.pendingToken = "" }
                    try persist(rejected)
                }
                throw WalkerError.rejected(status, detail)
            }
            var next = state
            if returning {
                try disk.retire(state, kind: "returned")
                next.data = nil; next.token = ""; next.pendingToken = ""; next.revoked = false; next.returnPending = false
            } else {
                guard try !disk.retired(token) else { throw WalkerError.message("This phone session was already retired. Start a fresh handoff.") }
                next.battleSession=nil;next.battlePeer=nil;next.battlePaid=nil;next.battleDone=nil;next.battleRewarded=nil;next.battleWinner=nil;next.battleRewardChoice=nil
                next.trainerKey = WalkerCore.text(received, 4)
                next.data = received; next.token = token; next.pendingToken = ""; next.revoked = false; next.returnPending = false
                next.mode = 2; next.motionAnchor = Date(); next.motionCredited = 0
                let rate = UInt32(next.rate)
                _ = WalkerCore.mutate(&next.data!) { pw_resume($0, WalkerCore.now, rate) }
                try disk.archive(next, reason: "received")
            }
            try persist(next)
            message = returning ? "Returned. Console unlocked." : "Received. Console copy is locked."
        } catch { message = "Sync failed: \(error.localizedDescription) Retry with the same console. Console recovery revokes this session and restores its earlier snapshot." }
    }
}
