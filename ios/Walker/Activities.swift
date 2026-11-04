import SwiftUI
import WebKit

struct RewardView: View {
    @EnvironmentObject var model: WalkerModel
    let item: Bool
    let choice: Int
    var initialResult: Int? = nil
    @State private var result = -2
    @State private var message = ""
    var body: some View {
        VStack(spacing: 16) {
            Text(message).font(.title2)
            if result == -1 {
                Text("Choose a slot to replace, or keep your current pack.")
                ForEach(0..<3) { n in
                    Button("Replace slot \(n + 1): \(label(n))") { store(n) }.disabled(!model.usable)
                }
            }
            Text("Use Back to leave. A replacement is saved immediately.")
        }.padding().buttonStyle(WalkerButton()).navigationTitle(item ? "Item found" : "Pokémon caught")
            .onAppear { if result == -2 { if let initialResult { result = initialResult; updateMessage() } else { store(-1) } } }
    }
    private func label(_ slot: Int) -> String {
        let id = model.packID(item: item, slot: slot)
        return item ? WalkerCore.item(Int64(id)) : (id > 0 && id < 494 ? WalkerCore.species(Int64(id)) : "Empty")
    }
    private func store(_ slot: Int) {
        result = model.command(item ? 10 : 3, Int32(((slot + 1) << (item ? 16 : 8)) | choice))
        updateMessage()
    }
    private func updateMessage() {
        message = result == -3 ? "Not saved: \(model.message) Recovery copies remain available in Settings." : result < 0 ? "\(model.rewardName(item: item, choice: choice)) · pack full" : result == 1 ? (item ? "You found \(model.rewardName(item: true, choice: choice))!" : "You caught \(model.rewardName(item: false, choice: choice))!") : "No reward was added."
    }
}

struct RadarView: View {
    @EnvironmentObject var model: WalkerModel
    @State private var started = false
    @State private var target = -1
    @State private var round = 0
    @State private var rounds = 0
    @State private var encounter = 0
    @State private var phase = 0 // 0 waiting, 1 rustling, 2 battle, 3 result
    @State private var epoch = 0
    @State private var message = ""
    @State private var deadline = Date()
    @State private var window = 1.0
    @State private var animating = false
    @State private var visualAction = -1
    @State private var animationStart = Date()
    @State private var previousHP: [Int32] = [4,4]
    @State private var savedReward: Int? = nil
    @State private var battle = [Int32](arrayLiteral: 4, 4, 0, 0, 0, 0, 0)
    @State private var reward = false
    var body: some View {
        ScrollView {
            VStack(spacing: 14) {
                WalkerPicture(data: model.state.data, compact: phase < 2, battle: phase >= 2 ? battle : nil, action: visualAction, animationStart: animationStart, animating: animating, previousHP: previousHP).frame(height: 320)
                WalkerDetailsView()
                Text(message).font(.title3)
                if phase < 2 {
                    TimelineView(.periodic(from: .now, by: 0.04)) { timeline in
                        ProgressView(value: phase == 1 ? max(0, min(1, deadline.timeIntervalSince(timeline.date) / window)) : 0)
                    }.tint(WalkerTheme.gold)
                    LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())]) {
                        ForEach(0..<4) { n in GrassPatch(number: n + 1, rustling: target == n) { tap(n) } }
                    }
                    Text("Search \(round + 1) / \(rounds)")
                } else if phase == 2 {
                    Text("Your HP: \(battle[0]) / 4 · Wild HP: \(battle[1]) / 4").font(.headline)
                    Button("Attack") { action(0) }.disabled(animating || !model.usable); Button("Evade") { action(1) }.disabled(animating || !model.usable); Button("Throw Poké Ball") { action(2) }.disabled(animating || !model.usable)
                }
                if !model.usable { Text("Activities paused while syncing.") }
            }.padding().buttonStyle(WalkerButton())
        }.navigationTitle("Poké Radar").background(WalkerTheme.background)
            .task(id: epoch) { await rustle() }
            .onDisappear { epoch += 1 }
            .onAppear { if !started { start() } }
            .navigationDestination(isPresented: $reward) { RewardView(item: false, choice: encounter, initialResult: savedReward) }
    }
    private func random(_ n: Int) -> Int { max(0, model.command(0)) % n }
    private func start() {
        started = true
        guard model.usable else { phase = 3; message = "Receive a Pokémon from your console first."; return }
        let visitor = Int(model.info[11])
        if visitor > 0 { _ = model.command(5); encounter = visitor - 1; startBattle(); return }
        guard model.info[5] >= 10, let choice = model.encounter() else { phase = 3; message = "Radar needs 10 watts and usable route data."; return }
        guard model.command(1, 10) != 0 else { phase = 3; message = "Could not spend watts."; return }
        encounter = Int(choice[0]); rounds = 3 - encounter + random(2); phase = 0; message = "Wait for rustling grass."; epoch += 1
    }
    private func rustle() async {
        guard started, phase == 0, model.usable else { return }
        do {
            try await Task.sleep(nanoseconds: UInt64(500 + random(900)) * 1_000_000)
            try Task.checkCancellation()
            guard model.usable else { phase = 3; message = "Search paused. Return to start again."; return }
            target = random(4); phase = 1
            window = Double([64, 48, 32, 24][min(round, 3)]) / 30
            deadline = Date().addingTimeInterval(window); message = "Tap the rustling patch marked !"
            try await Task.sleep(nanoseconds: UInt64(window * 1_000_000_000))
            try Task.checkCancellation()
            if phase == 1 { phase = 3; message = "It got away. The grass stopped rustling." }
        } catch { /* Navigation or the next round cancels this search. */ }
    }
    private func tap(_ n: Int) {
        guard model.usable, phase == 1 else { return }
        guard Date() <= deadline else { phase = 3; message = "It got away."; return }
        if n != target {
            if round == 0 { phase = 0; target = -1; epoch += 1 }
            else { phase = 3; message = "The Pokémon escaped." }
            return
        }
        round += 1
        if round >= rounds { startBattle() }
        else { phase = 0; target = -1; epoch += 1 }
    }
    private func startBattle() { phase = 2; epoch += 1; battle = [4, 4, 0, 0, 0, Int32(encounter), 0]; visualAction = -1; animationStart = Date(); animating = false; previousHP = [4,4]; message = "Attack, evade or catch." }
    private func action(_ action: Int32) {
        guard model.usable, !animating else { return }
        previousHP = [battle[0], battle[1]]; battle = model.battle(battle, action: action)
        guard !model.storageBlocked else { message = model.message; return }
        if battle[4] != 0 {
            savedReward = model.command(3, Int32(encounter))
            if savedReward == -3 { message = model.message; return }
        }
        visualAction = Int(action); animationStart = Date(); animating = true; epoch += 1
        let current = epoch, profile = model.selectedProfileID
        message = action == 2 ? "The Poké Ball is flying…" : action == 1 ? "Evading…" : "Attacking…"
        Task { @MainActor in
            try? await Task.sleep(nanoseconds: action == 2 ? 1_800_000_000 : 1_200_000_000)
            guard current == epoch, profile == model.selectedProfileID else { return }
            animating = false
            let outcomes = ["You attacked; the wild Pokémon replied.", "It dodged and attacked!", "A critical hit, then a reply!", "A stare-down…", "It fled!", "Caught!", "It escaped from the ball and fled.", "You evaded and counter-attacked!", "Your partner was defeated; up to 10 watts were lost."]
            message = outcomes[min(max(0, Int(battle[6])), outcomes.count - 1)]
            if battle[4] != 0 { phase = 3; reward = true }
            else if battle[3] != 0 { phase = 3 }
        }
    }

}

struct DowsingView: View {
    @EnvironmentObject var model: WalkerModel
    @State private var started = false
    @State private var attempts = 2
    @State private var hidden = 0
    @State private var item = -1
    @State private var empty = -1
    @State private var message = ""
    @State private var reward = false
    var body: some View {
        VStack(spacing: 16) {
            Text(message).font(.title3)
            Text("\(attempts) attempts left")
            LazyVGrid(columns: Array(repeating: GridItem(.flexible()), count: 3)) {
                ForEach(0..<6) { n in GrassPatch(number: n + 1, searched: n == empty) { guess(n) }.disabled(attempts == 0 || item < 0 || !model.usable) }
            }
        }.padding().navigationTitle("Dowsing").background(WalkerTheme.background)
            .onAppear { if !started { start() } }
            .navigationDestination(isPresented: $reward) { RewardView(item: true, choice: item) }
    }
    private func start() {
        started = true
        guard model.usable, model.info[5] >= 3 else { item = -1; message = "Dowsing needs 3 watts."; return }
        item = model.command(2)
        guard item >= 0, model.command(1, 3) != 0 else { item = -1; message = "No usable route item data."; return }
        hidden = max(0, model.command(0)) % 6; message = "An item is hidden in one of six patches."
    }
    private func guess(_ n: Int) {
        guard model.usable, attempts > 0, item >= 0 else { return }
        if n == hidden { attempts = 0; reward = true; message = "Item found!"; return }
        empty = n; attempts -= 1
        let distance = abs(n % 3 - hidden % 3) + abs(n / 3 - hidden / 3)
        message = attempts == 0 ? "Nothing found. Try another search." : distance <= 1 ? "It's near!" : "It's far away…"
    }
}

struct PolicyView: UIViewRepresentable {
    let name: String
    func makeUIView(context: Context) -> WKWebView {
        let configuration = WKWebViewConfiguration(); configuration.defaultWebpagePreferences.allowsContentJavaScript = false
        let view = WKWebView(frame: .zero, configuration: configuration)
        view.navigationDelegate = context.coordinator
        if let url = Bundle.main.url(forResource: name, withExtension: "html"), let html = try? String(contentsOf: url, encoding: .utf8) { view.loadHTMLString(html, baseURL: nil) }
        return view
    }
    func updateUIView(_ view: WKWebView, context: Context) {}
    func makeCoordinator() -> Coordinator { Coordinator() }
    final class Coordinator: NSObject, WKNavigationDelegate {
        func webView(_ webView: WKWebView, decidePolicyFor navigationAction: WKNavigationAction, decisionHandler: @escaping (WKNavigationActionPolicy) -> Void) {
            guard navigationAction.navigationType == .linkActivated else { decisionHandler(.allow); return }
            if let url = navigationAction.request.url, ["https", "mailto"].contains(url.scheme ?? "") { UIApplication.shared.open(url) }
            decisionHandler(.cancel)
        }
    }
}
