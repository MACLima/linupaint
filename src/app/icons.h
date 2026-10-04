#pragma once

#include <QIcon>
#include <QPalette>

namespace app {

// Tool icons are SVGs whose `currentColor` follows the palette text color (light and dark themes).
QIcon themedIcon(const QString& name, const QPalette& palette);
QIcon appIcon();

} // namespace app
