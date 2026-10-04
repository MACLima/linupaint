#include "icons.h"

#include <QFile>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

namespace app {

QIcon themedIcon(const QString& name, const QPalette& palette)
{
    QFile f(QStringLiteral(":/icons/%1.svg").arg(name));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QByteArray svg = f.readAll();
    svg.replace("currentColor", palette.color(QPalette::WindowText).name().toLatin1());
    QSvgRenderer renderer(svg);
    QIcon icon;
    for (const int size : {16, 24, 32, 48, 64}) {
        QPixmap pm(size, size);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        renderer.render(&p);
        p.end();
        icon.addPixmap(pm);
    }
    return icon;
}

QIcon appIcon()
{
    return QIcon(QStringLiteral(":/icons/linupaint.svg"));
}

} // namespace app
