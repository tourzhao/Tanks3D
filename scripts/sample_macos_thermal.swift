// Read-only thermal pressure observations. This is not a temperature sensor or
// power meter; Foundation reports the operating system's thermal-pressure class.
import Foundation

let start = ProcessInfo.processInfo.systemUptime
while true {
    let state = ProcessInfo.processInfo.thermalState
    let name: String
    switch state {
    case .nominal: name = "nominal"
    case .fair: name = "fair"
    case .serious: name = "serious"
    case .critical: name = "critical"
    @unknown default: name = "unknown"
    }
    let sample: [String: Any] = [
        "elapsed_seconds": ProcessInfo.processInfo.systemUptime - start,
        "thermal_state": name
    ]
    if let data = try? JSONSerialization.data(withJSONObject: sample, options: [.sortedKeys]) {
        FileHandle.standardOutput.write(data)
        FileHandle.standardOutput.write(Data([10]))
    }
    Thread.sleep(forTimeInterval: 5)
}
