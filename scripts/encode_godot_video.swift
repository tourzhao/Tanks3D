// Original project utility, PolyForm Noncommercial 1.0.0.
// Encode actual Godot MovieWriter PNG frames with macOS system frameworks.
// No generated/interpolated frames, audio, third-party codec, or network use.
//
// Build:
// xcrun swiftc -O -module-cache-path build/godot/swift-module-cache \
//   scripts/encode_godot_video.swift -o build/godot/encode_godot_video
// Usage: build/godot/encode_godot_video INPUT_DIRECTORY OUTPUT.mp4 [FPS=60]

import AVFoundation
import CoreGraphics
import CoreVideo
import Foundation
import ImageIO

enum EncodingError: Error, CustomStringConvertible
{
    case invalid(String)

    var description: String
    {
        switch self
        {
        case .invalid(let message): return message
        }
    }
}

func fail(_ message: String) -> EncodingError
{
    return .invalid(message)
}

func imageSource(_ url: URL) throws -> CGImageSource
{
    guard let source = CGImageSourceCreateWithURL(url as CFURL, nil),
          CGImageSourceGetCount(source) == 1,
          CGImageSourceGetType(source) as String? == "public.png" else
    {
        throw fail("Cannot read a single PNG image: \(url.lastPathComponent)")
    }
    return source
}

func dimensions(_ url: URL) throws -> (Int, Int)
{
    let source = try imageSource(url)
    guard let properties = CGImageSourceCopyPropertiesAtIndex(source, 0, nil)
            as? [CFString: Any],
          let width = properties[kCGImagePropertyPixelWidth] as? Int,
          let height = properties[kCGImagePropertyPixelHeight] as? Int,
          width > 0, height > 0 else
    {
        throw fail("Cannot read image dimensions: \(url.lastPathComponent)")
    }
    return (width, height)
}

func frameBuffer(_ url: URL, pool: CVPixelBufferPool,
                 width: Int, height: Int) throws -> CVPixelBuffer
{
    let source = try imageSource(url)
    guard let image = CGImageSourceCreateImageAtIndex(source, 0, nil),
          image.width == width, image.height == height else
    {
        throw fail("Cannot decode frame, or dimensions changed: \(url.lastPathComponent)")
    }
    var optionalBuffer: CVPixelBuffer?
    let result = CVPixelBufferPoolCreatePixelBuffer(kCFAllocatorDefault, pool,
                                                   &optionalBuffer)
    guard result == kCVReturnSuccess, let buffer = optionalBuffer else
    {
        throw fail("Cannot allocate pixel buffer (CoreVideo \(result))")
    }
    let lock = CVPixelBufferLockBaseAddress(buffer, [])
    guard lock == kCVReturnSuccess else
    {
        throw fail("Cannot lock pixel buffer (CoreVideo \(lock))")
    }
    defer { CVPixelBufferUnlockBaseAddress(buffer, []) }
    let bitmapInfo = CGBitmapInfo.byteOrder32Big.rawValue |
                     CGImageAlphaInfo.noneSkipFirst.rawValue
    guard let address = CVPixelBufferGetBaseAddress(buffer),
          let colorSpace = CGColorSpace(name: CGColorSpace.sRGB),
          let context = CGContext(data: address, width: width, height: height,
                                  bitsPerComponent: 8,
                                  bytesPerRow: CVPixelBufferGetBytesPerRow(buffer),
                                  space: colorSpace, bitmapInfo: bitmapInfo) else
    {
        throw fail("Cannot create ARGB drawing context")
    }
    context.interpolationQuality = .none
    context.setBlendMode(.copy)
    context.draw(image, in: CGRect(x: 0, y: 0, width: width, height: height))
    return buffer
}

func encode() throws
{
    let arguments = CommandLine.arguments
    guard arguments.count == 3 || arguments.count == 4 else
    {
        throw fail("Usage: encode_godot_video INPUT_DIRECTORY OUTPUT.mp4 [FPS=60]")
    }
    let fps = arguments.count == 4 ? Int(arguments[3]) : 60
    guard let fps, (1...240).contains(fps) else
    {
        throw fail("FPS must be an integer from 1 through 240")
    }
    let manager = FileManager.default
    let input = URL(fileURLWithPath: arguments[1], isDirectory: true)
        .standardizedFileURL
    let output = URL(fileURLWithPath: arguments[2]).standardizedFileURL
    guard output.pathExtension.lowercased() == "mp4" else
    {
        throw fail("Output must have an .mp4 extension")
    }
    guard !manager.fileExists(atPath: output.path) else
    {
        throw fail("Refusing to overwrite existing output: \(output.path)")
    }
    let entries = try manager.contentsOfDirectory(at: input,
        includingPropertiesForKeys: [.isRegularFileKey], options: [.skipsHiddenFiles])
    let frames = try entries.filter
    {
        guard $0.pathExtension.lowercased() == "png" else { return false }
        return (try $0.resourceValues(forKeys: [.isRegularFileKey])).isRegularFile == true
    }.sorted
    {
        $0.lastPathComponent.compare($1.lastPathComponent,
            options: [.numeric, .literal],
            locale: Locale(identifier: "en_US_POSIX")) == .orderedAscending
    }
    guard let first = frames.first else
    {
        throw fail("No PNG frames found in \(input.path)")
    }
    let (width, height) = try dimensions(first)
    guard width % 2 == 0 && height % 2 == 0 else
    {
        throw fail("H.264 requires even frame dimensions; got \(width)×\(height)")
    }
    for frame in frames.dropFirst()
    {
        let size = try dimensions(frame)
        guard size.0 == width && size.1 == height else
        {
            throw fail("Mixed frame dimensions: \(frame.lastPathComponent) is " +
                       "\(size.0)×\(size.1), expected \(width)×\(height)")
        }
    }
    try manager.createDirectory(at: output.deletingLastPathComponent(),
                                withIntermediateDirectories: true)
    let temporary = output.deletingLastPathComponent()
        .appendingPathComponent(".encoding-\(UUID().uuidString).mp4")
    defer { try? manager.removeItem(at: temporary) }
    let writer = try AVAssetWriter(outputURL: temporary, fileType: .mp4)
    writer.shouldOptimizeForNetworkUse = true
    let settings: [String: Any] = [
        AVVideoCodecKey: AVVideoCodecType.h264,
        AVVideoWidthKey: width,
        AVVideoHeightKey: height,
        AVVideoCompressionPropertiesKey: [
            AVVideoAverageBitRateKey: max(2_000_000, width * height * 12),
            AVVideoExpectedSourceFrameRateKey: fps,
            AVVideoMaxKeyFrameIntervalKey: fps * 2,
            AVVideoProfileLevelKey: AVVideoProfileLevelH264HighAutoLevel
        ],
        AVVideoColorPropertiesKey: [
            AVVideoColorPrimariesKey: AVVideoColorPrimaries_ITU_R_709_2,
            AVVideoTransferFunctionKey: AVVideoTransferFunction_ITU_R_709_2,
            AVVideoYCbCrMatrixKey: AVVideoYCbCrMatrix_ITU_R_709_2
        ]
    ]
    guard writer.canApply(outputSettings: settings, forMediaType: .video) else
    {
        throw fail("macOS cannot encode the requested H.264 settings")
    }
    let track = AVAssetWriterInput(mediaType: .video, outputSettings: settings)
    track.expectsMediaDataInRealTime = false
    let adaptor = AVAssetWriterInputPixelBufferAdaptor(assetWriterInput: track,
        sourcePixelBufferAttributes: [
            kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32ARGB,
            kCVPixelBufferWidthKey as String: width,
            kCVPixelBufferHeightKey as String: height,
            kCVPixelBufferCGImageCompatibilityKey as String: true,
            kCVPixelBufferCGBitmapContextCompatibilityKey as String: true
        ])
    guard writer.canAdd(track) else
    {
        throw fail("Cannot add the video track")
    }
    writer.add(track)
    guard writer.startWriting() else
    {
        throw fail("Cannot start encoding: \(writer.error?.localizedDescription ?? "unknown error")")
    }
    var completed = false
    defer
    {
        if !completed { writer.cancelWriting() }
    }
    writer.startSession(atSourceTime: .zero)
    guard let pool = adaptor.pixelBufferPool else
    {
        throw fail("Video encoder did not create a pixel-buffer pool")
    }
    for (index, frame) in frames.enumerated()
    {
        let deadline = Date().addingTimeInterval(30)
        while !track.isReadyForMoreMediaData
        {
            guard writer.status == .writing else
            {
                throw fail("Encoding stopped: \(writer.error?.localizedDescription ?? "unknown error")")
            }
            guard Date() < deadline else { throw fail("Encoder readiness timed out") }
            Thread.sleep(forTimeInterval: 0.002)
        }
        try autoreleasepool
        {
            let buffer = try frameBuffer(frame, pool: pool, width: width, height: height)
            let time = CMTime(value: Int64(index), timescale: Int32(fps))
            guard adaptor.append(buffer, withPresentationTime: time) else
            {
                throw fail("Cannot append frame \(index): " +
                           "\(writer.error?.localizedDescription ?? "unknown error")")
            }
        }
    }
    writer.endSession(atSourceTime: CMTime(value: Int64(frames.count),
                                          timescale: Int32(fps)))
    track.markAsFinished()
    let finished = DispatchSemaphore(value: 0)
    writer.finishWriting { finished.signal() }
    guard finished.wait(timeout: .now() + 60) == .success else
    {
        throw fail("Timed out finishing the video")
    }
    guard writer.status == .completed else
    {
        throw fail("Cannot finish encoding: \(writer.error?.localizedDescription ?? "unknown error")")
    }
    try manager.moveItem(at: temporary, to: output)
    completed = true
    let result: [String: Any] = [
        "output": output.path, "frames": frames.count, "fps": fps,
        "width": width, "height": height, "codec": "H.264",
        "duration_seconds": Double(frames.count) / Double(fps),
        "audio": false
    ]
    let receipt = try JSONSerialization.data(withJSONObject: result,
                                             options: [.prettyPrinted, .sortedKeys])
    print(String(decoding: receipt, as: UTF8.self))
}

do
{
    try encode()
}
catch
{
    FileHandle.standardError.write(Data("Video encoding failed: \(error)\n".utf8))
    exit(1)
}
