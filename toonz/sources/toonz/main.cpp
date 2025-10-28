

// Tnz6 includes
#include "crashhandler.h"
#include "mainwindow.h"
#include "flipbook.h"
#include "tapp.h"
#include "iocommand.h"
#include "previewfxmanager.h"
#include "cleanupsettingspopup.h"
#include "filebrowsermodel.h"
#include "expressionreferencemanager.h"
#include "thirdparty.h"

// TnzTools includes
#include "tools/tool.h"
#include "tools/toolcommandids.h"

// TnzQt includes
#include "toonzqt/dvdialog.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/tmessageviewer.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/gutil.h"
#include "toonzqt/pluginloader.h"

// TnzStdfx includes
#include "stdfx/shaderfx.h"

// TnzLib includes
#include "toonz/preferences.h"
#include "toonz/toonzfolders.h"
#include "toonz/tproject.h"
#include "toonz/studiopalette.h"
#include "toonz/stylemanager.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/tproject.h"
#include "toonz/scriptengine.h"

// TnzSound includes
#include "tnzsound.h"

// TnzImage includes
#include "tnzimage.h"

// TnzBase includes
#include "permissionsmanager.h"
#include "tenv.h"
#include "tcli.h"

// TnzCore includes
#include "tsystem.h"
#include "tthread.h"
#include "tthreadmessage.h"
#include "tundo.h"
#include "tconvert.h"
#include "tiio_std.h"
#include "timagecache.h"
#include "tofflinegl.h"
#include "tpluginmanager.h"
#include "tsimplecolorstyles.h"
#include "toonz/imagestyles.h"
#include "tvectorbrushstyle.h"
#include "tfont.h"

#include "kis_tablet_support_win8.h"

#ifdef MACOSX
#include "tipc.h"
#endif

// Qt includes
#include <QApplication>
#include <QGuiApplication>  // For setHighDpiScaleFactorRoundingPolicy
#include <QAbstractEventDispatcher>
#include <QAbstractNativeEventFilter>
#include <QSplashScreen>
#include <QGLPixelBuffer>
#include <QTranslator>
#include <QFileInfo>
#include <QSettings>
#include <QLibraryInfo>
#include <QHash>

#ifdef _WIN32
#ifndef x64
#include <float.h>
#endif
#include <QtPlatformHeaders/QWindowsWindowFunctions>
#endif

using namespace DVGui;

TEnv::IntVar EnvSoftwareCurrentFontSize("SoftwareCurrentFontSize", 12);

// These are the same as the default values. See tenv.cpp and tversion.h
const char* rootVarName     = "TOONZROOT";
const char* systemVarPrefix = "TOONZ";

#ifdef MACOSX
#include "tthread.h"
void postThreadMsg(TThread::Message*) {}
void qt_mac_set_menubar_merge(bool enable);
#endif

// Modification for Toonz: No license types are needed
#define NO_LICENSE
//-----------------------------------------------------------------------------

static void fatalError(QString msg) {
  DVGui::MsgBoxInPopup(
      CRITICAL,
      msg + "\n" +
          QObject::tr("Installing %1 again could fix the problem.")
              .arg(QString::fromStdString(TEnv::getApplicationFullName())));
  exit(0);
}
//-----------------------------------------------------------------------------

static void lastWarningError(QString msg) {
  DVGui::error(msg);
  // exit(0);
}
//-----------------------------------------------------------------------------

static void toonzRunOutOfContMemHandler(unsigned long size) {
#ifdef _WIN32
  static bool firstTime = true;
  if (firstTime) {
    MessageBox(NULL, (LPCWSTR)L"Run out of contiguous physical memory: please save all and restart Toonz!",
               (LPCWSTR)L"Warning", MB_OK | MB_SYSTEMMODAL);
    firstTime = false;
  }
#endif
}

//-----------------------------------------------------------------------------

// TODO: Move to a header file
DV_IMPORT_API void initStdFx();
DV_IMPORT_API void initColorFx();

//-----------------------------------------------------------------------------

// Initializes the Toonz environment
// Sets the project root and stuff directory, ensures the output directory
// exists (creates it if it doesn't), and verifies that the stuff directory
// exists
static void initToonzEnv(QHash<QString, QString>& argPathValues) {
  StudioPalette::enable(true);
  TEnv::setRootVarName(rootVarName);
  TEnv::setSystemVarPrefix(systemVarPrefix);

  QHash<QString, QString>::const_iterator i = argPathValues.constBegin();
  while (i != argPathValues.constEnd()) {
    if (!TEnv::setArgPathValue(i.key().toStdString(), i.value().toStdString()))
      DVGui::error(
          QObject::tr("The qualifier %1 is not a valid key name. Skipping.")
              .arg(i.key()));
    ++i;
  }

  QCoreApplication::setOrganizationName("OpenToonz");
  QCoreApplication::setOrganizationDomain("");
  QCoreApplication::setApplicationName(
      QString::fromStdString(TEnv::getApplicationName()));

  // Verify TOONZROOT path
  // Check if TOONZROOT is defined and corresponds to an existing folder
  TFilePath stuffDir = TEnv::getStuffDir();
  if (stuffDir == TFilePath())
    fatalError("Undefined or empty: \"" + toQString(TEnv::getRootVarPath()) +
               "\"");
  else if (!TFileStatus(stuffDir).isDirectory())
    fatalError("Folder \"" + toQString(stuffDir) +
               "\" not found or not readable");

  Tiio::defineStd();
  initImageIo();
  initSoundIo();
  initStdFx();
  initColorFx();

  // TPluginManager::instance()->loadStandardPlugins();

  TFilePath library = ToonzFolder::getLibraryFolder();

  TRasterImagePatternStrokeStyle::setRootDir(library);
  TVectorImagePatternStrokeStyle::setRootDir(library);
  TVectorBrushStyle::setRootDir(library);

  CustomStyleManager::setRootPath(library);

  // Seems necessary for reading .tab 2.2 files:
  TPalette::setRootDir(library);
  TImageStyle::setLibraryDir(library);

  // TProjectManager::instance()->enableTabMode(true);

  TProjectManager* projectManager = TProjectManager::instance();

  // Retrieve the TOONZPROJECTS path set (TOONZPROJECTS can be multiple paths
  // separated by semicolons)
  TFilePathSet projectsRoots = ToonzFolder::getProjectsFolders();
  TFilePathSet::iterator it;
  for (it = projectsRoots.begin(); it != projectsRoots.end(); ++it)
    projectManager->addProjectsRoot(*it);

  // If not already present, create a sandbox project in TOONZROOT/sandbox
  projectManager->createSandboxIfNeeded();

  /*
TProjectP project = projectManager->getCurrentProject();
Not needed for Tab:
project->setFolder(TProject::Drawings, TFilePath("$scenepath"));
project->setFolder(TProject::Extras, TFilePath("$scenepath"));
project->setUseScenePath(TProject::Drawings, false);
project->setUseScenePath(TProject::Extras, false);
*/
  // Set the root directory for ImageCache
  // Configure TOONZCACHEROOT
  TFilePath cacheDir = ToonzFolder::getCacheRootFolder();
  if (cacheDir.isEmpty()) cacheDir = TEnv::getStuffDir() + "cache";
  TImageCache::instance()->setRootDir(cacheDir);
}

//-----------------------------------------------------------------------------

static void script_output(int type, const QString& value) {
  if (type == ScriptEngine::ExecutionError ||
      type == ScriptEngine::SyntaxError ||
      type == ScriptEngine::UndefinedEvaluationResult ||
      type == ScriptEngine::Warning)
    std::cerr << value.toStdString() << std::endl;
  else
    std::cout << value.toStdString() << std::endl;
}

//-----------------------------------------------------------------------------

int main(int argc, char* argv[]) {
#ifdef Q_OS_WIN
  // Enable standard input/output on Windows Platform for debug
  if (::AttachConsole(ATTACH_PARENT_PROCESS)) {
    freopen("CON", "r", stdin);
    freopen("CON", "w", stdout);
    freopen("CON", "w", stderr);
    atexit([]() { ::FreeConsole(); });
  }
#endif

  // Install signal handlers to catch crashes
  CrashHandler::install();

  // Parsing arguments and qualifiers
  TFilePath loadFilePath;
  QString argumentLayoutFileName = "";
  QHash<QString, QString> argumentPathValues;
  if (argc > 1) {
    TCli::Usage usage(argv[0]);
    TCli::UsageLine usageLine;
    TCli::FilePathArgument loadFileArg(
        "filePath", "Source scene file to open or script file to run");
    TCli::StringQualifier layoutFileQual(
        "-layout filename",
        "Custom layout file to be used, it should be saved in "
        "$TOONZPROFILES\\layouts\\personal\\[CurrentLayoutName].[UserName]\\. "
        "layouts.txt is used by default.");
    usageLine = usageLine + layoutFileQual;

    // System path qualifiers
    std::map<QString, std::unique_ptr<TCli::QualifierT<TFilePath>>>
        systemPathQualMap;
    QString qualKey  = QString("%1ROOT").arg(systemVarPrefix);
    QString qualName = QString("-%1 folderpath").arg(qualKey);
    QString qualHelp =
        QString(
            "%1 path. It will automatically set other system paths to %1 "
            "unless individually specified with other qualifiers.")
            .arg(qualKey);
    systemPathQualMap[qualKey].reset(new TCli::QualifierT<TFilePath>(
        qualName.toStdString(), qualHelp.toStdString()));
    usageLine = usageLine + *systemPathQualMap[qualKey];

    const std::map<std::string, std::string>& spm = TEnv::getSystemPathMap();
    for (auto itr = spm.begin(); itr != spm.end(); ++itr) {
      qualKey = QString("%1%2")
                    .arg(systemVarPrefix)
                    .arg(QString::fromStdString((*itr).first));
      qualName = QString("-%1 folderpath").arg(qualKey);
      qualHelp = QString("%1 path.").arg(qualKey);
      systemPathQualMap[qualKey].reset(new TCli::QualifierT<TFilePath>(
          qualName.toStdString(), qualHelp.toStdString()));
      usageLine = usageLine + *systemPathQualMap[qualKey];
    }
    usage.add(usageLine);
    usage.add(usageLine + loadFileArg);

    if (!usage.parse(argc, argv)) exit(1);

    loadFilePath = loadFileArg.getValue();
    if (layoutFileQual.isSelected())
      argumentLayoutFileName =
          QString::fromStdString(layoutFileQual.getValue());
    for (auto q_itr = systemPathQualMap.begin();
         q_itr != systemPathQualMap.end(); ++q_itr) {
      if (q_itr->second->isSelected())
        argumentPathValues.insert(q_itr->first,
                                  q_itr->second->getValue().getQString());
    }

    argc = 1;
  }

  // High-DPI settings for proper scaling on modern displays
  // Enable high-DPI scaling to support automatic scaling based on display DPI
  // (Qt 5.6+) Use PassThrough rounding policy (Qt 5.15) to allow fractional
  // scaling (e.g., 1.25, 1.5) to prevent raster/vector misalignment when
  // QT_SCALE_FACTOR is set Enable high-DPI pixmaps for smooth icon rendering
  // Removed Qt::AA_Use96Dpi (workaround #20230627) as it forces 96 DPI, causing
  // scaling issues Menu bar icon size is set explicitly later to maintain
  // workaround behavior
  QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
      Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
  QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

  // Construct QApplication after setting DPI attributes
  QApplication a(argc, argv);

#ifdef MACOSX
  // Workaround for Qt 5.6.0 on macOS to avoid missing left button issue
  // Sends NSLeftButtonDown event before NSLeftMouseDragged to invalidate
  // m_rightButtonClicked in Qt/qnsview.mm
  // See mousedragfilter.mm for details
#include "mousedragfilter.h"

  class OSXMouseDragFilter final : public QAbstractNativeEventFilter {
    bool leftButtonPressed = false;

  public:
    bool nativeEventFilter(const QByteArray& eventType, void* message,
                           long*) Q_DECL_OVERRIDE {
      if (IsLeftMouseDown(message)) {
        leftButtonPressed = true;
      }
      if (IsLeftMouseUp(message)) {
        leftButtonPressed = false;
      }

      if (eventType == "mac_generic_NSEvent") {
        if (IsLeftMouseDragged(message) && !leftButtonPressed) {
          std::cout << "force mouse press event" << std::endl;
          SendLeftMousePressEvent();
          return true;
        }
      }
      return false;
    }
  };

  a.installNativeEventFilter(new OSXMouseDragFilter);
#endif

#ifdef Q_OS_WIN
  // Force Qt to use desktop OpenGL, as OpenToonz does not work with software or
  // ANGLE OpenGL Note: This should ideally be set before QApplication
  // construction, but ANGLE may be enabled
  a.setAttribute(Qt::AA_UseDesktopOpenGL, true);
#endif

  // Ensure proper destruction of Qt objects by maintaining a living
  // QApplication Static QApplication caused issues on macOS, so use a QObject
  // scope instead
  std::unique_ptr<QObject> mainScope(
      new QObject(&a));  // QObject destroyed before QApplication
  mainScope->setObjectName(
      "mainScope");  // Accessible via QApplication's children

#ifdef _WIN32
#ifndef x64
  // Store the floating point control word for 32-bit Windows
  // Will be restored after Toonz initialization to avoid precision issues
  unsigned int fpWord = 0;
  _controlfp_s(&fpWord, 0, 0);
#endif
#endif

#ifdef _WIN32
  // Prevent flickering when dragging panel separators on Windows (Qt 4.5.2
  // issue)
  a.setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
#endif

#ifdef _WIN32
  // Compress high-frequency and tablet events to optimize event handling
  a.setAttribute(Qt::AA_CompressHighFrequencyEvents);
  a.setAttribute(Qt::AA_CompressTabletEvents);
#endif

  // Set locale for numeric operations to standard C for consistent atof()
  // behavior
  setlocale(LC_NUMERIC, "C");

// Set current directory to the bundle/application path for correct relative
// paths
#ifdef MACOSX
  {
    QDir appDir(QApplication::applicationDirPath());
    appDir.cdUp(), appDir.cdUp(), appDir.cdUp();

    bool ret = QDir::setCurrent(appDir.absolutePath());
    assert(ret);
  }
#endif

  // Enable icons in menus (can be disabled selectively with iconVisibleInMenu)
  QApplication::instance()->setAttribute(Qt::AA_DontShowIconsInMenus, false);

  TEnv::setApplicationFileName(argv[0]);

  // Splash screen
  QPixmap splashPixmap = QIcon(":Resources/splash.svg").pixmap(QSize(610, 344));

#ifdef _WIN32
  QFont font("Segoe UI", -1);
#else
  QFont font("Helvetica", -1);
#endif
  font.setPixelSize(13);
  font.setWeight(50);
  a.setFont(font);

  QString offsetStr("\n\n\n\n\n\n\n\n");

  TSystem::hasMainLoop(true);

  TMessageRepository::instance();

  bool isRunScript = (loadFilePath.getType() == "toonzscript");

  QSplashScreen splash(splashPixmap);
  if (!isRunScript) splash.show();
  a.processEvents();

  splash.showMessage(offsetStr + "Initializing QGLFormat...", Qt::AlignCenter,
                     Qt::white);
  a.processEvents();

  // OpenGL
  QGLFormat fmt;
  fmt.setAlpha(true);
  fmt.setStencil(true);
  QGLFormat::setDefaultFormat(fmt);

#ifndef __HAIKU__
  glutInit(&argc, argv);
#endif

  splash.showMessage(offsetStr + "Initializing Toonz environment ...",
                     Qt::AlignCenter, Qt::white);
  a.processEvents();

  // Install callback for running out of contiguous memory
  TBigMemoryManager::instance()->setRunOutOfContiguousMemoryHandler(
      &toonzRunOutOfContMemHandler);

  // Initialize Toonz environment
  initToonzEnv(argumentPathValues);

  // Setup third-party components
  ThirdParty::initialize();

  // Prepare for 30-bit display if enabled
  if (Preferences::instance()->is30bitDisplayEnabled()) {
    QSurfaceFormat sFmt = QSurfaceFormat::defaultFormat();
    sFmt.setRedBufferSize(10);
    sFmt.setGreenBufferSize(10);
    sFmt.setBlueBufferSize(10);
    sFmt.setAlphaBufferSize(2);
    QSurfaceFormat::setDefaultFormat(sFmt);
  }

  // Initialize thread components
  TThread::init();

  TProjectManager* projectManager = TProjectManager::instance();
  if (Preferences::instance()->isSVNEnabled()) {
    // Read Version Control repositories and add them to project manager as
    // special SVN project roots
    VersionControl::instance()->init();
    QList<SVNRepository> repositories =
        VersionControl::instance()->getRepositories();
    int count = repositories.size();
    for (int i = 0; i < count; i++) {
      SVNRepository r = repositories.at(i);

      TFilePath localPath(r.m_localPath.toStdWString());
      if (!TFileStatus(localPath).doesExist()) {
        try {
          TSystem::mkDir(localPath);
        } catch (TException& e) {
          fatalError(QString::fromStdWString(e.getMessage()));
        }
      }
      projectManager->addSVNProjectsRoot(localPath);
    }
  }

#if defined(MACOSX) && defined(__LP64__)

  // Load shared memory settings
  int shmmax = Preferences::instance()->getShmMax();
  int shmseg = Preferences::instance()->getShmSeg();
  int shmall = Preferences::instance()->getShmAll();
  int shmmni = Preferences::instance()->getShmMni();

  if (shmall < 0)  // Ensure at least 100 MB of shared memory is available
    shmall = (tipc::shm_maxSharedPages() < (100 << 8)) ? (100 << 8) : -1;

  tipc::shm_set(shmmax, shmseg, shmall, shmmni);

#endif

  // DVDirModel must be instantiated after Version Control initialization
  FolderListenerManager::instance()->addListener(DvDirModel::instance());

  splash.showMessage(offsetStr + "Loading Translator ...", Qt::AlignCenter,
                     Qt::white);
  a.processEvents();

  // Load translation from toonz.qm if present
  QString languagePathString =
      QString::fromStdString(::to_string(TEnv::getConfigDir() + "loc"));
#ifndef WIN32
  // Menu merging on macOS can cause issues with different languages for the
  // Preferences menu qt_mac_set_menubar_merge(false);
  languagePathString += "/" + Preferences::instance()->getCurrentLanguage();
#else
  languagePathString += "\\" + Preferences::instance()->getCurrentLanguage();
#endif
  QTranslator translator;
  translator.load("toonz", languagePathString);

  // Install the translator
  a.installTranslator(&translator);

  // Load translation from toonzqt.qm if present
  QTranslator translator2;
  translator2.load("toonzqt", languagePathString);
  a.installTranslator(&translator2);

  // Load translation from tnzcore.qm if present
  QTranslator tnzcoreTranslator;
  tnzcoreTranslator.load("tnzcore", languagePathString);
  qApp->installTranslator(&tnzcoreTranslator);

  // Load translation from toonzlib.qm if present
  QTranslator toonzlibTranslator;
  toonzlibTranslator.load("toonzlib", languagePathString);
  qApp->installTranslator(&toonzlibTranslator);

  // Load translation from colorfx.qm if present
  QTranslator colorfxTranslator;
  colorfxTranslator.load("colorfx", languagePathString);
  qApp->installTranslator(&colorfxTranslator);

  // Load translation from tools.qm
  QTranslator toolTranslator;
  toolTranslator.load("tnztools", languagePathString);
  qApp->installTranslator(&toolTranslator);

  // Load translation for file writer properties
  QTranslator imageTranslator;
  imageTranslator.load("image", languagePathString);
  qApp->installTranslator(&imageTranslator);

  QTranslator qtTranslator;
  qtTranslator.load("qt_" + QLocale::system().name(),
                    QLibraryInfo::location(QLibraryInfo::TranslationsPath));
  a.installTranslator(&qtTranslator);

  // Update translation of all tool properties
  TTool::updateToolsPropertiesTranslation();
  // Apply translation to file writer properties
  Tiio::updateFileWritersPropertiesTranslation();

  // Force left-to-right layout direction in any language environment
  // Must be called after installTranslator()
  a.setLayoutDirection(Qt::LeftToRight);

  splash.showMessage(offsetStr + "Loading styles ...", Qt::AlignCenter,
                     Qt::white);
  a.processEvents();

  // Set application style to Fusion to avoid Windows native style glitches with
  // fractional scaling
  QApplication::setStyle("fusion");

  IconGenerator::setFilmstripIconSize(Preferences::instance()->getIconSize());

  splash.showMessage(offsetStr + "Loading shaders ...", Qt::AlignCenter,
                     Qt::white);
  a.processEvents();

  loadShaderInterfaces(ToonzFolder::getLibraryFolder() + TFilePath("shaders"));

  splash.showMessage(offsetStr + "Initializing OpenToonz ...", Qt::AlignCenter,
                     Qt::white);
  a.processEvents();

  // Initialize ThemeManager before TApp
  auto& themeManager = ThemeManager::getInstance();
  themeManager.initialize();

  TTool::setApplication(TApp::instance());
  TApp::instance()->init();

  splash.showMessage(offsetStr + "Loading Plugins...", Qt::AlignCenter,
                     Qt::white);
  a.processEvents();
  // Poll until plugin loading completes
  // PluginLoader uses a blocking main thread queue to ensure setup handlers
  // are called in a dedicated thread, requiring processEvents
  while (!PluginLoader::load_entries("")) {
    a.processEvents();
  }

  splash.showMessage(offsetStr + "Creating main window ...", Qt::AlignCenter,
                     Qt::white);
  a.processEvents();

  // Pass layout file name to MainWindow constructor
  MainWindow w(argumentLayoutFileName);
  // Set menu bar icon size explicitly to maintain workaround #20230627 behavior
  // Maintain 16px icons in menus (since QMenuBar doesn't support setIconSize)
  // === WORKAROUND #20230627: Logical 16px icons in menu (HiDPI-safe) ===
  QMenuBar* menuBar = w.menuBar();
  if (menuBar) {
      menuBar->setIconSize(QSize(16, 16));  // 16 logical px → Qt scales automatically
  }
  
  // === FIX BRUSH AT 125% DPI ===
  QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
  fmt.setSamples(0);  // Prevents OpenGL glitch with fractional scaling
  QSurfaceFormat::setDefaultFormat(fmt);
  
  // Force the viewer to recalculate coordinates
  QTimer::singleShot(100, [&w]() {
      if (auto viewer = w.getCurrentRoom()->getViewer()) {
          viewer->update();
          viewer->setIgnoreTabletEvent(true);
          viewer->setIgnoreTabletEvent(false);
      }
  });
  
  CrashHandler::attachParentWindow(&w);
  CrashHandler::reportProjectInfo(true);

  if (isRunScript) {
    // Load script
    if (TFileStatus(loadFilePath).doesExist()) {
      // Find project for this script file
      TProjectManager* pm = TProjectManager::instance();
      auto sceneProject   = pm->loadSceneProject(loadFilePath);
      TFilePath oldProjectPath;
      if (!sceneProject) {
        std::cerr << QObject::tr(
                         "It is not possible to load the scene %1 because it "
                         "does not belong to any project.")
                         .arg(loadFilePath.getQString())
                         .toStdString()
                  << std::endl;
        return 1;
      }
      if (sceneProject && !sceneProject->isCurrent()) {
        oldProjectPath = pm->getCurrentProjectPath();
        pm->setCurrentProjectPath(sceneProject->getProjectPath());
      }
      ScriptEngine engine;
      QObject::connect(&engine, &ScriptEngine::output, script_output);
      QString s = QString::fromStdWString(loadFilePath.getWideString())
                      .replace("\\", "\\\\")
                      .replace("\"", "\\\"");
      QString cmd = QString("run(\"%1\")").arg(s);
      engine.evaluate(cmd);
      engine.wait();
      if (!oldProjectPath.isEmpty()) pm->setCurrentProjectPath(oldProjectPath);
      return 1;
    } else {
      std::cerr << QObject::tr("Script file %1 does not exist.")
                       .arg(loadFilePath.getQString())
                       .toStdString()
                << std::endl;
      return 1;
    }
  }

#ifdef _WIN32
  // Ensure border in fullscreen OpenGL-based windows
  // See
  // http://doc.qt.io/qt-5/windows-issues.html#fullscreen-opengl-based-windows
  if (w.windowHandle())
    QWindowsWindowFunctions::setHasBorderInFullScreen(w.windowHandle(), true);
#endif

  // Enable WinTab for tablet support in customized Qt 5.15.2 (Windows x64,
  // WITH_WINTAB) See
  // https://github.com/shun-iwasawa/qt5/releases/tag/v5.15.2_wintab
#ifdef WITH_WINTAB
  bool useQtNativeWinInk = Preferences::instance()->isQtNativeWinInkEnabled();
  QWindowsWindowFunctions::setWinTabEnabled(!useQtNativeWinInk);
#endif

  splash.showMessage(offsetStr + "Loading style sheet ...", Qt::AlignCenter,
                     Qt::white);
  a.processEvents();

  // Load stylesheet
  QString currentStyle = Preferences::instance()->getCurrentStyleSheet();
  a.setStyleSheet(currentStyle);

  // Parse initial stylesheet in ThemeManager
  themeManager.parseCustomPropertiesFromStylesheet(currentStyle);

  // w.setWindowTitle(QString::fromStdString(TEnv::getApplicationFullName()));
  w.changeWindowTitle();
  if (TEnv::getIsPortable()) {
    splash.showMessage(offsetStr + "Starting OpenToonz Portable ...",
                       Qt::AlignCenter, Qt::white);
  } else {
    splash.showMessage(offsetStr + "Starting main window ...", Qt::AlignCenter,
                       Qt::white);
  }
  a.processEvents();

  TFilePath fp = ToonzFolder::getModuleFile("mainwindow.ini");
  QSettings settings(toQString(fp), QSettings::IniFormat);
  if (settings.contains("MainWindowGeometry"))
    w.restoreGeometry(settings.value("MainWindowGeometry").toByteArray());
  else  // Maximize window on first launch
    w.setWindowState(w.windowState() | Qt::WindowMaximized);

  ExpressionReferenceManager::instance()->init();

#ifndef MACOSX
  // Workaround for maximized window case: Qt delivers two resize events
  // One in normal geometry before maximizing, then another after
  // This affects docking system geometry restoration
  // Disable currentRoom layout before show() and re-enable after normal resize
  if (w.isMaximized()) w.getCurrentRoom()->layout()->setEnabled(false);
#endif

  QRect splashGeometry = splash.geometry();
  splash.finish(&w);

  a.setQuitOnLastWindowClosed(false);
  // a.connect(&a, SIGNAL(lastWindowClosed()), &a, SLOT(quit()));
  if (Preferences::instance()->isLatestVersionCheckEnabled())
    w.checkForUpdates();

  w.show();

  // Show floating panels after main window is shown
  w.startupFloatingPanels();

  CommandManager::instance()->execute(T_Hand);
  if (!loadFilePath.isEmpty()) {
    splash.showMessage(
        QString("Loading file '") + loadFilePath.getQString() + "'...",
        Qt::AlignCenter, Qt::white);
    if (TFileStatus(loadFilePath).doesExist()) IoCmd::loadScene(loadFilePath);
  }

  QFont* myFont;
  QString fontName  = Preferences::instance()->getInterfaceFont();
  QString fontStyle = Preferences::instance()->getInterfaceFontStyle();

  TFontManager* fontMgr = TFontManager::instance();
  std::vector<std::wstring> typefaces;
  bool isBold = false, isItalic = false, hasKerning = false;
  try {
    fontMgr->loadFontNames();
    fontMgr->setFamily(fontName.toStdWString());
    fontMgr->getAllTypefaces(typefaces);
    isBold     = fontMgr->isBold(fontName, fontStyle);
    isItalic   = fontMgr->isItalic(fontName, fontStyle);
    hasKerning = fontMgr->hasKerning();
  } catch (TFontCreationError&) {
    // Do nothing. A default font should load
  }

  myFont = new QFont(fontName);
  myFont->setPixelSize(EnvSoftwareCurrentFontSize);
  myFont->setBold(isBold);
  myFont->setItalic(isItalic);
  myFont->setKerning(hasKerning);

  a.setFont(*myFont);

  QAction* action = CommandManager::instance()->getAction("MI_OpenTMessage");
  if (action)
    QObject::connect(TMessageRepository::instance(),
                     SIGNAL(openMessageCenter()), action, SLOT(trigger()));

  QObject::connect(TUndoManager::manager(), SIGNAL(somethingChanged()),
                   TApp::instance()->getCurrentScene(), SLOT(setDirtyFlag()));

#ifdef _WIN32
#ifndef x64
  // Restore floating point control word on 32-bit Windows
  // Prevents precision issues from AVI codec initialization (e.g., VFAPI)
  _controlfp_s(0, fpWord, -1);
#endif
#endif

#ifdef _WIN32
  if (Preferences::instance()->isWinInkEnabled()) {
    KisTabletSupportWin8* penFilter = new KisTabletSupportWin8();
    if (penFilter->init()) {
      a.installNativeEventFilter(penFilter);
    } else {
      delete penFilter;
    }
  }
#endif

  a.installEventFilter(TApp::instance());

  int ret = a.exec();

  TUndoManager::manager()->reset();
  PreviewFxManager::instance()->reset();

  return ret;
}
