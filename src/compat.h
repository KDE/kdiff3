// clang-format off
/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2023 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#ifndef COMPAT_H
#define COMPAT_H

/*
    KF5I18n rightfully complains if included in the auto-test builds as they aren't actually setup
    for translations.
*/
#ifndef AUTOTEST

#include <KLocalizedString>
#include <KMessageBox>
#else
#define i18n(expr, ...) expr
#define i18nc(c, expr, ...) expr
#endif
#define KF_VERSION_CHECK(major, minor, patch) ((major << 16) | (minor << 8) | (patch))
#define KF_VERSION KF_VERSION_CHECK(KF_VERSION_MAJOR, KF_VERSION_MINOR, KF_VERSION_PATCH)

#ifndef AUTOTEST
namespace Compat {

inline KMessageBox::ButtonCode warningTwoActionsCancel(QWidget *parent,
                                                       const QString &text,
                                                       const QString &title,
                                                       const KGuiItem &primaryAction,
                                                       const KGuiItem &secondaryAction,
                                                       const KGuiItem &cancelAction = KStandardGuiItem::cancel(),
                                                       const QString &dontAskAgainName = QString(),
                                                       KMessageBox::Options options = KMessageBox::Options(KMessageBox::Notify | KMessageBox::Dangerous))
{
    return KMessageBox::warningTwoActionsCancel(parent,
                                                text,
                                                title,
                                                primaryAction,
                                                secondaryAction,
                                                cancelAction,
                                                dontAskAgainName,
                                                options);
}

inline KMessageBox::ButtonCode warningTwoActions(QWidget *parent,
                                                 const QString &text,
                                                 const QString &title,
                                                 const KGuiItem &primaryAction,
                                                 const KGuiItem &secondaryAction,
                                                 const QString &dontAskAgainName = QString(),
                                                 KMessageBox::Options options = KMessageBox::Options(KMessageBox::Notify | KMessageBox::Dangerous))
{
    return KMessageBox::warningTwoActions(parent,
                                          text,
                                          title,
                                          primaryAction,
                                          secondaryAction,
                                          dontAskAgainName,
                                          options);
}

constexpr KMessageBox::ButtonCode PrimaryAction = KMessageBox::PrimaryAction;
constexpr KMessageBox::ButtonCode SecondaryAction = KMessageBox::SecondaryAction;

} //namespace Compat
#endif // ndef AUTOTEST

#endif /* COMPAT_H */
