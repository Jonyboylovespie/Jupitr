import AppKit

@MainActor
final class AppDelegate: NSObject, NSApplicationDelegate {
    let model = JupitrModel()
    private var statusItemController: StatusItemController?

    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.accessory)
        statusItemController = StatusItemController(model: model)
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        false
    }
}
