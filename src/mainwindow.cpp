/*
 * mainwindow.cpp
 *
 * Copyright (C) 2007-2011 Christoph Pfister <christophpfister@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "mainwindow.h"

#include <QDBusConnection>
#include <QHoverEvent>
#include <QStackedLayout>
#include <QCommandLineParser>
#include <KActionCollection>
#include <QFileDialog>
#include <QInputDialog>
#include <kio/deletejob.h>
#include <QMenu>
#include <QMenuBar>
#include <KRecentFilesAction>
#include <KShortcutsDialog>
#include <KStatusNotifierItem>
#include <QTabBar>
#include <KLocalizedString>
#include <KToolBar>
#include <KHelpMenu>
#include <KConfigGroup>
#include "dvb/dvbtab.h"
#include "playlist/playlisttab.h"
#include "configuration.h"
#include "configurationdialog.h"
#include "dbusobjects.h"

class StackedLayout : public QStackedLayout
{
public:
	explicit StackedLayout(QWidget *parent) : QStackedLayout(parent) { }
	~StackedLayout() { }

	QSize minimumSize() const
	{
		QWidget *widget = currentWidget();

		if (widget != NULL) {
			return widget->minimumSizeHint();
		}

		return QSize();
	}
};

class StartTab : public TabBase
{
public:
	explicit StartTab(MainWindow *mainWindow);
	~StartTab() { }

private:
	void activate() { }

	QAbstractButton *addShortcut(const QString &name, const QIcon &icon, QWidget *parent);
};

StartTab::StartTab(MainWindow *mainWindow)
{
	setBackgroundRole(QPalette::Base);
	setAutoFillBackground(true);

	QGridLayout *gridLayout = new QGridLayout(this);
	gridLayout->setAlignment(Qt::AlignCenter);
	gridLayout->setMargin(10);
	gridLayout->setSpacing(15);

	QAbstractButton *button =
		addShortcut(i18n("&1 Play File"), QIcon::fromTheme(QLatin1String("video-x-generic")), this);
	button->setShortcut(Qt::Key_1);
	connect(button, SIGNAL(clicked()), mainWindow, SLOT(open()));
	gridLayout->addWidget(button, 0, 0);

	button = addShortcut(i18n("&2 Play Audio CD"), QIcon::fromTheme(QLatin1String("media-optical-audio")), this);
	button->setShortcut(Qt::Key_2);
	connect(button, SIGNAL(clicked()), mainWindow, SLOT(openAudioCd()));
	gridLayout->addWidget(button, 0, 1);

	button = addShortcut(i18n("&3 Play Video CD"), QIcon::fromTheme(QLatin1String("media-optical")), this);
	button->setShortcut(Qt::Key_3);
	connect(button, SIGNAL(clicked()), mainWindow, SLOT(openVideoCd()));
	gridLayout->addWidget(button, 0, 2);

	button = addShortcut(i18n("&4 Play DVD"), QIcon::fromTheme(QLatin1String("media-optical")), this);
	button->setShortcut(Qt::Key_4);
	connect(button, SIGNAL(clicked()), mainWindow, SLOT(openDvd()));
	gridLayout->addWidget(button, 1, 0);

#if HAVE_DVB == 1
	button = addShortcut(i18n("&5 Digital TV"), QIcon::fromTheme(QLatin1String("video-television")), this);
	button->setShortcut(Qt::Key_5);
	connect(button, SIGNAL(clicked()), mainWindow, SLOT(playDvb()));
	gridLayout->addWidget(button, 1, 1);
#endif /* HAVE_DVB == 1 */
}

QAbstractButton *StartTab::addShortcut(const QString &name, const QIcon &icon, QWidget *parent)
{
	// QPushButton has visual problems with big icons
	QToolButton *button = new QToolButton(parent);
	button->setText(name);
	button->setIcon(icon);
	button->setFocusPolicy(Qt::NoFocus);
	button->setIconSize(QSize(48, 48));
	button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
	button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	return button;
}

class PlayerTab : public TabBase
{
public:
	explicit PlayerTab(MediaWidget *mediaWidget_);
	~PlayerTab() { }

	void activate()
	{
		layout()->addWidget(mediaWidget);
		mediaWidget->setFocus();
	}

private:
	MediaWidget *mediaWidget;
};

PlayerTab::PlayerTab(MediaWidget *mediaWidget_) : mediaWidget(mediaWidget_)
{
	QHBoxLayout *layout = new QHBoxLayout(this);
	layout->setMargin(0);
}

MainWindow::MainWindow()
{
	// menu structure

	QMenuBar *menuBar = KMainWindow::menuBar();
	collection = new KActionCollection(this);

	helpMenu = new KHelpMenu;

	QMenu *menu = new QMenu(i18n("&File"), this);
	menuBar->addMenu(menu);

	QAction *action = KStandardAction::open(this, SLOT(open()), collection);
	menu->addAction(collection->addAction(QLatin1String("file_open"), action));

	action = new QAction(QIcon::fromTheme(QLatin1String("text-html")),
		i18nc("@action:inmenu", "Open URL..."), collection);
	action->setShortcut(Qt::CTRL | Qt::Key_U);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(openUrl()));
	menu->addAction(collection->addAction(QLatin1String("file_open_url"), action));

	actionOpenRecent = KStandardAction::openRecent(this, SLOT(openUrl(QUrl)), collection);
	actionOpenRecent->loadEntries(Configuration::instance()->config()->group("Recent Files"));
	menu->addAction(collection->addAction(QLatin1String("file_open_recent"), actionOpenRecent));

	menu->addSeparator();

	action = new QAction(QIcon::fromTheme(QLatin1String("media-optical-audio")), i18n("Play Audio CD"), collection);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(openAudioCd()));
	menu->addAction(collection->addAction(QLatin1String("file_play_audiocd"), action));

	action = new QAction(QIcon::fromTheme(QLatin1String("media-optical")), i18n("Play Video CD"), collection);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(openVideoCd()));
	menu->addAction(collection->addAction(QLatin1String("file_play_videocd"), action));

	action = new QAction(QIcon::fromTheme(QLatin1String("media-optical")), i18n("Play DVD"), collection);
	connect(action, SIGNAL(triggered(bool)), this, SLOT(openDvd()));
	menu->addAction(collection->addAction(QLatin1String("file_play_dvd"), action));

	action = new QAction(QIcon::fromTheme(QLatin1String("media-optical")), i18nc("@action:inmenu", "Play DVD Folder"),
		collection);
	connect(action, SIGNAL(triggered()), this, SLOT(playDvdFolder()));
	menu->addAction(collection->addAction(QLatin1String("file_play_dvd_folder"), action));

	menu->addSeparator();

	action = KStandardAction::quit(this, SLOT(close()), collection);
	menu->addAction(collection->addAction(QLatin1String("file_quit"), action));

	QMenu *playerMenu = new QMenu(i18n("&Playback"), this);
	menuBar->addMenu(playerMenu);

	QMenu *playlistMenu = new QMenu(i18nc("menu bar", "Play&list"), this);
	menuBar->addMenu(playlistMenu);

#if HAVE_DVB == 1
	QMenu *dvbMenu = new QMenu(i18n("&Television"), this);
	menuBar->addMenu(dvbMenu);
#endif /* HAVE_DVB == 1 */

	menu = new QMenu(i18n("&Settings"), this);
	menuBar->addMenu(menu);

	action = KStandardAction::keyBindings(this, SLOT(configureKeys()), collection);
	menu->addAction(collection->addAction(QLatin1String("settings_keys"), action));

	action = KStandardAction::preferences(this, SLOT(configureKaffeine()), collection);
	menu->addAction(collection->addAction(QLatin1String("settings_kaffeine"), action));

	menuBar->addSeparator();
	menuBar->addMenu(helpMenu->menu());

	// navigation bar - keep in sync with TabIndex enum!

	navigationBar = new KToolBar(QLatin1String("navigation_bar"), this, Qt::LeftToolBarArea);
	connect(navigationBar, SIGNAL(orientationChanged(Qt::Orientation)),
		this, SLOT(navigationBarOrientationChanged(Qt::Orientation)));

	tabBar = new QTabBar(navigationBar);
	tabBar->addTab(QIcon::fromTheme(QLatin1String("start-here-kde")), i18n("Start"));
	tabBar->addTab(QIcon::fromTheme(QLatin1String("kaffeine")), i18n("Playback"));
	tabBar->addTab(QIcon::fromTheme(QLatin1String("view-media-playlist")), i18n("Playlist"));
#if HAVE_DVB == 1
	tabBar->addTab(QIcon::fromTheme(QLatin1String("video-television")), i18n("Television"));
#endif /* HAVE_DVB == 1 */
	tabBar->setShape(QTabBar::RoundedWest);
	tabBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	connect(tabBar, SIGNAL(currentChanged(int)), this, SLOT(activateTab(int)));
	navigationBar->addWidget(tabBar);

	// control bar

	controlBar = new KToolBar(QLatin1String("control_bar"), this, Qt::BottomToolBarArea);
	controlBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

	autoHideControlBar = false;
	cursorHideTimer = new QTimer(this);
	cursorHideTimer->setInterval(1500);
	cursorHideTimer->setSingleShot(true);
	connect(cursorHideTimer, SIGNAL(timeout()), this, SLOT(hideCursor()));

	// main area

	QWidget *widget = new QWidget(this);
	stackedLayout = new StackedLayout(widget);
	setCentralWidget(widget);

	mediaWidget = new MediaWidget(playerMenu, controlBar, collection, widget);
	connect(mediaWidget, SIGNAL(displayModeChanged()), this, SLOT(displayModeChanged()));
	connect(mediaWidget, SIGNAL(changeCaption(QString)), this, SLOT(setCaption(QString)));
	connect(mediaWidget, SIGNAL(resizeToVideo(MediaWidget::ResizeFactor)),
		this, SLOT(resizeToVideo(MediaWidget::ResizeFactor)));
	connect(mediaWidget, SIGNAL(open()), this, SLOT(open()));

	// tabs - keep in sync with TabIndex enum!

	TabBase *startTab = new StartTab(this);
	tabs.append(startTab);
	stackedLayout->addWidget(startTab);

	playerTab = new PlayerTab(mediaWidget);
	playerTab->activate();
	tabs.append(playerTab);
	stackedLayout->addWidget(playerTab);

	playlistTab = new PlaylistTab(playlistMenu, collection, mediaWidget);
	tabs.append(playlistTab);
	stackedLayout->addWidget(playlistTab);

#if HAVE_DVB == 1
	dvbTab = new DvbTab(dvbMenu, collection, mediaWidget);
	connect(this, SIGNAL(mayCloseApplication(bool*,QWidget*)),
		dvbTab, SLOT(mayCloseApplication(bool*,QWidget*)));
	tabs.append(dvbTab);
	stackedLayout->addWidget(dvbTab);
#endif /* HAVE_DVB == 1 */

	currentTabIndex = StartTabId;

	// actions also have to work if the menu bar is hidden (fullscreen)
	collection->addAssociatedWidget(this);

	// restore custom key bindings
	collection->readSettings();

	// let KMainWindow save / restore its settings
	setAutoSaveSettings();

	// make sure that the bars are visible (fullscreen -> quit -> restore -> hidden)
	menuBar->show();
	navigationBar->show();
	controlBar->show();

	// workaround setAutoSaveSettings() which doesn't accept "IconOnly" as initial state
	controlBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

	KStatusNotifierItem *trayIcon = new KStatusNotifierItem(this);
	trayIcon->setIconByName(QLatin1String("kaffeine"));
	trayIcon->setStatus(KStatusNotifierItem::Active);
	trayIcon->setToolTipIconByName(QLatin1String("kaffeine"));
	trayIcon->setToolTipTitle(i18n("Kaffeine"));

	// initialize random number generator
	qsrand(QTime().msecsTo(QTime::currentTime()));

	// initialize dbus objects
	QDBusConnection::sessionBus().registerObject(QLatin1String("/"), new MprisRootObject(this),
		QDBusConnection::ExportAllContents);
	QDBusConnection::sessionBus().registerObject(QLatin1String("/Player"),
		new MprisPlayerObject(this, mediaWidget, playlistTab, this),
		QDBusConnection::ExportAllContents);
	QDBusConnection::sessionBus().registerObject(QLatin1String("/TrackList"),
		new MprisTrackListObject(playlistTab, this), QDBusConnection::ExportAllContents);
#if HAVE_DVB == 1
	QDBusConnection::sessionBus().registerObject(QLatin1String("/Television"),
		new DBusTelevisionObject(dvbTab, this), QDBusConnection::ExportAllContents);
#endif /* HAVE_DVB == 1 */
	QDBusConnection::sessionBus().registerService(QLatin1String("org.mpris.kaffeine"));

	show();

	// set display mode
	switch (Configuration::instance()->getStartupDisplayMode()) {
	case Configuration::StartupNormalMode:
		// nothing to do
		break;
	case Configuration::StartupMinimalMode:
		mediaWidget->setDisplayMode(MediaWidget::MinimalMode);
		break;
	case Configuration::StartupFullScreenMode:
		mediaWidget->setDisplayMode(MediaWidget::FullScreenMode);
		break;
	case Configuration::StartupRememberLastSetting: {
		int value = Configuration::instance()->config()->group("MainWindow").readEntry("DisplayMode", 0);

		switch (value) {
		case 0:
			// nothing to do
			break;
		case 1:
			mediaWidget->setDisplayMode(MediaWidget::MinimalMode);
			break;
		case 2:
			mediaWidget->setDisplayMode(MediaWidget::FullScreenMode);
			break;
		}

		break;
	    }
	}
}

MainWindow::~MainWindow()
{
	delete helpMenu;

	actionOpenRecent->saveEntries(Configuration::instance()->config()->group("Recent Files"));

	int value = 0;

	switch (mediaWidget->getDisplayMode()) {
	case MediaWidget::NormalMode: value = 0; break;
	case MediaWidget::MinimalMode: value = 1; break;
	case MediaWidget::FullScreenMode: value = 2; break;
	case MediaWidget::FullScreenReturnToMinimalMode: value = 2; break;
	}

	Configuration::instance()->config()->group("MainWindow").writeEntry("DisplayMode", value);
}

void MainWindow::parseArgs()
{
    QCommandLineOption fullscreen(QStringList() << "f" << "fullscreen", ki18n("Start in full screen mode").toString());
    QCommandLineOption audiocd("audiocd", ki18n("Play Audio CD").toString());
    QCommandLineOption videocd("videocd", ki18n("Play Video CD").toString());
    QCommandLineOption dvd("dvd", ki18n("Play DVD").toString());
    QCommandLineOption tv("tv", ki18nc("command line option", "(deprecated option)").toString(), "channel");
    QCommandLineOption channel("channel", ki18nc("command line option", "Play TV channel").toString(), "name / number");
    QCommandLineOption lastchannel("lastchannel", ki18nc("command line option", "Play last tuned TV channel").toString());
    QCommandLineOption dumpdvb("dumpdvb", ki18nc("command line option", "Dump dvb data (debug option)").toString());

    QCommandLineParser parser;
    parser.addOption(fullscreen);
    parser.addOption(audiocd);
    parser.addOption(videocd);
    parser.addOption(dvd);
    parser.addOption(tv);
    parser.addOption(channel);
    parser.addOption(lastchannel);
    parser.addOption(dumpdvb);
    parser.addPositionalArgument("file", ki18n("Files or URLs to play").toString());
    parser.process(*qApp);

	if (parser.isSet(fullscreen)) {
		mediaWidget->setDisplayMode(MediaWidget::FullScreenMode);
	}

	if (parser.isSet(audiocd)) {
		if (parser.values(audiocd).empty()) {
			openAudioCd();
		} else {
			openAudioCd(parser.values(audiocd).first());
		}

		return;
	}

	if (parser.isSet(videocd)) {
		if (parser.values(videocd).empty()) {
			openVideoCd();
		} else {
			openVideoCd(parser.values(videocd).first());
		}

		return;
	}

	if (parser.isSet(dvd)) {
		if (parser.values(dvd).empty()) {
			openDvd();
		} else {
			openDvd(parser.values(dvd).first());
		}

		return;
	}

#if HAVE_DVB == 1
	if (parser.isSet(dumpdvb)) {
		dvbTab->enableDvbDump();
	}

	if (parser.isSet(channel)) {
		activateTab(DvbTabId);
		dvbTab->playChannel(parser.value(channel));

		return;
	}

	if (parser.isSet(tv)) {
		activateTab(DvbTabId);
		dvbTab->playChannel(parser.value(tv));

		return;
	}

	if (parser.isSet(lastchannel)) {
		activateTab(DvbTabId);
		dvbTab->playLastChannel();

		return;
	}
#endif /* HAVE_DVB == 1 */

	QList<QUrl> urls;

	foreach (QUrl url, parser.positionalArguments())
		if (url.isValid())
			urls.append(url);

	if (urls.size() >= 2) {
		activateTab(PlaylistTabId);
		playlistTab->appendToVisiblePlaylist(urls, true);
	} else if (!urls.isEmpty()) {
		openUrl(urls.first());
	}
}

void MainWindow::displayModeChanged()
{
	switch (mediaWidget->getDisplayMode()) {
	case MediaWidget::FullScreenMode:
	case MediaWidget::FullScreenReturnToMinimalMode:
		setWindowState(windowState() | Qt::WindowFullScreen);
		break;
	case MediaWidget::MinimalMode:
	case MediaWidget::NormalMode:
		setWindowState(windowState() & (~Qt::WindowFullScreen));
		break;
	}

	switch (mediaWidget->getDisplayMode()) {
	case MediaWidget::FullScreenMode:
	case MediaWidget::FullScreenReturnToMinimalMode:
	case MediaWidget::MinimalMode:
		menuBar()->hide();
		navigationBar->hide();
		controlBar->hide();
		autoHideControlBar = true;
		cursorHideTimer->start();

		stackedLayout->setCurrentIndex(PlayerTabId);
		playerTab->activate();
		break;
	case MediaWidget::NormalMode:
		menuBar()->show();
		navigationBar->show();
		controlBar->show();
		autoHideControlBar = false;
		cursorHideTimer->stop();
		unsetCursor();

		stackedLayout->setCurrentIndex(currentTabIndex);
		tabs.at(currentTabIndex)->activate();
		break;
	}
}

void MainWindow::open()
{
	QList<QUrl> urls = QFileDialog::getOpenFileUrls(this, QString(), QUrl(), MediaWidget::extensionFilter());

	if (urls.size() >= 2) {
		activateTab(PlaylistTabId);
		playlistTab->appendToVisiblePlaylist(urls, true);
	} else if (!urls.isEmpty()) {
		openUrl(urls.at(0));
	}
}

void MainWindow::openUrl()
{
	openUrl(QInputDialog::getText(this, i18nc("@title:window", "Open URL"), i18n("Enter a URL:")));
}

void MainWindow::openUrl(const QUrl &url)
{
	if (!url.isValid()) {
		return;
	}

	// we need to copy "url" because addUrl() may invalidate it
	QUrl copy(url);
	actionOpenRecent->addUrl(copy); // moves the url to the top of the list

	if (currentTabIndex != PlaylistTabId) {
		activateTab(PlayerTabId);
	}

	playlistTab->appendToVisiblePlaylist(QList<QUrl>() << copy, true);
}

void MainWindow::openAudioCd(const QString &device)
{
	activateTab(PlayerTabId);
	mediaWidget->playAudioCd(device);
}

void MainWindow::openVideoCd(const QString &device)
{
	activateTab(PlayerTabId);
	mediaWidget->playVideoCd(device);
}

void MainWindow::openDvd(const QString &device)
{
	activateTab(PlayerTabId);
	mediaWidget->playDvd(device);
}

void MainWindow::playDvdFolder()
{
	QString folder = QFileDialog::getExistingDirectory(this);

	if (!folder.isEmpty()) {
		openDvd(folder);
	}
}

void MainWindow::playDvb()
{
	activateTab(DvbTabId);
	dvbTab->playLastChannel();
}

void MainWindow::resizeToVideo(MediaWidget::ResizeFactor resizeFactor)
{
	if (!isFullScreen() && !mediaWidget->sizeHint().isEmpty()) {
		if (isMaximized()) {
			setWindowState(windowState() & ~Qt::WindowMaximized);
		}

		QSize videoSize;

		switch (resizeFactor) {
		case MediaWidget::ResizeOff:
			break;
		case MediaWidget::OriginalSize:
			videoSize = mediaWidget->sizeHint();
			break;
		case MediaWidget::DoubleSize:
			videoSize = (2 * mediaWidget->sizeHint());
			break;
		}

		if (!videoSize.isEmpty()) {
			resize(size() - centralWidget()->size() + videoSize);
		}
	}
}

void MainWindow::configureKeys()
{
	KShortcutsDialog::configure(collection);
}

void MainWindow::configureKaffeine()
{
	QDialog *dialog = new ConfigurationDialog(this);
	dialog->setAttribute(Qt::WA_DeleteOnClose, true);
	dialog->setModal(true);
	dialog->show();
}

void MainWindow::navigationBarOrientationChanged(Qt::Orientation orientation)
{
	if (orientation == Qt::Horizontal) {
		tabBar->setShape(QTabBar::RoundedNorth);
	} else {
		tabBar->setShape(QTabBar::RoundedWest);
	}
}

void MainWindow::activateTab(int tabIndex)
{
	currentTabIndex = tabIndex;
	tabBar->setCurrentIndex(tabIndex);

	if (!autoHideControlBar) {
		stackedLayout->setCurrentIndex(currentTabIndex);
		tabs.at(currentTabIndex)->activate();
	}
}

void MainWindow::hideCursor()
{
	setCursor(Qt::BlankCursor);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	bool ok = true;
	emit mayCloseApplication(&ok, this);

	if (!ok) {
		event->ignore();
	} else {
		KMainWindow::closeEvent(event);
	}
}

bool MainWindow::event(QEvent *event)
{
	bool retVal = KMainWindow::event(event); // this has to be done before calling setVisible()

	// FIXME we depend on QEvent::HoverMove (instead of QEvent::MouseMove)
	// but the latter depends on mouse tracking being enabled on this widget
	// and all its children (especially the video widget) ...

	if ((event->type() == QEvent::HoverMove) && autoHideControlBar) {
		int y = reinterpret_cast<QHoverEvent *> (event)->pos().y();

		if ((y < 0) || (y >= height())) {
			// QHoverEvent sometimes reports quite strange coordinates - ignore them
			return retVal;
		}

		cursorHideTimer->stop();
		unsetCursor();

		switch (toolBarArea(controlBar)) {
		case Qt::TopToolBarArea:
			controlBar->setVisible(y < 60);
			break;
		case Qt::BottomToolBarArea:
			controlBar->setVisible(y >= (height() - 60));
			break;
		default:
			break;
		}

		if (controlBar->isHidden()) {
			cursorHideTimer->start();
		}
	}

	return retVal;
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Escape) {
		mediaWidget->setDisplayMode(MediaWidget::NormalMode);
	}

	KMainWindow::keyPressEvent(event);
}

void MainWindow::leaveEvent(QEvent *event)
{
	if (autoHideControlBar) {
		controlBar->setVisible(false);
	}

	KMainWindow::leaveEvent(event);
}
