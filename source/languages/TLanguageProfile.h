#pragma once

#include <QList>
#include <QString>
#include <QStringList>

struct TLanguageProfile {
    QString id;
    QString displayName;
    QStringList fileExtensions;

    bool matchesFilePath(const QString &filePath) const;
};

class TLanguageRegistry final {
public:
    static const QList<TLanguageProfile>& profiles();
    static const TLanguageProfile& profileForFilePath(const QString &filePath);
    static const TLanguageProfile* profileForId(const QString &languageId);
    static const TLanguageProfile& plainTextProfile();
};
