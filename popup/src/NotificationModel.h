#pragma once

#include <QAbstractListModel>
#include <QDBusArgument>
#include <QDBusMessage>
#include <QStringList>

/**
 * Active notifications of the Nabu daemon, newest first.
 *
 * The model loads the list when the daemon appears on the session bus and
 * follows its Posted and Closed signals. It is empty while the daemon is not
 * running.
 */
class NotificationModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        AppNameRole,
        AppIconRole,
        SummaryRole,
        BodyRole,
        DesktopEntryRole,
        ImagePathRole,
        ActionKeysRole,
        ActionLabelsRole,
        UrgencyRole,
        ResidentRole,
    };

    explicit NotificationModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;

    /** Triggers an action of the notification. The daemon closes it unless it is resident. */
    Q_INVOKABLE void invoke(uint id, const QString &key);

    /** Closes the notification as dismissed by the user. */
    Q_INVOKABLE void dismiss(uint id);

signals:
    void countChanged();

private slots:
    void onPosted(const QDBusMessage &message);
    void onClosed(uint id, uint reason);
    void onServiceRegistered();
    void onServiceUnregistered();

private:
    struct Note {
        uint id = 0;
        QString appName;
        QString appIcon;
        QString summary;
        QString body;
        QString desktopEntry;
        QString imagePath;
        QStringList actions;
        uchar urgency = 1;
        bool resident = false;
    };

    static Note readNote(const QDBusArgument &argument);
    void load();
    void put(const Note &note);
    void clear();
    int indexOf(uint id) const;
    void call(const QString &method, const QVariantList &arguments);

    QList<Note> m_notes;
};
