/*
    Copyright © 2014-2018 by The qTox Project Contributors

    This file is part of qTox, a Qt-based graphical interface for Tox.

    qTox is libre software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    qTox is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with qTox.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "style.h"
#include "src/persistence/settings.h"
#include "src/widget/gui.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFontInfo>
#include <QMap>
#include <QPainter>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringBuilder>
#include <QStyle>
#include <QSvgRenderer>
#include <QWidget>

/**
 * @enum Style::Font
 *
 * @var ExtraBig
 * @brief [SystemDefault + 2]px, bold
 *
 * @var Big
 * @brief [SystemDefault]px
 *
 * @var BigBold
 * @brief [SystemDefault]px, bold
 *
 * @var Medium
 * @brief [SystemDefault - 1]px
 *
 * @var MediumBold
 * @brief [SystemDefault - 1]px, bold
 *
 * @var Small
 * @brief [SystemDefault - 2]px
 *
 * @var SmallLight
 * @brief [SystemDefault - 2]px, light
 *
 * @var BuiltinThemePath
 * @brief Path to the theme built into the application binary
 */

namespace {
    const QLatin1Literal ThemeSubFolder{"themes/"};
    const QLatin1Literal BuiltinThemePath{":themes/default/"};
}

// helper functions
QFont appFont(int pixelSize, int weight)
{
    QFont font;
    font.setPixelSize(pixelSize);
    font.setWeight(weight);
    return font;
}

QString qssifyFont(QFont font)
{
    return QString("%1 %2px \"%3\"").arg(font.weight() * 8).arg(font.pixelSize()).arg(font.family());
}

// colors as defined in
// https://github.com/ItsDuke/Tox-UI/blob/master/UI%20GUIDELINES.md
static QColor palette[] = {
    QColor("#6bc260"), QColor("#cebf44"), QColor("#c84e4e"), QColor("#000000"), QColor("#1c1c1c"),
    QColor("#414141"), QColor("#414141").lighter(120), QColor("#d1d1d1"), QColor("#ffffff"),
    QColor("#ff7700"),

    // Theme colors
    QColor("#1c1c1c"), QColor("#2a2a2a"), QColor("#414141"), QColor("#4e4e4e"),
};

static QMap<QString, QString> dict;

QStringList Style::getThemeColorNames()
{
    return {QObject::tr("Default"), QObject::tr("Blue"), QObject::tr("Olive"), QObject::tr("Red"),
            QObject::tr("Violet")};
}

QString Style::getThemeName()
{
    //TODO: return name of the current theme
    const QString themeName = "default";
    return QStringLiteral("default");
}

QString Style::getThemeFolder()
{
    const QString themeName = getThemeName();
    const QString themeFolder = ThemeSubFolder % themeName;
    const QString fullPath = QStandardPaths::locate(QStandardPaths::AppDataLocation,
                                  themeFolder, QStandardPaths::LocateDirectory);

    // No themes available, fallback to builtin
    if(fullPath.isEmpty()) {
        return BuiltinThemePath;
    }

    return fullPath % QDir::separator();
}

QList<QColor> Style::themeColorColors = {QColor(), QColor("#004aa4"), QColor("#97ba00"),
                                         QColor("#c23716"), QColor("#4617b5")};

// stylesheet filename, font -> stylesheet
// QString implicit sharing deduplicates stylesheets rather than constructing a new one each time
std::map<std::pair<const QString, const QFont>, const QString> Style::stylesheetsCache;

const QString Style::getStylesheet(const QString& filename, const QFont& baseFont)
{
    const QString fullPath = getThemeFolder() + filename;
    const std::pair<const QString, const QFont> cacheKey(fullPath, baseFont);
    auto it = stylesheetsCache.find(cacheKey);
    if (it != stylesheetsCache.end())
    {
        // cache hit
        return it->second;
    }
    // cache miss, new styleSheet, read it from file and add to cache
    const QString newStylesheet = resolve(filename, baseFont);
    stylesheetsCache.insert(std::make_pair(cacheKey, newStylesheet));
    return newStylesheet;
}

static QStringList existingImagesCache;
const QString Style::getImagePath(const QString& filename)
{
    QString fullPath = getThemeFolder() + filename;

    // search for image in cache
    if (existingImagesCache.contains(fullPath)) {
        return fullPath;
    }

    // if not in cache
    if (QFileInfo::exists(fullPath)) {
        existingImagesCache << fullPath;
        return fullPath;
    } else {
        qWarning() << "Failed to open file (using defaults):" << fullPath;

        fullPath = BuiltinThemePath % filename;

        if (QFileInfo::exists(fullPath)) {
            return fullPath;
        } else {
            qWarning() << "Failed to open default file:" << fullPath;
            return {};
        }
    }
}

QColor Style::getColor(Style::ColorPalette entry)
{
    return palette[entry];
}

QFont Style::getFont(Style::Font font)
{
    // fonts as defined in
    // https://github.com/ItsDuke/Tox-UI/blob/master/UI%20GUIDELINES.md

    static int defSize = QFontInfo(QFont()).pixelSize();

    static QFont fonts[] = {
        appFont(defSize + 3, QFont::Bold),   // extra big
        appFont(defSize + 1, QFont::Normal), // big
        appFont(defSize + 1, QFont::Bold),   // big bold
        appFont(defSize, QFont::Normal),     // medium
        appFont(defSize, QFont::Bold),       // medium bold
        appFont(defSize - 1, QFont::Normal), // small
        appFont(defSize - 1, QFont::Light),  // small light
    };

    return fonts[font];
}

const QString Style::resolve(const QString& filename, const QFont& baseFont)
{
    QString themePath = getThemeFolder();
    QString fullPath = themePath + filename;
    QString qss;

    QFile file{fullPath};
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        qss = file.readAll();
    } else {
        qWarning() << "Failed to open file (using defaults):" << fullPath;

        fullPath = BuiltinThemePath;
        QFile file{fullPath};

        if (file.open(QFile::ReadOnly | QFile::Text)) {
            qss = file.readAll();
        } else {
            qWarning() << "Failed to open default file:" << fullPath;
            return {};
        }
    }

    if (dict.isEmpty()) {
        dict = {// colors
                {"@green", Style::getColor(Style::Green).name()},
                {"@yellow", Style::getColor(Style::Yellow).name()},
                {"@red", Style::getColor(Style::Red).name()},
                {"@black", Style::getColor(Style::Black).name()},
                {"@darkGrey", Style::getColor(Style::DarkGrey).name()},
                {"@mediumGrey", Style::getColor(Style::MediumGrey).name()},
                {"@mediumGreyLight", Style::getColor(Style::MediumGreyLight).name()},
                {"@lightGrey", Style::getColor(Style::LightGrey).name()},
                {"@white", Style::getColor(Style::White).name()},
                {"@orange", Style::getColor(Style::Orange).name()},
                {"@themeDark", Style::getColor(Style::ThemeDark).name()},
                {"@themeMediumDark", Style::getColor(Style::ThemeMediumDark).name()},
                {"@themeMedium", Style::getColor(Style::ThemeMedium).name()},
                {"@themeLight", Style::getColor(Style::ThemeLight).name()},

                // fonts
                {"@baseFont",
                 QString::fromUtf8("'%1' %2px").arg(baseFont.family()).arg(QFontInfo(baseFont).pixelSize())},
                {"@extraBig", qssifyFont(Style::getFont(Style::ExtraBig))},
                {"@big", qssifyFont(Style::getFont(Style::Big))},
                {"@bigBold", qssifyFont(Style::getFont(Style::BigBold))},
                {"@medium", qssifyFont(Style::getFont(Style::Medium))},
                {"@mediumBold", qssifyFont(Style::getFont(Style::MediumBold))},
                {"@small", qssifyFont(Style::getFont(Style::Small))},
                {"@smallLight", qssifyFont(Style::getFont(Style::SmallLight))}};
    }

    for (const QString& key : dict.keys()) {
        qss.replace(QRegularExpression(key % QLatin1Literal{"\\b"}), dict[key]);
    }

    // @getImagePath() function
    const QRegularExpression re{QStringLiteral(R"(@getImagePath\([^)\s]*\))")};
    QRegularExpressionMatchIterator i = re.globalMatch(qss);

    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        QString path = match.captured(0);
        const QString phrase = path;

        path.remove(QStringLiteral("@getImagePath("));
        path.chop(1);

        QString fullImagePath = getThemeFolder() + path;
        // image not in cache
        if (!existingImagesCache.contains(fullPath)) {
            if (QFileInfo::exists(fullImagePath)) {
                existingImagesCache << fullImagePath;
            } else {
                qWarning() << "Failed to open file (using defaults):" << fullImagePath;
                fullImagePath = BuiltinThemePath % path;
            }
        }

        qss.replace(phrase, fullImagePath);
    }

    return qss;
}

void Style::repolish(QWidget* w)
{
    w->style()->unpolish(w);
    w->style()->polish(w);

    for (QObject* o : w->children()) {
        QWidget* c = qobject_cast<QWidget*>(o);
        if (c) {
            c->style()->unpolish(c);
            c->style()->polish(c);
        }
    }
}

void Style::setThemeColor(int color)
{
    stylesheetsCache.clear(); // clear stylesheet cache which includes color info
    if (color < 0 || color >= themeColorColors.size())
        setThemeColor(QColor());
    else
        setThemeColor(themeColorColors[color]);
}

/**
 * @brief Set theme color.
 * @param color Color to set.
 *
 * Pass an invalid QColor to reset to defaults.
 */
void Style::setThemeColor(const QColor& color)
{
    if (!color.isValid()) {
        // Reset to default
        palette[ThemeDark] = QColor("#1c1c1c");
        palette[ThemeMediumDark] = QColor("#2a2a2a");
        palette[ThemeMedium] = QColor("#414141");
        palette[ThemeLight] = QColor("#4e4e4e");
    } else {
        palette[ThemeDark] = color.darker(155);
        palette[ThemeMediumDark] = color.darker(135);
        palette[ThemeMedium] = color.darker(120);
        palette[ThemeLight] = color.lighter(110);
    }

    dict["@themeDark"] = getColor(ThemeDark).name();
    dict["@themeMediumDark"] = getColor(ThemeMediumDark).name();
    dict["@themeMedium"] = getColor(ThemeMedium).name();
    dict["@themeLight"] = getColor(ThemeLight).name();
}

/**
 * @brief Reloads some CCS
 */
void Style::applyTheme()
{
    GUI::reloadTheme();
}

QPixmap Style::scaleSvgImage(const QString& path, uint32_t width, uint32_t height)
{
    QSvgRenderer render(path);
    QPixmap pixmap(width, height);
    pixmap.fill(QColor(0, 0, 0, 0));
    QPainter painter(&pixmap);
    render.render(&painter, pixmap.rect());
    return pixmap;
}
