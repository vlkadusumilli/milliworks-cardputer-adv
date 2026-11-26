// check_voice.swift - listens to each Breathe voice clip with macOS on-device speech
// recognition and checks it says the right word (in, hold, out, two .. eight).
//
//   swiftc -O tools/check_voice.swift -o /tmp/check_voice && /tmp/check_voice luna julian
//
// Each clip gets 0.6 s of silence added on both sides first: single short words are
// recognised much more reliably that way. Exit code 1 if any clip says something else.
import AVFoundation
import Foundation
import Speech

let root = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent()
let voicesDir = root.appendingPathComponent("design/sd/milliWorks/breathe/voices")
let expected: [(String, [String])] = [
    ("in", ["in"]), ("hold", ["hold"]), ("out", ["out"]), ("2", ["two", "2"]), ("3", ["three", "3"]),
    ("4", ["four", "4", "for"]), ("5", ["five", "5"]), ("6", ["six", "6"]), ("7", ["seven", "7"]), ("8", ["eight", "8"]),
]

func padded(_ url: URL) throws -> URL {
    let file = try AVAudioFile(forReading: url)
    let fmt = file.processingFormat
    let pad = AVAudioFrameCount(fmt.sampleRate * 0.6)
    let frames = AVAudioFrameCount(file.length)
    let buf = AVAudioPCMBuffer(pcmFormat: fmt, frameCapacity: frames + 2 * pad)!
    let src = AVAudioPCMBuffer(pcmFormat: fmt, frameCapacity: frames)!
    try file.read(into: src)
    buf.frameLength = frames + 2 * pad
    for c in 0..<Int(fmt.channelCount) {
        let d = buf.floatChannelData![c]
        for i in 0..<Int(buf.frameLength) { d[i] = 0 }
        for i in 0..<Int(frames) { d[Int(pad) + i] = src.floatChannelData![c][i] }
    }
    let out = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString + ".caf")
    let w = try AVAudioFile(forWriting: out, settings: fmt.settings)
    try w.write(from: buf)
    return out
}

func hear(_ url: URL, _ rec: SFSpeechRecognizer) -> String {
    let req = SFSpeechURLRecognitionRequest(url: url)
    req.requiresOnDeviceRecognition = true
    req.contextualStrings = expected.flatMap { $0.1 }
    var text = "(nothing heard)"
    var done = false
    rec.recognitionTask(with: req) { r, e in
        if let r = r, r.isFinal { text = r.bestTranscription.formattedString; done = true }
        else if e != nil { done = true }
    }
    while !done { RunLoop.current.run(until: Date() + 0.05) }
    return text
}

guard let rec = SFSpeechRecognizer(locale: Locale(identifier: "en-US")), rec.supportsOnDeviceRecognition else {
    print("on-device speech recognition is not available"); exit(2)
}
var bad = 0
for voice in CommandLine.arguments.dropFirst() {
    for (key, ok) in expected {
        let url = voicesDir.appendingPathComponent("\(voice)/\(key).wav")
        guard FileManager.default.fileExists(atPath: url.path) else { print("\(voice)/\(key).wav  missing  CHECK"); bad += 1; continue }
        let text = (try? hear(padded(url), rec)) ?? "(could not read)"
        let good = ok.contains(text.lowercased().trimmingCharacters(in: .punctuationCharacters.union(.whitespaces)))
        if !good { bad += 1 }
        print("\(voice)/\(key).wav".padding(toLength: 18, withPad: " ", startingAt: 0), "heard: \(text)".padding(toLength: 24, withPad: " ", startingAt: 0), good ? "ok" : "CHECK")
    }
}
print(bad == 0 ? "All clips say the right word." : "\(bad) clip(s) to check.")
exit(bad == 0 ? 0 : 1)
