import AppKit
import JupitrCore
import SwiftUI

@MainActor
final class StatusItemController: NSObject, NSPopoverDelegate {
    private let model: JupitrModel
    private let statusItem: NSStatusItem
    private let popover: NSPopover
    private let menu: NSMenu
    private var settingsWindow: NSWindow?
    private var tooltipTimer: Timer?

    init(model: JupitrModel) {
        self.model = model
        self.statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
        self.popover = NSPopover()
        self.menu = NSMenu(title: "Jupitr")
        super.init()
        configureStatusItem()
        configurePopover()
        configureMenu()
        updateTooltip()

        let timer = Timer(timeInterval: 30, target: self, selector: #selector(updateTooltip), userInfo: nil, repeats: true)
        tooltipTimer = timer
        RunLoop.main.add(timer, forMode: .common)
    }

    deinit {
        tooltipTimer?.invalidate()
    }

    @objc private func statusItemAction(_ sender: Any?) {
        if NSApp.currentEvent?.type == .rightMouseUp {
            showMenu()
        } else {
            togglePopover()
        }
    }

    @objc private func updateTooltip() {
        statusItem.button?.toolTip = model.tooltipText
    }

    func popoverDidClose(_ notification: Notification) {
        model.setPopupVisible(false)
        updateTooltip()
    }

    @objc private func editSchedule(_ sender: Any?) {
        showSettings()
    }

    @objc private func quit(_ sender: Any?) {
        NSApp.terminate(nil)
    }

    private func configureStatusItem() {
        guard let button = statusItem.button else { return }
        button.image = trayImage()
        button.imagePosition = .imageOnly
        button.imageScaling = .scaleProportionallyDown
        button.target = self
        button.action = #selector(statusItemAction(_:))
        button.sendAction(on: [.leftMouseUp, .rightMouseUp])
        button.setAccessibilityLabel("Jupitr school schedule")
    }

    private func configurePopover() {
        popover.behavior = .transient
        popover.animates = true
        popover.delegate = self
        popover.appearance = NSAppearance(named: .darkAqua)
        popover.contentSize = NSSize(width: 432, height: 520)
        popover.contentViewController = NSHostingController(
            rootView: PopupView(model: model) { [weak self] in
                self?.showSettings()
            }
        )
    }

    private func configureMenu() {
        let editItem = NSMenuItem(
            title: "Edit Schedule…",
            action: #selector(editSchedule(_:)),
            keyEquivalent: ""
        )
        editItem.target = self
        menu.addItem(editItem)
        menu.addItem(.separator())

        let quitItem = NSMenuItem(title: "Quit Jupitr", action: #selector(quit(_:)), keyEquivalent: "q")
        quitItem.target = self
        menu.addItem(quitItem)
    }

    private func togglePopover() {
        guard let button = statusItem.button else { return }
        if popover.isShown {
            popover.performClose(nil)
        } else {
            model.setPopupVisible(true)
            popover.show(relativeTo: button.bounds, of: button, preferredEdge: .minY)
            popover.contentViewController?.view.window?.makeKey()
        }
    }

    private func showMenu() {
        guard let button = statusItem.button else { return }
        popover.performClose(nil)
        menu.popUp(positioning: nil, at: NSPoint(x: 0, y: button.bounds.height + 4), in: button)
    }

    private func showSettings() {
        popover.performClose(nil)

        if settingsWindow == nil {
            let hostingController = NSHostingController(
                rootView: SettingsView(model: model) { [weak self] in
                    self?.settingsWindow?.close()
                }
            )
            let window = NSWindow(contentViewController: hostingController)
            window.title = "Edit Schedule — Jupitr"
            window.styleMask = [.titled, .closable, .miniaturizable, .resizable]
            window.isReleasedWhenClosed = false
            window.setContentSize(NSSize(width: 900, height: 500))
            settingsWindow = window
        }

        settingsWindow?.center()
        settingsWindow?.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }

    private func trayImage() -> NSImage {
        if let logoURL = SharedResources.logoURL(),
           let image = NSImage(contentsOf: logoURL) {
            image.size = NSSize(width: 18, height: 18)
            image.isTemplate = false
            return image
        }

        // Keep a small fallback so an asset packaging error does not remove
        // the only way to access or quit the menu-bar application.
        let image = NSImage(size: NSSize(width: 18, height: 18))
        image.lockFocus()
        NSColor(calibratedRed: 1.0, green: 0.42, blue: 0.21, alpha: 1).setFill()
        NSBezierPath(ovalIn: NSRect(x: 1, y: 1, width: 16, height: 16)).fill()
        NSColor(calibratedRed: 1.0, green: 0.82, blue: 0.25, alpha: 1).setStroke()
        let ring = NSBezierPath(ovalIn: NSRect(x: 2, y: 2, width: 14, height: 14))
        ring.lineWidth = 1
        ring.stroke()
        image.unlockFocus()
        return image
    }
}
