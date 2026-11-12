#include "BoardFile.h"
#include "Crypto.h"
#include "Exporter.h"
#include "PenTools.h"
#include "RelayServer.h"
#include "ShapeLibrary.h"
#include "SvgPath.h"
#include "SyncEngine.h"
#include "Templates.h"

#include <QBuffer>
#include <QPdfDocument>
#include <QGuiApplication>
#include <QImageReader>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTest>
#include <QWebSocket>

#include <private/qzipreader_p.h>
#include <private/qzipwriter_p.h>

class TestExport : public QObject
{
    Q_OBJECT

private:
    QByteArray m_photoPng;
    Document makeDoc()
    {
        Document d;
        d.title = QStringLiteral("Test board");
        auto add = [&](BoardItem::Type t, QRectF r, QString text = {}) {
            BoardItem it;
            it.id = BoardItem::newId();
            it.type = t;
            it.rect = r;
            it.text = text;
            d.items << it;
            return it.id;
        };
        BoardItem frame;
        frame.id = BoardItem::newId();
        frame.type = BoardItem::Type::Frame;
        frame.rect = QRectF(-50, -50, 1100, 700);
        frame.text = QStringLiteral("Sprint plan");
        d.items << frame;
        const QString a = add(BoardItem::Type::Sticky, QRectF(0, 0, 200, 200), QStringLiteral("Idea one"));
        d.items.last().fill = QColor(255, 249, 177);
        const QString b = add(BoardItem::Type::Ellipse, QRectF(400, 0, 160, 120), QStringLiteral("Decision"));
        add(BoardItem::Type::Diamond, QRectF(700, 0, 160, 120), QStringLiteral("Check"));
        add(BoardItem::Type::Text, QRectF(0, 300, 300, 40), QStringLiteral("Heading text"));
        BoardItem c;
        c.id = BoardItem::newId();
        c.type = BoardItem::Type::Connector;
        c.fromId = a;
        c.toId = b;
        d.items << c;
        BoardItem pen;
        pen.id = BoardItem::newId();
        pen.type = BoardItem::Type::Pen;
        pen.rect = QRectF(600, 300, 100, 50);
        pen.points = { {0, 0}, {30, 40}, {60, 10}, {100, 50} };
        pen.strokeWidth = 4;
        d.items << pen;

        // A noisy image so lossy re-encoding is detectable.
        QImage img(400, 300, QImage::Format_RGB32);
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                img.setPixel(x, y, qRgb((x * 7 + y * 3) % 256, (x * y) % 256, (x ^ y) % 256));
        QBuffer buf(&m_photoPng);
        buf.open(QIODevice::WriteOnly);
        img.save(&buf, "PNG");
        BoardItem im;
        im.id = BoardItem::newId();
        im.type = BoardItem::Type::Image;
        im.rect = QRectF(250, 350, 200, 150);
        im.assetKey = d.addAsset(m_photoPng);
        d.items << im;
        d.versions.append({ QDateTime::currentDateTime(), d.itemsToJson() });
        return d;
    }

    QList<Exporter::OutFile> run(const Document &d, Exporter::Compression c, Exporter::Scope scope = Exporter::Scope::WholeBoard)
    {
        Exporter::Options o;
        o.formats = Exporter::allFormats();
        o.compression = c;
        o.scope = scope;
        o.zip = true;
        QStringList warnings;
        const auto files = Exporter::render(d, o, warnings);
        for (const QString &w : warnings)
            qInfo() << "warning:" << w;
        return files;
    }

    static QByteArray fileFor(const QList<Exporter::OutFile> &files, const QString &suffix)
    {
        for (const auto &f : files)
            if (f.path.endsWith(suffix))
                return f.data;
        return {};
    }

private slots:
    void initTestCase() { QImageReader::setAllocationLimit(2048); }

    // Info stamps, links, groups, comments and the start view survive save / load;
    // comment pins are never exported.
    void miroFieldsRoundTrip()
    {
        Document d;
        BoardItem r;
        r.id = QStringLiteral("r1");
        r.rect = QRectF(0, 0, 100, 50);
        r.createdBy = QStringLiteral("ana@example.com");
        r.createdAt = 1762400000000;
        r.modifiedBy = QStringLiteral("ben@example.com");
        r.modifiedAt = 1762400500000;
        r.link = QStringLiteral("https://example.com");
        r.group = QStringLiteral("g1");
        d.items << r;
        BoardItem c;
        c.id = QStringLiteral("c1");
        c.type = BoardItem::Type::Comment;
        c.rect = QRectF(5000, 5000, 30, 30);
        c.text = QStringLiteral("Looks good");
        d.items << c;
        d.startView = QRectF(-10, -20, 800, 600);

        Document back;
        QVERIFY(BoardFile::load(BoardFile::save(d), back));
        QCOMPARE(back.items.size(), 2);
        const BoardItem &r2 = back.items[0];
        QCOMPARE(r2.createdBy, r.createdBy);
        QCOMPARE(r2.createdAt, r.createdAt);
        QCOMPARE(r2.modifiedBy, r.modifiedBy);
        QCOMPARE(r2.modifiedAt, r.modifiedAt);
        QCOMPARE(r2.link, r.link);
        QCOMPARE(r2.group, r.group);
        QCOMPARE(back.items[1].type, BoardItem::Type::Comment);
        QCOMPARE(back.items[1].text, c.text);
        QCOMPARE(back.startView, d.startView);

        // The far-away comment doesn't stretch the board, and isn't in exports.
        QCOMPARE(back.contentBounds(), back.itemBounds(r2));
        QVERIFY(back.itemAt(QPointF(5010, 5010), 1).isEmpty());
        Exporter::Options o;
        o.formats = { Exporter::Format::Svg };
        QStringList w;
        const QByteArray svg = fileFor(Exporter::render(back, o, w), QStringLiteral(".svg"));
        QVERIFY(!svg.isEmpty());
        QVERIFY(!svg.contains("Looks good"));
    }


    // ---------------------------------------------------------------- live collaboration
    static QByteArray itemsJson(const Document &d)
    {
        return QJsonDocument(d.itemsToJson()).toJson(QJsonDocument::Compact);
    }

    void syncConverges()
    {
        Document a = Templates::create("flowchart");
        QVERIFY(a.items.size() > 5);
        Document b;
        SyncEngine ea(QStringLiteral("alice")), eb(QStringLiteral("bob"));
        eb.adopt(b);
        QVERIFY(eb.apply(ea.diff(a), b));
        QCOMPARE(itemsJson(b), itemsJson(a));
        QCOMPARE(b.title, a.title);
        QVERIFY(ea.diff(a).isEmpty());                    // nothing new to send

        // Concurrent edits to different items both survive.
        a.items[0].rect.translate(100, 0);
        b.items[1].text = QStringLiteral("Edited by Bob");
        const QJsonArray fromA = ea.diff(a), fromB = eb.diff(b);
        QCOMPARE(fromA.size(), 1);
        QCOMPARE(fromB.size(), 1);
        eb.apply(fromA, b);
        ea.apply(fromB, a);
        QCOMPARE(itemsJson(a), itemsJson(b));
        QCOMPARE(a.items[1].text, QStringLiteral("Edited by Bob"));

        // Same item edited by both: one deterministic winner on both sides.
        a.items[2].text = QStringLiteral("A");
        b.items[2].text = QStringLiteral("B");
        const QJsonArray ca = ea.diff(a), cb = eb.diff(b);
        eb.apply(ca, b);
        ea.apply(cb, a);
        QCOMPARE(itemsJson(a), itemsJson(b));

        // Delete vs. move, z-order change and a new image asset.
        const QString victim = a.items[3].id;
        a.items.removeAt(3);
        b.items[4].rect.translate(0, 50);
        BoardItem front = b.items.takeAt(0);
        b.items.append(front);                            // bring to front
        BoardItem img;
        img.id = BoardItem::newId();
        img.type = BoardItem::Type::Image;
        img.rect = QRectF(0, 0, 10, 10);
        img.assetKey = b.addAsset(m_photoPng);
        b.items.append(img);
        const QJsonArray da = ea.diff(a), db = eb.diff(b);
        eb.apply(da, b);
        ea.apply(db, a);
        QCOMPARE(itemsJson(a), itemsJson(b));
        QVERIFY(!a.find(victim) && !b.find(victim));
        QCOMPARE(a.assets.value(img.assetKey).data, m_photoPng);   // original bytes travel intact

        // A late joiner rebuilds the same board from a snapshot.
        Document c;
        SyncEngine ec(QStringLiteral("carol"));
        ec.adopt(c);
        ec.apply(ea.snapshot(a), c);
        QCOMPARE(itemsJson(c), itemsJson(a));
    }

    void shapeAndIconLibraries()
    {
        const QPainterPath circle = SvgPath::parse(QStringLiteral("M12 3 a9 9 0 1 0 0.01 0"));
        const QRectF b = circle.boundingRect();
        QVERIFY(qAbs(b.width() - 18) < 0.5 && qAbs(b.height() - 18) < 0.5);
        QVERIFY(qAbs(b.center().x() - 12) < 0.5 && qAbs(b.center().y() - 12) < 0.5);

        QVERIFY(ShapeLibrary::shapes().size() >= 50);
        QVERIFY(ShapeLibrary::icons().size() >= 60);
        for (const auto &e : ShapeLibrary::shapes()) {
            const QRectF r = ShapeLibrary::shapePath(e.id, QRectF(0, 0, 100, 100)).boundingRect();
            QVERIFY2(!r.isEmpty(), qPrintable(e.id));
            QVERIFY2(QRectF(-1, -1, 102, 102).contains(r), qPrintable(e.id));
        }
        for (const auto &e : ShapeLibrary::icons()) {
            const QRectF r = ShapeLibrary::iconPath(e.id, QRectF(0, 0, 24, 24)).boundingRect();
            QVERIFY2(r.width() > 1 || r.height() > 1, qPrintable(e.id));
            // Edge checks (flat icons such as "minus" have zero height).
            QVERIFY2(r.left() >= -1 && r.top() >= -1 && r.right() <= 25 && r.bottom() <= 25, qPrintable(e.id));
        }
    }

    void smartDrawing()
    {
        auto poly = [](const QList<QPointF> &corners, int perSide) {
            QList<QPointF> pts;
            for (int i = 0; i < corners.size(); ++i)
                for (int k = 0; k < perSide; ++k)
                    pts << corners[i] + (corners[(i + 1) % corners.size()] - corners[i]) * (double(k) / perSide)
                               + QPointF((k % 3) - 1, ((k + 1) % 3) - 1) * 0.8;   // hand wobble
            pts << corners.first() + QPointF(2, 1);
            return pts;
        };
        using R = PenTools::Recognized;
        QCOMPARE(PenTools::recognize(poly({ {0, 0}, {200, 0}, {200, 120}, {0, 120} }, 20)).kind, R::Rectangle);
        QCOMPARE(PenTools::recognize(poly({ {100, 0}, {200, 170}, {0, 170} }, 25)).kind, R::Triangle);
        QCOMPARE(PenTools::recognize(poly({ {100, 0}, {200, 80}, {100, 160}, {0, 80} }, 20)).kind, R::Diamond);
        QList<QPointF> circle;
        for (int i = 0; i <= 60; ++i)
            circle << QPointF(100 + 90 * std::cos(i * M_PI / 30), 100 + 70 * std::sin(i * M_PI / 30));
        QCOMPARE(PenTools::recognize(circle).kind, R::Ellipse);
        QList<QPointF> line;
        for (int i = 0; i <= 30; ++i)
            line << QPointF(i * 10, i * 4 + (i % 2));
        QCOMPARE(PenTools::recognize(line).kind, R::Line);
        QList<QPointF> scribble;
        for (int i = 0; i <= 40; ++i)
            scribble << QPointF(i * 6, (i % 4) * 30);
        QCOMPARE(PenTools::recognize(scribble).kind, R::None);
    }

    void precisionEraser()
    {
        QList<QPointF> stroke;
        for (int i = 0; i <= 50; ++i)
            stroke << QPointF(i * 4, 0);
        const auto pieces = PenTools::eraseAround(stroke, QPointF(100, 0), 12);
        QCOMPARE(pieces.size(), 2);
        QVERIFY(pieces[0].last().x() < 90 && pieces[1].first().x() > 110);
        QVERIFY(PenTools::strokeHits(stroke, QPointF(50, 5), 6));
        QVERIFY(!PenTools::strokeHits(stroke, QPointF(50, 30), 6));
    }

    void layersHideAndSync()
    {
        Document d;
        d.title = QStringLiteral("Layers");
        d.layers.append({ QStringLiteral("L1"), QStringLiteral("Secret layer"), false, false });
        BoardItem visible;
        visible.id = BoardItem::newId();
        visible.type = BoardItem::Type::Sticky;
        visible.rect = QRectF(0, 0, 200, 200);
        visible.text = QStringLiteral("Shown note");
        BoardItem hidden = visible;
        hidden.id = BoardItem::newId();
        hidden.text = QStringLiteral("Hidden note");
        hidden.layer = QStringLiteral("L1");
        hidden.rect.translate(300, 0);
        d.items << hidden << visible;
        d.normalizeLayerOrder();
        QCOMPARE(d.items.first().text, QStringLiteral("Shown note"));   // default layer is drawn first

        Exporter::Options o;
        o.formats = { Exporter::Format::Svg, Exporter::Format::Csv };
        QStringList w;
        const auto files = Exporter::render(d, o, w);
        const QByteArray svg = fileFor(files, ".svg");
        QVERIFY(svg.contains("Shown note"));
        QVERIFY(!svg.contains("Hidden note"));
        QVERIFY(d.itemAt(QPointF(400, 100), 2).isEmpty());          // can't pick hidden items

        Document back;
        QVERIFY(BoardFile::load(BoardFile::save(d), back));
        QCOMPARE(back.layers.size(), 1);
        QVERIFY(!back.layers.first().visible);
        QCOMPARE(back.find(hidden.id)->layer, QStringLiteral("L1"));

        Document peer;
        SyncEngine ea(QStringLiteral("a")), eb(QStringLiteral("b"));
        eb.adopt(peer);
        eb.apply(ea.diff(d), peer);
        QCOMPARE(peer.layers.size(), 1);
        QCOMPARE(peer.layers.first().name, QStringLiteral("Secret layer"));
    }

    void sealedMessages()
    {
        const QByteArray key = Crypto::pbkdf2Sha256("secret", "blackmiro-room:abc", 10, 64);
        const QByteArray sealed = Crypto::seal(key, "hello board");
        QByteArray out;
        QVERIFY(Crypto::open(key, sealed, &out));
        QCOMPARE(out, QByteArray("hello board"));
        QByteArray bad = sealed;
        bad[20] = char(bad[20] ^ 1);
        QVERIFY(!Crypto::open(key, bad, &out));
        QVERIFY(!Crypto::open(Crypto::pbkdf2Sha256("other", "blackmiro-room:abc", 10, 64), sealed, &out));
    }

    void relayForwardsAndReplays()
    {
        RelayServer relay;
        QVERIFY(relay.listen(0));
        const QUrl url(QStringLiteral("ws://127.0.0.1:%1/r/room-one").arg(relay.port()));

        QWebSocket a, b;
        QSignalSpy aUp(&a, &QWebSocket::connected), bUp(&b, &QWebSocket::connected);
        a.open(url);
        b.open(url);
        QVERIFY(aUp.wait(3000) || aUp.count() == 1);
        QVERIFY(bUp.count() == 1 || bUp.wait(3000));

        QSignalSpy bGot(&b, &QWebSocket::binaryMessageReceived);
        const QByteArray ops = QByteArray(1, RelayFrame::Ops) + QByteArray("opaque-encrypted-ops");
        a.sendBinaryMessage(ops);
        QVERIFY(bGot.wait(3000));
        QCOMPARE(bGot.first().first().toByteArray(), ops);

        // Presence is forwarded but not kept; ops are replayed to late joiners.
        a.sendBinaryMessage(QByteArray(1, RelayFrame::Presence) + "cursor");
        QVERIFY(bGot.wait(3000));
        QWebSocket late;
        QSignalSpy lateGot(&late, &QWebSocket::binaryMessageReceived);
        late.open(url);
        QVERIFY(lateGot.wait(3000));
        QTRY_COMPARE_WITH_TIMEOUT(lateGot.count(), 1, 1000);
        QCOMPARE(lateGot.first().first().toByteArray(), ops);

        // Rooms are isolated.
        QWebSocket other;
        QSignalSpy otherGot(&other, &QWebSocket::binaryMessageReceived);
        QSignalSpy otherUp(&other, &QWebSocket::connected);
        other.open(QUrl(QStringLiteral("ws://127.0.0.1:%1/r/room-two").arg(relay.port())));
        QVERIFY(otherUp.wait(3000));
        a.sendBinaryMessage(ops);
        QVERIFY(bGot.wait(3000));
        QTest::qWait(200);
        QCOMPARE(otherGot.count(), 0);
    }

    void aesMatchesFips197()
    {
        const QByteArray pt = QByteArray::fromHex("00112233445566778899aabbccddeeff");
        unsigned char out[16];
        Crypto::Aes(QByteArray::fromHex("000102030405060708090a0b0c0d0e0f"))
            .encryptBlock(reinterpret_cast<const unsigned char *>(pt.constData()), out);
        QCOMPARE(QByteArray(reinterpret_cast<char *>(out), 16).toHex(), QByteArray("69c4e0d86a7b0430d8cdb78070b4c55a"));
        Crypto::Aes(QByteArray::fromHex("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"))
            .encryptBlock(reinterpret_cast<const unsigned char *>(pt.constData()), out);
        QCOMPARE(QByteArray(reinterpret_cast<char *>(out), 16).toHex(), QByteArray("8ea2b7ca516745bfeafc49904b496089"));
    }

    void pbkdf2MatchesRfcVectors()
    {
        QCOMPARE(Crypto::pbkdf2Sha256("password", "salt", 1, 32).toHex(),
                 QByteArray("120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b"));
        QCOMPARE(Crypto::pbkdf2Sha256("password", "salt", 2, 32).toHex(),
                 QByteArray("ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43"));
    }

    void privateBoardNeedsPassword()
    {
        Document d = makeDoc();
        d.isPrivate = true;
        d.owner = QStringLiteral("someone@example.com");
        const QByteArray enc = BoardFile::save(d, true, QStringLiteral("s3cret pass"));
        QVERIFY(BoardFile::isEncrypted(enc));
        QVERIFY(!enc.contains("Idea one"));                 // no plaintext inside
        QVERIFY(!enc.contains(m_photoPng.left(64)));

        Document out;
        QString err;
        QVERIFY(!BoardFile::load(enc, out, &err));           // no password
        QVERIFY(!BoardFile::load(enc, out, &err, QStringLiteral("wrong")));
        QCOMPARE(err, QStringLiteral("Wrong password."));
        QByteArray tampered = enc;
        tampered[tampered.size() / 2] = char(tampered[tampered.size() / 2] ^ 1);
        QVERIFY(!BoardFile::load(tampered, out, &err, QStringLiteral("s3cret pass")));

        QVERIFY2(BoardFile::load(enc, out, &err, QStringLiteral("s3cret pass")), qPrintable(err));
        QCOMPARE(out.itemsToJson(), d.itemsToJson());
        QCOMPARE(out.assets.cbegin()->data, m_photoPng);    // still byte-identical
        QVERIFY(out.isPrivate);
        QCOMPARE(out.owner, QStringLiteral("someone@example.com"));
    }

    void pdfExportWithPassword()
    {
        Exporter::Options o;
        o.formats = { Exporter::Format::Pdf };
        o.compression = Exporter::Compression::Low;
        o.password = QStringLiteral("open-me 123");
        QStringList warnings;
        const auto files = Exporter::render(makeDoc(), o, warnings);
        QCOMPARE(files.size(), 1);
        const QByteArray pdf = files.first().data;
        QVERIFY(pdf.contains("/Encrypt"));
        QVERIFY(pdf.contains("/AESV2"));
        QVERIFY(!pdf.contains("Test board"));                // title is encrypted too

        QTemporaryDir dir;
        const QString path = dir.filePath("locked.pdf");
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(pdf);
        f.close();

        QPdfDocument locked;
        QCOMPARE(locked.load(path), QPdfDocument::Error::IncorrectPassword);

        QPdfDocument wrong;
        wrong.setPassword(QStringLiteral("nope"));
        QCOMPARE(wrong.load(path), QPdfDocument::Error::IncorrectPassword);

        QPdfDocument doc;
        doc.setPassword(QStringLiteral("open-me 123"));
        QCOMPARE(doc.load(path), QPdfDocument::Error::None);
        QCOMPARE(doc.pageCount(), 1);
        QCOMPARE(doc.metaData(QPdfDocument::MetaDataField::Title).toString(), QStringLiteral("Test board"));
        // Content streams decrypt correctly: the page renders with the sticky note colour.
        const QImage page = doc.render(0, QSize(600, 400));
        QVERIFY(!page.isNull());
        bool foundYellow = false;
        for (int y = 0; y < page.height() && !foundYellow; y += 2)
            for (int x = 0; x < page.width(); x += 2) {
                const QColor c = page.pixelColor(x, y);
                if (c.red() > 240 && c.green() > 230 && c.blue() < 200) { foundYellow = true; break; }
            }
        QVERIFY(foundYellow);
    }

    void writeDemoBoard()
    {
        // Set BOARD_DEMO_OUT=path.board to keep a sample board for manual testing.
        const QString out = qEnvironmentVariable("BOARD_DEMO_OUT");
        if (out.isEmpty())
            QSKIP("BOARD_DEMO_OUT not set");
        QVERIFY(BoardFile::saveToFile(makeDoc(), out));
    }

    void boardRoundTripIsLossless()
    {
        const Document d = makeDoc();
        Document loaded;
        QString err;
        QVERIFY2(BoardFile::load(BoardFile::save(d), loaded, &err), qPrintable(err));
        QCOMPARE(loaded.items.size(), d.items.size());
        QCOMPARE(loaded.itemsToJson(), d.itemsToJson());
        QCOMPARE(loaded.assets.size(), 1);
        QCOMPARE(loaded.assets.cbegin()->data, m_photoPng); // byte-identical image
        QCOMPARE(loaded.versions.size(), 1);
    }

    void opensBoardsFromBeforeRename()
    {
        // Boards saved before the app was named Miru used format "infiniteboard".
        const Document d = makeDoc();
        QByteArray data = BoardFile::save(d);
        Document probe;
        QVERIFY(BoardFile::load(data, probe));
        QByteArray json;
        {
            QBuffer b(&data);
            b.open(QIODevice::ReadOnly);
            QZipReader zip(&b);
            json = zip.fileData(QStringLiteral("board.json"));
            QVERIFY(json.contains("\"format\": \"blackmiro\""));
        }
        json.replace("\"format\": \"blackmiro\"", "\"format\": \"infiniteboard\"");
        QByteArray legacy;
        {
            QBuffer b(&legacy);
            b.open(QIODevice::WriteOnly);
            QZipWriter zip(&b);
            zip.addFile(QStringLiteral("board.json"), json);
            zip.close();
        }
        Document old;
        QString err;
        QVERIFY2(BoardFile::load(legacy, old, &err), qPrintable(err));
        QCOMPARE(old.items.size(), d.items.size());
    }

    void lowCompressionIsLossless()
    {
        const Document d = makeDoc();
        const auto files = run(d, Exporter::Compression::Low);

        const QImage png = QImage::fromData(fileFor(files, ".png"));
        QVERIFY(!png.isNull());
        QCOMPARE(png.depth(), 64);                         // 16 bits per channel
        QVERIFY(qAbs(png.dotsPerMeterX() - qRound(600 / 0.0254)) < 2);

        const QImage tiff = QImage::fromData(fileFor(files, ".tiff"));
        QVERIFY(!tiff.isNull());
        QCOMPARE(tiff.depth(), 64);

        // Lossless WebP decodes to exactly the PNG pixels (8-bit).
        const QImage webp = QImage::fromData(fileFor(files, ".webp"));
        QVERIFY(!webp.isNull());
        QCOMPARE(webp.convertToFormat(QImage::Format_ARGB32), png.convertToFormat(QImage::Format_ARGB32));

        // SVG embeds the original file bytes.
        const QByteArray svg = fileFor(files, ".svg");
        QVERIFY(svg.contains(m_photoPng.toBase64()));
        QVERIFY(svg.contains("<tspan"));                   // text stays text

        // PDF: lossless images use Flate, never DCT (JPEG).
        const QByteArray pdf = fileFor(files, ".pdf");
        QVERIFY(pdf.startsWith("%PDF"));
        QVERIFY(!pdf.contains("/DCTDecode"));

        Document back;
        QVERIFY(BoardFile::load(fileFor(files, ".board"), back));
        QCOMPARE(back.assets.cbegin()->data, m_photoPng);
        QCOMPARE(back.versions.size(), 1);

        QVERIFY(fileFor(files, ".csv").contains("Idea one"));
        QVERIFY(fileFor(files, ".md").contains("## Sprint plan"));
        QVERIFY(fileFor(files, ".md").contains("Idea one → Decision"));
    }

    void compressionLevelsShrinkFiles()
    {
        const Document d = makeDoc();
        const auto low = run(d, Exporter::Compression::Low);
        const auto med = run(d, Exporter::Compression::Medium);
        const auto high = run(d, Exporter::Compression::High);
        for (const char *ext : { ".png", ".jpg", ".webp", ".tiff", ".pdf" }) {
            const qsizetype l = fileFor(low, ext).size();
            const qsizetype m = fileFor(med, ext).size();
            const qsizetype h = fileFor(high, ext).size();
            qInfo().noquote() << QStringLiteral("%1  low %2 KB  medium %3 KB  high %4 KB")
                                     .arg(QLatin1String(ext), 6).arg(l / 1024).arg(m / 1024).arg(h / 1024);
            QVERIFY2(l > 0 && m > 0 && h > 0, ext);
            QVERIFY2(l >= m && m >= h, ext);
        }
        QVERIFY(fileFor(med, ".pdf").contains("/DCTDecode"));   // medium: JPEG images in PDF
        Document hb;
        QVERIFY(BoardFile::load(fileFor(high, ".board"), hb));
        QCOMPARE(hb.versions.size(), 0);                        // high strips history
        const QImage hp = QImage::fromData(fileFor(high, ".png"));
        QCOMPARE(hp.format(), QImage::Format_Indexed8);         // high: 256-colour palette
    }

    void eachFrameMakesPages()
    {
        Document d = makeDoc();
        BoardItem f2;
        f2.id = BoardItem::newId();
        f2.type = BoardItem::Type::Frame;
        f2.rect = QRectF(2000, 0, 400, 300);
        f2.text = QStringLiteral("Second");
        d.items.prepend(f2);
        const auto files = run(d, Exporter::Compression::Medium, Exporter::Scope::EachFrame);
        int pngs = 0;
        for (const auto &f : files)
            if (f.path.endsWith(".png"))
                ++pngs;
        QCOMPARE(pngs, 2);
        QCOMPARE(fileFor(files, ".pdf").count("/Type /Page\n") + fileFor(files, ".pdf").count("/Type /Page "), 2);
    }

    void writesZip()
    {
        QTemporaryDir dir;
        Exporter::Options o;
        o.formats = Exporter::allFormats();
        o.zip = true;
        o.outputDir = dir.path();
        o.compression = Exporter::Compression::Medium;
        const auto r = Exporter::exportDocument(makeDoc(), o);
        QVERIFY2(r.ok(), qPrintable(r.error));
        QCOMPARE(r.files.size(), 1);
        QZipReader zip(r.files.first());
        QStringList names;
        for (const auto &fi : zip.fileInfoList())
            names << fi.filePath;
        QVERIFY(names.contains("PNG/board.png"));
        QVERIFY(names.contains("PDF/board.pdf"));
        QVERIFY(names.contains("BOARD/board.board"));
    }
};

QTEST_MAIN(TestExport)
#include "tst_export.moc"
