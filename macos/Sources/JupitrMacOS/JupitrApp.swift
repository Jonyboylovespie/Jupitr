import SwiftUI

@main
struct JupitrApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate

    var body: some Scene {
        // The app's visible entry point is the menu-bar status item. Keeping a
        // Settings scene also lets macOS expose the conventional settings
        // action when the application is activated by the system.
        Settings {
            SettingsView(model: appDelegate.model)
        }
    }
}
