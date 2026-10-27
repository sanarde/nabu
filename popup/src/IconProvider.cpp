#include "IconProvider.h"

#include <QIcon>

namespace {

constexpr int DefaultSize = 48;

}

IconProvider::IconProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap)
{
}

QPixmap IconProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    const int edge = requestedSize.isValid() ? qMax(requestedSize.width(), requestedSize.height()) : DefaultSize;
    const QPixmap pixmap = QIcon::fromTheme(id).pixmap(edge);

    *size = pixmap.size();
    return pixmap;
}
