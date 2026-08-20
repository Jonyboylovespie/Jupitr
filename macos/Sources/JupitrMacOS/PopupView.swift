import JupitrCore
import SwiftUI

struct PopupView: View {
    @ObservedObject var model: JupitrModel
    let onSettings: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 9) {
            HStack(alignment: .center, spacing: 8) {
                Text(model.headerText)
                    .font(.system(size: 15, weight: .bold))
                    .foregroundStyle(JupitrTheme.cream)
                    .lineLimit(1)

                Spacer(minLength: 0)

                Button(action: onSettings) {
                    Image(systemName: "gearshape")
                        .font(.system(size: 15, weight: .semibold))
                        .foregroundStyle(JupitrTheme.muted)
                        .frame(width: 28, height: 28)
                }
                .buttonStyle(.plain)
                .help("Edit schedule")
            }

            Text(model.status.timer)
                .font(.system(size: 30, weight: .bold, design: .rounded))
                .foregroundStyle(color(for: model.status.tone))

            Text(model.status.subtitle)
                .font(.system(size: 12))
                .foregroundStyle(JupitrTheme.yellow)
                .lineLimit(2)

            Divider()
                .overlay(JupitrTheme.hoverBackground)

            if model.scheduleItems.isEmpty {
                Text("The DHS day type is not available yet.")
                    .font(.system(size: 12))
                    .foregroundStyle(JupitrTheme.muted)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(.vertical, 10)
            } else {
                ScrollView {
                    LazyVStack(spacing: 3) {
                        ForEach(model.scheduleItems) { item in
                            ScheduleRow(item: item)
                        }
                    }
                }
                .scrollIndicators(.hidden)
                .frame(maxHeight: 390)
            }
        }
        .padding(16)
        .frame(width: 400)
        .background(JupitrTheme.darkBackground)
        .preferredColorScheme(.dark)
    }

    private func color(for tone: StatusTone) -> Color {
        switch tone {
        case .orange: return JupitrTheme.orange
        case .yellow: return JupitrTheme.yellow
        case .muted: return JupitrTheme.muted
        }
    }
}

private struct ScheduleRow: View {
    let item: ScheduleItem

    var body: some View {
        HStack(spacing: 6) {
            Text(item.isLunch ? item.name : item.name + (item.isAfterLunch ? " (cont.)" : ""))
                .font(.system(size: item.isLunch ? 11 : 11, weight: item.isCurrent ? .bold : .regular))
                .foregroundStyle(nameColor)
                .lineLimit(1)
                .frame(minWidth: 112, alignment: .leading)

            Text("\(BellSchedule.formatTime(item.startMinute)) – \(BellSchedule.formatTime(item.endMinute))")
                .font(.system(size: 10, design: .monospaced))
                .foregroundStyle(item.isLunch ? JupitrTheme.yellowLight : JupitrTheme.muted)
                .lineLimit(1)

            if !item.isLunch && !item.className.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
                Text(item.className)
                    .font(.system(size: 11))
                    .foregroundStyle(item.isCurrent ? JupitrTheme.yellow : JupitrTheme.cream)
                    .lineLimit(1)
                    .frame(maxWidth: .infinity, alignment: .trailing)
            } else {
                Spacer(minLength: 0)
            }
        }
        .padding(.horizontal, item.isCurrent ? 12 : 9)
        .padding(.vertical, item.isLunch ? 6 : 7)
        .background(item.isCurrent ? JupitrTheme.hoverBackground : JupitrTheme.cardBackground)
        .clipShape(RoundedRectangle(cornerRadius: 5))
    }

    private var nameColor: Color {
        if item.isLunch { return JupitrTheme.yellow }
        if item.name.localizedCaseInsensitiveContains("Advisory") { return JupitrTheme.muted }
        return item.isCurrent ? JupitrTheme.cream : JupitrTheme.muted
    }
}
