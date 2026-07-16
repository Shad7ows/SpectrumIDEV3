#include "TLanguageProfile.h"

#include <QtTest>

class TLanguageRegistryTest : public QObject {
    Q_OBJECT

private slots:
    void detectsAlifFiles();
    void detectsBaaFiles();
    void fallsBackToPlainText();
    void findsProfilesById();
};

void TLanguageRegistryTest::detectsAlifFiles() {
    QCOMPARE(TLanguageRegistry::profileForFilePath("example.alif").id, QStringLiteral("alif"));
    QCOMPARE(TLanguageRegistry::profileForFilePath("library.ALIFLIB").id, QStringLiteral("alif"));
}

void TLanguageRegistryTest::detectsBaaFiles() {
    QCOMPARE(TLanguageRegistry::profileForFilePath("main.baa").id, QStringLiteral("baa"));
    QCOMPARE(TLanguageRegistry::profileForFilePath("include/api.BAAHD").id, QStringLiteral("baa"));
}

void TLanguageRegistryTest::fallsBackToPlainText() {
    QCOMPARE(TLanguageRegistry::profileForFilePath("").id, QStringLiteral("plaintext"));
    QCOMPARE(TLanguageRegistry::profileForFilePath("README.md").id, QStringLiteral("plaintext"));
}

void TLanguageRegistryTest::findsProfilesById() {
    const TLanguageProfile *baa = TLanguageRegistry::profileForId("BAA");
    QVERIFY(baa != nullptr);
    QCOMPARE(baa->displayName, QStringLiteral("باء"));

    QVERIFY(TLanguageRegistry::profileForId("unknown") == nullptr);
}

QTEST_APPLESS_MAIN(TLanguageRegistryTest)

#include "tst_TLanguageRegistry.moc"
