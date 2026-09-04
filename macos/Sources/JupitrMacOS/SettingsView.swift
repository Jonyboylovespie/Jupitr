import JupitrCore
import SwiftUI

struct SettingsView: View {
    @ObservedObject var model: JupitrModel
    let onSaved: () -> Void

    @State private var draft: ScheduleConfig
    @State private var errorMessage: String?

    init(model: JupitrModel, onSaved: @escaping () -> Void = {}) {
        self.model = model
        self.onSaved = onSaved
        _draft = State(initialValue: model.config)
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Edit Schedule")
                .font(.system(size: 21, weight: .bold))
                .foregroundStyle(JupitrTheme.cream)

            Text("Enter your classes and lunch wave for each day.")
                .font(.system(size: 13))
                .foregroundStyle(JupitrTheme.muted)

            LazyVGrid(
                columns: [
                    GridItem(.fixed(64), alignment: .leading),
                    GridItem(.flexible(minimum: 120)),
                    GridItem(.flexible(minimum: 120)),
                    GridItem(.flexible(minimum: 120)),
                    GridItem(.flexible(minimum: 120)),
                    GridItem(.fixed(110))
                ],
                alignment: .leading,
                spacing: 8
            ) {
                Text("")
                ForEach(1...4, id: \.self) { block in
                    Text("Block \(block)")
                        .font(.system(size: 12, weight: .bold))
                        .foregroundStyle(JupitrTheme.muted)
                        .frame(maxWidth: .infinity, alignment: .center)
                }
                Text("Lunch")
                    .font(.system(size: 12, weight: .bold))
                    .foregroundStyle(JupitrTheme.muted)
                    .frame(maxWidth: .infinity, alignment: .center)

                ForEach(ScheduleConfig.dayLetters, id: \.self) { day in
                    Text("\(day) Day")
                        .font(.system(size: 12, weight: .bold))
                        .foregroundStyle(JupitrTheme.yellow)

                    ForEach(0..<4, id: \.self) { block in
                        TextField("Class name", text: classBinding(day: day, block: block))
                            .textFieldStyle(.roundedBorder)
                    }

                    Picker("Lunch", selection: lunchBinding(day: day)) {
                        Text("None").tag(0)
                        Text("Wave 1").tag(1)
                        Text("Wave 2").tag(2)
                        Text("Wave 3").tag(3)
                        Text("Wave 4").tag(4)
                    }
                    .labelsHidden()
                    .pickerStyle(.menu)
                }
            }

            HStack {
                Spacer()
                Button("Save") {
                    if let error = model.saveConfig(draft) {
                        errorMessage = error
                    } else {
                        onSaved()
                    }
                }
                .keyboardShortcut(.defaultAction)
                .buttonStyle(.borderedProminent)
                .tint(JupitrTheme.orange)
            }
        }
        .padding(20)
        .frame(minWidth: 860, minHeight: 440)
        .background(JupitrTheme.darkBackground)
        .preferredColorScheme(.dark)
        .onAppear {
            draft = model.config
        }
        .alert("Could not save schedule", isPresented: errorIsPresented) {
            Button("OK", role: .cancel) { errorMessage = nil }
        } message: {
            Text(errorMessage ?? "Unknown error")
        }
    }

    private var errorIsPresented: Binding<Bool> {
        Binding(
            get: { errorMessage != nil },
            set: { if !$0 { errorMessage = nil } }
        )
    }

    private func classBinding(day: String, block: Int) -> Binding<String> {
        Binding(
            get: { draft.className(for: day, blockIndex: block) },
            set: { draft.setClass($0, for: day, blockIndex: block) }
        )
    }

    private func lunchBinding(day: String) -> Binding<Int> {
        Binding(
            get: { draft.lunchWaves[day] ?? 1 },
            set: { draft.setLunchWave($0, for: day) }
        )
    }

}
