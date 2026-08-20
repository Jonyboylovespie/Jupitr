// swift-tools-version: 6.0

import PackageDescription

let package = Package(
    name: "JupitrMacOS",
    platforms: [
        .macOS(.v13)
    ],
    products: [
        .executable(name: "Jupitr", targets: ["JupitrMacOS"])
    ],
    targets: [
        .target(
            name: "JupitrCore",
            path: "Sources/JupitrCore"
        ),
        .executableTarget(
            name: "JupitrMacOS",
            dependencies: ["JupitrCore"],
            path: "Sources/JupitrMacOS"
        ),
        .testTarget(
            name: "JupitrCoreTests",
            dependencies: ["JupitrCore"],
            path: "Tests/JupitrCoreTests"
        )
    ]
)
