#pragma once

#include <QQuickImageProvider>

/** Serves icons of the current icon theme as image://icon/<name>. */
class IconProvider : public QQuickImageProvider
{
public:
    IconProvider();

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;
};
