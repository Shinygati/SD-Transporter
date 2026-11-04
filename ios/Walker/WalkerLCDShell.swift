import SwiftUI

// The supplied shell is drawn unchanged. Only the inner LCD rectangle
// is covered; Pikachu and the sample counters never become app state.
struct WalkerLCDShell: View {
    @AppStorage("menuHoldMilliseconds") private var menuHoldMilliseconds = 1000
    private var menuHoldSeconds: Double { Double([250,500,1000,1500,2000,3000].contains(menuHoldMilliseconds) ? menuHoldMilliseconds:1000)/1000 }
    var pixels: [UInt8]
    var contrast = 1
    var onPress: ((Int)->Void)? = nil
    var onCentreHold: (()->Void)? = nil
    var body: some View {
        GeometryReader { geometry in
            let w=geometry.size.width,h=geometry.size.height,r=min(w*0.46,h*0.48)
            ZStack {
                Canvas { context,size in
                    let body=CGRect(x:size.width/2-r,y:size.height/2-r,width:r*2,height:r*2)
                    context.draw(context.resolve(Image("WalkerShell")),in:body)
                    let lcd=CGRect(x:body.minX+body.width*230/1005,y:body.minY+body.height*261/1003,width:body.width*553/1005,height:body.height*383/1003)
                    var screen=context
                    screen.clip(to:Path(roundedRect:lcd,cornerRadius:r*0.035))
                    let palette=WalkerLCDShell.palette(contrast)
                    screen.fill(Path(lcd),with:.color(palette[0]))
                    guard pixels.count==6144 else { return }
                    let scale=min(lcd.width/96,lcd.height/64),x=lcd.midX-48*scale,y=lcd.midY-32*scale
                    for py in 0..<64 { for px in 0..<96 {
                        let left=(x+CGFloat(px)*scale).rounded(),top=(y+CGFloat(py)*scale).rounded()
                        let right=(x+CGFloat(px+1)*scale).rounded(),bottom=(y+CGFloat(py+1)*scale).rounded()
                        screen.fill(Path(CGRect(x:left,y:top,width:right-left,height:bottom-top)),with:.color(palette[Int(pixels[py*96+px]&3)]))
                    } }
                }
                if let onPress {
                    ForEach(0..<3) { button in
                        Group {
                            if button==1, let onCentreHold {
                                Circle().fill(Color.clear).frame(width:44,height:44).contentShape(Circle())
                                    .gesture(LongPressGesture(minimumDuration:menuHoldSeconds).exclusively(before:TapGesture()).onEnded { action in
                                        switch action {
                                        case .first(true): onCentreHold()
                                        case .second: onPress(button)
                                        default: break
                                        }
                                    })
                                    .accessibilityAddTraits(.isButton)
                                    .accessibilityAction { onPress(button) }
                                    .accessibilityAction(named:Text("Back to main LCD")) { onCentreHold() }
                            } else {
                                Button { onPress(button) } label: { Circle().fill(Color.clear).frame(width:44,height:44).contentShape(Circle()) }
                                    .buttonStyle(.plain)
                            }
                        }
                            .position(x:w/2+CGFloat(button-1)*r*0.45,y:h/2+r*(button==1 ? 0.61:0.55))
                            .accessibilityLabel(button==0 ? "Left":button==1 ? "Centre":"Right")
                    }
                }
            }
        }
    }
    static func palette(_ contrast: Int) -> [Color] {
        let colors: [[UInt32]]=[
            [0xb4b7aa,0x9a9f91,0x787e6f,0x535b4b],
            [0xb4b7aa,0x8d9383,0x555e4c,0x232b1d],
            [0xb4b7aa,0x7f8675,0x3e4835,0x10180d]]
        return colors[max(0,min(2,contrast))].map { Color(red:Double(($0>>16)&255)/255,green:Double(($0>>8)&255)/255,blue:Double($0&255)/255) }
    }
}
struct WalkerDetailsView: View {
    @EnvironmentObject var model: WalkerModel
    private func names(item: Bool) -> String {
        (0..<3).compactMap { slot -> String? in
            let id=model.packID(item:item,slot:slot)
            guard id>0,id<=(item ? 536:493) else { return nil }
            return item ? WalkerCore.item(Int64(id)):WalkerCore.species(Int64(id))
        }.joined(separator:", ")
    }
    var body: some View {
        if model.state.data != nil && !model.state.revoked && !model.state.returnPending {
            VStack(alignment:.leading,spacing:5) {
                Text("Course: \(model.text(2))")
                Text("\(model.info[3]) steps · \(model.info[5]) watts")
                Text("Items: \(names(item:true).isEmpty ? "None":names(item:true))")
                Text("Caught: \(names(item:false).isEmpty ? "None":names(item:false))")
            }.font(.callout).foregroundStyle(WalkerTheme.mint).frame(maxWidth:.infinity,alignment:.leading)
        }
    }
}
