#include "chrome_icon.h"
#import <AppKit/AppKit.h>
#include <QHash>

// Band's icon names as SF Symbols, so the Mac client's icons are the system's.
// A name without an entry (the Linux window buttons, "filter-off") keeps
// Band's own drawing, as does every name on a system without the symbol.
static NSString *symbolFor(const QString &name)
{
    static const QHash<QString, QString> symbols{
        {"menu", "line.3.horizontal"}, {"workspaces", "rectangle.grid.1x2"}, {"agents", "waveform.path.ecg"},
        {"shell", "terminal"}, {"apps", "square.grid.2x2"}, {"width", "arrow.left.and.right"},
        {"full", "arrow.up.left.and.arrow.down.right"}, {"host", "globe"}, {"close", "xmark"},
        {"sidebar", "sidebar.left"}, {"plus", "plus"}, {"more", "ellipsis"}, {"chevron", "chevron.down"},
        {"pin", "pin"}, {"check", "checkmark"}, {"info", "info.circle"}, {"help", "questionmark"}};
    const auto symbol = symbols.value(name);
    return symbol.isEmpty() ? nil : symbol.toNSString();
}

// The symbol drawn in one color, centered in an image of `pixels` device pixels.
QImage bandSystemSymbol(const QString &name, const QColor &color, const QSize &pixels)
{
    NSString *symbol = symbolFor(name);
    if (!symbol || pixels.isEmpty()) return {};
    @autoreleasepool {
        NSImage *image = [NSImage imageWithSystemSymbolName:symbol accessibilityDescription:nil];
        if (!image) return {};
        // The bitmap context below is one unit per device pixel.
        image = [image imageWithSymbolConfiguration:[NSImageSymbolConfiguration configurationWithPointSize:pixels.height() * 0.78 weight:NSFontWeightMedium]];
        QImage result(pixels, QImage::Format_RGBA8888_Premultiplied);
        result.fill(Qt::transparent);
        CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        CGContextRef bitmap = CGBitmapContextCreate(result.bits(), pixels.width(), pixels.height(), 8, result.bytesPerLine(), space, kCGImageAlphaPremultipliedLast);
        CGColorSpaceRelease(space);
        if (!bitmap) return {};
        [NSGraphicsContext saveGraphicsState];
        NSGraphicsContext.currentContext = [NSGraphicsContext graphicsContextWithCGContext:bitmap flipped:NO];
        const NSSize size = image.size;
        const CGFloat scale = qMin<CGFloat>(1, qMin(pixels.width() / size.width, pixels.height() / size.height));
        const NSRect target = NSMakeRect((pixels.width() - size.width * scale) / 2, (pixels.height() - size.height * scale) / 2, size.width * scale, size.height * scale);
        [image drawInRect:target fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1];
        // A symbol is a template: keep its coverage, replace its color.
        [[NSColor colorWithSRGBRed:color.redF() green:color.greenF() blue:color.blueF() alpha:color.alphaF()] set];
        NSRectFillUsingOperation(NSMakeRect(0, 0, pixels.width(), pixels.height()), NSCompositingOperationSourceIn);
        [NSGraphicsContext restoreGraphicsState];
        CGContextRelease(bitmap);
        return result;
    }
}
