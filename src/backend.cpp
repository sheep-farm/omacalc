#include "backend.h"

#include <QClipboard>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QLocale>
#include <QRect>
#include <QSettings>
#include <QTextStream>
#include <QVariantMap>
#include <cmath>

namespace {
const auto windowGeometrySetting = QStringLiteral("window/geometry");

const QString plusSign = QStringLiteral("+");
const QString minusSign = QStringLiteral("−");
const QString multiplySign = QStringLiteral("×");
const QString divideSign = QStringLiteral("÷");

bool isOperator(const QString &token) {
    return token == plusSign || token == minusSign
        || token == multiplySign || token == divideSign;
}

// Digits are entered raw, so "5." and "-" can linger while typing. Seal them
// into plain numbers before they join the expression.
QString sealNumber(const QString &entry) {
    QString sealed = entry;
    if (sealed.endsWith(QLatin1Char('.')))
        sealed.chop(1);
    if (sealed.isEmpty() || sealed == QStringLiteral("-"))
        return QStringLiteral("0");
    return sealed;
}
}

Backend::Backend(QObject *parent) : QObject(parent) {
    loadOmarchyTheme();
    watchOmarchyTheme();
    connect(&m_themeWatcher, &QFileSystemWatcher::fileChanged, this, [this]() {
        loadOmarchyTheme();
        watchOmarchyTheme();
    });
    connect(&m_themeWatcher, &QFileSystemWatcher::directoryChanged, this, [this]() {
        loadOmarchyTheme();
        watchOmarchyTheme();
    });
}

QString Backend::expression() const {
    if (m_errored || m_justEvaluated)
        return m_evaluatedExpression;
    return m_tokens.join(QLatin1Char(' '));
}

QString Backend::display() const {
    if (m_errored)
        return QStringLiteral("Error");
    if (!m_entry.isEmpty())
        return m_entry;
    if (m_justEvaluated)
        return m_result;
    return currentValue();
}

// The number the calculator is "at" right now: the entry being typed, or the
// operand the last operator was applied to, or the fresh-start zero.
QString Backend::currentValue() const {
    if (!m_entry.isEmpty())
        return sealNumber(m_entry);
    for (int i = m_tokens.size() - 1; i >= 0; --i) {
        if (!isOperator(m_tokens.at(i)))
            return m_tokens.at(i);
    }
    return QStringLiteral("0");
}

void Backend::pressKey(const QString &key) {
    if (key.size() == 1 && key.at(0).isDigit()) {
        pressDigit(key);
    } else if (key == QStringLiteral(".")) {
        pressDecimal();
    } else if (key == QStringLiteral("+")) {
        pressOperator(plusSign);
    } else if (key == QStringLiteral("-") || key == minusSign) {
        pressOperator(minusSign);
    } else if (key == QStringLiteral("*") || key == multiplySign) {
        pressOperator(multiplySign);
    } else if (key == QStringLiteral("/") || key == divideSign) {
        pressOperator(divideSign);
    } else if (key == QStringLiteral("=")) {
        pressEquals();
    } else if (key == QStringLiteral("%")) {
        pressPercent();
    } else if (key == QStringLiteral("sign")) {
        pressToggleSign();
    } else if (key == QStringLiteral("backspace")) {
        pressBackspace();
    } else if (key == QStringLiteral("clear")) {
        pressClear();
    } else {
        return;
    }

    emit calculationChanged();
}

void Backend::pressDigit(const QString &digit) {
    if (m_errored)
        pressClear();
    if (m_justEvaluated) {
        // A digit after equals starts a new calculation rather than
        // appending to the result.
        pressClear();
    }

    if (m_entry == QStringLiteral("0"))
        m_entry = digit;
    else if (m_entry == QStringLiteral("-0"))
        m_entry = QStringLiteral("-") + digit;
    else if (m_entry.size() < 16)
        m_entry += digit;
}

void Backend::pressDecimal() {
    if (m_errored)
        pressClear();
    if (m_justEvaluated)
        pressClear();

    if (m_entry.isEmpty())
        m_entry = QStringLiteral("0.");
    else if (!m_entry.contains(QLatin1Char('.')))
        m_entry += QLatin1Char('.');
}

void Backend::pressOperator(const QString &pretty) {
    if (m_errored)
        return;

    if (m_justEvaluated)
        beginEditingAfterResult();

    if (!m_entry.isEmpty()) {
        m_tokens << sealNumber(m_entry) << pretty;
        m_entry.clear();
    } else if (m_tokens.isEmpty()) {
        m_tokens << QStringLiteral("0") << pretty;
    } else if (isOperator(m_tokens.last())) {
        m_tokens.last() = pretty;
    } else {
        m_tokens << pretty;
    }
}

void Backend::pressEquals() {
    if (m_errored || m_justEvaluated)
        return;

    QStringList finalTokens = m_tokens;
    if (!m_entry.isEmpty())
        finalTokens << sealNumber(m_entry);
    while (!finalTokens.isEmpty() && isOperator(finalTokens.last()))
        finalTokens.removeLast();
    if (finalTokens.isEmpty())
        return;

    m_evaluatedExpression = finalTokens.join(QLatin1Char(' '));
    bool ok = false;
    const double value = evaluateTokens(finalTokens, &ok);
    if (!ok) {
        m_errored = true;
    } else {
        m_result = formatNumber(value);
        m_justEvaluated = true;
    }
    m_tokens.clear();
    m_entry.clear();
}

void Backend::pressPercent() {
    if (m_errored)
        return;
    if (m_justEvaluated)
        beginEditingAfterResult();

    bool ok = false;
    const double value = QLocale::c().toDouble(currentValue(), &ok);
    if (!ok)
        return;
    m_entry = formatNumber(value / 100.0);
}

void Backend::pressToggleSign() {
    if (m_errored)
        return;
    if (m_justEvaluated)
        beginEditingAfterResult();

    if (m_entry.isEmpty())
        m_entry = currentValue();
    if (m_entry.startsWith(QLatin1Char('-')))
        m_entry.remove(0, 1);
    else
        m_entry.prepend(QLatin1Char('-'));
}

void Backend::pressBackspace() {
    if (m_errored) {
        pressClear();
        return;
    }
    if (m_justEvaluated)
        beginEditingAfterResult();

    m_entry.chop(1);
    if (m_entry == QStringLiteral("-"))
        m_entry.clear();
}

void Backend::pressClear() {
    m_tokens.clear();
    m_entry.clear();
    m_result.clear();
    m_evaluatedExpression.clear();
    m_justEvaluated = false;
    m_errored = false;
}

// Editing after equals picks up from the result, with the old expression
// cleared away so the new one grows from "42" rather than "42 × 3 + 7".
void Backend::beginEditingAfterResult() {
    m_entry = m_result;
    m_tokens.clear();
    m_result.clear();
    m_evaluatedExpression.clear();
    m_justEvaluated = false;
}

double Backend::evaluateTokens(const QStringList &tokens, bool *ok) {
    *ok = false;
    if (tokens.size() % 2 == 0)
        return 0;

    // First fold × and ÷ into their neighbors, then sum what remains, giving
    // multiplication its usual precedence over addition.
    QList<double> values;
    QStringList additiveOperators;

    bool numberOk = false;
    values << QLocale::c().toDouble(tokens.first(), &numberOk);
    if (!numberOk)
        return 0;

    for (int i = 1; i + 1 < tokens.size(); i += 2) {
        const QString &op = tokens.at(i);
        const double operand = QLocale::c().toDouble(tokens.at(i + 1), &numberOk);
        if (!numberOk || !isOperator(op))
            return 0;

        if (op == multiplySign) {
            values.last() *= operand;
        } else if (op == divideSign) {
            values.last() /= operand;
        } else {
            additiveOperators << op;
            values << operand;
        }
    }

    double total = values.first();
    for (int i = 0; i < additiveOperators.size(); ++i) {
        if (additiveOperators.at(i) == plusSign)
            total += values.at(i + 1);
        else
            total -= values.at(i + 1);
    }

    if (!std::isfinite(total))
        return 0;

    *ok = true;
    return total;
}

QString Backend::formatNumber(double value) {
    if (value == 0)
        value = 0;  // Collapse negative zero.

    // Thirteen significant digits keeps binary-float noise like
    // 0.1 + 0.2 = 0.30000000000000004 out of the display while leaving
    // room for serious arithmetic; very large and small magnitudes fall
    // back to scientific notation.
    return QString::number(value, 'g', 13);
}

void Backend::copyResult() const {
    if (QClipboard *clipboard = QGuiApplication::clipboard())
        clipboard->setText(display());
}

QVariantMap Backend::windowGeometry() const {
    const QSettings settings;
    const QRect geometry = settings.value(windowGeometrySetting).toRect();
    QVariantMap map;
    map.insert(QStringLiteral("x"), geometry.isValid() ? geometry.x() : -1);
    map.insert(QStringLiteral("y"), geometry.isValid() ? geometry.y() : -1);
    map.insert(QStringLiteral("width"), geometry.isValid() ? geometry.width() : 0);
    map.insert(QStringLiteral("height"), geometry.isValid() ? geometry.height() : 0);
    map.insert(QStringLiteral("maximized"),
               settings.value(QStringLiteral("window/maximized"), false).toBool());
    return map;
}

void Backend::saveWindowGeometry(int x, int y, int width, int height, bool maximized) {
    QSettings settings;
    settings.setValue(windowGeometrySetting, QRect(x, y, width, height));
    settings.setValue(QStringLiteral("window/maximized"), maximized);
}

void Backend::setDarkMode(bool darkMode) {
    if (m_darkMode == darkMode)
        return;

    m_darkMode = darkMode;
    loadOmarchyTheme();
    emit darkModeChanged();
}

void Backend::setTextScale(qreal textScale) {
    if (qFuzzyCompare(m_textScale, textScale))
        return;

    m_textScale = textScale;
    emit textScaleChanged();
}

void Backend::loadOmarchyTheme() {
    m_themeBackground = m_darkMode ? QStringLiteral("#101010") : QStringLiteral("#ffffff");
    m_themeForeground = m_darkMode ? QStringLiteral("#eeeeee") : QStringLiteral("#222324");
    m_themeAccent = m_darkMode ? QStringLiteral("#5584aa") : QStringLiteral("#2077b2");
    m_themeSelection = m_darkMode ? QStringLiteral("#186a9a") : QStringLiteral("#2077b2");

    const QString colorsPath = QDir::homePath()
        + QStringLiteral("/.local/state/omarchy/current/theme/colors.toml");
    QString themeMode;
    QFile file(colorsPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
                continue;

            const int equals = line.indexOf(QLatin1Char('='));
            if (equals < 0)
                continue;

            const QString key = line.left(equals).trimmed();
            QString value = line.mid(equals + 1).trimmed();
            if (value.size() >= 2
                    && ((value.front() == QLatin1Char('"') && value.back() == QLatin1Char('"'))
                        || (value.front() == QLatin1Char('\'') && value.back() == QLatin1Char('\''))))
                value = value.mid(1, value.size() - 2);

            if (key == QStringLiteral("mode"))
                themeMode = value;
            else if (key == QStringLiteral("background"))
                m_themeBackground = value;
            else if (key == QStringLiteral("foreground"))
                m_themeForeground = value;
            else if (key == QStringLiteral("accent"))
                m_themeAccent = value;
            else if (key == QStringLiteral("selection"))
                m_themeSelection = value;
        }
    }

    bool themeModeKnown = false;
    bool themeIsDark = m_darkMode;
    if (themeMode == QStringLiteral("dark")) {
        themeIsDark = true;
        themeModeKnown = true;
    } else if (themeMode == QStringLiteral("light")) {
        themeIsDark = false;
        themeModeKnown = true;
    } else {
        const QColor background(m_themeBackground);
        if (background.isValid()) {
            const double luminance = 0.299 * background.redF()
                + 0.587 * background.greenF() + 0.114 * background.blueF();
            themeIsDark = luminance < 0.5;
            themeModeKnown = true;
        }
    }
    if (themeModeKnown && themeIsDark != m_darkMode) {
        m_darkMode = themeIsDark;
        emit darkModeChanged();
    }

    emit themeColorsChanged();
}

void Backend::watchOmarchyTheme() {
    const QStringList watched = m_themeWatcher.files() + m_themeWatcher.directories();
    if (!watched.isEmpty())
        m_themeWatcher.removePaths(watched);

    const QString currentDir = QDir::homePath()
        + QStringLiteral("/.local/state/omarchy/current");
    const QString themeDir = currentDir + QStringLiteral("/theme");
    const QString colorsPath = themeDir + QStringLiteral("/colors.toml");

    if (QDir(currentDir).exists())
        m_themeWatcher.addPath(currentDir);
    if (QDir(themeDir).exists())
        m_themeWatcher.addPath(themeDir);
    if (QFile::exists(colorsPath))
        m_themeWatcher.addPath(colorsPath);
}
