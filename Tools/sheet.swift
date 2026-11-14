// Labelled contact sheet. No PIL or ImageMagick on these Macs, and this ffmpeg has no drawtext.
//   swift Tools/sheet.swift <out.jpg> <width px> <columns> <title> <manifest.txt>
// Manifest: one cell per line, tab separated:  label <TAB> tag <TAB> image [<TAB> image ...]
// The images of a cell sit side by side over its label. A tag (e.g. LONG) is printed as a badge
// and tints the label band, so the marked cells can be found at a glance.
import AppKit

let args = CommandLine.arguments
guard args.count == 6, let width = Int(args[2]), let cols = Int(args[3]) else {
    print("usage: sheet.swift out.jpg width columns title manifest.txt"); exit(2)
}
let out = args[1], title = args[4]
struct Cell { let label: String; let tag: String; let images: [NSImage] }
var cells: [Cell] = []
for line in (try! String(contentsOfFile: args[5], encoding: .utf8)).split(separator: "\n") {
    let f = line.split(separator: "\t", omittingEmptySubsequences: false).map(String.init)
    if f.count < 3 { continue }
    let imgs = f[2...].compactMap { NSImage(contentsOfFile: $0) }
    if imgs.count != f.count - 2 { print("missing image for", f[0]) }
    if !imgs.isEmpty { cells.append(Cell(label: f[0], tag: f[1], images: imgs)) }
}
guard let first = cells.first else { print("no cells"); exit(1) }

let gap = CGFloat(max(6, width / 480))
let cellW = (CGFloat(width) - gap * CGFloat(cols + 1)) / CGFloat(cols)
let rep = first.images[0].representations[0]
let aspect = CGFloat(rep.pixelsHigh) / CGFloat(rep.pixelsWide)
let imgW = cellW / CGFloat(first.images.count)
let imgH = imgW * aspect
let labelH = max(44, cellW * 0.085)
let titleH = CGFloat(width) * 0.03
let rows = Int((Double(cells.count) / Double(cols)).rounded(.up))
let height = Int(titleH + gap + CGFloat(rows) * (imgH + labelH + gap))

let bmp = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: width, pixelsHigh: height, bitsPerSample: 8,
                           samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB,
                           bytesPerRow: 0, bitsPerPixel: 0)!
bmp.size = NSSize(width: width, height: height)     // one point per pixel, whatever the screen
NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: bmp)
NSGraphicsContext.current!.imageInterpolation = .high
NSColor(white: 0.07, alpha: 1).setFill()
NSRect(x: 0, y: 0, width: width, height: height).fill()

func text(_ s: String, _ rect: NSRect, size: CGFloat, color: NSColor, align: NSTextAlignment = .center) {
    let p = NSMutableParagraphStyle()
    p.alignment = align
    p.lineBreakMode = .byTruncatingTail
    let attr: [NSAttributedString.Key: Any] = [.font: NSFont.boldSystemFont(ofSize: size), .foregroundColor: color, .paragraphStyle: p]
    let h = (s as NSString).size(withAttributes: attr).height
    (s as NSString).draw(in: NSRect(x: rect.minX, y: rect.midY - h / 2, width: rect.width, height: h), withAttributes: attr)
}

// AppKit's origin is bottom left: lay out from the top by flipping y
func top(_ y: CGFloat, _ h: CGFloat) -> CGFloat { CGFloat(height) - y - h }

text(title, NSRect(x: gap * 2, y: top(0, titleH), width: CGFloat(width) - gap * 4, height: titleH),
     size: titleH * 0.5, color: NSColor(white: 0.92, alpha: 1), align: .left)
let amber = NSColor(red: 0.95, green: 0.62, blue: 0.18, alpha: 1)
for (i, cell) in cells.enumerated() {
    let x = gap + CGFloat(i % cols) * (cellW + gap)
    let y = titleH + gap + CGFloat(i / cols) * (imgH + labelH + gap)
    for (k, img) in cell.images.enumerated() {
        img.draw(in: NSRect(x: x + CGFloat(k) * imgW, y: top(y, imgH), width: imgW, height: imgH),
                 from: .zero, operation: .copy, fraction: 1)
    }
    let band = NSRect(x: x, y: top(y + imgH, labelH), width: cellW, height: labelH)
    (cell.tag.isEmpty ? NSColor(white: 0.16, alpha: 1) : NSColor(red: 0.30, green: 0.19, blue: 0.05, alpha: 1)).setFill()
    band.fill()
    var room = band.insetBy(dx: labelH * 0.25, dy: 0)
    if !cell.tag.isEmpty {
        let badgeW = labelH * 0.62 * CGFloat(cell.tag.count) * 0.72 + labelH * 0.4
        let badge = NSRect(x: band.maxX - badgeW - labelH * 0.15, y: band.minY + labelH * 0.15, width: badgeW, height: labelH * 0.7)
        amber.setFill()
        NSBezierPath(roundedRect: badge, xRadius: labelH * 0.12, yRadius: labelH * 0.12).fill()
        text(cell.tag, badge, size: labelH * 0.44, color: .black)
        room.size.width -= badgeW + labelH * 0.2
    }
    text(cell.label, room, size: labelH * 0.5, color: cell.tag.isEmpty ? NSColor(white: 0.9, alpha: 1) : amber, align: .left)
}
NSGraphicsContext.current!.flushGraphics()
let data = bmp.representation(using: .jpeg, properties: [.compressionFactor: 0.9])!
try! data.write(to: URL(fileURLWithPath: out))
print("sheet", out, "\(width)x\(height)", cells.count, "cells")
