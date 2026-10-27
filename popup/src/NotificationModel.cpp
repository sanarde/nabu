#include "NotificationModel.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>

namespace {

constexpr auto Service = "org.nuros.sanar.nabu";
constexpr auto Path = "/org/nuros/sanar/nabu";
constexpr auto Interface = "org.nuros.sanar.Nabu";

}

NotificationModel::NotificationModel(QObject *parent)
    : QAbstractListModel(parent)
{
    QDBusConnection bus = QDBusConnection::sessionBus();

    auto *watcher = new QDBusServiceWatcher(Service, bus, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceRegistered, this, &NotificationModel::onServiceRegistered);
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this, &NotificationModel::onServiceUnregistered);

    bus.connect(Service, Path, Interface, "Posted", this, SLOT(onPosted(QDBusMessage)));
    bus.connect(Service, Path, Interface, "Closed", this, SLOT(onClosed(uint, uint)));

    if (bus.interface()->isServiceRegistered(Service))
        load();
}

int NotificationModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_notes.size();
}

QVariant NotificationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_notes.size())
        return {};

    const Note &note = m_notes.at(index.row());
    switch (role) {
    case IdRole:
        return note.id;
    case AppNameRole:
        return note.appName;
    case AppIconRole:
        return note.appIcon;
    case SummaryRole:
        return note.summary;
    case BodyRole:
        return note.body;
    case DesktopEntryRole:
        return note.desktopEntry;
    case ImagePathRole:
        return note.imagePath;
    case ActionKeysRole:
    case ActionLabelsRole: {
        QStringList list;
        for (qsizetype i = role == ActionKeysRole ? 0 : 1; i < note.actions.size(); i += 2)
            list.append(note.actions.at(i));
        return list;
    }
    case UrgencyRole:
        return note.urgency;
    case ResidentRole:
        return note.resident;
    }
    return {};
}

QHash<int, QByteArray> NotificationModel::roleNames() const
{
    return {
        {IdRole, "notificationId"},
        {AppNameRole, "appName"},
        {AppIconRole, "appIcon"},
        {SummaryRole, "summary"},
        {BodyRole, "body"},
        {DesktopEntryRole, "desktopEntry"},
        {ImagePathRole, "imagePath"},
        {ActionKeysRole, "actionKeys"},
        {ActionLabelsRole, "actionLabels"},
        {UrgencyRole, "urgency"},
        {ResidentRole, "resident"},
    };
}

int NotificationModel::count() const
{
    return m_notes.size();
}

void NotificationModel::invoke(uint id, const QString &key)
{
    call("Invoke", {id, key});
}

void NotificationModel::dismiss(uint id)
{
    call("Dismiss", {id});
}

NotificationModel::Note NotificationModel::readNote(const QDBusArgument &argument)
{
    Note note;

    argument.beginStructure();
    argument >> note.id >> note.appName >> note.appIcon >> note.summary >> note.body
        >> note.desktopEntry >> note.imagePath >> note.actions >> note.urgency >> note.resident;
    argument.endStructure();
    return note;
}

void NotificationModel::onPosted(const QDBusMessage &message)
{
    if (!message.arguments().isEmpty())
        put(readNote(message.arguments().at(0).value<QDBusArgument>()));
}

void NotificationModel::onClosed(uint id, uint reason)
{
    Q_UNUSED(reason)

    const int row = indexOf(id);
    if (row < 0)
        return;
    beginRemoveRows({}, row, row);
    m_notes.removeAt(row);
    endRemoveRows();
    emit countChanged();
}

void NotificationModel::onServiceRegistered()
{
    load();
}

void NotificationModel::onServiceUnregistered()
{
    clear();
}

void NotificationModel::load()
{
    QDBusMessage message = QDBusMessage::createMethodCall(Service, Path, Interface, "GetActive");
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);

    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *done) {
        done->deleteLater();
        const QDBusMessage reply = done->reply();
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
            return;

        QList<Note> notes;
        const QDBusArgument argument = reply.arguments().at(0).value<QDBusArgument>();
        argument.beginArray();
        while (!argument.atEnd())
            notes.prepend(readNote(argument));
        argument.endArray();

        beginResetModel();
        m_notes = notes;
        endResetModel();
        emit countChanged();
    });
}

void NotificationModel::put(const Note &note)
{
    const int row = indexOf(note.id);
    if (row >= 0) {
        m_notes[row] = note;
        emit dataChanged(index(row), index(row));
        return;
    }
    beginInsertRows({}, 0, 0);
    m_notes.prepend(note);
    endInsertRows();
    emit countChanged();
}

void NotificationModel::clear()
{
    if (m_notes.isEmpty())
        return;
    beginResetModel();
    m_notes.clear();
    endResetModel();
    emit countChanged();
}

int NotificationModel::indexOf(uint id) const
{
    for (int i = 0; i < m_notes.size(); ++i) {
        if (m_notes.at(i).id == id)
            return i;
    }
    return -1;
}

void NotificationModel::call(const QString &method, const QVariantList &arguments)
{
    QDBusMessage message = QDBusMessage::createMethodCall(Service, Path, Interface, method);

    message.setArguments(arguments);
    QDBusConnection::sessionBus().asyncCall(message);
}
