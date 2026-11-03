import SwiftUI

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
struct WalkerButton: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label.frame(maxWidth: .infinity, minHeight: 44).padding(8)
            .foregroundStyle(WalkerTheme.ink).background(WalkerTheme.panel.opacity(configuration.isPressed ? 0.6 : 1))
            .clipShape(RoundedRectangle(cornerRadius: 13))
    }
}
struct WalkerHome: View {
    @EnvironmentObject var model: WalkerModel
    private let clock = Timer.publish(every: 1, on: .main, in: .common).autoconnect()
    @State private var ticks = 0
    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: 14) {
                    Text("iPhone Pokewalker").font(.largeTitle.bold())
                    WalkerPicture(data: model.state.returnPending || model.state.revoked ? nil : model.state.data).frame(height: 300)
                    if model.state.returnPending {
                        Text("Return awaiting confirmation").font(.title2.bold())
                        Text("Phone copy locked. Retry from the console sync screen.")
                    } else if model.state.data != nil {
                        Text(model.info[1] != 0 ? "\(WalkerCore.species(model.info[6])) · Lv \(model.info[7])" : "Walking solo · a wild Pokémon may join").font(.title2.bold())
                        Text("\(model.info[3]) steps · \(model.info[5]) watts").font(.title3).foregroundStyle(WalkerTheme.mint)
                        Text(model.state.revoked ? "Session revoked" : model.state.returnPending ? "Return pending — activities paused" : model.state.mode == 2 ? "iPhone pedometer" : model.state.mode == 1 ? "Time-based walking" : "Walking paused")
                    } else { Text("Sync with a 3DS or NDS console over local Wi-Fi").font(.title3) }
                    if !model.message.isEmpty { Text(model.message).foregroundStyle(WalkerTheme.gold).textSelection(.enabled) }
                    if model.storageBlocked { Button("Reopen saved state", action: model.retryStorage) }
                    NavigationLink("Walker profiles · \(model.profileName)") { ProfilesView() }.disabled(model.busy)
                    if model.active && !model.state.returnPending {
                        NavigationLink(model.info[11] > 0 ? "Wild visitor · free battle" : "Poké Radar · 10 watts") { RadarView() }.disabled(!model.usable || model.info[1] == 0)
                        NavigationLink("Dowsing · 3 watts") { DowsingView() }.disabled(!model.usable || model.info[1] == 0)
                        NavigationLink("Your pack") { PackView() }.disabled(!model.usable)
                        NavigationLink("Trainer card") { TrainerView() }.disabled(!model.usable)
                    }
                    NavigationLink("Settings") { SettingsView() }
                    NavigationLink("Console Wi-Fi sync") { SyncView() }
                    if model.busy { ProgressView("Keep both devices connected…") }
                }.padding()
            }.background(WalkerTheme.background).buttonStyle(WalkerButton()).tint(WalkerTheme.mint)
        }
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
                let frame = Int(timeline.date.timeIntervalSince1970 / 0.4) & 1
                let box = CGRect(x: 0, y: 0, width: size.width, height: size.height)
                context.fill(Path(roundedRect: box, cornerRadius: 24), with: .color(WalkerTheme.panel))
                let center = CGPoint(x: size.width / 2, y: size.height / 2)
                let radius = min(size.width * 0.45, size.height * 0.46)
                context.fill(Path(ellipseIn: CGRect(x: center.x - radius, y: center.y - radius + 4, width: radius * 2, height: radius * 2)), with: .color(WalkerTheme.background))
                context.fill(Path(ellipseIn: CGRect(x: center.x - radius, y: center.y - radius, width: radius * 2, height: radius * 2)), with: .color(Color(red: 0.86, green: 0.91, blue: 0.86)))
                var upper = Path()
                upper.addArc(center: center, radius: radius - 4, startAngle: .degrees(180), endAngle: .degrees(360), clockwise: false)
                upper.closeSubpath()
                context.fill(upper, with: .color(Color(red: 0.80, green: 0.34, blue: 0.34)))
                // Keep the pre-resize LCD/pixel dimensions; only its surrounding shell grows.
                let oldHeight: CGFloat = compact ? 120 : 200
                let scale = max(1, floor(min((size.width - 28) / 64, (oldHeight - 24) / 48)))
                let x0 = (size.width - 64 * scale) / 2, y0 = (size.height - 48 * scale) / 2
                let lcd = CGRect(x: x0, y: y0, width: 64 * scale, height: 48 * scale)
                context.fill(Path(roundedRect: lcd.insetBy(dx: -5, dy: -5), cornerRadius: 9), with: .color(Color(red: 0.17, green: 0.27, blue: 0.22)))
                context.fill(Path(lcd), with: .color(Color(red: 0.78, green: 0.86, blue: 0.66)))
                for n in -1...1 {
                    let buttonRadius = radius * 0.08
                    let button = CGRect(x: center.x + CGFloat(n) * radius * 0.38 - buttonRadius, y: center.y + radius * 0.76 - buttonRadius, width: buttonRadius * 2, height: buttonRadius * 2)
                    context.fill(Path(ellipseIn: button), with: .color(Color(red: 0.34, green: 0.49, blue: 0.44)))
                }
                guard let data, data.count == 65536, WalkerCore.info(data)[1] != 0 else { return }
                let palette = [Color.clear, Color(red: 0.60, green: 0.71, blue: 0.52), Color(red: 0.36, green: 0.49, blue: 0.36), Color(red: 0.17, green: 0.27, blue: 0.22)]
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
                    func sprite(_ offset: Int, _ x: CGFloat) {
                        for py in 0..<24 { for px in 0..<32 {
                            let at = offset + ((py / 8) * 32 + px) * 2
                            let color = Int(((data[at] >> (py & 7)) & 1) * 2 + ((data[at + 1] >> (py & 7)) & 1))
                            if color != 0 { context.fill(Path(CGRect(x: ax + (x + CGFloat(px)) * unit, y: ay + CGFloat(22 + py) * unit, width: unit, height: unit)), with: .color(palette[color])) }
                        } }
                    }
                    if showWild { sprite(0x9a7e + Int(battle[5]) * 384 + pose * 192, wild) }
                    if showPlayer { sprite(0x91be + pose * 192, player) }
                    if throwing {
                        let q = min(1, t / 0.3)
                        var bx = ax + CGFloat(70 - 46 * q) * unit
                        var by = ay + CGFloat(34 - 15 * sin(q * .pi)) * unit
                        if t > 0.3 { bx = ax + CGFloat(24 + (t < 0.8 ? sin(t * 40) * 2 : 0)) * unit; by = ay + 38 * unit }
                        context.fill(Path(ellipseIn: CGRect(x: bx - 5 * unit, y: by - 5 * unit, width: 10 * unit, height: 10 * unit)), with: .color(palette[3]))
                        context.fill(Path(ellipseIn: CGRect(x: bx - 3 * unit, y: by - 3 * unit, width: 6 * unit, height: 6 * unit)), with: .color(Color(red: 0.78, green: 0.86, blue: 0.66)))
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
                for y in 0..<48 { for x in 0..<64 {
                    let at = 0x933e + frame * 768 + ((y / 8) * 64 + x) * 2
                    let c = Int(((data[at] >> (y & 7)) & 1) * 2 + ((data[at + 1] >> (y & 7)) & 1))
                    if c != 0 { context.fill(Path(CGRect(x: x0 + CGFloat(x) * scale, y: y0 + CGFloat(y) * scale, width: scale, height: scale)), with: .color(palette[c])) }
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
    @EnvironmentObject var model: WalkerModel
    @State private var deleteCopy: URL?
    @State private var confirmCopy = false
    @State private var confirmActive = false
    var body: some View {
        Form {
            Section("Walking") {
                Picker("Step source", selection: Binding(get: { model.state.mode }, set: model.setMode)) {
                    Text("Paused").tag(0); Text("Time-based").tag(1); Text("iPhone pedometer").tag(2)
                }.disabled(model.busy || model.state.returnPending || model.storageBlocked)
                Picker("Seconds per step", selection: Binding(get: { model.state.rate }, set: model.setRate)) {
                    ForEach([1, 2, 5, 10, 30, 60, 300, 3600, 86400], id: \.self) { Text("\($0) seconds").tag($0) }
                }.disabled(model.busy || model.state.returnPending || model.storageBlocked)
                Text("Physical steps are the default. Motion permission is requested when a walker is active. iOS can provide up to seven days of physical step history. Time-based steps count only while this app is active; iOS does not keep a simulated timer running in the background.")
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
                NavigationLink("Privacy policy") { PolicyView(name: "privacy") }
                NavigationLink("Credits") { PolicyView(name: "credits") }
                NavigationLink("GNU GPL licence") { PolicyView(name: "licence") }
                Link("Support · Shinygati", destination: URL(string: "mailto:anthoney1313@gmail.com")!)
            }
        }.navigationTitle("Settings")
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
    var body: some View {
        Form {
            Section("Console Wi-Fi sync") {
                Text("Open Settings → Phone sync on the console and keep it open on the same local Wi-Fi.")
                TextField("Console IP, e.g. 192.168.1.20", text: $host).keyboardType(.decimalPad).textInputAutocapitalization(.never).autocorrectionDisabled()
                TextField("Console name", text: $name)
                Button("Save this console") { model.saveConsole(name, host: host) }
                ForEach(model.state.consoles) { console in Button("\(console.name) · \(console.host)") { host = console.host } }
                    .onDelete { offsets in let ids = offsets.map { model.state.consoles[$0].id }; for id in ids { model.removeConsole(id) } }
            }
            Section("Local transfer disclosure") {
                Text("Walker data includes trainer information, Pokémon and step totals. It is sent directly to the console IP you enter over an unencrypted local connection. Use a trusted network. No developer server receives it.")
                if !model.state.syncConsent { Button("I understand — enable local sync", action: model.consent) }
                Text("One device owns a walker at a time. Console recovery restores its earlier snapshot and revokes the old phone session; it cannot recover progress stored only on a lost phone.")
            }
            Section {
                Button("Receive Pokémon from console") {
                    if model.state.data != nil { replacing = true } else { Task { await model.sync(returning: false, host: host) } }
                }.disabled(model.active || !model.state.syncConsent)
                Button("Return Pokémon") { Task { await model.sync(returning: true, host: host) } }.disabled(!model.active || !model.state.syncConsent)
                if model.busy { ProgressView("Syncing…") }
                if !model.message.isEmpty { Text(model.message).textSelection(.enabled) }
                if !model.state.pendingToken.isEmpty { Text("An unfinished receive retries the same session. Use the same console.") }
            }
        }.navigationTitle("Wi-Fi sync").disabled(model.busy || model.storageBlocked)
            .onAppear { host = model.state.host }
            .confirmationDialog("Preserve the old phone copy and receive a fresh walker? Progress will not be merged.", isPresented: $replacing, titleVisibility: .visible) {
                Button("Preserve and receive") { Task { await model.sync(returning: false, host: host) } }
            }
    }
}

struct PackView: View {
    @EnvironmentObject var model: WalkerModel
    var body: some View {
        List {
            Section("Pokémon") { ForEach(0..<3) { n in Text(pokemon(n)) } }
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
                Button("Add walker profile") { name = ""; creating = true; naming = true }.disabled(model.busy)
                Button("Rename selected profile") { name = model.profileName; creating = false; naming = true }.disabled(model.busy)
                Text("To remove a Pokémon, select its profile and use Settings > Delete active walker. Recovery snapshots remain separate.")
            }
        }.navigationTitle("Walker profiles")
            .alert(creating ? "Add walker profile" : "Rename profile", isPresented: $naming) {
                TextField("Profile name", text: $name)
                Button("Save") { if creating { model.createProfile(name) } else { model.renameProfile(name) } }
                Button("Cancel", role: .cancel) { }
            }
    }
}
