#include "WebStorage.h"

#ifdef Q_OS_WASM

#include <QCoreApplication>
#include <QHash>
#include <QTimer>

#include <emscripten/bind.h>
#include <emscripten/val.h>

using emscripten::val;

namespace {
QHash<int, std::function<void(val)>> &pending()
{
    static QHash<int, std::function<void(val)>> p;
    return p;
}

int track(std::function<void(val)> cb)
{
    static int next = 0;
    pending().insert(++next, std::move(cb));
    return next;
}

// JS -> C++: every miruWeb call answers through Module.miruWebResult(id, value).
void miruWebResult(int id, val result)
{
    auto cb = pending().take(id);
    if (cb)
        QTimer::singleShot(0, qApp, [cb, result]() { cb(result); });
}

val api() { return val::global("miruWeb"); }
val callback() { return val::module_property("miruWebResult"); }

// Zero-copy view of the bytes; miruWeb copies it before anything asynchronous happens.
val view(const QByteArray &data)
{
    return val(emscripten::typed_memory_view(size_t(data.size()), reinterpret_cast<const unsigned char *>(data.constData())));
}

QByteArray bytes(const val &array)
{
    if (array.isNull() || array.isUndefined())
        return {};
    const int n = array["length"].as<int>();
    QByteArray out(n, Qt::Uninitialized);
    val(emscripten::typed_memory_view(size_t(n), reinterpret_cast<unsigned char *>(out.data()))).call<void>("set", array);
    return out;
}

QString str(const val &v) { return v.isString() ? QString::fromStdString(v.as<std::string>()) : QString(); }
std::string s(const QString &q) { return q.toStdString(); }
} // namespace

EMSCRIPTEN_BINDINGS(miru_web_storage)
{
    emscripten::function("miruWebResult", &miruWebResult);
}

namespace WebStorage {

bool canPickFile()
{
    return !api().isUndefined() && api()["canPickFile"].as<bool>();
}

void save(const QString &uid, const QString &title, const QByteArray &data, std::function<void(QString)> done)
{
    if (api().isUndefined())
        return done(QString());
    const int id = track([done](val r) { done(str(r)); });
    api().call<void>("save", id, s(uid), s(title), view(data), callback());
}

void saveToFile(const QString &uid, const QString &title, const QString &suggestedName, const QByteArray &data,
                bool asNew, std::function<void(bool, QString)> done)
{
    if (api().isUndefined())
        return done(false, QString());
    const int id = track([done](val r) { done(!r.isNull(), str(r)); });
    api().call<void>("saveToFile", id, s(uid), s(title), s(suggestedName), view(data), asNew, callback());
}

void openFile(std::function<void(bool, QString, QByteArray)> done)
{
    if (api().isUndefined())
        return done(false, QString(), QByteArray());
    const int id = track([done](val r) {
        if (r.isNull() || r.isUndefined())
            return done(false, QString(), QByteArray());
        if (r.isString())
            return done(true, QString(), QByteArray());   // cancelled
        done(true, str(r["name"]), bytes(r["bytes"]));
    });
    api().call<void>("openFile", id, callback());
}

void linkOpened(const QString &uid, const QString &title)
{
    if (!api().isUndefined())
        api().call<void>("linkOpened", s(uid), s(title));
}

void load(const QString &uid, std::function<void(QByteArray)> done)
{
    if (api().isUndefined())
        return done(QByteArray());
    const int id = track([done](val r) { done(bytes(r)); });
    api().call<void>("load", id, s(uid), callback());
}

void remove(const QString &uid)
{
    if (!api().isUndefined())
        api().call<void>("remove", s(uid));
}

} // namespace WebStorage

#endif
