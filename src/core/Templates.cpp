#include "Templates.h"

namespace Templates {

namespace {

// Palette shared by all templates
const QColor kYellow(255, 249, 177);
const QColor kGreen(213, 246, 146);
const QColor kBlue(166, 204, 245);
const QColor kPink(240, 147, 157);
const QColor kOrange(255, 189, 120);
const QColor kPurple(214, 190, 245);
const QColor kInk(26, 26, 26);
const QColor kAccent(66, 98, 255);

class Builder
{
public:
    explicit Builder(const QString &title) { doc.title = title; }

    QString add(BoardItem::Type type, QRectF rect, const QString &text = {}, QColor fill = Qt::white,
                double fontSize = 16, bool atBack = false)
    {
        BoardItem it;
        it.id = BoardItem::newId();
        it.type = type;
        it.rect = rect;
        it.text = text;
        it.fill = fill;
        it.fontSize = fontSize;
        if (type == BoardItem::Type::Sticky || type == BoardItem::Type::Frame || type == BoardItem::Type::Text)
            it.strokeWidth = 0;
        if (type == BoardItem::Type::Text)
            it.fill = Qt::transparent;
        if (atBack)
            doc.items.prepend(it);
        else
            doc.items.append(it);
        return it.id;
    }
    QString frame(QRectF r, const QString &title, QColor fill = Qt::white)
    {
        return add(BoardItem::Type::Frame, r, title, fill, 16, true);
    }
    QString sticky(double x, double y, const QString &text, QColor fill = kYellow, double size = 180)
    {
        return add(BoardItem::Type::Sticky, QRectF(x, y, size, size), text, fill, 18);
    }
    QString text(double x, double y, double w, const QString &t, double fontSize = 24, bool bold = true)
    {
        const QString id = add(BoardItem::Type::Text, QRectF(x, y, w, fontSize * 1.6), t, Qt::transparent, fontSize);
        doc.items.last().bold = bold;
        return id;
    }
    QString shape(BoardItem::Type type, QRectF r, const QString &t, QColor fill = Qt::white)
    {
        return add(type, r, t, fill, 16);
    }
    void connect(const QString &from, const QString &to, const QString &label = {},
                 BoardItem::ConnectorStyle style = BoardItem::ConnectorStyle::Elbow)
    {
        BoardItem c;
        c.id = BoardItem::newId();
        c.type = BoardItem::Type::Connector;
        c.fromId = from;
        c.toId = to;
        c.text = label;
        c.connectorStyle = style;
        c.fill = Qt::transparent;
        doc.items.append(c);
    }
    void line(QPointF a, QPointF b, double width = 2, QColor color = kInk)
    {
        BoardItem c;
        c.id = BoardItem::newId();
        c.type = BoardItem::Type::Connector;
        c.fromPoint = a;
        c.toPoint = b;
        c.arrowEnd = false;
        c.strokeWidth = width;
        c.stroke = color;
        c.fill = Qt::transparent;
        doc.items.append(c);
    }

    Document doc;
};

Document flowchart()
{
    Builder b(QStringLiteral("Flowchart"));
    using T = BoardItem::Type;
    const QString start = b.shape(T::RoundedRect, QRectF(0, 0, 200, 80), QStringLiteral("Start"), kGreen);
    const QString step = b.shape(T::Rectangle, QRectF(0, 160, 200, 90), QStringLiteral("Do the first step"));
    const QString decide = b.shape(T::Diamond, QRectF(-10, 330, 220, 140), QStringLiteral("Is it done?"), kYellow);
    const QString fix = b.shape(T::Rectangle, QRectF(320, 355, 200, 90), QStringLiteral("Fix and retry"));
    const QString end = b.shape(T::RoundedRect, QRectF(0, 560, 200, 80), QStringLiteral("End"), kPink);
    b.connect(start, step);
    b.connect(step, decide);
    b.connect(decide, end, QStringLiteral("Yes"));
    b.connect(decide, fix, QStringLiteral("No"));
    b.connect(fix, step);
    b.text(-10, -90, 520, QStringLiteral("Process flow"), 32);
    return b.doc;
}

Document mindMap()
{
    Builder b(QStringLiteral("Mind map"));
    using T = BoardItem::Type;
    const QString center = b.shape(T::Ellipse, QRectF(-120, -70, 240, 140), QStringLiteral("Main idea"), kYellow);
    b.doc.items.last().fontSize = 22;
    b.doc.items.last().bold = true;
    const QStringList topics = { QStringLiteral("Goals"), QStringLiteral("People"), QStringLiteral("Timeline"),
                                 QStringLiteral("Risks"), QStringLiteral("Budget"), QStringLiteral("Ideas") };
    const QList<QColor> fills = { kGreen, kBlue, kOrange, kPink, kPurple, kGreen };
    const QList<QPointF> pos = { { -480, -250 }, { -480, -40 }, { -480, 170 }, { 300, -250 }, { 300, -40 }, { 300, 170 } };
    for (int i = 0; i < topics.size(); ++i) {
        const QString t = b.shape(T::RoundedRect, QRectF(pos[i], QSizeF(180, 70)), topics[i], fills[i]);
        b.connect(center, t, {}, BoardItem::ConnectorStyle::Straight);
        b.doc.items.last().arrowEnd = false;
    }
    return b.doc;
}

Document kanban()
{
    Builder b(QStringLiteral("Kanban board"));
    const QStringList cols = { QStringLiteral("To do"), QStringLiteral("In progress"), QStringLiteral("Done") };
    const QList<QColor> notes = { kYellow, kBlue, kGreen };
    for (int c = 0; c < cols.size(); ++c) {
        const double x = c * 460.0;
        b.frame(QRectF(x, 0, 420, 900), cols[c]);
        for (int r = 0; r < 3; ++r)
            b.sticky(x + 30, 30 + r * 210, c == 0 && r == 0 ? QStringLiteral("New task") : QString(), notes[c], 170);
    }
    return b.doc;
}

Document retrospective()
{
    Builder b(QStringLiteral("Retrospective"));
    const QStringList cols = { QStringLiteral("Went well"), QStringLiteral("To improve"), QStringLiteral("Action items") };
    const QList<QColor> notes = { kGreen, kPink, kBlue };
    for (int c = 0; c < cols.size(); ++c) {
        const double x = c * 520.0;
        b.frame(QRectF(x, 0, 480, 640), cols[c]);
        for (int r = 0; r < 2; ++r)
            for (int k = 0; k < 2; ++k)
                b.sticky(x + 30 + k * 220, 30 + r * 220, {}, notes[c], 190);
    }
    b.text(0, -110, 900, QStringLiteral("Sprint retrospective"), 32);
    return b.doc;
}

Document impactEffort()
{
    Builder b(QStringLiteral("Impact / effort matrix"));
    using T = BoardItem::Type;
    const double s = 420;
    b.frame(QRectF(0, 0, 2 * s, 2 * s), QStringLiteral("Impact / effort"));
    b.shape(T::Rectangle, QRectF(0, 0, s, s), QStringLiteral("Quick wins\n(high impact, low effort)"), QColor(232, 248, 220));
    b.shape(T::Rectangle, QRectF(s, 0, s, s), QStringLiteral("Big bets\n(high impact, high effort)"), QColor(225, 236, 252));
    b.shape(T::Rectangle, QRectF(0, s, s, s), QStringLiteral("Fill-ins\n(low impact, low effort)"), QColor(255, 248, 225));
    b.shape(T::Rectangle, QRectF(s, s, s, s), QStringLiteral("Money pits\n(low impact, high effort)"), QColor(252, 230, 232));
    for (BoardItem &it : b.doc.items)
        if (it.isShape()) { it.stroke = QColor(200, 204, 212); it.strokeWidth = 1; it.textColor = QColor(90, 96, 110); }
    b.text(-40, 2 * s + 20, 300, QStringLiteral("Effort →"), 20, false);
    b.text(-170, s - 20, 160, QStringLiteral("Impact ↑"), 20, false);
    return b.doc;
}

Document userStoryMap()
{
    Builder b(QStringLiteral("User story map"));
    using T = BoardItem::Type;
    const QStringList acts = { QStringLiteral("Discover"), QStringLiteral("Sign up"), QStringLiteral("Use"), QStringLiteral("Share") };
    for (int i = 0; i < acts.size(); ++i) {
        const double x = i * 240.0;
        b.shape(T::RoundedRect, QRectF(x, 0, 200, 80), acts[i], kBlue);
        for (int r = 0; r < 3; ++r)
            b.sticky(x + 10, 120 + r * 200, r == 0 ? QStringLiteral("User story") : QString(), kYellow, 180);
    }
    const double w = acts.size() * 240.0 - 40;
    b.line(QPointF(-40, 310), QPointF(w + 40, 310), 2, kAccent);
    b.text(w + 60, 290, 220, QStringLiteral("Release 1"), 18);
    b.line(QPointF(-40, 510), QPointF(w + 40, 510), 2, kAccent);
    b.text(w + 60, 490, 220, QStringLiteral("Release 2"), 18);
    b.text(0, -100, 700, QStringLiteral("User story map"), 32);
    return b.doc;
}

Document brainstorm()
{
    Builder b(QStringLiteral("Brainstorm"));
    b.text(0, -110, 900, QStringLiteral("What should we explore?"), 34);
    const QList<QColor> fills = { kYellow, kGreen, kBlue, kPink, kOrange, kPurple };
    for (int r = 0; r < 2; ++r)
        for (int c = 0; c < 4; ++c)
            b.sticky(c * 220, r * 220, {}, fills[(r * 4 + c) % fills.size()], 190);
    return b.doc;
}

Document weeklyPlan()
{
    Builder b(QStringLiteral("Weekly plan"));
    const QStringList days = { QStringLiteral("Monday"), QStringLiteral("Tuesday"), QStringLiteral("Wednesday"),
                               QStringLiteral("Thursday"), QStringLiteral("Friday") };
    for (int d = 0; d < days.size(); ++d) {
        b.frame(QRectF(d * 300.0, 0, 270, 700), days[d]);
        b.sticky(d * 300.0 + 30, 30, {}, kYellow, 210);
    }
    return b.doc;
}

Document swot()
{
    Builder b(QStringLiteral("SWOT analysis"));
    const QStringList names = { QStringLiteral("Strengths"), QStringLiteral("Weaknesses"),
                                QStringLiteral("Opportunities"), QStringLiteral("Threats") };
    const QList<QColor> fills = { QColor(232, 248, 220), QColor(252, 230, 232), QColor(225, 236, 252), QColor(255, 243, 220) };
    for (int i = 0; i < 4; ++i) {
        const double x = (i % 2) * 560.0, y = (i / 2) * 560.0;
        b.frame(QRectF(x, y, 520, 520), names[i], fills[i]);
        b.sticky(x + 30, y + 30, {}, kYellow, 170);
    }
    return b.doc;
}

} // namespace

QList<Info> list()
{
    return {
        { QStringLiteral("flowchart"), QStringLiteral("Flowchart"), QStringLiteral("Steps, decisions and outcomes"), QStringLiteral("Diagramming & mapping") },
        { QStringLiteral("mindmap"), QStringLiteral("Mind map"), QStringLiteral("Branch out from one central idea"), QStringLiteral("Ideation & brainstorming") },
        { QStringLiteral("kanban"), QStringLiteral("Kanban board"), QStringLiteral("To do, in progress, done"), QStringLiteral("Agile workflows") },
        { QStringLiteral("retro"), QStringLiteral("Retrospective"), QStringLiteral("What went well and what to improve"), QStringLiteral("Meetings & workshops") },
        { QStringLiteral("impact"), QStringLiteral("Impact / effort"), QStringLiteral("Prioritise ideas on a 2×2 matrix"), QStringLiteral("Strategy & planning") },
        { QStringLiteral("storymap"), QStringLiteral("User story map"), QStringLiteral("Activities, stories and releases"), QStringLiteral("Agile workflows") },
        { QStringLiteral("brainstorm"), QStringLiteral("Brainstorm"), QStringLiteral("A question and a wall of notes"), QStringLiteral("Ideation & brainstorming") },
        { QStringLiteral("weekly"), QStringLiteral("Weekly plan"), QStringLiteral("Monday to Friday columns"), QStringLiteral("Strategy & planning") },
        { QStringLiteral("swot"), QStringLiteral("SWOT analysis"), QStringLiteral("Strengths, weaknesses, opportunities, threats"), QStringLiteral("Strategy & planning") },
    };
}

Document create(const QString &id)
{
    if (id == QLatin1String("flowchart")) return flowchart();
    if (id == QLatin1String("mindmap")) return mindMap();
    if (id == QLatin1String("kanban")) return kanban();
    if (id == QLatin1String("retro")) return retrospective();
    if (id == QLatin1String("impact")) return impactEffort();
    if (id == QLatin1String("storymap")) return userStoryMap();
    if (id == QLatin1String("brainstorm")) return brainstorm();
    if (id == QLatin1String("weekly")) return weeklyPlan();
    if (id == QLatin1String("swot")) return swot();
    return {};
}

} // namespace Templates
