import SwiftUI
import AVFoundation

@main struct WalkerApp: App {
    @StateObject private var model = WalkerModel()
    @Environment(\.scenePhase) private var phase
    var body: some Scene {
        WindowGroup {
            WalkerHome().environmentObject(model).preferredColorScheme(.dark)
                .onChange(of: phase) { model.setForeground($0 == .active) }
                .onAppear { model.setForeground(phase == .active) }
        }
    }
}

enum WalkerTheme {
    static let ink = Color(red: 0.91, green: 0.96, blue: 0.94)
    static let mint = Color(red: 0.27, green: 0.87, blue: 0.70)
    static let gold = Color(red: 0.99, green: 0.79, blue: 0.40)
    static let panel = Color(red: 0.09, green: 0.19, blue: 0.23)
    static let background = Color(red: 0.04, green: 0.10, blue: 0.16)
}
private struct WalkerMenuPreference: PreferenceKey {
    static var defaultValue: Int = 0
    static func reduce(value: inout Int, nextValue: () -> Int) { value = nextValue() }
}
struct WalkerMenuLabel: View {
    let title: String
    let icon: Int
    var body: some View { Text(title).preference(key:WalkerMenuPreference.self,value:icon) }
}
struct PixelMenuIcon: View {
    let kind: Int
    private var rows: [UInt16] { var rows=[UInt16](repeating:0,count:16);walkerMenuIcon(UInt32(clamping:kind),&rows);return rows }
    var body: some View {
        Canvas { context,size in
            context.fill(Path(roundedRect:CGRect(origin:.zero,size:size),cornerRadius:6),with:.color(Color(red:0.14,green:0.29,blue:0.27)))
            let rows=rows,unit=min(size.width,size.height)/20,x=(size.width-16*unit)/2,y=(size.height-16*unit)/2
            for py in 0..<16 { for px in 0..<16 where rows[py]&(1<<px) != 0 { context.fill(Path(CGRect(x:x+CGFloat(px)*unit,y:y+CGFloat(py)*unit,width:unit,height:unit)),with:.color(WalkerTheme.mint)) } }
        }.accessibilityHidden(true)
    }
}
private struct WalkerButtonContent<Label: View>: View {
    let label: Label
    let pressed: Bool
    @State private var icon = 0
    var body: some View {
        HStack(spacing:12) { PixelMenuIcon(kind:icon).frame(width:32,height:32);label.frame(maxWidth:.infinity,alignment:.leading) }
            .frame(minHeight:44).padding(10).foregroundStyle(WalkerTheme.ink)
            .background(WalkerTheme.panel.opacity(pressed ? 0.6 : 1)).clipShape(RoundedRectangle(cornerRadius:13))
            .onPreferenceChange(WalkerMenuPreference.self) { icon=$0 }
    }
}
struct WalkerButton: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View { WalkerButtonContent(label:configuration.label,pressed:configuration.isPressed) }
}
extension NavigationLink where Label == WalkerMenuLabel {
    init(_ title: String, icon: Int, @ViewBuilder destination: () -> Destination) { self.init(destination:destination,label:{WalkerMenuLabel(title:title,icon:icon)}) }
}
extension Button where Label == WalkerMenuLabel {
    init(_ title: String, icon: Int, action: @escaping () -> Void) { self.init(action:action,label:{WalkerMenuLabel(title:title,icon:icon)}) }
}
struct WalkerHome: View {
    @EnvironmentObject var model: WalkerModel
    private let clock = Timer.publish(every: 1, on: .main, in: .common).autoconnect()
    @State private var ticks = 0
    @State private var classicPresented = false
    @State private var startClassicOnly = true
    @State private var classicFirstButton: Int32 = -1
    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: 14) {
                    Text("iPhone Pokewalker").font(.largeTitle.bold())
                    WalkerPicture(data: model.state.returnPending || model.state.revoked ? nil : model.state.data).frame(height: 300)
                        .contentShape(Rectangle()).onTapGesture { if model.usable { startClassicOnly=true;classicFirstButton = -1;classicPresented = true } }
                        .accessibilityAddTraits(.isButton).accessibilityLabel("Pokewalker. Open original-style controls")
                    HStack {
                        ForEach(0..<3) { key in
                            Button(key==0 ? "◀":key==1 ? "●":"▶") {
                                startClassicOnly=false;classicFirstButton=Int32(key);classicPresented=true
                            }.disabled(!model.usable)
                        }
                    }
                    if model.state.returnPending {
                        Text("Return awaiting confirmation").font(.title2.bold())
                        Text("Phone copy locked. Retry from the console sync screen.")
                    } else if model.state.data != nil {
                        Text(model.info[1] != 0 ? "\(WalkerCore.species(model.info[6])) · Lv \(model.info[7])" : "No partner · send a Pokémon from HGSS").font(.title2.bold())
                        Text("\(model.info[3]) steps · \(model.info[5]) watts").font(.title3).foregroundStyle(WalkerTheme.mint)
                        Text(model.state.revoked ? "Session revoked" : model.state.returnPending ? "Return pending — activities paused" : model.state.mode == 2 ? "iPhone pedometer" : model.state.mode == 1 ? "Time-based walking" : "Walking paused")
                    } else { Text("Sync with a 3DS or NDS console over local Wi-Fi").font(.title3) }
                    if !model.message.isEmpty { Text(model.message).foregroundStyle(WalkerTheme.gold).textSelection(.enabled) }
                    if model.storageBlocked { Button("Reopen saved state", action: model.retryStorage) }
                    WalkerDetailsView()
                    NavigationLink("Walker profiles · \(model.profileName)", icon:6) { ProfilesView() }.disabled(model.busy)
                    if model.active && !model.state.returnPending {
                        NavigationLink(model.info[11] > 0 ? "Wild visitor · free battle" : "Poké Radar · 10 watts") { RadarView() }.disabled(!model.usable || model.info[1] == 0)
                        NavigationLink("Dowsing · 3 watts", icon:2) { DowsingView() }.disabled(!model.usable || model.info[1] == 0)
                        NavigationLink("Your pack", icon:3) { PackView() }.disabled(!model.usable)
                        NavigationLink("Trainer card", icon:4) { TrainerView() }.disabled(!model.usable)
                    }
                    NavigationLink("Connect another Pokewalker", icon:18) { ConnectionChoiceView() }.disabled(!model.usable)
                    if (model.state.battleRewardChoice ?? -1)>=0 { NavigationLink("Claim battle reward",icon:3) { BattleRewardView() }.disabled(!model.usable) }
                    NavigationLink("Settings", icon:5) { SettingsView() }
                    NavigationLink("Console Wi-Fi sync", icon:8) { SyncView() }
                    if model.busy { ProgressView("Keep both devices connected…") }
                }.padding()
            }.background(WalkerTheme.background).buttonStyle(WalkerButton()).tint(WalkerTheme.mint)
        }
        .sheet(isPresented: $classicPresented) { ClassicWalkerView(initiallyClassic:startClassicOnly,initialButton:classicFirstButton).environmentObject(model) }
        .onReceive(clock) { _ in model.tick(); ticks += 1; if ticks % 30 == 0 { model.refreshMotion() } }
    }
}

struct WalkerPicture: View {
    let data: Data?
    var compact = false
    var battle: [Int32]? = nil
    var action = -1
    var animationStart = Date.distantPast
    var animating = false
    var previousHP: [Int32] = [4,4]
    var body: some View {
        TimelineView(.periodic(from: .now, by: battle == nil ? 0.4 : 0.08)) { timeline in
            Canvas { context, size in
                var context=context
                let frame = Int(timeline.date.timeIntervalSince1970 / 0.4) & 1
                let box = CGRect(x: 0, y: 0, width: size.width, height: size.height)
                context.fill(Path(roundedRect: box, cornerRadius: 24), with: .color(WalkerTheme.panel))
                let center = CGPoint(x: size.width / 2, y: size.height / 2)
                let radius = min(size.width * 0.45, size.height * 0.46)
                let body=CGRect(x:center.x-radius,y:center.y-radius,width:radius*2,height:radius*2)
                context.draw(context.resolve(Image("WalkerShell")),in:body)
                let lcd=CGRect(x:body.minX+body.width*230/1005,y:body.minY+body.height*261/1003,width:body.width*553/1005,height:body.height*383/1003)
                context.clip(to:Path(roundedRect:lcd,cornerRadius:radius*0.035))
                context.fill(Path(lcd),with:.color(WalkerLCDShell.palette(1)[0]))
                guard let data, data.count == 65536 else { return }
                let palette = WalkerLCDShell.palette(1)
                if let battle, battle.count == 7 {
                    let elapsed = max(0, timeline.date.timeIntervalSince(animationStart))
                    let t = min(1, elapsed / (action == 2 ? 1.8 : 1.2))
                    let unit = max(1, floor(min(lcd.width / 96, lcd.height / 64)))
                    let ax = lcd.midX - 48 * unit, ay = lcd.midY - 32 * unit
                    let pose = Int(elapsed / 0.16) & 1
                    var player: CGFloat = 56, wild: CGFloat = 8
                    var showPlayer = true, showWild = true
                    if action < 0 { wild -= CGFloat(max(0, 1 - elapsed / 0.45)) * 40 }
                    else if animating && action == 0 {
                        if t < 0.45 { player -= CGFloat(sin(t / 0.45 * .pi)) * 16 }
                        else { wild += CGFloat(sin((t - 0.45) / 0.55 * .pi)) * 16 }
                        if t > 0.3 && t < 0.48 { showWild = pose == 0 }
                        if t > 0.7 && t < 0.86 { showPlayer = pose == 0 }
                    } else if animating && action == 1 {
                        wild += CGFloat(sin(min(1, t * 2) * .pi)) * 16
                        player += CGFloat(min(8, t * 20))
                        if battle[6] == 7 && t > 0.5 { player = 56 - CGFloat(sin((t - 0.5) * 2 * .pi)) * 16 }
                        if battle[6] == 4 { wild -= CGFloat(t) * 42 }
                    }
                    let throwing = animating && action == 2
                    if throwing && t > 0.28 { showWild = false }
                    func sprite(_ offset: Int, _ x: CGFloat, _ flip: Bool) {
                        for py in 0..<24 { for px in 0..<32 {
                            let at = offset + ((py / 8) * 32 + px) * 2
                            let shift: Int = py & 7
                            let highBit: Int = (Int(data[at]) >> shift) & 1
                            let lowBit: Int = (Int(data[at + 1]) >> shift) & 1
                            let color: Int = highBit * 2 + lowBit
                            if color != 0 { context.fill(Path(CGRect(x: ax + (x + CGFloat(flip ? 31-px:px)) * unit, y: ay + CGFloat(22 + py) * unit, width: unit, height: unit)), with: .color(palette[color])) }
                        } }
                    }
                    if showWild { sprite(0x9a7e + Int(battle[5]) * 384 + pose * 192, wild, (data[0x8f52+Int(battle[5])*16+14]&1)==0) }
                    if showPlayer { sprite(0x91be + pose * 192, player, false) }
                    if throwing {
                        let q = min(1, t / 0.3)
                        var bx = ax + CGFloat(70 - 46 * q) * unit
                        var by = ay + CGFloat(34 - 15 * sin(q * .pi)) * unit
                        if t > 0.3 { bx = ax + CGFloat(24 + (t < 0.8 ? sin(t * 40) * 2 : 0)) * unit; by = ay + 38 * unit }
                        context.fill(Path(ellipseIn: CGRect(x: bx - 5 * unit, y: by - 5 * unit, width: 10 * unit, height: 10 * unit)), with: .color(palette[3]))
                        context.fill(Path(ellipseIn: CGRect(x: bx - 3 * unit, y: by - 3 * unit, width: 6 * unit, height: 6 * unit)), with: .color(palette[0]))
                        context.fill(Path(CGRect(x: bx - 4 * unit, y: by - unit / 2, width: 8 * unit, height: unit)), with: .color(palette[3]))
                        if t > 0.8 && battle[4] != 0 {
                            for n in -1...1 { let sx = bx + CGFloat(n) * 9 * unit, sy = by - CGFloat(n == 0 ? 13 : 8) * unit
                                context.fill(Path(CGRect(x: sx - unit, y: sy - 3 * unit, width: 2 * unit, height: 6 * unit)), with: .color(palette[3]))
                                context.fill(Path(CGRect(x: sx - 3 * unit, y: sy - unit, width: 6 * unit, height: 2 * unit)), with: .color(palette[3]))
                            }
                        }
                    }
                    let playerHP = animating && t < 0.8 ? previousHP[0] : battle[0]
                    let wildHP = animating && t < 0.4 ? previousHP[1] : battle[1]
                    for n in 0..<4 {
                        context.fill(Path(CGRect(x: ax + CGFloat(8 + n * 6) * unit, y: ay + 54 * unit, width: 4 * unit, height: 3 * unit)), with: .color(palette[n < Int(wildHP) ? 3 : 1]))
                        context.fill(Path(CGRect(x: ax + CGFloat(56 + n * 6) * unit, y: ay + 54 * unit, width: 4 * unit, height: 3 * unit)), with: .color(palette[n < Int(playerHP) ? 3 : 1]))
                    }
                    return
                }
                let pixels=WalkerCore.homeFrame(data,ms:UInt32(truncatingIfNeeded:UInt64(timeline.date.timeIntervalSince1970*1000)))
                let scale=min(lcd.width/96,lcd.height/64),x0=lcd.midX-48*scale,y0=lcd.midY-32*scale
                for y in 0..<64 { for x in 0..<96 {
                    context.fill(Path(CGRect(x:(x0+CGFloat(x)*scale).rounded(),y:(y0+CGFloat(y)*scale).rounded(),width:(x0+CGFloat(x+1)*scale).rounded()-(x0+CGFloat(x)*scale).rounded(),height:(y0+CGFloat(y+1)*scale).rounded()-(y0+CGFloat(y)*scale).rounded())),with:.color(palette[Int(pixels[y*96+x]&3)]))
                } }
            }
        }.accessibilityLabel("Original walker display")
    }
}

struct GrassPatch: View {
    let number: Int
    var rustling = false
    var searched = false
    let action: () -> Void
    var body: some View {
        Button(action: action) {
            VStack(spacing: 3) {
                Text(rustling ? "!" : " ").font(.title.bold()).foregroundStyle(WalkerTheme.gold)
                Canvas { context, size in
                    for row in 0..<3 { for column in 0..<5 {
                        let x = CGFloat(column) * size.width / 5 + 4
                        let y = size.height - CGFloat(row) * 12
                        var path = Path(); path.move(to: CGPoint(x: x, y: y))
                        path.addLine(to: CGPoint(x: x + (rustling ? 12 : 8), y: y - 24))
                        path.addLine(to: CGPoint(x: x + 18, y: y)); path.closeSubpath()
                        context.fill(path, with: .color(rustling ? WalkerTheme.gold : WalkerTheme.mint))
                    } }
                }.frame(height: 58)
                Text(searched ? "Searched" : "Patch \(number)").font(.callout)
            }.opacity(searched ? 0.35 : 1)
        }.buttonStyle(WalkerButton()).disabled(searched)
            .accessibilityLabel("Patch \(number)\(rustling ? ", rustling" : "")\(searched ? ", searched" : "")")
    }
}

struct SettingsView: View {
    @AppStorage("menuHoldMilliseconds") private var menuHoldMilliseconds = 1000
    @EnvironmentObject var model: WalkerModel
    @State private var deleteCopy: URL?
    @State private var confirmCopy = false
    @State private var confirmActive = false
    @State private var confirmExpanded = false
    var body: some View {
        Form {
            Section("Controls") {
                Picker("Menu button hold",selection:$menuHoldMilliseconds) {
                    Text("0.25 seconds").tag(250)
                    Text("0.5 seconds").tag(500)
                    Text("1 second").tag(1000)
                    Text("1.5 seconds").tag(1500)
                    Text("2 seconds").tag(2000)
                    Text("3 seconds").tag(3000)
                }
                Text("Hold the centre button for this long to return to the main LCD while browsing. Applies to every profile.")
            }
            Section("Walking") {
                Picker("Step source", selection: Binding(get: { model.state.mode }, set: model.setMode)) {
                    Text("Paused").tag(0); Text("Time-based").tag(1); Text("iPhone pedometer").tag(2)
                }.disabled(model.busy || model.state.returnPending || model.storageBlocked)
                Picker("Seconds per step", selection: Binding(get: { model.state.rate }, set: model.setRate)) {
                    ForEach([1, 2, 5, 10, 30, 60, 300, 3600, 86400], id: \.self) { Text("\($0) seconds").tag($0) }
                }.disabled(model.busy || model.state.returnPending || model.storageBlocked)
                Text("Physical steps are the default. Motion permission is requested when a walker is active. iOS can provide up to seven days of physical step history. Time-based steps count only while this app is active; iOS does not keep a simulated timer running in the background.")
            }
            Section("Dowsing rewards") {
                Toggle("Expanded items", isOn: Binding(get: { model.state.expandedItems == true }, set: { if $0 { confirmExpanded = true } else { model.setExpandedItems(false) } })).disabled(model.busy || model.state.returnPending || model.storageBlocked)
                Text("Adds Master Ball, PP items, TMs, vitamins, Rare Candy, EV-training items, evolution items, held items and Heart Scales. 92% of Dowsing finds still use the course. Off by default. Battle rewards expand only when both phones enable this option.")
            }
            Section("Recovery copies") {
                Text("Exported snapshots preserve earlier walkers. They do not reactivate an old phone session or merge progress.")
                ForEach(model.histories, id: \.self) { url in
                    Text(model.historyTrainer(url)).font(.callout)
                    ShareLink("Export \(url.lastPathComponent)", item: url).font(.callout)
                    Button("Delete this recovery copy", role: .destructive) { deleteCopy = url; confirmCopy = true }.disabled(model.busy)
                }
                if model.histories.isEmpty { Text("No recovery copies yet.") }
                if model.state.data != nil { Button("Delete active walker", role: .destructive) { confirmActive = true }.disabled(model.busy) }
            }
            Section("About") {
                NavigationLink("Privacy policy", icon:14) { PolicyView(name: "privacy") }
                NavigationLink("Credits", icon:15) { PolicyView(name: "credits") }
                NavigationLink("GNU GPL licence", icon:15) { PolicyView(name: "licence") }

            }
        }.navigationTitle("Settings")
            .confirmationDialog("Opt in to the expanded Dowsing item pool?", isPresented: $confirmExpanded, titleVisibility: .visible) {
                Button("Opt in", icon:0) { model.setExpandedItems(true) }
            }
            .confirmationDialog("Permanently delete this recovery snapshot? Active data and session records are kept.", isPresented: $confirmCopy, titleVisibility: .visible) {
                Button("Delete recovery copy", role: .destructive) { if let deleteCopy { model.deleteHistory(deleteCopy) } }
            }
            .confirmationDialog("Delete the active walker from this iPhone? A recovery copy is preserved first. If the console is still locked, use Recover there. Deleting locally does not notify it.", isPresented: $confirmActive, titleVisibility: .visible) {
                Button("Delete active walker", role: .destructive, action: model.deleteActive)
            }
    }
}

struct SyncView: View {
    @EnvironmentObject var model: WalkerModel
    @State private var host = ""
    @State private var name = "My console"
    @State private var replacing = false
    @State private var recoveringReturn = false
    var body: some View {
        Form {
            Section("Console Wi-Fi sync") {
                Text("Open Settings → Phone sync on the console and keep it open on the same local Wi-Fi.")
                TextField("Console IP, e.g. 192.168.1.20", text: $host).keyboardType(.decimalPad).textInputAutocapitalization(.never).autocorrectionDisabled()
                TextField("Console name", text: $name)
                Button("Save this console", icon:8) { model.saveConsole(name, host: host) }
                ForEach(model.state.consoles) { console in Button("\(console.name) · \(console.host)", icon:8) { host = console.host } }
                    .onDelete { offsets in let ids = offsets.map { model.state.consoles[$0].id }; for id in ids { model.removeConsole(id) } }
            }
            Section("Local transfer disclosure") {
                Text("Walker data includes trainer information, Pokémon and step totals. It is sent directly to the console IP you enter over an unencrypted local connection. Use a trusted network. No developer server receives it.")
                if !model.state.syncConsent { Button("I understand — enable local sync", action: model.consent) }
                Text("One device owns a walker at a time. Console recovery restores its earlier snapshot and revokes the old phone session; it cannot recover progress stored only on a lost phone.")
            }
            Section {
                Button("Receive Pokémon from console", icon:8) {
                    if model.state.data != nil { replacing = true } else { Task { await model.sync(returning: false, host: host) } }
                }.disabled(model.active || !model.state.syncConsent)
                Button("Return Pokémon", icon:16) { if model.state.revoked { recoveringReturn = true } else { Task { await model.sync(returning: true, host: host) } } }.disabled((!model.active && !(model.state.revoked && model.state.data != nil)) || !model.state.syncConsent)
                if model.busy { ProgressView("Syncing…") }
                if !model.message.isEmpty { Text(model.message).textSelection(.enabled) }
                if !model.state.pendingToken.isEmpty { Text("An unfinished receive retries the same session. Use the same console.") }
            }
        }.navigationTitle("Wi-Fi sync").disabled(model.busy || model.storageBlocked)
            .confirmationDialog("On the console press Y from the walker list, select the HGSS destination and confirm Receive phone copy. Only a missing target can be restored.", isPresented: $recoveringReturn, titleVisibility: .visible) {
                Button("Return phone copy", icon:16) { Task { await model.sync(returning: true, host: host, recovery: true) } }
            }
            .onAppear { host = model.state.host }
            .confirmationDialog("Keep a recovery copy and receive a new Pokémon? Progress will not be merged.", isPresented: $replacing, titleVisibility: .visible) {
                Button("Preserve and receive", icon:8) { Task { await model.sync(returning: false, host: host) } }
            }
    }
}

struct PackView: View {
    @EnvironmentObject var model: WalkerModel
    var body: some View {
        List {
            Section("Pokémon") { ForEach(0..<3) { n in Text(pokemon(n)) } }
            Section("Friend gifts") { ForEach(0..<10) { n in Text(model.friendItem(n)) } }
            Section("Items") { ForEach(0..<3) { n in let id = model.packID(item: true, slot: n); Text("Slot \(n + 1): \(WalkerCore.item(Int64(id)))") } }
        }.navigationTitle("Your pack")
    }
    private func pokemon(_ n: Int) -> String {
        let id = model.packID(item: false, slot: n)
        return "Slot \(n + 1): \(id > 0 && id < 494 ? WalkerCore.species(Int64(id)) : "Empty")"
    }
}
struct TrainerView: View {
    @EnvironmentObject var model: WalkerModel
    var body: some View {
        List {
            LabeledContent("Trainer", value: model.text(0)); LabeledContent("Route", value: model.text(2))
            LabeledContent("Friendship", value: "\(model.info[8]) / 255")
            LabeledContent("Today", value: "\(model.info[4]) steps")
            LabeledContent("Lifetime", value: "\(model.info[2]) steps")
            Text("Return earned watts to HGSS to contribute toward unlocking routes. The current route's encounter and item tables come from your walker.")
        }.navigationTitle("Trainer card")
    }
}

struct ProfilesView: View {
    @EnvironmentObject var model: WalkerModel
    @State private var name = ""
    @State private var naming = false
    @State private var creating = false
    var body: some View {
        Form {
            Section {
                Text("All active profiles earn steps. Each profile keeps its own Pokémon, progress, trainer pairing and recovery copies.")
                if !model.message.isEmpty { Text(model.message).foregroundStyle(WalkerTheme.gold) }
                ForEach(model.profiles) { profile in
                    Button {
                        model.selectProfile(profile.id)
                    } label: {
                        VStack(alignment: .leading) {
                            Text((profile.id == model.selectedProfileID ? "Selected · " : "") + profile.name)
                            Text(model.profileSummary(profile.id)).font(.caption).foregroundStyle(.secondary)
                        }
                    }.disabled(model.busy)
                }
            }
            Section {
                Button("Add walker profile", icon:6) { name = ""; creating = true; naming = true }.disabled(model.busy)
                Button("Rename selected profile", icon:6) { name = model.profileName; creating = false; naming = true }.disabled(model.busy)
                Text("To remove a Pokémon, select its profile and use Settings > Delete active walker. Recovery snapshots remain separate.")
            }
        }.navigationTitle("Walker profiles")
            .alert(creating ? "Add walker profile" : "Rename profile", isPresented: $naming) {
                TextField("Profile name", text: $name)
                Button("Save", icon:17) { if creating { model.createProfile(name) } else { model.renameProfile(name) } }
                Button("Cancel", role: .cancel) { }
            }
    }
}


@MainActor final class ClassicAudio: ObservableObject {
    private var player: AVAudioPlayer?
    func play(_ data: Data, volume: Int) {
        stop(); guard !data.isEmpty, volume > 0 else { return }
        do { try AVAudioSession.sharedInstance().setCategory(.ambient); player = try AVAudioPlayer(data: data); player?.volume = volume == 1 ? 0.25 : 0.5; player?.play() } catch { player = nil }
    }
    func stop() { player?.stop(); player = nil }
}
struct ProfileVisitView: View {
    @EnvironmentObject var model: WalkerModel
    @State private var target: String?
    var body: some View {
        List {
            Text("The Pokémon play together and each Pokewalker receives a gift. The same two devices can meet once per day. Partners remain in their own profiles.")
            ForEach(model.profiles.filter { $0.id != model.selectedProfileID }) { profile in
                Button(profile.name) { target = profile.id }.disabled(!model.usable)
            }
            if !model.message.isEmpty { Text(model.message) }
        }.navigationTitle("Connect another Pokewalker")
            .confirmationDialog("Connect these two profiles? Both gifts are saved together before success.", isPresented: Binding(get: { target != nil }, set: { if !$0 { target = nil } }), titleVisibility: .visible) {
                Button("Connect", icon:18) { if let target { model.visit(target) }; target = nil }
            }
    }
}
struct ClassicWalkerView: View {
    @AppStorage("menuHoldMilliseconds") private var menuHoldMilliseconds = 1000
    private var menuHoldSeconds: Double { Double([250,500,1000,1500,2000,3000].contains(menuHoldMilliseconds) ? menuHoldMilliseconds:1000)/1000 }
    var initiallyClassic = true
    var initialButton: Int32 = -1
    @State private var classicOnly = true
    @EnvironmentObject var model: WalkerModel
    @Environment(\.dismiss) private var dismiss
    @StateObject private var audio = ClassicAudio()
    @State private var values = [Int32](repeating: 0, count: 40)
    @State private var pixels = [UInt8](repeating: 0, count: 6144)
    @State private var initialized = false
    @State private var showPeers = false
    @State private var showConsole = false
    @State private var profile = ""
    private let clock = Timer.publish(every: 0.1, on: .main, in: .common).autoconnect()
    private var milliseconds: UInt32 { UInt32(truncatingIfNeeded: UInt64(ProcessInfo.processInfo.systemUptime * 1000)) }
    var body: some View {
        NavigationStack {
            VStack(spacing: 12) {
                WalkerLCDShell(pixels:pixels,contrast:Int(values[20]),onPress:{ press(Int32($0)) },onCentreHold:{ press(3) }).frame(height:340)
                Toggle("Classic mode",isOn:$classicOnly).tint(WalkerTheme.mint)
                if !classicOnly {
                    HStack {
                        Button("◀") { press(0) }
                        Button("●") { press(1) }
                            .highPriorityGesture(LongPressGesture(minimumDuration:menuHoldSeconds).exclusively(before:TapGesture()).onEnded { action in
                                switch action { case .first(true): press(3);case .second: press(1);default: break }
                            })
                        Button("▶") { press(2) }
                    }
                    WalkerDetailsView()
                    if !model.message.isEmpty { Text(model.message).font(.callout) }
                    Button("Back to app") { dismiss() }
                }
            }.padding().toolbar(.hidden,for:.navigationBar)
                .sheet(isPresented:$showPeers) { NavigationStack { ConnectionChoiceView() }.environmentObject(model) }
                .sheet(isPresented:$showConsole) { NavigationStack { SyncView() }.environmentObject(model) }
        }
        .onAppear { if !initialized { initialized=true;classicOnly=initiallyClassic;profile=model.selectedProfileID;values[19]=Int32(model.state.classicVolume ?? 2);values[20]=Int32(model.state.classicContrast ?? 1);press(initialButton) } }
        .onReceive(clock) { _ in if !showPeers && !showConsole { press(-1) } }
        .onDisappear { audio.stop() }
    }
    private func press(_ button:Int32) {
        guard initialized, profile==model.selectedProfileID, model.usable else { audio.stop();return }
        model.classicInput(button,values:&values,milliseconds:milliseconds)
        guard model.usable else { audio.stop();return }
        if values[10]>0 { let score=Int(values[10]-1);values[10]=0;audio.play(model.classicSound(score),volume:Int(values[19])) }
        pixels=model.classicFrame(values,milliseconds:milliseconds)
        if values[0]==10 { values[0]=0;showPeers=true;audio.stop() }
        else if values[0]==8 && values[2]==1 { values[2]=0;showConsole=true;audio.stop() }
    }
}
