#include "TSyntaxDefinition.h"

// ==================== تعريفات اللغة لاظهار الشرح  ====================

LanguageDefinition::LanguageDefinition() {
    QStringList keywords = {
        "و", "او", "ك", "متوقع", "مزامنة", "انتظر", "توقف", "استمر"
        "احذف", "اواذا", "والا", "خلل", "خطا", "نهاية", "لكل",
        "من", "عام", "اذا", "استورد", "في", "هل", "عدم",
        "نطاق", "ليس", "مرر", "ارجع", "صح", "حاول",
        "بينما", "عند", "ولد", "خطية", "لاجل", "طابق", "اظهر"
    };

    QStringList builtins = {
        "اطبع", "ادخل", "طول", "منطق", "فهرس", "هل_نوع", "صحيح", "عشري",
        "اقصى", "ادنى", "مصفوفة", "اجمع", "تحقق_اي", "مدى", "مميزة", "افتح",
        "مترابطة", "نوع", "اصل", "مقرون", "هذا", "قطعة", "نص"
    };

    QStringList magics = {
        "__تهيئة__", "__اس_ع__", "__عرض__", "__استدعاء__", "__اس__", "__سالب__",
        "__اضرب__", "__اطرح__", "__اجمع__", "__اجمع_ع__", "__اطرح_ع__", "__اضرب_ع__"
    };

    keywordSet = QSet<QString>(keywords.begin(), keywords.end());
    builtinSet = QSet<QString>(builtins.begin(), builtins.end());
    magicSet = QSet<QString>(magics.begin(), magics.end());

    hexPattern = QRegularExpression(R"(\b0[xX][0-9a-fA-F]+\b)");
    numberPattern = QRegularExpression(R"(\b\d+(\.\d+)?([eE][+-]?\d+)?\b)");
}
