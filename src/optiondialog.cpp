/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "optiondialog.h"
#include "OptionItems.h"
#include "ui_scroller.h"

#include "common.h"
#include "defmac.h"
#include "smalldialogs.h"
#include "TypeUtils.h"

#include <map>

#include <KColorButton>
#include <KHelpClient>
#include <KLocalizedString>
#include <KMessageBox>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QFontDialog>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QTextCodec>

class OptionCheckBox: public QCheckBox, public OptionBool
{
  public:
    OptionCheckBox(const QString& text, bool bDefaultVal, const QString& saveName, bool* pbVar,
                   QWidget* pParent):
        QCheckBox(text, pParent),
        OptionBool(pbVar, bDefaultVal, saveName)
    {
    }
    void setToDefault() override { setChecked(getDefault()); }
    void setToCurrent() override { setChecked(getCurrent()); }

    using OptionBool::apply;
    void apply() override { apply(isChecked()); }

  private:
    Q_DISABLE_COPY(OptionCheckBox)
};

class OptionRadioButton: public QRadioButton, public OptionBool
{
  public:
    OptionRadioButton(const QString& text, bool bDefaultVal, const QString& saveName, bool* pbVar,
                      QWidget* pParent):
        QRadioButton(text, pParent),
        OptionBool(pbVar, bDefaultVal, saveName)
    {
    }

    void setToDefault() override { setChecked(getDefault()); }
    void setToCurrent() override { setChecked(getCurrent()); }

    using OptionBool::apply;
    void apply() override { apply(isChecked()); }

  private:
    Q_DISABLE_COPY(OptionRadioButton)
};

FontChooser::FontChooser(QWidget* pParent):
    QGroupBox(pParent)
{
    QVBoxLayout* pLayout = new QVBoxLayout(this);
    m_pLabel = new QLabel(QString());
    pLayout->addWidget(m_pLabel);

    QChar visualTab(0x2192);
    QChar visualSpace((ushort)0xb7);
    m_pExampleTextEdit = new QPlainTextEdit(i18n("The quick brown fox jumps over the river\n"
                                                 "but the little red hen escapes with a shiver.\n"
                                                 ":-)") +
                                                visualTab + visualSpace,
                                            this);
    m_pExampleTextEdit->setFont(m_font);
    m_pExampleTextEdit->setReadOnly(true);
    pLayout->addWidget(m_pExampleTextEdit);

    m_pSelectFont = new QPushButton(i18nc("Button title", "Change Font"));
    m_pSelectFont->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    chk_connect(m_pSelectFont, &QPushButton::clicked, this, &FontChooser::slotSelectFont);
    pLayout->addWidget(m_pSelectFont);
    pLayout->setAlignment(m_pSelectFont, Qt::AlignRight);
}

QFont FontChooser::font()
{
    return m_font; //QFont("courier",10);
}

void FontChooser::setFont(const QFont& font, bool)
{
    m_font = font;
    m_pExampleTextEdit->setFont(m_font);
    m_pLabel->setText(i18nc("Font sample display, %1 = family, %2 = style, %3 = size", "Font: %1, %2, %3\n\nExample:", m_font.family(), m_font.styleName(), m_font.pointSize()));

    //update();
}

void FontChooser::slotSelectFont()
{
    bool bOk;
    m_font = QFontDialog::getFont(&bOk, m_font);
    m_pExampleTextEdit->setFont(m_font);
    m_pLabel->setText(i18nc("Font sample display, %1 = family, %2 = style, %3 = size", "Font: %1, %2, %3\n\nExample:", m_font.family(), m_font.styleName(), m_font.pointSize()));
}

class OptionFontChooser: public FontChooser, public OptionFont
{
  public:
    OptionFontChooser(const QFont& defaultVal, const QString& saveName, QFont* pVar, QWidget* pParent):
        FontChooser(pParent),
        OptionFont(pVar, defaultVal, saveName)
    {
    }

    void setToDefault() override { setFont(getDefault(), false); }
    void setToCurrent() override { setFont(getCurrent(), false); }
    using OptionFont::apply;
    void apply() override { apply(font()); }

  private:
    Q_DISABLE_COPY(OptionFontChooser)
};

class OptionColorButton: public KColorButton, public OptionColor
{
  public:
    OptionColorButton(const QColor& defaultVal, const QString& saveName, QColor* pVar, QWidget* pParent):
        KColorButton(pParent), OptionColor(pVar, defaultVal, saveName)
    {
    }

    void setToDefault() override { setColor(getDefault()); }
    void setToCurrent() override { setColor(getCurrent()); }
    using OptionColor::apply;
    void apply() override { apply(color()); }

  private:
    Q_DISABLE_COPY(OptionColorButton)
};

class OptionLineEdit: public QComboBox, public OptionString
{
  public:
    OptionLineEdit(const QString& defaultVal, const QString& saveName, QString* pVar,
                   QWidget* pParent):
        QComboBox(pParent),
        OptionString(pVar, defaultVal, saveName)
    {
        setMinimumWidth(50);
        setEditable(true);
        m_list.push_back(defaultVal);
        insertText();
    }
    void setToDefault() override
    {
        setEditText(getDefault());
    }
    void setToCurrent() override
    {
        setEditText(getCurrent());
    }

    using OptionString::apply;
    void apply() override
    {
        apply(currentText());
        insertText();
    }
    void write(ValueMap* config) const override
    {
        config->writeEntry(m_saveName, m_list);
    }
    void read(ValueMap* config) override
    {
        m_list = config->readEntry(m_saveName, QStringList(m_defaultVal));
        if(!m_list.empty()) setCurrent(m_list.front());
        clear();
        insertItems(0, m_list);
    }

  private:
    void insertText()
    { // Check if the text exists. If yes remove it and push it in as first element
        QString current = currentText();
        m_list.removeAll(current);
        m_list.push_front(current);
        clear();
        if(m_list.size() > 10)
            m_list.erase(m_list.begin() + 10, m_list.end());
        insertItems(0, m_list);
    }

    Q_DISABLE_COPY(OptionLineEdit)
    QStringList m_list;
};

class OptionIntEdit: public QLineEdit, public OptionNum<qint32>
{
  public:
    OptionIntEdit(qint32 defaultVal, const QString& saveName, qint32* pVar, qint32 rangeMin, qint32 rangeMax,
                  QWidget* pParent):
        QLineEdit(pParent),
        OptionNum<qint32>(pVar, defaultVal, saveName)
    {
        QIntValidator* v = new QIntValidator(this);
        v->setRange(rangeMin, rangeMax);
        setValidator(v);
    }
    void setToDefault() override
    {
        //QString::setNum does not account for locale settings
        setText(OptionNum::toString(getDefault()));
    }

    void setToCurrent() override
    {
        setText(getString());
    }

    using OptionNum<qint32>::apply;
    void apply() override
    {
        const QIntValidator* v = static_cast<const QIntValidator*>(validator());
        setCurrent(qBound(v->bottom(), text().toInt(), v->top()));

        setText(getString());
    }

  private:
    Q_DISABLE_COPY(OptionIntEdit)
};

class OptionComboBox: public QComboBox, public OptionItemBase
{
  public:
    OptionComboBox(qint32 defaultVal, const QString& saveName, qint32* pVarNum,
                   QWidget* pParent):
        QComboBox(pParent),
        OptionItemBase(saveName)
    {
        setMinimumWidth(50);
        m_pVarNum = pVarNum;
        m_pVarStr = nullptr;
        m_defaultVal = defaultVal;
        setEditable(false);
    }
    OptionComboBox(qint32 defaultVal, const QString& saveName, QString* pVarStr,
                   QWidget* pParent):
        QComboBox(pParent),
        OptionItemBase(saveName)
    {
        m_pVarNum = nullptr;
        m_pVarStr = pVarStr;
        m_defaultVal = defaultVal;
        setEditable(false);
    }
    void setToDefault() override
    {
        setCurrentIndex(m_defaultVal);
        if(m_pVarStr != nullptr)
        {
            *m_pVarStr = currentText();
        }
    }
    void setToCurrent() override
    {
        if(m_pVarNum != nullptr)
            setCurrentIndex(*m_pVarNum);
        else
            setText(*m_pVarStr);
    }
    using OptionItemBase::apply;
    void apply() override
    {
        if(m_pVarNum != nullptr)
        {
            *m_pVarNum = currentIndex();
        }
        else
        {
            *m_pVarStr = currentText();
        }
    }
    void write(ValueMap* config) const override
    {
        if(m_pVarStr != nullptr)
            config->writeEntry(m_saveName, *m_pVarStr);
        else
            config->writeEntry(m_saveName, *m_pVarNum);
    }
    void read(ValueMap* config) override
    {
        if(m_pVarStr != nullptr)
            setText(config->readEntry(m_saveName, currentText()));
        else
            *m_pVarNum = config->readEntry(m_saveName, *m_pVarNum);
    }
    void preserveImp() override
    {
        if(m_pVarStr != nullptr)
        {
            m_preservedStrVal = *m_pVarStr;
        }
        else
        {
            m_preservedNumVal = *m_pVarNum;
        }
    }
    void unpreserveImp() override
    {
        if(m_pVarStr != nullptr)
        {
            *m_pVarStr = m_preservedStrVal;
        }
        else
        {
            *m_pVarNum = m_preservedNumVal;
        }
    }

  private:
    Q_DISABLE_COPY(OptionComboBox)
    qint32* m_pVarNum;
    qint32 m_preservedNumVal = 0;
    QString* m_pVarStr;
    QString m_preservedStrVal;
    qint32 m_defaultVal;

    void setText(const QString& s)
    {
        // Find the string in the combobox-list, don't change the value if nothing fits.
        for(qint32 i = 0; i < count(); ++i)
        {
            if(itemText(i) == s)
            {
                if(m_pVarNum != nullptr) *m_pVarNum = i;
                if(m_pVarStr != nullptr) *m_pVarStr = s;
                setCurrentIndex(i);
                return;
            }
        }
    }
};

class OptionEncodingComboBox: public QComboBox, public OptionCodec
{
    Q_OBJECT
    QVector<QTextCodec*> m_codecVec;
    QTextCodec** m_ppVarCodec;

  public:
    OptionEncodingComboBox(const QString& saveName, QTextCodec** ppVarCodec,
                           QWidget* pParent):
        QComboBox(pParent),
        OptionCodec(saveName)
    {
        m_ppVarCodec = ppVarCodec;
        insertCodec(i18n("Unicode, 8 bit"), QTextCodec::codecForName("UTF-8"));
        insertCodec(i18n("Unicode"), QTextCodec::codecForName("iso-10646-UCS-2"));
        insertCodec(i18n("Latin1"), QTextCodec::codecForName("iso 8859-1"));

        // First sort codec names:
        std::map<QString, QTextCodec*> names;
        const QList<qint32> mibs = QTextCodec::availableMibs();
        for(qint32 i: mibs)
        {
            QTextCodec* c = QTextCodec::codecForMib(i);
            if(c != nullptr)
                names[QString(QLatin1String(c->name())).toUpper()] = c;
        }

        std::map<QString, QTextCodec*>::const_iterator it;
        for(const auto& pair: names)
        {
            insertCodec("", pair.second);
        }

        setToolTip(i18n(
            "Change this if non-ASCII characters are not displayed correctly."));
    }

    void insertCodec(const QString& visibleCodecName, QTextCodec* c)
    {
        if(c != nullptr)
        {
            const QByteArray nameArray = c->name();
            const QLatin1String codecName = QLatin1String(nameArray);

            for(QtSizeType i = 0; i < m_codecVec.size(); ++i)
            {
                if(c == m_codecVec[i])
                    return; // don't insert any codec twice
            }

            // The m_codecVec.size will at this point return the value we need for the index.
            if(codecName == defaultName())
                saveDefaultIndex(m_codecVec.size());
            QString itemText = visibleCodecName.isEmpty() ? codecName : visibleCodecName + u8" (" + codecName + u8")";
            addItem(itemText, m_codecVec.size());
            m_codecVec.push_back(c);
        }
    }

    void setToDefault() override
    {
        qint32 index = getDefaultIndex();

        setCurrentIndex(index);
        if(m_ppVarCodec != nullptr)
        {
            *m_ppVarCodec = m_codecVec[index];
        }
    }

    void setToCurrent() override
    {
        if(m_ppVarCodec != nullptr)
        {
            for(qint32 i = 0; i < m_codecVec.size(); ++i)
            {
                if(*m_ppVarCodec == m_codecVec[i])
                {
                    setCurrentIndex(i);
                    break;
                }
            }
        }
    }

    using OptionCodec::apply;
    void apply() override
    {
        if(m_ppVarCodec != nullptr)
        {
            *m_ppVarCodec = m_codecVec[currentIndex()];
        }
    }

    void write(ValueMap* config) const override
    {
        if(m_ppVarCodec != nullptr) config->writeEntry(m_saveName, (const char*)(*m_ppVarCodec)->name());
    }

    void read(ValueMap* config) override
    {
        QString codecName = config->readEntry(m_saveName, (const char*)m_codecVec[currentIndex()]->name());
        for(qint32 i = 0; i < m_codecVec.size(); ++i)
        {
            if(codecName == QLatin1String(m_codecVec[i]->name()))
            {
                setCurrentIndex(i);
                if(m_ppVarCodec != nullptr) *m_ppVarCodec = m_codecVec[i];
                break;
            }
        }
    }

  protected:
    void preserveImp() override { m_preservedVal = currentIndex(); }
    void unpreserveImp() override { setCurrentIndex(m_preservedVal); }
    qint32 m_preservedVal;
};

OptionDialog::OptionDialog(bool bShowDirMergeSettings, QWidget* parent):
    KPageDialog(parent)
{
    setFaceType(List);
    setWindowTitle(i18n("Configure"));
    setStandardButtons(QDialogButtonBox::Help | QDialogButtonBox::RestoreDefaults | QDialogButtonBox::Apply | QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    setModal(true);
    setMinimumSize(600, 500);

    m_options->init();
    setupFontPage();
    setupColorPage();
    setupEditPage();
    setupDiffPage();
    setupMergePage();

    if(bShowDirMergeSettings)
        setupDirectoryMergePage();

    setupRegionalPage();
    setupIntegrationPage();

    // Initialize all values in the dialog
    resetToDefaults();
    slotApply();
    chk_connect(button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &OptionDialog::slotApply);
    chk_connect(button(QDialogButtonBox::Ok), &QPushButton::clicked, this, &OptionDialog::slotOk);
    chk_connect(button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, &OptionDialog::slotDefault);
    chk_connect(button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, &QDialog::reject);
    chk_connect(button(QDialogButtonBox::Help), &QPushButton::clicked, this, &OptionDialog::helpRequested);
}

void OptionDialog::helpRequested()
{
    KHelpClient::invokeHelp();
}

OptionDialog::~OptionDialog() = default;

void OptionDialog::setupFontPage()
{
    QFrame* page = new QFrame();
    KPageWidgetItem* pageItem = new KPageWidgetItem(page, i18n("Font"));

    pageItem->setHeader(i18n("Editor & Diff Output Font"));
    //not all themes have this icon
    if(QIcon::hasThemeIcon(QStringLiteral("font-select-symbolic")))
        pageItem->setIcon(QIcon::fromTheme(QStringLiteral("font-select-symbolic")));
    else
        pageItem->setIcon(QIcon::fromTheme(QStringLiteral("preferences-desktop-font")));
    addPage(pageItem);

    QVBoxLayout* topLayout = new QVBoxLayout(page);
    topLayout->setContentsMargins(5, 5, 5, 5);

    //requires QT 5.2 or later.
    static const QFont defaultFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    static QFont defaultAppFont = QApplication::font();

    OptionFontChooser* pAppFontChooser = new OptionFontChooser(defaultAppFont, "ApplicationFont", &m_options->mAppFont, page);

    topLayout->addWidget(pAppFontChooser);
    pAppFontChooser->setTitle(i18n("Application font"));

    OptionFontChooser* pFontChooser = new OptionFontChooser(defaultFont, "Font", &m_options->mFont, page);

    topLayout->addWidget(pFontChooser);
    pFontChooser->setTitle(i18n("File view font"));

    QGridLayout* gbox = new QGridLayout();
    topLayout->addLayout(gbox);
    //qint32 line=0;

    // This currently does not work (see rendering in class DiffTextWindow)
    //OptionCheckBox* pItalicDeltas = new OptionCheckBox( i18n("Italic font for deltas"), false, "ItalicForDeltas", &m_options->m_bItalicForDeltas, page, this );

    //gbox->addWidget( pItalicDeltas, line, 0, 1, 2 );
    //pItalicDeltas->setToolTip( i18n(
    //   "Selects the italic version of the font for differences.\n"
    //   "If the font doesn't support italic characters, then this does nothing.")
    //   );
}

void OptionDialog::setupColorPage()
{
    QScrollArea* pageFrame = new QScrollArea();
    KPageWidgetItem* pageItem = new KPageWidgetItem(pageFrame, i18nc("Title for color settings page", "Color"));
    pageItem->setHeader(i18n("Colors Settings"));
    pageItem->setIcon(QIcon::fromTheme(QStringLiteral("colormanagement")));
    addPage(pageItem);

    QVBoxLayout* scrollLayout = new QVBoxLayout();
    scrollLayout->setContentsMargins(2, 2, 2, 2);
    scrollLayout->addWidget(pageFrame);

    QScopedPointer<Ui::ScrollArea> scrollArea(new Ui::ScrollArea());
    scrollArea->setupUi(pageFrame);

    QWidget* page = pageFrame->findChild<QWidget*>("contents");
    QVBoxLayout* topLayout = new QVBoxLayout(page);
    topLayout->setContentsMargins(5, 5, 5, 5);

    QGridLayout* gbox = new QGridLayout();
    gbox->setColumnStretch(1, 5);
    topLayout->addLayout(gbox);

    QLabel* label;
    qint32 line = 0;

    qint32 depth = QPixmap::defaultDepth();
    bool bLowColor = depth <= 8;

    label = new QLabel(i18n("Editor and Diff Views:"), page);
    gbox->addWidget(label, line, 0);
    QFont f(label->font());
    f.setBold(true);
    label->setFont(f);
    ++line;

    OptionColorButton* pFgColor = new OptionColorButton(Qt::black, "FgColor", &m_options->m_fgColor, page);
    label = new QLabel(i18n("Foreground color:"), page);
    label->setBuddy(pFgColor);

    gbox->addWidget(label, line, 0);
    gbox->addWidget(pFgColor, line, 1);
    ++line;

    OptionColorButton* pBgColor = new OptionColorButton(Qt::white, "BgColor", &m_options->m_bgColor, page);
    label = new QLabel(i18n("Background color:"), page);
    label->setBuddy(pBgColor);

    gbox->addWidget(label, line, 0);
    gbox->addWidget(pBgColor, line, 1);

    ++line;

    OptionColorButton* pDiffBgColor = new OptionColorButton(
        bLowColor ? QColor(Qt::lightGray) : qRgb(224, 224, 224), "DiffBgColor", &m_options->m_diffBgColor, page);
    label = new QLabel(i18n("Diff background color:"), page);
    label->setBuddy(pDiffBgColor);

    gbox->addWidget(label, line, 0);
    gbox->addWidget(pDiffBgColor, line, 1);
    ++line;

    OptionColorButton* pColorA = new OptionColorButton(
        bLowColor ? qRgb(0, 0, 255) : qRgb(0, 0, 200) /*blue*/, "ColorA", &m_options->m_colorA, page);
    label = new QLabel(i18n("Color A:"), page);
    label->setBuddy(pColorA);

    gbox->addWidget(label, line, 0);
    gbox->addWidget(pColorA, line, 1);
    ++line;

    OptionColorButton* pColorB = new OptionColorButton(
        bLowColor ? qRgb(0, 128, 0) : qRgb(0, 150, 0) /*green*/, "ColorB", &m_options->m_colorB, page);
    label = new QLabel(i18n("Color B:"), page);
    label->setBuddy(pColorB);

    gbox->addWidget(label, line, 0);
    gbox->addWidget(pColorB, line, 1);
    ++line;

    OptionColorButton* pColorC = new OptionColorButton(
        bLowColor ? qRgb(128, 0, 128) : qRgb(150, 0, 150) /*magenta*/, "ColorC", &m_options->m_colorC, page);
    label = new QLabel(i18n("Color C:"), page);
    label->setBuddy(pColorC);

    gbox->addWidget(label, line, 0);
    gbox->addWidget(pColorC, line, 1);
    ++line;

    OptionColorButton* pColorForConflict = new OptionColorButton(Qt::red, "ColorForConflict", &m_options->m_colorForConflict, page);
    label = new QLabel(i18n("Conflict color:"), page);
    label->setBuddy(pColorForConflict);

    gbox->addWidget(label, line, 0);
    gbox->addWidget(pColorForConflict, line, 1);
    ++line;

    OptionColorButton* pColor = new OptionColorButton(
        bLowColor ? qRgb(192, 192, 192) : qRgb(220, 220, 100), "CurrentRangeBgColor", &m_options->m_currentRangeBgColor, page);
    label = new QLabel(i18n("Current range background color:"), page);
    label->setBuddy(pColor);

    gbox->addWidget(label, line, 0);
    gbox->addWidget(pColor, line, 1);
    ++line;

    pColor = new OptionColorButton(
        bLowColor ? qRgb(255, 255, 0) : qRgb(255, 255, 150), "CurrentRangeDiffBgColor", &m_options->m_currentRangeDiffBgColor, page);
    label = new QLabel(i18n("Current range diff background color:"), page);
    label->setBuddy(pColor);

    gbox->addWidget(label, line, 0);
    gbox->addWidget(pColor, line, 1);
    ++line;

    pColor = new OptionColorButton(qRgb(0xff, 0xd0, 0x80), "ManualAlignmentRangeColor", &m_options->m_manualHelpRangeColor, page);
    label = new QLabel(i18n("Color for manually aligned difference ranges:"), page);
    label->setBuddy(pColor);

    gbox->addWidget(label, line, 0);
    gbox->addWidget(pColor, line, 1);
    ++line;

    label = new QLabel(i18n("Folder Comparison View:"), page);
    gbox->addWidget(label, line, 0);
    label->setFont(f);
    ++line;

    pColor = new OptionColorButton(qRgb(0, 0xd0, 0), "NewestFileColor", &m_options->m_newestFileColor, page);
    label = new QLabel(i18n("Newest file color:"), page);
    label->setBuddy(pColor);

    gbox->addWidget(label, line, 0);
    gbox->addWidget(pColor, line, 1);
    QString dirColorTip = i18n("Changing this color will only be effective when starting the next folder comparison.");
    label->setToolTip(dirColorTip);
    ++line;

    pColor = new OptionColorButton(qRgb(0xf0, 0, 0), "OldestFileColor", &m_options->m_oldestFileColor, page);
    label = new QLabel(i18n("Oldest file color:"), page);
    label->setBuddy(pColor);

    gbox->addWidget(label, line, 0);
    gbox->addWidget(pColor, line, 1);
    label->setToolTip(dirColorTip);
    ++line;

    pColor = new OptionColorButton(qRgb(0xc0, 0xc0, 0), "MidAgeFileColor", &m_options->m_midAgeFileColor, page);
    label = new QLabel(i18n("Middle age file color:"), page);
    label->setBuddy(pColor);

    gbox->addWidget(label, line, 0);
    gbox->addWidget(pColor, line, 1);
    label->setToolTip(dirColorTip);
    ++line;

    pColor = new OptionColorButton(qRgb(0, 0, 0), "MissingFileColor", &m_options->m_missingFileColor, page);
    label = new QLabel(i18n("Color for missing files:"), page);
    label->setBuddy(pColor);

    gbox->addWidget(label, line, 0);
    gbox->addWidget(pColor, line, 1);
    label->setToolTip(dirColorTip);
    ++line;

    topLayout->addStretch(10);
}

void OptionDialog::setupEditPage()
{
    QScrollArea* pageFrame = new QScrollArea();
    KPageWidgetItem* pageItem = new KPageWidgetItem(pageFrame, i18n("Editor"));
    pageItem->setHeader(i18n("Editor Behavior"));
    pageItem->setIcon(QIcon::fromTheme(QStringLiteral("accessories-text-editor")));
    addPage(pageItem);

    QVBoxLayout* scrollLayout = new QVBoxLayout();
    scrollLayout->setContentsMargins(2, 2, 2, 2);
    scrollLayout->addWidget(pageFrame);

    QScopedPointer<Ui::ScrollArea> scrollArea(new Ui::ScrollArea());
    scrollArea->setupUi(pageFrame);

    QWidget* page = pageFrame->findChild<QWidget*>("contents");

    QVBoxLayout* topLayout = new QVBoxLayout(page);
    topLayout->setContentsMargins(5, 5, 5, 5);

    QGridLayout* gbox = new QGridLayout();
    gbox->setColumnStretch(1, 5);
    topLayout->addLayout(gbox);
    QLabel* label;
    qint32 line = 0;

    OptionCheckBox* pReplaceTabs = new OptionCheckBox(i18n("Tab inserts spaces"), false, "ReplaceTabs", &m_options->m_bReplaceTabs, page);

    gbox->addWidget(pReplaceTabs, line, 0, 1, 2);
    pReplaceTabs->setToolTip(i18n(
        "On: Pressing tab generates the appropriate number of spaces.\n"
        "Off: A tab character will be inserted."));
    ++line;

    OptionIntEdit* pTabSize = new OptionIntEdit(8, "TabSize", &m_options->m_tabSize, 1, 100, page);
    label = new QLabel(i18n("Tab size:"), page);
    label->setBuddy(pTabSize);

    gbox->addWidget(label, line, 0);
    gbox->addWidget(pTabSize, line, 1);
    ++line;

    OptionCheckBox* pAutoIndentation = new OptionCheckBox(i18n("Auto indentation"), true, "AutoIndentation", &m_options->m_bAutoIndentation, page);
    gbox->addWidget(pAutoIndentation, line, 0, 1, 2);

    pAutoIndentation->setToolTip(i18n(
        "On: The indentation of the previous line is used for a new line.\n"));
    ++line;

    OptionCheckBox* pAutoCopySelection = new OptionCheckBox(i18n("Auto copy selection"), false, "AutoCopySelection", &m_options->m_bAutoCopySelection, page);
    gbox->addWidget(pAutoCopySelection, line, 0, 1, 2);

    pAutoCopySelection->setToolTip(i18n(
        "On: Any selection is immediately written to the clipboard.\n"
        "Off: You must explicitly copy e.g. via Ctrl-C."));
    ++line;

    label = new QLabel(i18n("Line end style:"), page);
    gbox->addWidget(label, line, 0);

    OptionComboBox* pLineEndStyle = new OptionComboBox(eLineEndStyleAutoDetect, "LineEndStyle", (qint32*)&m_options->m_lineEndStyle, page);
    gbox->addWidget(pLineEndStyle, line, 1);

    pLineEndStyle->insertItem(eLineEndStyleUnix, i18nc("Unix line ending", "Unix"));
    pLineEndStyle->insertItem(eLineEndStyleDos, i18nc("Dos/Windows line ending", "Dos/Windows"));
    pLineEndStyle->insertItem(eLineEndStyleAutoDetect, i18nc("Automatically detected line ending", "Autodetect"));

    label->setToolTip(i18n(
        "Sets the line endings for when an edited file is saved.\n"
        "DOS/Windows: CR+LF; UNIX: LF; with CR=0D, LF=0A"));
    ++line;

    topLayout->addStretch(10);
}

void OptionDialog::setupDiffPage()
{
    QScrollArea* pageFrame = new QScrollArea();
    KPageWidgetItem* pageItem = new KPageWidgetItem(pageFrame, i18n("Diff"));
    pageItem->setHeader(i18n("Diff Settings"));
    pageItem->setIcon(QIcon::fromTheme(QStringLiteral("text-x-patch")));
    addPage(pageItem);

    QVBoxLayout* scrollLayout = new QVBoxLayout();
    scrollLayout->setContentsMargins(2, 2, 2, 2);
    scrollLayout->addWidget(pageFrame);

    QScopedPointer<Ui::ScrollArea> scrollArea(new Ui::ScrollArea());
    scrollArea->setupUi(pageFrame);

    QWidget* page = pageFrame->findChild<QWidget*>("contents");

    QVBoxLayout* topLayout = new QVBoxLayout(page);
    topLayout->setContentsMargins(5, 5, 5, 5);

    QGridLayout* gbox = new QGridLayout();
    gbox->setColumnStretch(1, 5);
    topLayout->addLayout(gbox);
    qint32 line = 0;

    QLabel* label = nullptr;

    OptionCheckBox* pIgnoreNumbers = new OptionCheckBox(i18n("Ignore numbers (treat as white space)"), false, "IgnoreNumbers", &m_options->m_bIgnoreNumbers, page);
    gbox->addWidget(pIgnoreNumbers, line, 0, 1, 2);

    pIgnoreNumbers->setToolTip(i18n(
        "Ignore number characters during line matching phase. (Similar to Ignore white space.)\n"
        "Might help to compare files with numeric data."));
    ++line;

    OptionCheckBox* pIgnoreComments = new OptionCheckBox(i18n("Ignore C/C++ comments (treat as white space)"), false, "IgnoreComments", &m_options->m_bIgnoreComments, page);
    gbox->addWidget(pIgnoreComments, line, 0, 1, 2);

    pIgnoreComments->setToolTip(i18n("Treat C/C++ comments like white space."));
    ++line;

    OptionCheckBox* pIgnoreCase = new OptionCheckBox(i18n("Ignore case (treat as white space)"), false, "IgnoreCase", &m_options->m_bIgnoreCase, page);
    gbox->addWidget(pIgnoreCase, line, 0, 1, 2);

    pIgnoreCase->setToolTip(i18n(
        "Treat case differences like white space changes. ('a'<=>'A')"));
    ++line;

    label = new QLabel(i18n("Preprocessor command:"), page);
    gbox->addWidget(label, line, 0);
    OptionLineEdit* pLE = new OptionLineEdit("", "PreProcessorCmd", &m_options->m_PreProcessorCmd, page);
    gbox->addWidget(pLE, line, 1);

    label->setToolTip(i18n("User defined pre-processing. (See the docs for details.)"));
    ++line;

    label = new QLabel(i18n("Line-matching preprocessor command:"), page);
    gbox->addWidget(label, line, 0);
    pLE = new OptionLineEdit("", "LineMatchingPreProcessorCmd", &m_options->m_LineMatchingPreProcessorCmd, page);
    gbox->addWidget(pLE, line, 1);

    label->setToolTip(i18n("This pre-processor is only used during line matching.\n(See the docs for details.)"));
    ++line;

    OptionCheckBox* pTryHard = new OptionCheckBox(i18n("Try hard (slower)"), true, "TryHard", &m_options->m_bTryHard, page);
    gbox->addWidget(pTryHard, line, 0, 1, 2);

    pTryHard->setToolTip(i18n(
        "Enables the --minimal option for the external diff.\n"
        "The analysis of big files will be much slower."));
    ++line;

    OptionCheckBox* pDiff3AlignBC = new OptionCheckBox(i18n("Align B and C for 3 input files"), false, "Diff3AlignBC", &m_options->m_bDiff3AlignBC, page);
    gbox->addWidget(pDiff3AlignBC, line, 0, 1, 2);

    pDiff3AlignBC->setToolTip(i18n(
        "Try to align B and C when comparing or merging three input files.\n"
        "Not recommended for merging because merge might get more complicated.\n"
        "(Default is off.)"));
    ++line;

    topLayout->addStretch(10);
}

void OptionDialog::setupMergePage()
{
    QScrollArea* pageFrame = new QScrollArea();
    KPageWidgetItem* pageItem = new KPageWidgetItem(pageFrame, i18nc("Settings page", "Merge"));
    pageItem->setHeader(i18n("Merge Settings"));
    pageItem->setIcon(QIcon::fromTheme(QStringLiteral("merge")));
    addPage(pageItem);

    QVBoxLayout* scrollLayout = new QVBoxLayout();
    scrollLayout->setContentsMargins(2, 2, 2, 2);
    scrollLayout->addWidget(pageFrame);

    QScopedPointer<Ui::ScrollArea> scrollArea(new Ui::ScrollArea());
    scrollArea->setupUi(pageFrame);

    QWidget* page = pageFrame->findChild<QWidget*>("contents");

    QVBoxLayout* topLayout = new QVBoxLayout(page);
    topLayout->setContentsMargins(5, 5, 5, 5);

    QGridLayout* gbox = new QGridLayout();
    gbox->setColumnStretch(1, 5);
    topLayout->addLayout(gbox);
    qint32 line = 0;

    QLabel* label = nullptr;

    label = new QLabel(i18n("Auto advance delay (ms):"), page);
    gbox->addWidget(label, line, 0);
    OptionIntEdit* pAutoAdvanceDelay = new OptionIntEdit(500, "AutoAdvanceDelay", &m_options->m_autoAdvanceDelay, 0, 2000, page);
    gbox->addWidget(pAutoAdvanceDelay, line, 1);

    label->setToolTip(i18n(
        "When in Auto-Advance mode the result of the current selection is shown \n"
        "for the specified time, before jumping to the next conflict. Range: 0-2000 ms"));
    ++line;

    OptionCheckBox* pShowInfoDialogs = new OptionCheckBox(i18n("Show info dialogs"), true, "ShowInfoDialogs", &m_options->m_bShowInfoDialogs, page);
    gbox->addWidget(pShowInfoDialogs, line, 0, 1, 2);

    pShowInfoDialogs->setToolTip(i18n("Show a dialog with information about the number of conflicts."));
    ++line;

    label = new QLabel(i18n("White space 2-file merge default:"), page);
    gbox->addWidget(label, line, 0);
    OptionComboBox* pWhiteSpace2FileMergeDefault = new OptionComboBox(0, "WhiteSpace2FileMergeDefault", &m_options->m_whiteSpace2FileMergeDefault, page);
    gbox->addWidget(pWhiteSpace2FileMergeDefault, line, 1);

    pWhiteSpace2FileMergeDefault->insertItem(0, i18n("Manual Choice"));
    pWhiteSpace2FileMergeDefault->insertItem(1, i18n("A"));
    pWhiteSpace2FileMergeDefault->insertItem(2, i18n("B"));
    label->setToolTip(i18n(
        "Allow the merge algorithm to automatically select an input for "
        "white-space-only changes."));
    ++line;

    label = new QLabel(i18n("White space 3-file merge default:"), page);
    gbox->addWidget(label, line, 0);
    OptionComboBox* pWhiteSpace3FileMergeDefault = new OptionComboBox(0, "WhiteSpace3FileMergeDefault", &m_options->m_whiteSpace3FileMergeDefault, page);
    gbox->addWidget(pWhiteSpace3FileMergeDefault, line, 1);

    pWhiteSpace3FileMergeDefault->insertItem(0, i18n("Manual Choice"));
    pWhiteSpace3FileMergeDefault->insertItem(1, i18n("A"));
    pWhiteSpace3FileMergeDefault->insertItem(2, i18n("B"));
    pWhiteSpace3FileMergeDefault->insertItem(3, i18n("C"));
    label->setToolTip(i18n(
        "Allow the merge algorithm to automatically select an input for "
        "white-space-only changes."));
    ++line;

    QGroupBox* pGroupBox = new QGroupBox(i18n("Automatic Merge Regular Expression"));
    gbox->addWidget(pGroupBox, line, 0, 1, 2);
    ++line;
    {
        gbox = new QGridLayout(pGroupBox);
        gbox->setColumnStretch(1, 10);
        line = 0;

        label = new QLabel(i18n("Auto merge regular expression:"), page);
        gbox->addWidget(label, line, 0);
        m_pAutoMergeRegExpLineEdit = new OptionLineEdit(".*\\$(Version|Header|Date|Author).*\\$.*", "AutoMergeRegExp", &m_options->m_autoMergeRegExp, page);
        gbox->addWidget(m_pAutoMergeRegExpLineEdit, line, 1);

        label->setToolTip(s_autoMergeRegExpToolTip);
        ++line;

        OptionCheckBox* pAutoMergeRegExp = new OptionCheckBox(i18n("Run regular expression auto merge on merge start"), false, "RunRegExpAutoMergeOnMergeStart", &m_options->m_bRunRegExpAutoMergeOnMergeStart, page);

        gbox->addWidget(pAutoMergeRegExp, line, 0, 1, 2);
        pAutoMergeRegExp->setToolTip(i18n("Run the merge for auto merge regular expressions\n"
                                          "immediately when a merge starts.\n"));
        ++line;
    }

    pGroupBox = new QGroupBox(i18n("Version Control History Merging"));
    gbox->addWidget(pGroupBox, line, 0, 1, 2);
    ++line;
    {
        gbox = new QGridLayout(pGroupBox);
        gbox->setColumnStretch(1, 10);
        line = 0;

        label = new QLabel(i18n("History start regular expression:"), page);
        gbox->addWidget(label, line, 0);
        m_pHistoryStartRegExpLineEdit = new OptionLineEdit(".*\\$Log.*\\$.*", "HistoryStartRegExp", &m_options->m_historyStartRegExp, page);
        gbox->addWidget(m_pHistoryStartRegExpLineEdit, line, 1);

        label->setToolTip(s_historyStartRegExpToolTip);
        ++line;

        label = new QLabel(i18n("History entry start regular expression:"), page);
        gbox->addWidget(label, line, 0);
        // Example line:  "** \main\rolle_fsp_dev_008\1   17 Aug 2001 10:45:44   rolle"
        QString historyEntryStartDefault =
            "\\s*\\\\main\\\\(\\S+)\\s+"                         // Start with  "\main\"
            "([0-9]+) "                                          // day
            "(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) " //month
            "([0-9][0-9][0-9][0-9]) "                            // year
            "([0-9][0-9]:[0-9][0-9]:[0-9][0-9])\\s+(.*)";        // time, name

        m_pHistoryEntryStartRegExpLineEdit = new OptionLineEdit(historyEntryStartDefault, "HistoryEntryStartRegExp", &m_options->m_historyEntryStartRegExp, page);
        gbox->addWidget(m_pHistoryEntryStartRegExpLineEdit, line, 1);

        label->setToolTip(s_historyEntryStartRegExpToolTip);
        ++line;

        m_pHistoryMergeSorting = new OptionCheckBox(i18n("History merge sorting"), false, "HistoryMergeSorting", &m_options->m_bHistoryMergeSorting, page);
        gbox->addWidget(m_pHistoryMergeSorting, line, 0, 1, 2);

        m_pHistoryMergeSorting->setToolTip(i18n("Sort version control history by a key."));
        ++line;
        //QString branch = newHistoryEntry.cap(1);
        //qint32 day    = newHistoryEntry.cap(2).toInt();
        //qint32 month  = QString("Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec").find(newHistoryEntry.cap(3))/4 + 1;
        //qint32 year   = newHistoryEntry.cap(4).toInt();
        //QString time = newHistoryEntry.cap(5);
        //QString name = newHistoryEntry.cap(6);
        QString defaultSortKeyOrder = "4,3,2,5,1,6"; //QDate(year,month,day).toString(Qt::ISODate) +" "+ time + " " + branch + " " + name;

        label = new QLabel(i18n("History entry start sort key order:"), page);
        gbox->addWidget(label, line, 0);
        m_pHistorySortKeyOrderLineEdit = new OptionLineEdit(defaultSortKeyOrder, "HistoryEntryStartSortKeyOrder", &m_options->m_historyEntryStartSortKeyOrder, page);
        gbox->addWidget(m_pHistorySortKeyOrderLineEdit, line, 1);

        label->setToolTip(s_historyEntryStartSortKeyOrderToolTip);
        m_pHistorySortKeyOrderLineEdit->setEnabled(false);
        chk_connect(m_pHistoryMergeSorting, &OptionCheckBox::toggled, m_pHistorySortKeyOrderLineEdit, &OptionLineEdit::setEnabled);
        ++line;

        m_pHistoryAutoMerge = new OptionCheckBox(i18n("Merge version control history on merge start"), false, "RunHistoryAutoMergeOnMergeStart", &m_options->m_bRunHistoryAutoMergeOnMergeStart, page);

        gbox->addWidget(m_pHistoryAutoMerge, line, 0, 1, 2);
        m_pHistoryAutoMerge->setToolTip(i18n("Run version control history automerge on merge start."));
        ++line;

        OptionIntEdit* pMaxNofHistoryEntries = new OptionIntEdit(-1, "MaxNofHistoryEntries", &m_options->m_maxNofHistoryEntries, -1, 1000, page);
        label = new QLabel(i18n("Max number of history entries:"), page);
        gbox->addWidget(label, line, 0);
        gbox->addWidget(pMaxNofHistoryEntries, line, 1);

        pMaxNofHistoryEntries->setToolTip(i18n("Cut off after specified number. Use -1 for infinite number of entries."));
        ++line;
    }

    QPushButton* pButton = new QPushButton(i18n("Test your regular expressions"), page);
    gbox->addWidget(pButton, line, 0);
    chk_connect(pButton, &QPushButton::clicked, this, &OptionDialog::slotHistoryMergeRegExpTester);
    ++line;

    label = new QLabel(i18n("Irrelevant merge command:"), page);
    gbox->addWidget(label, line, 0);
    OptionLineEdit* pLE = new OptionLineEdit("", "IrrelevantMergeCmd", &m_options->m_IrrelevantMergeCmd, page);
    gbox->addWidget(pLE, line, 1);

    label->setToolTip(i18n("If specified this script is run after automerge\n"
                           "when no other relevant changes were detected.\n"
                           "Called with the parameters: filename1 filename2 filename3"));
    ++line;

    OptionCheckBox* pAutoSaveAndQuit = new OptionCheckBox(i18n("Auto save and quit on merge without conflicts"), false,
                                                          "AutoSaveAndQuitOnMergeWithoutConflicts", &m_options->m_bAutoSaveAndQuitOnMergeWithoutConflicts, page);
    gbox->addWidget(pAutoSaveAndQuit, line, 0, 1, 2);

    pAutoSaveAndQuit->setToolTip(i18n("If KDiff3 was started for a file-merge from the command line and all\n"
                                      "conflicts are solvable without user interaction then automatically save and quit.\n"
                                      "(Similar to command line option \"--auto\".)"));
    ++line;

    topLayout->addStretch(10);
}

void OptionDialog::setupDirectoryMergePage()
{
    QScrollArea* pageFrame = new QScrollArea();
    KPageWidgetItem* pageItem = new KPageWidgetItem(pageFrame, i18n("Folder"));
    pageItem->setHeader(i18n("Folder"));
    pageItem->setIcon(QIcon::fromTheme(QStringLiteral("inode-directory")));
    addPage(pageItem);

    QVBoxLayout* scrollLayout = new QVBoxLayout();
    scrollLayout->setContentsMargins(2, 2, 2, 2);
    scrollLayout->addWidget(pageFrame);

    QScopedPointer<Ui::ScrollArea> scrollArea(new Ui::ScrollArea());
    scrollArea->setupUi(pageFrame);

    QWidget* page = pageFrame->findChild<QWidget*>("contents");
    QVBoxLayout* topLayout = new QVBoxLayout(page);
    topLayout->setContentsMargins(5, 5, 5, 5);

    QGridLayout* gbox = new QGridLayout();
    gbox->setColumnStretch(1, 5);
    topLayout->addLayout(gbox);
    qint32 line = 0;

    OptionCheckBox* pRecursiveDirs = new OptionCheckBox(i18n("Recursive folders"), true, "RecursiveDirs", &m_options->m_bDmRecursiveDirs, page);
    gbox->addWidget(pRecursiveDirs, line, 0, 1, 2);

    pRecursiveDirs->setToolTip(i18n("Whether to analyze subfolders or not."));
    ++line;
    QLabel* label = new QLabel(i18n("File pattern(s):"), page);
    gbox->addWidget(label, line, 0);
    OptionLineEdit* pFilePattern = new OptionLineEdit("*", "FilePattern", &m_options->m_DmFilePattern, page);
    gbox->addWidget(pFilePattern, line, 1);

    label->setToolTip(i18n(
        "Pattern(s) of files to be analyzed. \n"
        "Wildcards: '*' and '?'\n"
        "Several Patterns can be specified by using the separator: ';'"));
    ++line;

    label = new QLabel(i18n("File-anti-pattern(s):"), page);
    gbox->addWidget(label, line, 0);
    OptionLineEdit* pFileAntiPattern = new OptionLineEdit("*.orig;*.o;*.obj;*.rej;*.bak", "FileAntiPattern", &m_options->m_DmFileAntiPattern, page);
    gbox->addWidget(pFileAntiPattern, line, 1);

    label->setToolTip(i18n(
        "Pattern(s) of files to be excluded from analysis. \n"
        "Wildcards: '*' and '?'\n"
        "Several Patterns can be specified by using the separator: ';'"));
    ++line;

    label = new QLabel(i18n("Folder-anti-pattern(s):"), page);
    gbox->addWidget(label, line, 0);
    OptionLineEdit* pDirAntiPattern = new OptionLineEdit("CVS;.deps;.svn;.hg;.git", "DirAntiPattern", &m_options->m_DmDirAntiPattern, page);
    gbox->addWidget(pDirAntiPattern, line, 1);

    label->setToolTip(i18n(
        "Pattern(s) of folders to be excluded from analysis. \n"
        "Wildcards: '*' and '?'\n"
        "Several Patterns can be specified by using the separator: ';'"));
    ++line;

    OptionCheckBox* pUseCvsIgnore = new OptionCheckBox(i18n("Use Ignore File"), false, "UseCvsIgnore", &m_options->m_bDmUseCvsIgnore, page);
    gbox->addWidget(pUseCvsIgnore, line, 0, 1, 2);

    pUseCvsIgnore->setToolTip(i18n(
        "Extends the antipattern to anything that would be ignored by source control.\n"
        "Via local ignore files this can be folder-specific."));
    ++line;

    OptionCheckBox* pFindHidden = new OptionCheckBox(i18n("Find hidden files and folders"), true, "FindHidden", &m_options->m_bDmFindHidden, page);
    gbox->addWidget(pFindHidden, line, 0, 1, 2);

    pFindHidden->setToolTip(i18n("Finds hidden files and folders."));
    ++line;

    OptionCheckBox* pFollowFileLinks = new OptionCheckBox(i18n("Follow file links"), true, "FollowFileLinks", &m_options->m_bDmFollowFileLinks, page);
    gbox->addWidget(pFollowFileLinks, line, 0, 1, 2);

    pFollowFileLinks->setToolTip(i18n(
        "On: Compare the file the link points to.\n"
        "Off: Compare the links."));
    ++line;

    OptionCheckBox* pFollowDirLinks = new OptionCheckBox(i18n("Follow folder links"), true, "FollowDirLinks", &m_options->m_bDmFollowDirLinks, page);
    gbox->addWidget(pFollowDirLinks, line, 0, 1, 2);

    pFollowDirLinks->setToolTip(i18n(
        "On: Compare the folder the link points to.\n"
        "Off: Compare the links."));
    ++line;

#if defined(Q_OS_WIN)
    bool bCaseSensitiveFilenameComparison = false;
#else
    bool bCaseSensitiveFilenameComparison = true;
#endif
    OptionCheckBox* pCaseSensitiveFileNames = new OptionCheckBox(i18n("Case sensitive filename comparison"), bCaseSensitiveFilenameComparison, "CaseSensitiveFilenameComparison", &m_options->m_bDmCaseSensitiveFilenameComparison, page);
    gbox->addWidget(pCaseSensitiveFileNames, line, 0, 1, 2);

    pCaseSensitiveFileNames->setToolTip(i18n(
        "The folder comparison will compare files or folders when their names match.\n"
        "Set this option if the case of the names must match. (Default for Windows is off, otherwise on.)"));
    ++line;

    OptionCheckBox* pUnfoldSubdirs = new OptionCheckBox(i18n("Unfold all subfolders on load"), false, "UnfoldSubdirs", &m_options->m_bDmUnfoldSubdirs, page);
    gbox->addWidget(pUnfoldSubdirs, line, 0, 1, 2);

    pUnfoldSubdirs->setToolTip(i18n(
        "On: Unfold all subfolders when starting a folder diff.\n"
        "Off: Leave subfolders folded."));
    ++line;

    OptionCheckBox* pSkipDirStatus = new OptionCheckBox(i18n("Skip folder status report"), false, "SkipDirStatus", &m_options->m_bDmSkipDirStatus, page);
    gbox->addWidget(pSkipDirStatus, line, 0, 1, 2);

    pSkipDirStatus->setToolTip(i18n(
        "On: Do not show the Folder Comparison Status.\n"
        "Off: Show the status dialog on start."));
    ++line;

    QGroupBox* pBG = new QGroupBox(i18n("File Comparison Mode"));
    gbox->addWidget(pBG, line, 0, 1, 2);

    QVBoxLayout* pBGLayout = new QVBoxLayout(pBG);

    OptionRadioButton* pBinaryComparison = new OptionRadioButton(i18n("Binary comparison"), true, "BinaryComparison", &m_options->m_bDmBinaryComparison, pBG);

    pBinaryComparison->setToolTip(i18n("Binary comparison of each file. (Default)"));
    pBGLayout->addWidget(pBinaryComparison);

    OptionRadioButton* pFullAnalysis = new OptionRadioButton(i18n("Full analysis"), false, "FullAnalysis", &m_options->m_bDmFullAnalysis, pBG);

    pFullAnalysis->setToolTip(i18n("Do a full analysis and show statistics information in extra columns.\n"
                                   "(Slower than a binary comparison, much slower for binary files.)"));
    pBGLayout->addWidget(pFullAnalysis);

    OptionRadioButton* pTrustDate = new OptionRadioButton(i18n("Trust the size and modification date (unsafe)"), false, "TrustDate", &m_options->m_bDmTrustDate, pBG);

    pTrustDate->setToolTip(i18n("Assume that files are equal if the modification date and file length are equal.\n"
                                "Files with equal contents but different modification dates will appear as different.\n"
                                "Useful for big folders or slow networks."));
    pBGLayout->addWidget(pTrustDate);

    OptionRadioButton* pTrustDateFallbackToBinary = new OptionRadioButton(i18n("Trust the size and date, but use binary comparison if date does not match (unsafe)"), false, "TrustDateFallbackToBinary", &m_options->m_bDmTrustDateFallbackToBinary, pBG);

    pTrustDateFallbackToBinary->setToolTip(i18n("Assume that files are equal if the modification date and file length are equal.\n"
                                                "If the dates are not equal but the sizes are, use binary comparison.\n"
                                                "Useful for big folders or slow networks."));
    pBGLayout->addWidget(pTrustDateFallbackToBinary);

    OptionRadioButton* pTrustSize = new OptionRadioButton(i18n("Trust the size (unsafe)"), false, "TrustSize", &m_options->m_bDmTrustSize, pBG);

    pTrustSize->setToolTip(i18n("Assume that files are equal if their file lengths are equal.\n"
                                "Useful for big folders or slow networks when the date is modified during download."));
    pBGLayout->addWidget(pTrustSize);

    ++line;

    // Some two Dir-options: Affects only the default actions.
    OptionCheckBox* pSyncMode = new OptionCheckBox(i18n("Synchronize folders"), false, "SyncMode", &m_options->m_bDmSyncMode, page);

    gbox->addWidget(pSyncMode, line, 0, 1, 2);
    pSyncMode->setToolTip(i18n(
        "Offers to store files in both folders so that\n"
        "both folders are the same afterwards.\n"
        "Works only when comparing two folders without specifying a destination."));
    ++line;

    // Allow white-space only differences to be considered equal
    OptionCheckBox* pWhiteSpaceDiffsEqual = new OptionCheckBox(i18n("White space differences considered equal"), true, "WhiteSpaceEqual", &m_options->m_bDmWhiteSpaceEqual, page);

    gbox->addWidget(pWhiteSpaceDiffsEqual, line, 0, 1, 2);
    pWhiteSpaceDiffsEqual->setToolTip(i18n(
        "If files differ only by white space consider them equal.\n"
        "This is only active when full analysis is chosen."));
    chk_connect(pFullAnalysis, &OptionRadioButton::toggled, pWhiteSpaceDiffsEqual, &OptionCheckBox::setEnabled);
    pWhiteSpaceDiffsEqual->setEnabled(false);
    ++line;

    OptionCheckBox* pCopyNewer = new OptionCheckBox(i18n("Copy newer instead of merging (unsafe)"), false, "CopyNewer", &m_options->m_bDmCopyNewer, page);

    gbox->addWidget(pCopyNewer, line, 0, 1, 2);
    pCopyNewer->setToolTip(i18n(
        "Do not look inside, just take the newer file.\n"
        "(Use this only if you know what you are doing!)\n"
        "Only effective when comparing two folders."));
    ++line;

    OptionCheckBox* pCreateBakFiles = new OptionCheckBox(i18n("Backup files (.orig)"), true, "CreateBakFiles", &m_options->m_bDmCreateBakFiles, page);
    gbox->addWidget(pCreateBakFiles, line, 0, 1, 2);

    pCreateBakFiles->setToolTip(i18n(
        "If a file would be saved over an old file, then the old file\n"
        "will be renamed with a '.orig' extension instead of being deleted."));
    ++line;

    topLayout->addStretch(10);
}
void OptionDialog::setupRegionalPage()
{
    QScrollArea* pageFrame = new QScrollArea();
    KPageWidgetItem* pageItem = new KPageWidgetItem(pageFrame, i18n("Regional Settings"));
    pageItem->setHeader(i18n("Regional Settings"));
    pageItem->setIcon(QIcon::fromTheme(QStringLiteral("preferences-desktop-locale")));
    addPage(pageItem);

    QVBoxLayout* scrollLayout = new QVBoxLayout();
    scrollLayout->setContentsMargins(2, 2, 2, 2);
    scrollLayout->addWidget(pageFrame);

    QScopedPointer<Ui::ScrollArea> scrollArea(new Ui::ScrollArea());
    scrollArea->setupUi(pageFrame);

    QWidget* page = pageFrame->findChild<QWidget*>("contents");

    QVBoxLayout* topLayout = new QVBoxLayout(page);
    topLayout->setContentsMargins(5, 5, 5, 5);

    QGridLayout* gbox = new QGridLayout();
    gbox->setColumnStretch(1, 5);
    topLayout->addLayout(gbox);
    qint32 line = 0;

    QLabel* label;

    m_pSameEncoding = new OptionCheckBox(i18n("Use the same encoding for everything:"), true, "SameEncoding", &m_options->m_bSameEncoding, page);

    gbox->addWidget(m_pSameEncoding, line, 0, 1, 2);
    m_pSameEncoding->setToolTip(i18n(
        "Enable this allows to change all encodings by changing the first only.\n"
        "Disable this if different individual settings are needed."));
    ++line;

    label = new QLabel(i18n("Note: Local Encoding is \"%1\"", QLatin1String(QTextCodec::codecForLocale()->name())), page);
    gbox->addWidget(label, line, 0);
    ++line;

    label = new QLabel(i18n("File Encoding for A:"), page);
    gbox->addWidget(label, line, 0);
    m_pEncodingAComboBox = new OptionEncodingComboBox("EncodingForA", &m_options->m_pEncodingA, page);

    gbox->addWidget(m_pEncodingAComboBox, line, 1);

    QString autoDetectToolTip = i18n(
        "If enabled then Unicode (UTF-16 or UTF-8) encoding will be detected.\n"
        "If the file is not Unicode then the selected encoding will be used as fallback.\n"
        "(Unicode detection depends on the first bytes of a file.)");
    m_pAutoDetectUnicodeA = new OptionCheckBox(i18n("Auto Detect Unicode"), true, "AutoDetectUnicodeA", &m_options->m_bAutoDetectUnicodeA, page);
    gbox->addWidget(m_pAutoDetectUnicodeA, line, 2);

    m_pAutoDetectUnicodeA->setToolTip(autoDetectToolTip);
    ++line;

    label = new QLabel(i18n("File Encoding for B:"), page);
    gbox->addWidget(label, line, 0);
    m_pEncodingBComboBox = new OptionEncodingComboBox("EncodingForB", &m_options->m_pEncodingB, page);

    gbox->addWidget(m_pEncodingBComboBox, line, 1);
    m_pAutoDetectUnicodeB = new OptionCheckBox(i18n("Auto Detect Unicode"), true, "AutoDetectUnicodeB", &m_options->m_bAutoDetectUnicodeB, page);

    gbox->addWidget(m_pAutoDetectUnicodeB, line, 2);
    m_pAutoDetectUnicodeB->setToolTip(autoDetectToolTip);
    ++line;

    label = new QLabel(i18n("File Encoding for C:"), page);
    gbox->addWidget(label, line, 0);
    m_pEncodingCComboBox = new OptionEncodingComboBox("EncodingForC", &m_options->m_pEncodingC, page);

    gbox->addWidget(m_pEncodingCComboBox, line, 1);
    m_pAutoDetectUnicodeC = new OptionCheckBox(i18n("Auto Detect Unicode"), true, "AutoDetectUnicodeC", &m_options->m_bAutoDetectUnicodeC, page);

    gbox->addWidget(m_pAutoDetectUnicodeC, line, 2);
    m_pAutoDetectUnicodeC->setToolTip(autoDetectToolTip);
    ++line;

    label = new QLabel(i18n("File Encoding for Merge Output and Saving:"), page);
    gbox->addWidget(label, line, 0);
    m_pEncodingOutComboBox = new OptionEncodingComboBox("EncodingForOutput", &m_options->m_pEncodingOut, page);

    gbox->addWidget(m_pEncodingOutComboBox, line, 1);
    m_pAutoSelectOutEncoding = new OptionCheckBox(i18n("Auto Select"), true, "AutoSelectOutEncoding", &m_options->m_bAutoSelectOutEncoding, page);

    gbox->addWidget(m_pAutoSelectOutEncoding, line, 2);
    m_pAutoSelectOutEncoding->setToolTip(i18n(
        "If enabled then the encoding from the input files is used.\n"
        "In ambiguous cases a dialog will ask the user to choose the encoding for saving."));
    ++line;
    label = new QLabel(i18n("File Encoding for Preprocessor Files:"), page);
    gbox->addWidget(label, line, 0);
    m_pEncodingPPComboBox = new OptionEncodingComboBox("EncodingForPP", &m_options->m_pEncodingPP, page);

    gbox->addWidget(m_pEncodingPPComboBox, line, 1);
    ++line;

    chk_connect(m_pSameEncoding, &OptionCheckBox::toggled, this, &OptionDialog::slotEncodingChanged);
    chk_connect(m_pEncodingAComboBox, static_cast<void (OptionEncodingComboBox::*)(qint32)>(&OptionEncodingComboBox::activated), this, &OptionDialog::slotEncodingChanged);
    chk_connect(m_pAutoDetectUnicodeA, &OptionCheckBox::toggled, this, &OptionDialog::slotEncodingChanged);
    chk_connect(m_pAutoSelectOutEncoding, &OptionCheckBox::toggled, this, &OptionDialog::slotEncodingChanged);

    OptionCheckBox* pRightToLeftLanguage = new OptionCheckBox(i18n("Right To Left Language"), false, "RightToLeftLanguage", &m_options->m_bRightToLeftLanguage, page);

    gbox->addWidget(pRightToLeftLanguage, line, 0, 1, 2);
    pRightToLeftLanguage->setToolTip(i18n(
        "Some languages are read from right to left.\n"
        "This setting will change the viewer and editor accordingly."));
    ++line;

    topLayout->addStretch(10);
}

void OptionDialog::setupIntegrationPage()
{
    QScrollArea* pageFrame = new QScrollArea();
    KPageWidgetItem* pageItem = new KPageWidgetItem(pageFrame, i18n("Integration"));
    pageItem->setHeader(i18n("Integration Settings"));
    pageItem->setIcon(QIcon::fromTheme(QStringLiteral("utilities-terminal")));
    addPage(pageItem);

    QVBoxLayout* scrollLayout = new QVBoxLayout();
    scrollLayout->setContentsMargins(2, 2, 2, 2);
    scrollLayout->addWidget(pageFrame);

    QScopedPointer<Ui::ScrollArea> scrollArea(new Ui::ScrollArea());
    scrollArea->setupUi(pageFrame);

    QWidget* page = pageFrame->findChild<QWidget*>("contents");
    QVBoxLayout* topLayout = new QVBoxLayout(page);
    topLayout->setContentsMargins(5, 5, 5, 5);

    QGridLayout* gbox = new QGridLayout();
    gbox->setColumnStretch(2, 5);
    topLayout->addLayout(gbox);
    qint32 line = 0;

    QLabel* label;
    label = new QLabel(i18n("Command line options to ignore:"), page);
    gbox->addWidget(label, line, 0);
    OptionLineEdit* pIgnorableCmdLineOptions = new OptionLineEdit("-u;-query;-html;-abort", "IgnorableCmdLineOptions", &m_options->m_ignorableCmdLineOptions, page);
    gbox->addWidget(pIgnorableCmdLineOptions, line, 1, 1, 2);

    label->setToolTip(i18n(
        "List of command line options that should be ignored when KDiff3 is used by other tools.\n"
        "Several values can be specified if separated via ';'\n"
        "This will suppress the \"Unknown option\" error."));
    ++line;

    OptionCheckBox* pEscapeKeyQuits = new OptionCheckBox(i18n("Quit also via Escape key"), false, "EscapeKeyQuits", &m_options->m_bEscapeKeyQuits, page);
    gbox->addWidget(pEscapeKeyQuits, line, 0, 1, 2);

    pEscapeKeyQuits->setToolTip(i18n(
        "Fast method to exit.\n"
        "For those who are used to using the Escape key."));
    ++line;

    topLayout->addStretch(10);
}

void OptionDialog::slotEncodingChanged()
{
    if(m_pSameEncoding->isChecked())
    {
        m_pEncodingBComboBox->setEnabled(false);
        m_pEncodingBComboBox->setCurrentIndex(m_pEncodingAComboBox->currentIndex());
        m_pEncodingCComboBox->setEnabled(false);
        m_pEncodingCComboBox->setCurrentIndex(m_pEncodingAComboBox->currentIndex());
        m_pEncodingOutComboBox->setEnabled(false);
        m_pEncodingOutComboBox->setCurrentIndex(m_pEncodingAComboBox->currentIndex());
        m_pEncodingPPComboBox->setEnabled(false);
        m_pEncodingPPComboBox->setCurrentIndex(m_pEncodingAComboBox->currentIndex());
        m_pAutoDetectUnicodeB->setEnabled(false);
        m_pAutoDetectUnicodeB->setCheckState(m_pAutoDetectUnicodeA->checkState());
        m_pAutoDetectUnicodeC->setEnabled(false);
        m_pAutoDetectUnicodeC->setCheckState(m_pAutoDetectUnicodeA->checkState());
        m_pAutoSelectOutEncoding->setEnabled(false);
        m_pAutoSelectOutEncoding->setCheckState(m_pAutoDetectUnicodeA->checkState());
    }
    else
    {
        m_pEncodingBComboBox->setEnabled(true);
        m_pEncodingCComboBox->setEnabled(true);
        m_pEncodingOutComboBox->setEnabled(true);
        m_pEncodingPPComboBox->setEnabled(true);
        m_pAutoDetectUnicodeB->setEnabled(true);
        m_pAutoDetectUnicodeC->setEnabled(true);
        m_pAutoSelectOutEncoding->setEnabled(true);
        m_pEncodingOutComboBox->setEnabled(m_pAutoSelectOutEncoding->checkState() == Qt::Unchecked);
    }
}

void OptionDialog::slotOk()
{
    slotApply();

    accept();
}

/** Copy the values from the widgets to the public variables.*/
void OptionDialog::slotApply()
{
    Options::apply();

    Q_EMIT applyDone();
}

/** Set the default values in the widgets only, while the
    public variables remain unchanged. */
void OptionDialog::slotDefault()
{
    qint32 result = KMessageBox::warningContinueCancel(this, i18n("This resets all options. Not only those of the current topic."));
    if(result == KMessageBox::Cancel)
        return;
    else
        resetToDefaults();
}

void OptionDialog::resetToDefaults()
{
    Options::resetToDefaults();

    slotEncodingChanged();
}

/** Initialise the widgets using the values in the public varibles. */
void OptionDialog::setState()
{
    Options::setToCurrent();

    slotEncodingChanged();
}

void OptionDialog::saveOptions(KSharedConfigPtr config)
{
    // No i18n()-Translations here!
    m_options->saveOptions(config);
}

void OptionDialog::readOptions(KSharedConfigPtr config)
{
    // No i18n()-Translations here!
    m_options->readOptions(config);

    setState();
}

const QString OptionDialog::parseOptions(const QStringList& optionList)
{

    return m_options->parseOptions(optionList);
}

QString OptionDialog::calcOptionHelp()
{
    return m_options->calcOptionHelp();
}

void OptionDialog::slotHistoryMergeRegExpTester()
{
    QPointer<RegExpTester> dlg = QPointer<RegExpTester>(new RegExpTester(this, s_autoMergeRegExpToolTip, s_historyStartRegExpToolTip,
                                                                         s_historyEntryStartRegExpToolTip, s_historyEntryStartSortKeyOrderToolTip));
    dlg->init(m_pAutoMergeRegExpLineEdit->currentText(), m_pHistoryStartRegExpLineEdit->currentText(),
              m_pHistoryEntryStartRegExpLineEdit->currentText(), m_pHistorySortKeyOrderLineEdit->currentText());
    if(dlg->exec())
    {
        m_pAutoMergeRegExpLineEdit->setEditText(dlg->autoMergeRegExp());
        m_pHistoryStartRegExpLineEdit->setEditText(dlg->historyStartRegExp());
        m_pHistoryEntryStartRegExpLineEdit->setEditText(dlg->historyEntryStartRegExp());
        m_pHistorySortKeyOrderLineEdit->setEditText(dlg->historySortKeyOrder());
    }
}

#include "optiondialog.moc"
