// mainwindow.cpp - see mainwindow.h.
#include "mainwindow.h"

#include "gamedetect.h"
#include "gamesettings.h"
#include "launcher.h"
#include "platform.h"
#include "stfs/stfspackage.h"
#include "xctdprep.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QCoreApplication>
#include <QProcess>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QThread>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>

namespace {
// Data root override key - normally unset, so dataRoot == game root.
const QString kDataRootKey = QStringLiteral("paths/data_root");

// One settings row: label + control, with a short muted explanation
// under the control. Visible microcopy on purpose - a wall of
// unexplained knobs is how settings pages become superstition.
void addSettingRow(QFormLayout *form, const QString &label, QWidget *control,
                   const QString &description)
{
    form->addRow(label, control);
    if (!description.isEmpty()) {
        auto *desc = new QLabel(description, control->parentWidget());
        desc->setWordWrap(true);
        desc->setObjectName(QStringLiteral("descLabel")); // theme-colored
        form->addRow(QString(), desc);
    }
}

// A path field with a Browse... button, as one form-row widget.
QWidget *pathField(QWidget *parent, QLineEdit *edit, QPushButton *browse)
{
    auto *holder = new QWidget(parent);
    auto *row = new QHBoxLayout(holder);
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(edit, 1);
    row->addWidget(browse);
    return holder;
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), profile_(activeProfile())
{
    setWindowTitle(profile_.launcherName);
    resize(640, 540);

    settings_ = GameSettings::load();
    gameRoot_ = resolveGameRoot(profile_);
    dataRoot_ = settingsObject()->value(kDataRootKey).toString();
    if (dataRoot_.isEmpty())
        dataRoot_ = gameRoot_;

    auto *tabs = new QTabWidget(this);
    setCentralWidget(tabs);

    // ---------------- Game tab ----------------
    auto *gameTab = new QWidget(tabs);
    gameTab->setObjectName(QStringLiteral("gameTab"));
    auto *gameLayout = new QVBoxLayout(gameTab);
    // Themed banner header (art embedded via resources.qrc). The
    // full picture is scaled to the label's width in refreshBanner(),
    // which also sets the label height to match - nothing is cropped.
    bannerLabel_ = new QLabel(gameTab);
    bannerLabel_->setObjectName(QStringLiteral("bannerLabel"));
    bannerLabel_->setAlignment(Qt::AlignCenter);
    bannerPixmap_.load(QStringLiteral(":/assets/banner.png"));
    gameLayout->addWidget(bannerLabel_);
    statusLabel_ = new QLabel(gameTab);
    statusLabel_->setObjectName(QStringLiteral("statusLabel"));
    rootLabel_ = new QLabel(gameTab);
    rootLabel_->setWordWrap(true);
    playButton_ = new QPushButton(QStringLiteral("PLAY"), gameTab);
    playButton_->setObjectName(QStringLiteral("playButton"));
#if defined(TP_GAME_LOLLIPOP)
    importButton_ = new QPushButton(QStringLiteral("Import game data (ISO / folder)..."), gameTab);
#elif defined(TP_GAME_BEJEWELED)
    importButton_ = new QPushButton(QStringLiteral("Import game data (XBLA package / folder)..."), gameTab);
#else
    importButton_ = new QPushButton(QStringLiteral("Import XBLA package..."), gameTab);
#endif
    commandLabel_ = new QLabel(gameTab);
    commandLabel_->setObjectName(QStringLiteral("commandLabel"));
    commandLabel_->setWordWrap(true);
    commandLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    gameLayout->addWidget(statusLabel_);
    gameLayout->addWidget(rootLabel_);
    gameLayout->addWidget(playButton_);
    gameLayout->addWidget(importButton_);
    gameLayout->addWidget(commandLabel_);
    gameLayout->addStretch();
    tabs->addTab(gameTab, QStringLiteral("Game"));
    connect(playButton_, &QPushButton::clicked, this, &MainWindow::onPlayClicked);
    connect(importButton_, &QPushButton::clicked, this, &MainWindow::onImportClicked);

    // ---------------- Settings tab ----------------
    // Every option the runtime accepts, in four sections. The page
    // scrolls; each row explains itself in one muted sentence.
    auto *scroll = new QScrollArea(tabs);
    scroll->setWidgetResizable(true);
    auto *page = new QWidget(scroll);
    page->setObjectName(QStringLiteral("settingsPage"));
    auto *pageLayout = new QVBoxLayout(page);

    // --- Graphics ---
    auto *graphicsBox = new QGroupBox(QStringLiteral("Graphics"), page);
    auto *graphics = new QFormLayout(graphicsBox);
    resolutionCombo_ = new QComboBox(graphicsBox);
    // Item data is the runtime's integer scale multiplier over the
    // 720p internal resolution - the scheme the 'Splosion launcher
    // proved (720p = 1x, 1440p = 2x, 2160p = 3x).
    resolutionCombo_->addItem(QStringLiteral("720p (1x)"), 1);
    resolutionCombo_->addItem(QStringLiteral("1440p (2x)"), 2);
    resolutionCombo_->addItem(QStringLiteral("2160p (3x)"), 3);
    upscalingCombo_ = new QComboBox(graphicsBox);
    upscalingCombo_->addItems({QStringLiteral("FSR"), QStringLiteral("Bilinear")});
    postEffectCombo_ = new QComboBox(graphicsBox);
    // Item data is the runtime's swap_post_effect value string.
    postEffectCombo_->addItem(QStringLiteral("None"), QStringLiteral("none"));
    postEffectCombo_->addItem(QStringLiteral("FXAA"), QStringLiteral("fxaa"));
    postEffectCombo_->addItem(QStringLiteral("FXAA Extreme"),
                              QStringLiteral("fxaa_extreme"));
    anisotropicCombo_ = new QComboBox(graphicsBox);
    // Item data is the setting's value: the filtering factor, 0 = the
    // game's own (launcher.cpp maps it to the runtime's level).
    anisotropicCombo_->addItem(QStringLiteral("Game's own"), 0);
    anisotropicCombo_->addItem(QStringLiteral("2x"), 2);
    anisotropicCombo_->addItem(QStringLiteral("4x"), 4);
    anisotropicCombo_->addItem(QStringLiteral("8x"), 8);
    anisotropicCombo_->addItem(QStringLiteral("16x"), 16);
    nativeMsaaCheck_ = new QCheckBox(graphicsBox);
    asyncShadersCheck_ = new QCheckBox(graphicsBox);
    pipelineThreadsSpin_ = new QSpinBox(graphicsBox);
    pipelineThreadsSpin_->setRange(0, 32);
    pipelineThreadsSpin_->setSpecialValueText(QStringLiteral("Default"));
    vsyncCheck_ = new QCheckBox(graphicsBox);
#if defined(TP_GAME_LOLLIPOP)
    fps60Check_ = new QCheckBox(graphicsBox);
#endif
    frameLimitSpin_ = new QSpinBox(graphicsBox);
    frameLimitSpin_->setRange(0, 1000);
    frameLimitSpin_->setSpecialValueText(QStringLiteral("Unlimited"));
    frameLimitSpin_->setSuffix(QStringLiteral(" fps"));
    windowedCheck_ = new QCheckBox(graphicsBox);

    addSettingRow(graphics, QStringLiteral("Resolution scale"), resolutionCombo_,
                  QStringLiteral("Internal render resolution: 1x = 720p, 2x = 1440p, "
                                 "3x = 2160p. Higher is sharper and costs GPU."));
    addSettingRow(graphics, QStringLiteral("Upscaling"), upscalingCombo_,
                  QStringLiteral("How the image is upscaled to your display. AMD FSR "
                                 "sharpens while upscaling; Bilinear is softer and cheaper."));
    addSettingRow(graphics, QStringLiteral("FXAA"), postEffectCombo_,
                  QStringLiteral("Post-process anti-aliasing that smooths jagged edges. "
                                 "FXAA Extreme is stronger and a touch softer."));
    addSettingRow(graphics, QStringLiteral("Anisotropic filtering"), anisotropicCombo_,
                  QStringLiteral("Texture filtering for surfaces at an angle (floors, "
                                 "roads). 16x is sharpest; Game's own leaves each "
                                 "texture's filtering as the game set it."));
    addSettingRow(graphics, QStringLiteral("Native 2x MSAA"), nativeMsaaCheck_,
                  QStringLiteral("Renders with 2x multisample anti-aliasing. Cleaner "
                                 "edges than FXAA at a higher GPU cost."));
    addSettingRow(graphics, QStringLiteral("Async shader compilation"), asyncShadersCheck_,
                  QStringLiteral("Compile shaders in the background. On: fewer long "
                                 "hitches, brief pop-in possible while shaders finish. "
                                 "Off: everything compiles up front."));
    addSettingRow(graphics, QStringLiteral("Pipeline creation threads"), pipelineThreadsSpin_,
                  QStringLiteral("CPU threads used to build graphics pipelines. 0 = "
                                 "runtime default. More can reduce load-time hitching "
                                 "on many-core CPUs."));
    addSettingRow(graphics, QStringLiteral("VSync"), vsyncCheck_,
                  QStringLiteral("Match frame output to your display's refresh. On: no "
                                 "tearing, slightly more input lag. Off: fastest, may tear."));
#if defined(TP_GAME_LOLLIPOP)
    addSettingRow(graphics, QStringLiteral("60 FPS unlock"), fps60Check_,
                  QStringLiteral("Run at 60 frames per second instead of the original "
                                 "Xbox 360 cap of 30, using the recomp's own fps hook. "
                                 "Pacing follows the game's frame divider."));
#endif
    addSettingRow(graphics, QStringLiteral("Frame limit"), frameLimitSpin_,
                  QStringLiteral("Cap the frame rate. 0 = unlimited. Matching your "
                                 "monitor's refresh (e.g. 75) is a good middle ground."));
    addSettingRow(graphics, QStringLiteral("Windowed"), windowedCheck_,
                  QStringLiteral("Run fullscreen or in a window."));
    pageLayout->addWidget(graphicsBox);

    // --- Audio ---
    auto *audioBox = new QGroupBox(QStringLiteral("Audio"), page);
    auto *audio = new QFormLayout(audioBox);
    audioGainSpin_ = new QDoubleSpinBox(audioBox);
    audioGainSpin_->setRange(0.0, 2.0);
    audioGainSpin_->setSingleStep(0.05);
    audioGainSpin_->setDecimals(2);
    audioHighpassSpin_ = new QSpinBox(audioBox);
    audioHighpassSpin_->setRange(0, 500);
    audioHighpassSpin_->setSuffix(QStringLiteral(" Hz"));
    forceStereoCheck_ = new QCheckBox(audioBox);
    frontOnlyCheck_ = new QCheckBox(audioBox);
    audioMaxQFramesSpin_ = new QSpinBox(audioBox);
    audioMaxQFramesSpin_->setRange(0, 1024);
    audioMaxQFramesSpin_->setSpecialValueText(QStringLiteral("Default"));
    xmaWorkerCheck_ = new QCheckBox(audioBox);

    addSettingRow(audio, QStringLiteral("Audio gain"), audioGainSpin_,
                  QStringLiteral("Master volume. 1.00 is the game's original level."));
#if defined(TP_HOUSE_AUDIO)
    // This game's runtime has no highpass, front-only or XMA worker
    // flags; the widgets stay (settings load/save is shared) but hidden.
    audioHighpassSpin_->hide();
    frontOnlyCheck_->hide();
    xmaWorkerCheck_->hide();
#else
    addSettingRow(audio, QStringLiteral("Audio highpass"), audioHighpassSpin_,
                  QStringLiteral("Removes bass/rumble below this frequency. 0 = full "
                                 "range, filter off."));
#endif
    addSettingRow(audio, QStringLiteral("Force stereo"), forceStereoCheck_,
                  QStringLiteral("Downmix surround to stereo. Useful on stereo "
                                 "speakers/headphones with odd channel mapping."));
#if !defined(TP_HOUSE_AUDIO)
    addSettingRow(audio, QStringLiteral("Front speakers only"), frontOnlyCheck_,
                  QStringLiteral("Play audio only from the front speakers. A "
                                 "workaround for broken surround setups."));
#endif
    addSettingRow(audio, QStringLiteral("Audio queue frames"), audioMaxQFramesSpin_,
                  QStringLiteral("Audio buffering depth. 0 = runtime default. Raise "
                                 "it if you hear crackling or dropouts."));
#if !defined(TP_HOUSE_AUDIO)
    addSettingRow(audio, QStringLiteral("XMA worker thread"), xmaWorkerCheck_,
                  QStringLiteral("Decode the game's XMA audio on a worker thread. "
                                 "Leave on unless audio misbehaves."));
#endif
    pageLayout->addWidget(audioBox);

    // --- Storage & logs ---
    auto *storageBox = new QGroupBox(QStringLiteral("Storage & Logs"), page);
    auto *storage = new QFormLayout(storageBox);
    userDataRootEdit_ = new QLineEdit(storageBox);
    userDataRootEdit_->setPlaceholderText(QStringLiteral("(game default)"));
    auto *browseUserData = new QPushButton(QStringLiteral("Browse..."), storageBox);
    openDataButton_ = new QPushButton(QStringLiteral("Open data folder"), storageBox);
    logDirEdit_ = new QLineEdit(storageBox);
    logDirEdit_->setPlaceholderText(QStringLiteral("(logs folder next to the game data)"));
    auto *browseLogDir = new QPushButton(QStringLiteral("Browse..."), storageBox);
    logLevelCombo_ = new QComboBox(storageBox);
    // Item data is the runtime's log_level value string.
    logLevelCombo_->addItem(QStringLiteral("Info"), QStringLiteral("info"));
    logLevelCombo_->addItem(QStringLiteral("Off"), QStringLiteral("off"));
    logLevelCombo_->addItem(QStringLiteral("Debug"), QStringLiteral("debug"));

    addSettingRow(storage, QStringLiteral("Save data folder"),
                  pathField(storageBox, userDataRootEdit_, browseUserData),
                  QStringLiteral("Where saves and profile data live. Empty = the "
                                 "game's default (~/.local/share). Changing this "
                                 "starts with a fresh profile - your old saves stay "
                                 "in the previous folder, untouched."));
    storage->addRow(QString(), openDataButton_);
    addSettingRow(storage, QStringLiteral("Log folder"),
                  pathField(storageBox, logDirEdit_, browseLogDir),
                  QStringLiteral("Where the game's log file goes. Empty = a logs "
                                 "folder next to the game data."));
    addSettingRow(storage, QStringLiteral("Log level"), logLevelCombo_,
                  QStringLiteral("How much the game writes to its log. Debug is very "
                                 "verbose and can slow the game; use it when diagnosing."));
    auto *settingsFileLabel = new QLabel(
        QStringLiteral("Settings file: %1").arg(settingsObject()->fileName()),
        storageBox);
    settingsFileLabel->setWordWrap(true);
    settingsFileLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    settingsFileLabel->setStyleSheet(QStringLiteral("color: #7f7f7f;"));
    storage->addRow(QString(), settingsFileLabel);
    pageLayout->addWidget(storageBox);

    connect(browseUserData, &QPushButton::clicked, this, &MainWindow::onBrowseUserDataRoot);
    connect(browseLogDir, &QPushButton::clicked, this, &MainWindow::onBrowseLogDir);
    connect(openDataButton_, &QPushButton::clicked, this, &MainWindow::onOpenDataFolder);

    // --- Advanced ---
    auto *advancedBox = new QGroupBox(QStringLiteral("Advanced"), page);
    auto *advanced = new QFormLayout(advancedBox);
    sparseMemoryCheck_ = new QCheckBox(advancedBox);
    asyncSkipCheck_ = new QCheckBox(advancedBox);
    userLanguageSpin_ = new QSpinBox(advancedBox);
    userLanguageSpin_->setRange(0, 99999);
    userLanguageSpin_->setSpecialValueText(QStringLiteral("Default"));
    userCountrySpin_ = new QSpinBox(advancedBox);
    userCountrySpin_->setRange(0, 999);
    userCountrySpin_->setSpecialValueText(QStringLiteral("Default"));

    addSettingRow(advanced, QStringLiteral("Sparse shared memory"), sparseMemoryCheck_,
                  QStringLiteral("Vulkan sparse shared memory. Leave OFF: it can "
                                 "black-screen on AMD/RADV. Only for experimenting."));
    addSettingRow(advanced, QStringLiteral("Skip incomplete frames"), asyncSkipCheck_,
                  QStringLiteral("Present frames before their shaders finish "
                                 "compiling. Can stall this engine family at the "
                                 "title screen - leave OFF unless testing."));
    addSettingRow(advanced, QStringLiteral("User language"), userLanguageSpin_,
                  QStringLiteral("Locale ID the game sees for its profile language. "
                                 "0 = runtime default."));
    addSettingRow(advanced, QStringLiteral("User country"), userCountrySpin_,
                  QStringLiteral("Country ID the game sees. 0 = runtime default."));
    pageLayout->addWidget(advancedBox);

    // Reset restores the GameSettings struct defaults (gamesettings.h)
    // and persists them immediately. The stored game root is separate
    // state (gamedetect) and is NOT touched.
    resetButton_ = new QPushButton(QStringLiteral("Reset to defaults"), page);
    pageLayout->addWidget(resetButton_);
    pageLayout->addStretch();
    connect(resetButton_, &QPushButton::clicked, this, &MainWindow::onResetDefaultsClicked);

    scroll->setWidget(page);
    tabs->addTab(scroll, QStringLiteral("Settings"));

    loadSettingsIntoUi();

    // Persist on any change - no Apply button to forget.
    connect(resolutionCombo_, &QComboBox::currentTextChanged, this, &MainWindow::onSettingsChanged);
    connect(upscalingCombo_, &QComboBox::currentTextChanged, this, &MainWindow::onSettingsChanged);
    connect(postEffectCombo_, &QComboBox::currentIndexChanged, this, &MainWindow::onSettingsChanged);
    connect(anisotropicCombo_, &QComboBox::currentIndexChanged, this, &MainWindow::onSettingsChanged);
    connect(nativeMsaaCheck_, &QCheckBox::toggled, this, &MainWindow::onSettingsChanged);
    connect(asyncShadersCheck_, &QCheckBox::toggled, this, &MainWindow::onSettingsChanged);
    connect(pipelineThreadsSpin_, &QSpinBox::valueChanged, this, &MainWindow::onSettingsChanged);
    connect(vsyncCheck_, &QCheckBox::toggled, this, &MainWindow::onSettingsChanged);
#if defined(TP_GAME_LOLLIPOP)
    connect(fps60Check_, &QCheckBox::toggled, this, &MainWindow::onSettingsChanged);
#endif
    connect(frameLimitSpin_, &QSpinBox::valueChanged, this, &MainWindow::onSettingsChanged);
    connect(windowedCheck_, &QCheckBox::toggled, this, &MainWindow::onSettingsChanged);
    connect(audioGainSpin_, &QDoubleSpinBox::valueChanged, this, &MainWindow::onSettingsChanged);
    connect(audioHighpassSpin_, &QSpinBox::valueChanged, this, &MainWindow::onSettingsChanged);
    connect(forceStereoCheck_, &QCheckBox::toggled, this, &MainWindow::onSettingsChanged);
    connect(frontOnlyCheck_, &QCheckBox::toggled, this, &MainWindow::onSettingsChanged);
    connect(audioMaxQFramesSpin_, &QSpinBox::valueChanged, this, &MainWindow::onSettingsChanged);
    connect(xmaWorkerCheck_, &QCheckBox::toggled, this, &MainWindow::onSettingsChanged);
    connect(userDataRootEdit_, &QLineEdit::textChanged, this, &MainWindow::onSettingsChanged);
    connect(logDirEdit_, &QLineEdit::textChanged, this, &MainWindow::onSettingsChanged);
    connect(logLevelCombo_, &QComboBox::currentIndexChanged, this, &MainWindow::onSettingsChanged);
    connect(sparseMemoryCheck_, &QCheckBox::toggled, this, &MainWindow::onSettingsChanged);
    connect(asyncSkipCheck_, &QCheckBox::toggled, this, &MainWindow::onSettingsChanged);
    connect(userLanguageSpin_, &QSpinBox::valueChanged, this, &MainWindow::onSettingsChanged);
    connect(userCountrySpin_, &QSpinBox::valueChanged, this, &MainWindow::onSettingsChanged);

    refreshGameStatus();
}

void MainWindow::loadSettingsIntoUi()
{
    // Block signals so loading doesn't immediately re-save.
    const QSignalBlocker blockers[] = {
        QSignalBlocker(resolutionCombo_), QSignalBlocker(upscalingCombo_),
        QSignalBlocker(postEffectCombo_), QSignalBlocker(anisotropicCombo_),
        QSignalBlocker(nativeMsaaCheck_), QSignalBlocker(asyncShadersCheck_),
        QSignalBlocker(pipelineThreadsSpin_), QSignalBlocker(vsyncCheck_),
#if defined(TP_GAME_LOLLIPOP)
        QSignalBlocker(fps60Check_),
#endif
        QSignalBlocker(frameLimitSpin_),  QSignalBlocker(windowedCheck_),
        QSignalBlocker(audioGainSpin_),   QSignalBlocker(audioHighpassSpin_),
        QSignalBlocker(forceStereoCheck_), QSignalBlocker(frontOnlyCheck_),
        QSignalBlocker(audioMaxQFramesSpin_), QSignalBlocker(xmaWorkerCheck_),
        QSignalBlocker(userDataRootEdit_), QSignalBlocker(logDirEdit_),
        QSignalBlocker(logLevelCombo_),   QSignalBlocker(sparseMemoryCheck_),
        QSignalBlocker(asyncSkipCheck_),  QSignalBlocker(userLanguageSpin_),
        QSignalBlocker(userCountrySpin_),
    };
    Q_UNUSED(blockers);

    // A stored value that matches no combo item falls back to the
    // first item, so what the user sees is what collectSettings()
    // will return. Combos with value semantics match on item data -
    // the stored value itself, not the display label.
    const int resIndex = resolutionCombo_->findData(settings_.resolutionScale);
    if (resIndex < 0)
        settings_.resolutionScale = resolutionCombo_->itemData(0).toInt();
    resolutionCombo_->setCurrentIndex(resIndex < 0 ? 0 : resIndex);
    if (upscalingCombo_->findText(settings_.upscaling) < 0)
        settings_.upscaling = upscalingCombo_->itemText(0);
    upscalingCombo_->setCurrentText(settings_.upscaling);
    const int postIndex = postEffectCombo_->findData(settings_.swapPostEffect);
    if (postIndex < 0)
        settings_.swapPostEffect = postEffectCombo_->itemData(0).toString();
    postEffectCombo_->setCurrentIndex(postIndex < 0 ? 0 : postIndex);
    const int anisoIndex = anisotropicCombo_->findData(settings_.anisotropic);
    if (anisoIndex < 0)
        settings_.anisotropic = anisotropicCombo_->itemData(0).toInt();
    anisotropicCombo_->setCurrentIndex(anisoIndex < 0 ? 0 : anisoIndex);
    nativeMsaaCheck_->setChecked(settings_.native2xMsaa);
    asyncShadersCheck_->setChecked(settings_.asyncShaderCompilation);
    pipelineThreadsSpin_->setValue(settings_.pipelineThreads);
    vsyncCheck_->setChecked(settings_.vsync);
#if defined(TP_GAME_LOLLIPOP)
    fps60Check_->setChecked(settings_.fps60);
#endif
    frameLimitSpin_->setValue(settings_.frameLimit);
    windowedCheck_->setChecked(settings_.windowed);
    audioGainSpin_->setValue(settings_.audioGain);
    audioHighpassSpin_->setValue(settings_.audioHighpassHz);
    forceStereoCheck_->setChecked(settings_.audioForceStereo);
    frontOnlyCheck_->setChecked(settings_.audioFrontOnly);
    audioMaxQFramesSpin_->setValue(settings_.audioMaxQFrames);
    xmaWorkerCheck_->setChecked(settings_.xmaWorker);
    userDataRootEdit_->setText(settings_.userDataRoot);
    logDirEdit_->setText(settings_.logDir);
    const int levelIndex = logLevelCombo_->findData(settings_.logLevel);
    if (levelIndex < 0)
        settings_.logLevel = logLevelCombo_->itemData(0).toString();
    logLevelCombo_->setCurrentIndex(levelIndex < 0 ? 0 : levelIndex);
    sparseMemoryCheck_->setChecked(settings_.sparseSharedMemory);
    asyncSkipCheck_->setChecked(settings_.asyncSkipIncompleteFrames);
    userLanguageSpin_->setValue(settings_.userLanguage);
    userCountrySpin_->setValue(settings_.userCountry);

    updateOpenDataButton();
}

GameSettings MainWindow::collectSettings() const
{
    GameSettings s;
    // Graphics
    s.resolutionScale = resolutionCombo_->currentData().toInt();
    s.upscaling = upscalingCombo_->currentText();
    s.swapPostEffect = postEffectCombo_->currentData().toString();
    s.anisotropic = anisotropicCombo_->currentData().toInt();
    s.native2xMsaa = nativeMsaaCheck_->isChecked();
    s.asyncShaderCompilation = asyncShadersCheck_->isChecked();
    s.pipelineThreads = pipelineThreadsSpin_->value();
    s.vsync = vsyncCheck_->isChecked();
#if defined(TP_GAME_LOLLIPOP)
    s.fps60 = fps60Check_->isChecked();
#endif
    s.frameLimit = frameLimitSpin_->value();
    s.windowed = windowedCheck_->isChecked();
    // Audio
    s.audioGain = audioGainSpin_->value();
    s.audioHighpassHz = audioHighpassSpin_->value();
    s.audioForceStereo = forceStereoCheck_->isChecked();
    s.audioFrontOnly = frontOnlyCheck_->isChecked();
    s.audioMaxQFrames = audioMaxQFramesSpin_->value();
    s.xmaWorker = xmaWorkerCheck_->isChecked();
    // Storage & logs
    s.userDataRoot = userDataRootEdit_->text().trimmed();
    s.logDir = logDirEdit_->text().trimmed();
    s.logLevel = logLevelCombo_->currentData().toString();
    // Advanced
    s.sparseSharedMemory = sparseMemoryCheck_->isChecked();
    s.asyncSkipIncompleteFrames = asyncSkipCheck_->isChecked();
    s.userLanguage = userLanguageSpin_->value();
    s.userCountry = userCountrySpin_->value();
    return s;
}

void MainWindow::onSettingsChanged()
{
    settings_ = collectSettings();
    settings_.save();
    updateOpenDataButton();
}

void MainWindow::onResetDefaultsClicked()
{
    // Struct defaults are the single source of truth; save() writes
    // them over the stored ones immediately. The stored game root
    // lives outside GameSettings and survives.
    settings_ = GameSettings();
    settings_.save();
    loadSettingsIntoUi();
}

void MainWindow::onBrowseUserDataRoot()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Save data folder"),
        userDataRootEdit_->text().isEmpty() ? QDir::homePath()
                                           : userDataRootEdit_->text());
    if (!dir.isEmpty())
        userDataRootEdit_->setText(dir); // textChanged saves it
}

void MainWindow::onBrowseLogDir()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Log folder"),
        logDirEdit_->text().isEmpty() ? QDir::homePath() : logDirEdit_->text());
    if (!dir.isEmpty())
        logDirEdit_->setText(dir); // textChanged saves it
}

void MainWindow::onOpenDataFolder()
{
    // Only ever opens the folder the user explicitly chose (the
    // setting); the runtime's own default location is not guessed.
    const QString dir = userDataRootEdit_->text().trimmed();
    if (dir.isEmpty())
        return;
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void MainWindow::updateOpenDataButton()
{
    // Enabled only when a save-data folder is actually set.
    openDataButton_->setEnabled(!userDataRootEdit_->text().trimmed().isEmpty());
}

void MainWindow::refreshGameStatus()
{
    if (gameRoot_.isEmpty()) {
        statusLabel_->setText(QStringLiteral("%1: not found").arg(profile_.name));
        rootLabel_->clear();
        playButton_->setEnabled(false);
        return;
    }

    // The data root and the program set resolve independently (an
    // imported root is data-only); show where the program comes from
    // when it is not the data root itself.
    const ExeSetResolution exeSet = resolveExecutableSet(profile_, gameRoot_);
    if (!exeSet.found) {
        statusLabel_->setText(
            QStringLiteral("%1: program files not found").arg(profile_.name));
        rootLabel_->setText(
            QStringLiteral("%1\nSearched for the program set in: %2")
                .arg(gameRoot_, exeSet.searched.join(QStringLiteral(", "))));
        // PLAY stays enabled: it surfaces the full searched list.
        playButton_->setEnabled(true);
    } else if (exeSet.dir == QDir::cleanPath(gameRoot_)) {
        statusLabel_->setText(QStringLiteral("%1: ready").arg(profile_.name));
        rootLabel_->setText(gameRoot_);
        playButton_->setEnabled(true);
    } else {
        statusLabel_->setText(QStringLiteral("%1: ready \u2014 data %2, program %3")
                                  .arg(profile_.name, gameRoot_, exeSet.dir));
        rootLabel_->setText(gameRoot_);
        playButton_->setEnabled(true);
    }
}

void MainWindow::onPlayClicked()
{
    if (gameRoot_.isEmpty())
        return;
    settings_ = collectSettings();
    settings_.save();

    if (dataRoot_.isEmpty())
        dataRoot_ = gameRoot_;
    const LaunchPlan plan = buildLaunchPlan(profile_, settings_, gameRoot_, dataRoot_);

    platform::vacuumGuestMemory();

    QString error;
    if (executeLaunchPlan(plan, &error)) {
        commandLabel_->setText(QStringLiteral("Launched: %1").arg(plan.commandLine));
    } else {
        commandLabel_->setText(QStringLiteral("Launch failed: %1").arg(error));
    }
}

void MainWindow::onImportClicked()
{
#if defined(TP_GAME_LOLLIPOP)
    // Lollipop Chainsaw is a disc title: game data is an extracted
    // disc folder (default.xex + AshGame/ + Engine/), not an STFS
    // package. Two ways in: use a folder that already holds the
    // extracted game (validated, remembered, nothing copied), or pick
    // an ISO and extract it with extract-xiso into a new folder.
    QMessageBox choice(this);
    choice.setWindowTitle(QStringLiteral("Import game data"));
    choice.setText(QStringLiteral(
        "Import from an ISO image (extracts it into a folder you "
        "choose), or use a folder that already holds the extracted "
        "game?"));
    auto *isoButton = choice.addButton(QStringLiteral("ISO image..."),
                                       QMessageBox::ActionRole);
    auto *folderButton = choice.addButton(QStringLiteral("Extracted folder..."),
                                          QMessageBox::ActionRole);
    choice.addButton(QMessageBox::Cancel);
    choice.exec();

    if (choice.clickedButton() == folderButton) {
        const QString dir = QFileDialog::getExistingDirectory(
            this, QStringLiteral("Select extracted game folder"),
            QDir::homePath());
        if (dir.isEmpty())
            return;
        if (!isGameRoot(profile_, dir)) {
            QMessageBox::warning(
                this, QStringLiteral("Import game data"),
                QStringLiteral("That folder does not hold Lollipop Chainsaw "
                               "game data (default.xex for title %1 not "
                               "found).").arg(profile_.titleId));
            return;
        }
        storeGameRoot(profile_, dir);
        gameRoot_ = dir;
        dataRoot_ = dir;
        refreshGameStatus();
        return;
    }
    if (choice.clickedButton() != isoButton)
        return;

    const QString iso = QFileDialog::getOpenFileName(
        this, QStringLiteral("Select Xbox 360 ISO"), QDir::homePath(),
        QStringLiteral("ISO images (*.iso);;All files (*)"));
    if (iso.isEmpty())
        return;
    const QString parent = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Extract ISO into"), QDir::homePath());
    if (parent.isEmpty())
        return;
    const QString dest = QDir(parent).filePath(QStringLiteral("LollipopChainsaw"));
    QDir().mkpath(dest);

    QString xiso = QStandardPaths::findExecutable(QStringLiteral("extract-xiso"));
    if (xiso.isEmpty()) {
        const QString beside = QDir(QCoreApplication::applicationDirPath())
                                   .filePath(QStringLiteral("extract-xiso"));
        if (QFile::exists(beside))
            xiso = beside;
    }
    if (xiso.isEmpty()) {
        QMessageBox::warning(
            this, QStringLiteral("Import game data"),
            QStringLiteral("extract-xiso was not found. Install it, or use "
                           "the extracted-folder option instead."));
        return;
    }

    auto *progress = new QProgressDialog(QStringLiteral("Extracting ISO..."),
                                         QStringLiteral("Cancel"), 0, 0, this);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    auto *proc = new QProcess(this);
    importButton_->setEnabled(false);
    connect(progress, &QProgressDialog::canceled, proc, &QProcess::kill);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, progress, dest](int code, QProcess::ExitStatus status) {
                progress->close();
                importButton_->setEnabled(true);
                const QString out = QString::fromUtf8(proc->readAllStandardOutput() +
                                                      proc->readAllStandardError());
                proc->deleteLater();
                progress->deleteLater();
                if (status == QProcess::NormalExit && code == 0 &&
                    isGameRoot(profile_, dest)) {
                    storeGameRoot(profile_, dest);
                    gameRoot_ = dest;
                    dataRoot_ = dest;
                    refreshGameStatus();
                    QMessageBox::information(
                        this, QStringLiteral("Import game data"),
                        QStringLiteral("Game data imported to:\n%1").arg(dest));
                } else {
                    QMessageBox::warning(
                        this, QStringLiteral("Import game data"),
                        QStringLiteral("Extraction did not produce a valid game "
                                       "folder.\n\n%1").arg(out.right(800)));
                }
            });
    proc->setWorkingDirectory(dest);
    proc->start(xiso, {QStringLiteral("-x"), iso});
    progress->show();
#else
#if defined(TP_GAME_BEJEWELED)
    // Bejeweled Blitz Live: the game comes as a LIVE package (extracted
    // into a new folder) or as an already-extracted folder (used where it
    // is, see importExtractedFolder). Either way the XCTD content files
    // are decoded once (xctdprep.h).
    {
        QMessageBox choice(this);
        choice.setWindowTitle(QStringLiteral("Import game data"));
        choice.setText(QStringLiteral(
            "Import from the game's XBLA package file (extracted into a "
            "folder you choose), or use a folder that already holds the "
            "extracted game (Default.xex, blitz.pak, ...)?"));
        auto *packageButton = choice.addButton(QStringLiteral("XBLA package..."),
                                               QMessageBox::ActionRole);
        auto *folderButton = choice.addButton(QStringLiteral("Extracted folder..."),
                                              QMessageBox::ActionRole);
        choice.addButton(QMessageBox::Cancel);
        choice.exec();
        if (choice.clickedButton() == folderButton) {
            importExtractedFolder();
            return;
        }
        if (choice.clickedButton() != packageButton)
            return;
    }
#endif
    const QString packagePath = QFileDialog::getOpenFileName(
        this, QStringLiteral("Select XBLA package"), QDir::homePath(),
        QStringLiteral("All files (*)"));
    if (packagePath.isEmpty())
        return;

    // Validate on the GUI thread: parsing the header and file table
    // is a handful of block reads even for large packages, so a bad
    // file is reported immediately instead of inside the worker.
    {
        StfsPackage probe;
        QString error;
        if (!probe.open(packagePath, &error)) {
            QMessageBox::warning(
                this, QStringLiteral("Import XBLA package"),
                QStringLiteral("Could not read package:\n%1").arg(error));
            return;
        }
    }

    const QString destParent = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Extract package into"),
        QDir::home().filePath(QStringLiteral("Games")));
    if (destParent.isEmpty())
        return;

    // The tree lands at <dest>/<PackageName>/gamedata/<files> - the
    // layout detection expects of a game root, with the recomp
    // executable dropped next to gamedata/ by the user.
    const QString baseName = QFileInfo(packagePath).completeBaseName();
    const QString root = QDir(destParent).filePath(baseName);
    const QString dataDir = QDir(root).filePath(QStringLiteral("gamedata"));

    struct Result {
        bool ok = false;
        QString error;
        qint64 files = 0;
        quint64 bytes = 0;
    };
    auto result = std::make_shared<Result>();

    auto *dialog = new QProgressDialog(QStringLiteral("Extracting..."),
                                       QString(), 0, 0, this);
    dialog->setWindowTitle(QStringLiteral("Import XBLA package"));
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setCancelButton(nullptr);
    dialog->setMinimumDuration(0);
    dialog->show();
    importButton_->setEnabled(false);

    QThread *thread = QThread::create([packagePath, dataDir, dialog, result]() {
        StfsPackage pkg; // Own instance: StfsPackage is not shared
        QString error;
        if (!pkg.open(packagePath, &error)) {
            result->error = error;
            return;
        }
        const QVector<StfsPackage::Entry> list = pkg.entries();
        for (const StfsPackage::Entry &e : list)
            result->bytes += e.size;
        auto progress = [dialog](qint64 done, qint64 total, const QString &name) {
            QMetaObject::invokeMethod(
                dialog,
                [dialog, done, total, name]() {
                    dialog->setMaximum(int(total));
                    dialog->setValue(int(done));
                    if (!name.isEmpty())
                        dialog->setLabelText(
                            QStringLiteral("Extracting %1").arg(name));
                },
                Qt::QueuedConnection);
        };
        if (!pkg.extractAll(dataDir, progress, &error)) {
            result->error = error;
            return;
        }
#if defined(TP_GAME_BEJEWELED)
        auto status = [dialog](const QString &text) {
            QMetaObject::invokeMethod(
                dialog,
                [dialog, text]() {
                    dialog->setMaximum(0);
                    dialog->setLabelText(text);
                },
                Qt::QueuedConnection);
        };
        if (!prepareXctdData(dataDir, status, &error)) {
            result->error = error;
            return;
        }
#endif
        result->files = list.size();
        result->ok = true;
    });

    connect(thread, &QThread::finished, this,
            [this, thread, dialog, result, root, baseName]() {
                thread->deleteLater();
                dialog->close();
                dialog->deleteLater();
                importButton_->setEnabled(true);
                if (!result->ok) {
                    QMessageBox::warning(
                        this, QStringLiteral("Import XBLA package"),
                        QStringLiteral("Extraction failed:\n%1").arg(result->error));
                    refreshGameStatus();
                    return;
                }
                // Register the fresh tree as the game root when it
                // holds the game data detection looks for.
                const QDir rootDir(root);
                if (QFile::exists(rootDir.filePath(
                        QStringLiteral("gamedata/default.xex"))) ||
                    QFile::exists(rootDir.filePath(QStringLiteral("default.xex")))) {
                    storeGameRoot(profile_, root);
                    gameRoot_ = root;
                    if (settingsObject()->value(kDataRootKey).toString().isEmpty())
                        dataRoot_ = root;
                }
                refreshGameStatus();
                statusLabel_->setText(
                    QStringLiteral("Imported %1: %2 files, %3 MB extracted")
                        .arg(baseName)
                        .arg(result->files)
                        .arg(result->bytes / (1024 * 1024)));
            });
    thread->start();
#endif
}

#if defined(TP_GAME_BEJEWELED)
// Use an extracted game folder where it is. A folder that is already
// prepared is registered as-is. One that still holds XCTD-encoded content
// files is prepared in place after one confirmation (or, on request or
// when the folder is read-only, copied to a new <dest>/BejeweledBlitzLive/
// gamedata first). Decoding/copying runs on a worker thread.
void MainWindow::importExtractedFolder()
{
    const QString src = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Select extracted game folder"), QDir::homePath());
    if (src.isEmpty())
        return;
    const QDir srcDir(src);
    if (!QFile::exists(srcDir.filePath(QStringLiteral("default.xex"))) &&
        !QFile::exists(srcDir.filePath(QStringLiteral("Default.xex")))) {
        QMessageBox::warning(
            this, QStringLiteral("Import game data"),
            QStringLiteral("That folder does not hold the extracted game "
                           "(no Default.xex found)."));
        return;
    }

    const QStringList encoded = findXctdFiles(src);
    const bool needsXex = !QFile::exists(srcDir.filePath(QStringLiteral("default.xex")));
    const bool writable = QFileInfo(src).isWritable();

    QString root = src;    // registered game root
    bool copyFirst = false;
    if (!encoded.isEmpty() || (needsXex && !writable)) {
        QMessageBox ask(this);
        ask.setWindowTitle(QStringLiteral("Import game data"));
        if (writable) {
            QStringList names;
            for (const QString &path : encoded)
                names << srcDir.relativeFilePath(path);
            ask.setText(QStringLiteral(
                "%1 of the game's content files are compressed (%2). The "
                "Xbox 360 decompresses them on the fly; this port needs them "
                "decompressed once.\n\nDecompress them in this folder?")
                            .arg(encoded.size())
                            .arg(names.join(QStringLiteral(", "))));
        } else {
            ask.setText(QStringLiteral(
                "This folder is read-only and still needs one-time preparation "
                "(decompressing the game's content files). Make a prepared "
                "copy somewhere else?"));
        }
        QAbstractButton *inPlace = nullptr;
        if (writable)
            inPlace = ask.addButton(QStringLiteral("Decompress here"),
                                    QMessageBox::AcceptRole);
        auto *copyButton = ask.addButton(QStringLiteral("Make a copy..."),
                                         QMessageBox::ActionRole);
        ask.addButton(QMessageBox::Cancel);
        ask.setDefaultButton(qobject_cast<QPushButton *>(inPlace ? inPlace : copyButton));
        ask.exec();
        if (ask.clickedButton() == copyButton)
            copyFirst = true;
        else if (!inPlace || ask.clickedButton() != inPlace)
            return;
    }

    QString dataDir = src;
    if (copyFirst) {
        const QString destParent = QFileDialog::getExistingDirectory(
            this, QStringLiteral("Create the prepared copy in"),
            QDir::home().filePath(QStringLiteral("Games")));
        if (destParent.isEmpty())
            return;
        root = QDir(destParent).filePath(QStringLiteral("BejeweledBlitzLive"));
        dataDir = QDir(root).filePath(QStringLiteral("gamedata"));
        if (QFileInfo::exists(dataDir)) {
            QMessageBox::warning(
                this, QStringLiteral("Import game data"),
                QStringLiteral("%1 already exists. Choose another location or "
                               "remove it first.").arg(dataDir));
            return;
        }
    }

    struct Result {
        bool ok = false;
        QString error;
    };
    auto result = std::make_shared<Result>();
    auto *dialog = new QProgressDialog(QStringLiteral("Preparing..."), QString(), 0, 0, this);
    dialog->setWindowTitle(QStringLiteral("Import game data"));
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setCancelButton(nullptr);
    dialog->setMinimumDuration(0);
    dialog->show();
    importButton_->setEnabled(false);

    QThread *thread = QThread::create([src, dataDir, copyFirst, dialog, result]() {
        auto status = [dialog](const QString &text) {
            QMetaObject::invokeMethod(
                dialog, [dialog, text]() { dialog->setLabelText(text); },
                Qt::QueuedConnection);
        };
        QString error;
        if ((copyFirst && !copyTree(src, dataDir, status, &error)) ||
            !prepareXctdData(dataDir, status, &error)) {
            result->error = error;
            return;
        }
        result->ok = true;
    });
    const bool quiet = !copyFirst && encoded.isEmpty();
    connect(thread, &QThread::finished, this, [this, thread, dialog, result, root, quiet]() {
        thread->deleteLater();
        dialog->close();
        dialog->deleteLater();
        importButton_->setEnabled(true);
        if (!result->ok || !isGameRoot(profile_, root)) {
            QMessageBox::warning(
                this, QStringLiteral("Import game data"),
                QStringLiteral("Import failed:\n%1")
                    .arg(result->error.isEmpty()
                             ? QStringLiteral("the folder does not validate "
                                              "as Bejeweled Blitz Live (title %1).")
                                   .arg(profile_.titleId)
                             : result->error));
            refreshGameStatus();
            return;
        }
        storeGameRoot(profile_, root);
        gameRoot_ = root;
        if (settingsObject()->value(kDataRootKey).toString().isEmpty())
            dataRoot_ = root;
        refreshGameStatus();
        if (quiet)
            statusLabel_->setText(QStringLiteral("Using game data in %1").arg(root));
        else
            QMessageBox::information(this, QStringLiteral("Import game data"),
                                     QStringLiteral("Game data ready in:\n%1").arg(root));
    });
    thread->start();
}
#endif

// ---- Banner (theme header) ----
// Show the WHOLE source picture: scale it to the label's current
// width keeping the aspect ratio (shrink-only - never upscale past
// the native size), then set the label's fixed height to the scaled
// image height so the header is exactly the full picture.

void MainWindow::refreshBanner()
{
    if (!bannerLabel_ || bannerPixmap_.isNull())
        return;
    const int w = qMax(bannerLabel_->width(), 1);
    QPixmap scaled = bannerPixmap_;
    if (w < bannerPixmap_.width())
        scaled = bannerPixmap_.scaledToWidth(w, Qt::SmoothTransformation);
    const QSize target = scaled.size();
    // Setting a fresh pixmap invalidates the label's size hint,
    // which can retrigger layout + resize in a loop; only touch
    // the pixmap (and height) when the target size actually changed.
    if (target == bannerSize_ && !bannerLabel_->pixmap().isNull())
        return;
    bannerLabel_->setFixedHeight(target.height());
    bannerLabel_->setPixmap(scaled);
    bannerSize_ = target;
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    refreshBanner();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    refreshBanner();
}
