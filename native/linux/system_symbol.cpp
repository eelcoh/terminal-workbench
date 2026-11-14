#include "chrome_icon.h"

// No system symbol set on Linux: Band draws its own icons.
QImage bandSystemSymbol(const QString &, const QColor &, const QSize &) { return {}; }
