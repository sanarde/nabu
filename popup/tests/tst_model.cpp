#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

#include "NotificationModel.h"

class TestModel : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void postedNotificationAppears();
    void replacementKeepsTheRow();
    void newestComesFirst();
    void dismissRemovesTheRow();
    void invokeRemovesTheRow();
    void loadsExistingNotifications();

private:
    uint post(const QString &summary, uint replaces = 0, const QStringList &actions = {});

    QTemporaryDir m_dir;
    QProcess m_daemon;
};

uint TestModel::post(const QString &summary, uint replaces, const QStringList &actions)
{
    QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.Notifications",
        "/org/freedesktop/Notifications", "org.freedesktop.Notifications", "Notify");

    message.setArguments({"test", replaces, "dialog-information", summary, "body of " + summary, actions,
                          QVariantMap{{"desktop-entry", "org.example.app"}}, 0});
    QDBusReply<uint> reply = QDBusConnection::sessionBus().call(message);
    return reply.isValid() ? reply.value() : 0;
}

void TestModel::initTestCase()
{
    QVERIFY(m_dir.isValid());
    qputenv("XDG_RUNTIME_DIR", m_dir.filePath("run").toUtf8());
    qputenv("XDG_STATE_HOME", m_dir.filePath("state").toUtf8());
    QVERIFY(QDir().mkpath(m_dir.filePath("run")));
    QFile::setPermissions(m_dir.filePath("run"), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

    m_daemon.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    m_daemon.start(qEnvironmentVariable("NABUD"), {});
    QVERIFY(m_daemon.waitForStarted());

    QDBusConnection bus = QDBusConnection::sessionBus();
    QTRY_VERIFY_WITH_TIMEOUT(bus.interface()->isServiceRegistered("org.nuros.sanar.nabu"), 5000);
}

void TestModel::cleanupTestCase()
{
    m_daemon.terminate();
    m_daemon.waitForFinished(5000);
}

void TestModel::postedNotificationAppears()
{
    NotificationModel model;
    QSignalSpy counts(&model, &NotificationModel::countChanged);

    const uint id = post("hello", 0, {"default", "Open", "mute", "Mute"});
    QVERIFY(id != 0);
    QTRY_COMPARE(model.rowCount(), 1);
    QCOMPARE(model.count(), 1);
    QVERIFY(!counts.isEmpty());

    const QModelIndex row = model.index(0);
    QCOMPARE(model.data(row, NotificationModel::IdRole).toUInt(), id);
    QCOMPARE(model.data(row, NotificationModel::AppNameRole).toString(), "test");
    QCOMPARE(model.data(row, NotificationModel::AppIconRole).toString(), "dialog-information");
    QCOMPARE(model.data(row, NotificationModel::SummaryRole).toString(), "hello");
    QCOMPARE(model.data(row, NotificationModel::BodyRole).toString(), "body of hello");
    QCOMPARE(model.data(row, NotificationModel::DesktopEntryRole).toString(), "org.example.app");
    QCOMPARE(model.data(row, NotificationModel::ActionKeysRole).toStringList(), (QStringList{"default", "mute"}));
    QCOMPARE(model.data(row, NotificationModel::ActionLabelsRole).toStringList(), (QStringList{"Open", "Mute"}));
    QCOMPARE(model.data(row, NotificationModel::UrgencyRole).toInt(), 1);
    QCOMPARE(model.data(row, NotificationModel::ResidentRole).toBool(), false);

    model.dismiss(id);
    QTRY_COMPARE(model.rowCount(), 0);
}

void TestModel::replacementKeepsTheRow()
{
    NotificationModel model;

    const uint id = post("first");
    QTRY_COMPARE(model.rowCount(), 1);
    QCOMPARE(post("second", id), id);
    QTRY_COMPARE(model.data(model.index(0), NotificationModel::SummaryRole).toString(), "second");
    QCOMPARE(model.rowCount(), 1);

    model.dismiss(id);
    QTRY_COMPARE(model.rowCount(), 0);
}

void TestModel::newestComesFirst()
{
    NotificationModel model;

    const uint older = post("older");
    const uint newer = post("newer");
    QTRY_COMPARE(model.rowCount(), 2);
    QCOMPARE(model.data(model.index(0), NotificationModel::IdRole).toUInt(), newer);
    QCOMPARE(model.data(model.index(1), NotificationModel::IdRole).toUInt(), older);

    model.dismiss(older);
    model.dismiss(newer);
    QTRY_COMPARE(model.rowCount(), 0);
}

void TestModel::dismissRemovesTheRow()
{
    NotificationModel model;

    const uint id = post("gone");
    QTRY_COMPARE(model.rowCount(), 1);
    model.dismiss(id);
    QTRY_COMPARE(model.rowCount(), 0);
    QCOMPARE(model.count(), 0);
}

void TestModel::invokeRemovesTheRow()
{
    NotificationModel model;

    const uint id = post("act", 0, {"default", "Open"});
    QTRY_COMPARE(model.rowCount(), 1);
    model.invoke(id, "default");
    QTRY_COMPARE(model.rowCount(), 0);
}

void TestModel::loadsExistingNotifications()
{
    const uint first = post("one");
    const uint second = post("two");

    NotificationModel model;
    QTRY_COMPARE(model.rowCount(), 2);
    QCOMPARE(model.data(model.index(0), NotificationModel::IdRole).toUInt(), second);
    QCOMPARE(model.data(model.index(1), NotificationModel::IdRole).toUInt(), first);

    model.dismiss(first);
    model.dismiss(second);
    QTRY_COMPARE(model.rowCount(), 0);
}

QTEST_MAIN(TestModel)

#include "tst_model.moc"
