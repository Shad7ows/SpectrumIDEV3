#include "TLanguageProfile.h"

#include <QFileInfo>

bool TLanguageProfile::matchesFilePath(const QString &filePath) const {
    if (filePath.trimmed().isEmpty()) {
        return false;
    }

    const QString fileName = QFileInfo(filePath).fileName();
    for (QString extension : fileExtensions) {
        if (!extension.startsWith('.')) {
            extension.prepend('.');
        }

        if (fileName.endsWith(extension, Qt::CaseInsensitive)) {
            return true;
        }
    }

    return false;
}

const QList<TLanguageProfile>& TLanguageRegistry::profiles() {
    static const QList<TLanguageProfile> languageProfiles = {
        {
            QStringLiteral("alif"),
            QStringLiteral("ألف"),
            {QStringLiteral(".alif"), QStringLiteral(".aliflib")}
        },
        {
            QStringLiteral("baa"),
            QStringLiteral("باء"),
            {QStringLiteral(".baa"), QStringLiteral(".baahd")}
        }
    };

    return languageProfiles;
}

const TLanguageProfile& TLanguageRegistry::profileForFilePath(const QString &filePath) {
    for (const TLanguageProfile &profile : profiles()) {
        if (profile.matchesFilePath(filePath)) {
            return profile;
        }
    }

    return plainTextProfile();
}

const TLanguageProfile* TLanguageRegistry::profileForId(const QString &languageId) {
    if (languageId.compare(plainTextProfile().id, Qt::CaseInsensitive) == 0) {
        return &plainTextProfile();
    }

    for (const TLanguageProfile &profile : profiles()) {
        if (languageId.compare(profile.id, Qt::CaseInsensitive) == 0) {
            return &profile;
        }
    }

    return nullptr;
}

const TLanguageProfile& TLanguageRegistry::plainTextProfile() {
    static const TLanguageProfile profile = {
        QStringLiteral("plaintext"),
        QStringLiteral("نص عادي"),
        {}
    };

    return profile;
}
