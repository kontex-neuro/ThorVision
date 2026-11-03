#pragma once

#include <QString>

// Camera display names are not filesystem names. In particular, ':' opens an
// NTFS alternate data stream instead of creating a visible recording file.
inline QString recording_filename_component(QString name)
{
    const QString reserved = QStringLiteral("<>:\"/\\|?*");
    for (auto &character : name) {
        if (character.unicode() < 32 || reserved.contains(character)) character = QLatin1Char('_');
    }
    while (name.endsWith(QLatin1Char(' ')) || name.endsWith(QLatin1Char('.'))) name.chop(1);
    if (name.isEmpty()) return QStringLiteral("Camera");

    const auto stem = name.section(QLatin1Char('.'), 0, 0).toUpper();
    const bool numbered_device = stem.size() == 4 &&
        (stem.startsWith(QStringLiteral("COM")) || stem.startsWith(QStringLiteral("LPT"))) &&
        stem.at(3) >= QLatin1Char('1') && stem.at(3) <= QLatin1Char('9');
    if (stem == QStringLiteral("CON") || stem == QStringLiteral("PRN") ||
        stem == QStringLiteral("AUX") || stem == QStringLiteral("NUL") || numbered_device) {
        name.prepend(QLatin1Char('_'));
    }
    return name;
}
