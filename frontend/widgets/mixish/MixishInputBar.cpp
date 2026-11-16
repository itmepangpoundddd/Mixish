/******************************************************************************
    Copyright (C) 2026 by Mixish contributors

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "MixishInputBar.hpp"
#include "MixishAudioMixer.hpp"
#include "MixishInputTile.hpp"
#include "MixishPlaylistController.hpp"
#include "MixishPlaylistManager.hpp"
#include "MixishPlaylistPanel.hpp"
#include "MixishSwitcher.hpp"
#include "MixishTheme.hpp"
#include "MixishVideoControls.hpp"

#include <OBSApp.hpp>
#include <util/config-file.h>
#include <widgets/OBSBasic.hpp>

#include <QAbstractItemModel>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QColor>
#include <QFont>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStyle>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "moc_MixishInputBar.cpp"

MixishInputBar::MixishInputBar(MixishSwitcher *switcher_, OBSBasic *main_, QWidget *parent)
	: QWidget(parent),
	  switcher(switcher_),
	  main(main_)
{
	setObjectName(QStringLiteral("mixishInputBar"));
	setMinimumHeight(248);

	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(MixishTheme::Space3, MixishTheme::Space1,
				  MixishTheme::Space3, MixishTheme::Space1);
	outer->setSpacing(MixishTheme::Space1);

	auto *headerRow = new QHBoxLayout();
	headerRow->setSpacing(MixishTheme::Space3);

	auto *title = new QLabel(tr("INPUTS"), this);
	MixishTheme::MakeSectionLabel(title);
	headerRow->addWidget(title);

	videoControls = new MixishVideoControls(switcher, this);
	headerRow->addWidget(videoControls, 1);

	playlistToggle = new QPushButton(tr("Playlist"), this);
	playlistToggle->setObjectName(QStringLiteral("mixishPlaylistToggle"));
	MixishTheme::SetVariant(playlistToggle, "secondary");
	playlistToggle->setCheckable(true);
	playlistToggle->setFixedHeight(MixishTheme::IconButtonSize);
	playlistToggle->setFocusPolicy(Qt::NoFocus);
	playlistToggle->setToolTip(tr("Show or hide the selected playlist's queue"));
	playlistToggle->setEnabled(false);
	headerRow->addWidget(playlistToggle);

	mixerToggle = new QPushButton(tr("Audio Mixer"), this);
	mixerToggle->setObjectName(QStringLiteral("mixishMixerToggle"));
	MixishTheme::SetVariant(mixerToggle, "secondary");
	mixerToggle->setCheckable(true);
	mixerToggle->setFixedHeight(MixishTheme::IconButtonSize);
	mixerToggle->setFocusPolicy(Qt::NoFocus);
	mixerToggle->setToolTip(tr("Show or hide the audio mixer"));
	headerRow->addWidget(mixerToggle);

	outer->addLayout(headerRow);

	contentSplitter = new QSplitter(Qt::Horizontal, this);
	contentSplitter->setObjectName(QStringLiteral("mixishInputSplitter"));
	contentSplitter->setChildrenCollapsible(false);
	contentSplitter->setHandleWidth(4);
	scrollArea = new QScrollArea(contentSplitter);
	scrollArea->setObjectName(QStringLiteral("mixishTileScroll"));
	scrollArea->setFrameShape(QFrame::NoFrame);
	scrollArea->setWidgetResizable(true);
	scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	auto *scrollWidget = new QWidget(scrollArea);
	scrollWidget->setObjectName(QStringLiteral("mixishTileScrollContent"));
	tileLayout = new QHBoxLayout(scrollWidget);
	tileLayout->setContentsMargins(0, 0, 0, 0);
	tileLayout->setSpacing(MixishTheme::Space2);
	tileLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
	scrollArea->setWidget(scrollWidget);
	contentSplitter->addWidget(scrollArea);

	audioMixer = new MixishAudioMixer(switcher, contentSplitter);
	contentSplitter->addWidget(audioMixer);

	playlistPanel = new MixishPlaylistPanel(switcher, contentSplitter);
	playlistPanel->hide();
	contentSplitter->addWidget(playlistPanel);

	contentSplitter->setStretchFactor(0, 1);
	contentSplitter->setSizes({1000, 0, 0});
	contentSplitter->setStretchFactor(1, 0);
	contentSplitter->setStretchFactor(2, 0);

	outer->addWidget(contentSplitter, 1);

	connect(mixerToggle, &QPushButton::toggled, this,
		[this](bool checked) { SetMixerVisible(checked); });
	connect(playlistToggle, &QPushButton::toggled, this,
		[this](bool checked) { SetPlaylistVisible(checked); });

	{
		const bool visible = config_get_bool(App()->GetUserConfig(), "Mixish", "ShowAudioMixer");
		QSignalBlocker block(mixerToggle);
		mixerToggle->setChecked(visible);
		SetMixerVisible(visible);
	}

	rebuildTimer = new QTimer(this);
	rebuildTimer->setSingleShot(true);
	rebuildTimer->setInterval(120);
	connect(rebuildTimer, &QTimer::timeout, this, &MixishInputBar::Rebuild);

	thumbnailTimer = new QTimer(this);
	thumbnailTimer->setInterval(250);
	connect(thumbnailTimer, &QTimer::timeout, this, &MixishInputBar::RequestNextThumbnail);
	thumbnailTimer->start();

	if (main->ui->scenes) {
		QAbstractItemModel *model = main->ui->scenes->model();
		if (model) {
			connect(model, &QAbstractItemModel::rowsInserted, this, &MixishInputBar::ScheduleRebuild);
			connect(model, &QAbstractItemModel::rowsRemoved, this, &MixishInputBar::ScheduleRebuild);
			connect(model, &QAbstractItemModel::rowsMoved, this, &MixishInputBar::ScheduleRebuild);
			connect(model, &QAbstractItemModel::modelReset, this, &MixishInputBar::ScheduleRebuild);
			connect(model, &QAbstractItemModel::layoutChanged, this, &MixishInputBar::ScheduleRebuild);
			connect(model, &QAbstractItemModel::dataChanged, this, &MixishInputBar::ScheduleRebuild);
		}

		connect(main->ui->scenes, &QListWidget::currentItemChanged, this,
			[this](QListWidgetItem *, QListWidgetItem *) { UpdateStates(); });
	}

	connect(main, &OBSBasic::PreviewProgramModeChanged, this, [this](bool) { UpdateStates(); });

	QApplication::instance()->installEventFilter(this);

	MixishTheme::Apply(this);
	Rebuild();
}

MixishInputBar::~MixishInputBar()
{
	if (QApplication::instance())
		QApplication::instance()->removeEventFilter(this);
}

void MixishInputBar::ScheduleRebuild()
{
	if (rebuildTimer)
		rebuildTimer->start();
}

void MixishInputBar::SetMixerVisible(bool visible)
{
	if (visible) {
		mixerRestoreOnPlaylistExit = false;
		ShowPanel(InputPanelMode::Mixer);
	} else {
		ShowPanel(InputPanelMode::None);
	}

	config_set_bool(App()->GetUserConfig(), "Mixish", "ShowAudioMixer", visible);
}

void MixishInputBar::SetPlaylistVisible(bool visible)
{
	if (visible) {
		if (!playlistToggle || !playlistToggle->isEnabled()) {
			if (playlistToggle) {
				QSignalBlocker block(playlistToggle);
				playlistToggle->setChecked(false);
			}
			return;
		}

		if (panelMode == InputPanelMode::Mixer)
			mixerRestoreOnPlaylistExit = true;
		if (playlistPanel)
			playlistPanel->SetController(panelController);
		ShowPanel(InputPanelMode::Playlist);
	} else if (panelMode == InputPanelMode::Playlist) {
		ShowPanel(InputPanelMode::None);
		mixerRestoreOnPlaylistExit = false;
	}
}

void MixishInputBar::ShowPanel(InputPanelMode mode)
{
	const bool changed = panelMode != mode;
	panelMode = mode;

	if (audioMixer) {
		audioMixer->setVisible(mode == InputPanelMode::Mixer);
		if (mode == InputPanelMode::Mixer)
			audioMixer->Refresh();
	}

	if (playlistPanel) {
		playlistPanel->setVisible(mode == InputPanelMode::Playlist);
		if (mode == InputPanelMode::Playlist)
			playlistPanel->Refresh();
	}

	if (mixerToggle) {
		QSignalBlocker block(mixerToggle);
		mixerToggle->setChecked(mode == InputPanelMode::Mixer);
		MixishTheme::SetState(mixerToggle,
				      mode == InputPanelMode::Mixer ? "selected" : "idle");
	}

	if (playlistToggle) {
		QSignalBlocker block(playlistToggle);
		playlistToggle->setEnabled(panelController != nullptr);
		playlistToggle->setChecked(mode == InputPanelMode::Playlist);
		MixishTheme::SetState(playlistToggle,
				      mode == InputPanelMode::Playlist ? "selected" : "idle");
	}

	if (changed && mode != InputPanelMode::None && contentSplitter) {
		if (mode == InputPanelMode::Mixer)
			contentSplitter->setSizes(QList<int>({1000, 320, 0}));
		else
			contentSplitter->setSizes(QList<int>({1000, 0, 440}));
	} else if (changed && mode == InputPanelMode::None && contentSplitter) {
		contentSplitter->setSizes(QList<int>({1000, 0, 0}));
	}
}

void MixishInputBar::RefreshAudioMixer()
{
	if (audioMixer)
		audioMixer->Refresh();
}

MixishInputTile *MixishInputBar::SelectionTile() const
{
	if (selectedTile && selectedTile->Scene())
		return selectedTile;

	/* No manual selection yet: follow the Preview tile. */
	OBSSource preview = main->GetCurrentSceneSource();
	for (MixishInputTile *candidate : tiles) {
		if (!candidate)
			continue;
		OBSScene scene = candidate->Scene();
		if (scene && obs_scene_get_source(scene) == preview)
			return candidate;
	}

	return nullptr;
}

MixishVideoControls *MixishInputBar::VideoControls() const
{
	return videoControls;
}

void MixishInputBar::UpdateVideoControls()
{
	if (!videoControls)
		return;

	MixishInputTile *tile = SelectionTile();
	videoControls->SetScene(tile ? tile->Scene() : OBSScene());
	UpdatePlaylistPanel();
}

void MixishInputBar::UpdatePlaylistPanel()
{
	if (!playlistToggle)
		return;

	MixishPlaylistController *controller = nullptr;
	MixishInputTile *tile = SelectionTile();
	if (tile && tile->Scene()) {
		MixishPlaylistManager *manager = switcher ? switcher->GetPlaylistManager() : nullptr;
		if (manager)
			controller = manager->FindByScene(obs_scene_get_source(tile->Scene()));
	}

	const bool hasController = controller != nullptr;
	playlistToggle->setEnabled(hasController);

	if (controller != panelController) {
		panelController = controller;
		if (playlistPanel)
			playlistPanel->SetController(controller);

		if (hasController) {
			if (panelMode != InputPanelMode::Playlist) {
				mixerRestoreOnPlaylistExit = (panelMode == InputPanelMode::Mixer);
				ShowPanel(InputPanelMode::Playlist);
			}
		} else if (panelMode == InputPanelMode::Playlist) {
			ShowPanel(mixerRestoreOnPlaylistExit ? InputPanelMode::Mixer
							     : InputPanelMode::None);
			mixerRestoreOnPlaylistExit = false;
		}
	} else if (!hasController && panelMode == InputPanelMode::Playlist) {
		ShowPanel(InputPanelMode::None);
	}
}

void MixishInputBar::ClearTiles()
{
	if (!tileLayout)
		return;

	while (QLayoutItem *item = tileLayout->takeAt(0)) {
		if (QWidget *widget = item->widget())
			widget->deleteLater();
		delete item;
	}

	tiles.clear();
	colorTarget = nullptr;
	selectedTile = nullptr;
	emptyLabel = nullptr;
}

void MixishInputBar::PrepareForSceneCollectionChange()
{
	if (rebuildTimer)
		rebuildTimer->stop();
	if (thumbnailTimer)
		thumbnailTimer->stop();

	thumbnailIndex = 0;

	if (playlistPanel)
		playlistPanel->SetController(nullptr);
	panelController = nullptr;
	if (playlistToggle) {
		QSignalBlocker block(playlistToggle);
		playlistToggle->setChecked(false);
		playlistToggle->setEnabled(false);
	}
	if (panelMode == InputPanelMode::Playlist)
		ShowPanel(InputPanelMode::None);
	mixerRestoreOnPlaylistExit = false;

	ClearTiles();
}

void MixishInputBar::OnSceneCollectionLoaded()
{
	thumbnailIndex = 0;

	if (thumbnailTimer && !thumbnailTimer->isActive())
		thumbnailTimer->start();

	ScheduleRebuild();
	RefreshAudioMixer();
}

void MixishInputBar::Rebuild()
{
	if (!tileLayout || !main->ui->scenes)
		return;

	ClearTiles();

	for (int i = 0; i < main->ui->scenes->count(); i++) {
		QListWidgetItem *item = main->ui->scenes->item(i);
		OBSScene scene = GetOBSRef<OBSScene>(item);
		if (!scene)
			continue;

		auto *tile = new MixishInputTile(switcher, scene, i + 1, tileLayout->parentWidget());
		connect(tile, &MixishInputTile::clicked, this, &MixishInputBar::SelectTile);
		connect(tile, &MixishInputTile::doubleClicked, this, &MixishInputBar::CutTile);
		tileLayout->addWidget(tile);
		tiles.push_back(tile);
	}

	if (tiles.isEmpty()) {
		emptyLabel = new QLabel(tr("No scenes yet. Use Add Input to create one."), tileLayout->parentWidget());
		emptyLabel->setAlignment(Qt::AlignCenter);
		MixishTheme::SetRole(emptyLabel, "muted");
		tileLayout->addWidget(emptyLabel);
	}

	tileLayout->addStretch(1);
	thumbnailIndex = 0;

	UpdateStates();
	UpdateVideoControls();

	if (!tiles.isEmpty())
		tiles.first()->RequestThumbnail();
}

void MixishInputBar::UpdateStates()
{
	OBSSource preview = main->GetCurrentSceneSource();
	OBSSource program = main->GetProgramSource();

	for (MixishInputTile *tile : tiles) {
		if (!tile)
			continue;

		OBSScene tileScene = tile->Scene();
		OBSSource source = tileScene ? obs_scene_get_source(tileScene) : nullptr;
		const bool isPreview = source && source == preview;
		const bool isProgram = source && source == program;
		tile->SetState(isPreview, isProgram);
		tile->SetSelected(tile == selectedTile);

		if (isPreview && !colorTarget)
			colorTarget = tile;
	}

	UpdateVideoControls();
}

void MixishInputBar::SelectTile(MixishInputTile *tile)
{
	if (!tile)
		return;

	colorTarget = tile;
	selectedTile = tile;
	UpdateVideoControls();
	switcher->SelectScene(tile->Scene());
	UpdateStates();

	/* Selecting a Playlist input always brings its queue tab forward. */
	if (panelController) {
		if (playlistPanel)
			playlistPanel->SetController(panelController);
		if (panelMode == InputPanelMode::Mixer)
			mixerRestoreOnPlaylistExit = true;
		ShowPanel(InputPanelMode::Playlist);
	}
}

void MixishInputBar::CutTile(MixishInputTile *tile)
{
	if (!tile)
		return;

	colorTarget = tile;
	switcher->CutToProgram(tile->Scene());
	UpdateStates();
}

bool MixishInputBar::HandleNumberKey(int number)
{
	if (number < 1 || number > tiles.size())
		return false;

	SelectTile(tiles[number - 1]);
	return true;
}

void MixishInputBar::RequestNextThumbnail()
{
	if (!isVisible() || tiles.isEmpty())
		return;

	thumbnailIndex = (thumbnailIndex + 1) % tiles.size();

	/* Ask for every visible tile each tick. OBS's thumbnail manager queues
	 * these and updates them round-robin, which keeps the previews close to
	 * real time instead of one tile per second. */
	for (MixishInputTile *tile : tiles) {
		if (!tile || !tile->isVisible())
			continue;
		tile->RequestThumbnail();
		tile->RefreshAudioState();
	}
}

bool MixishInputBar::eventFilter(QObject *object, QEvent *event)
{
	if (event->type() != QEvent::KeyPress)
		return QWidget::eventFilter(object, event);

	if (!isVisible() || QApplication::activeWindow() != main)
		return QWidget::eventFilter(object, event);

	QWidget *focus = QApplication::focusWidget();
	if (qobject_cast<QLineEdit *>(focus) || qobject_cast<QAbstractSpinBox *>(focus) ||
	    qobject_cast<QTextEdit *>(focus) || qobject_cast<QPlainTextEdit *>(focus))
		return QWidget::eventFilter(object, event);

	auto *keyEvent = static_cast<QKeyEvent *>(event);
	if (keyEvent->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
		return QWidget::eventFilter(object, event);

	const int key = keyEvent->key();
	if (key >= Qt::Key_1 && key <= Qt::Key_9) {
		if (HandleNumberKey(key - Qt::Key_0))
			return true;
	}

	return QWidget::eventFilter(object, event);
}
