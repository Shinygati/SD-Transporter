import SwiftUI

struct ConnectionChoiceView: View {
    @EnvironmentObject var model: WalkerModel
    var body: some View {
        ScrollView {
            VStack(alignment:.leading,spacing:14) {
                Text("Visit another profile on this phone, or connect to a nearby phone over Bluetooth.")
                NavigationLink("Another profile",icon:6) { ProfileVisitView() }.disabled(!model.usable)
                NavigationLink("Nearby phone",icon:7) { NearbyChoiceView() }.disabled(!model.usable)
            }.padding()
        }.navigationTitle("Connect").background(WalkerTheme.background).buttonStyle(WalkerButton())
    }
}
struct NearbyChoiceView: View {
    var body: some View {
        ScrollView {
            VStack(alignment:.leading,spacing:14) {
                NavigationLink("Visit a Pokéwalker",icon:18) { NearbyRoleView(battle:false) }
                NavigationLink("Friendly battle · 30 watts",icon:1) { NearbyRoleView(battle:true) }
                Text("Each Pokémon stays with its owner. Console transfers use Wi-Fi.")
            }.padding()
        }.navigationTitle("Nearby phone").background(WalkerTheme.background).buttonStyle(WalkerButton())
    }
}
struct NearbyRoleView: View {
    let battle: Bool
    var body: some View {
        ScrollView {
            VStack(alignment:.leading,spacing:14) {
                Text(battle ? "Both players pay 30 watts after accepting. Winner receives a rare item reward. Expanded items must be enabled on both phones; otherwise rewards use course items." : "Each Pokéwalker receives its usual daily visit gift. The same pair can meet once per day.")
                if battle {
                    Text("Battles use retained moves, power, accuracy and PP. Status moves brace against the next attack. Type matchups and other special effects are not simulated.").font(.callout)
                }
                NavigationLink("Host a connection",icon:7) { NearbySessionView(battle:battle,host:true) }
                NavigationLink("Search nearby phones",icon:7) { NearbySessionView(battle:battle,host:false) }
            }.padding()
        }.navigationTitle(battle ? "Friendly battle":"Nearby visit").background(WalkerTheme.background).buttonStyle(WalkerButton())
    }
}
struct NearbySessionView: View {
    @EnvironmentObject var model: WalkerModel
    let battle: Bool
    let host: Bool
    @State private var session: PhonePeerSession?
    @State private var error = ""
    var body: some View {
        Group {
            if let session { NearbyActivityView(session:session) }
            else { Text(error.isEmpty ? "Connecting…":error).padding() }
        }
        .navigationTitle(battle ? "Friendly battle":"Nearby visit")
        .onAppear {
            guard session==nil,error.isEmpty else { return }
            do { let value=try PhonePeerSession(model:model,battle:battle);session=value;value.start(host:host) }
            catch { self.error=error.localizedDescription }
        }
        .onDisappear { session?.close() }
    }
}
private struct NearbyActivityView: View {
    @ObservedObject var session: PhonePeerSession
    @State private var confirmRewardMismatch = false
    @Environment(\.dismiss) private var dismiss
    var body: some View {
        ScrollView {
            VStack(alignment:.leading,spacing:14) {
                if let playback = session.visitPlayback { WalkerVisitDisplay(playback: playback) }
                TimelineView(.periodic(from:.now,by:0.12)) { timeline in
                    let milliseconds=UInt32(truncatingIfNeeded:UInt64(ProcessInfo.processInfo.systemUptime*1000))
                    if let pixels=session.frame(ms:milliseconds) {
                        WalkerLCDShell(pixels:pixels,onPress:{ button in
                            if button==1 { session.choose() } else { session.select(button==0 ? -1:1) }
                        }).frame(height:340).accessibilityLabel("Pokéwalker battle display")
                    }
                }
                WalkerDetailsView()
                if !session.remoteName.isEmpty { Text(session.remoteName).font(.title3) }
                Text(session.message).foregroundStyle(WalkerTheme.gold)
                ForEach(session.phones) { phone in Button(phone.name,icon:7) { session.connect(phone.id) } }
                if session.canAccept { Button(session.battle ? "Accept · 30 watts":"Start visit",icon:18) { if session.rewardsDiffer { confirmRewardMismatch=true } else { session.accept() } } }
                if session.canMove {
                    HStack {
                        Button { session.select(-1) } label: { Text("◀").frame(minWidth:44,minHeight:44) }.accessibilityLabel("Previous move")
                        Button { session.choose() } label: { Text("●").frame(minWidth:44,minHeight:44) }.accessibilityLabel("Use selected move")
                        Button { session.select(1) } label: { Text("▶").frame(minWidth:44,minHeight:44) }.accessibilityLabel("Next move")
                    }
                }
                Button("Disconnect",icon:7) { session.close();dismiss() }
            }.padding()
        }.background(WalkerTheme.background).buttonStyle(WalkerButton())
            .alert("Reward settings differ",isPresented:$confirmRewardMismatch) {
                Button("Continue · 30 watts") { session.accept() }
                Button("Cancel",role:.cancel) {}
            } message: {
                Text("One phone has Expanded Dowsing items disabled. This battle will use only original course items, with rarer finds weighted more heavily. Each player still pays 30 watts.")
            }
    }
}
struct BattleRewardView: View {
    @EnvironmentObject var model: WalkerModel
    @State private var slot: Int?
    @State private var confirm = false
    @State private var leaving = false
    var body: some View {
        Form {
            Text(model.battleRewardName).font(.title2)
            Text("Your pack is full. Replace an item or leave the reward.")
            ForEach(0..<3) { index in
                Button("Replace slot \(index+1)",icon:3) { slot=index;confirm=true }
            }
            Button("Leave reward",role:.destructive) { leaving=true }
            if !model.message.isEmpty { Text(model.message) }
        }.navigationTitle("Battle reward")
            .confirmationDialog("Replace the item in this slot?",isPresented:$confirm,titleVisibility:.visible) {
                Button("Replace item") { if let slot { model.claimBattleReward(slot) } }
            }
            .confirmationDialog("Leave this reward? It cannot be claimed again.",isPresented:$leaving,titleVisibility:.visible) {
                Button("Leave reward",role:.destructive) { model.claimBattleReward(nil) }
            }
    }
}
