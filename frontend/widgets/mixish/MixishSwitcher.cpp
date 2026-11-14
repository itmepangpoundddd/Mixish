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

#include "MixishAddInputController.hpp"
#include "MixishRecordingSettings.hpp"
#include "MixishStreamingSettings.hpp"
#include "MixishSwitcher.hpp"
#include "MixishTheme.hpp"
#include "MixishInputTile.hpp"
#include "MixishTopBar.hpp"
#include "MixishTransitionPanel.hpp"

#include <dialogs/OBSBasicSourceSelect.hpp>
#include <widgets/OBSBasic.hpp>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBoxLayout>
#include <QColor>
#include <QCursor>
#include <QDockWidget>
#include <QFrame>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include "moc_MixishSwitcher.cpp"

#include <algorithm>

MixishSwitcher::MixishSwitcher(OBSBasic *main_) : QObject(main_), main(main_)
{
	setObjectName(QStringLiteral("mixishSwitcher"));

	CreateLayoutMenu();

	/* Overlay stages must exist before the transition column builds its
	 * overlay strip. */
	overlayController = new MixishOverlayController(this, main);
	connect(overlayController, &MixishOverlayController::ChannelsChanged, this,
		&MixishSwitcher::OverlaysChanged);

	CreatePlaceholders();

	topBar = new MixishTopBar(this, main, main);
	main->addToolBar(Qt::TopToolBarArea, topBar);
	topBar->hide();

	main->setWindowIcon(QIcon(QStringLiteral(":/res/images/mixish-icon.svg")));
	QApplication::setWindowIcon(QIcon(QStringLiteral(":/res/images/mixish-icon.svg")));

	connect(main, &OBSBasic::PreviewProgramModeChanged, this, &MixishSwitcher::OnPreviewProgramModeChanged);

	auto reapplyLayout = [this](bool) {
		QTimer::singleShot(0, this, [this]() {
			if (mixishMode)
				SetMixishLayout(true, false);
		});
	};

	connect(main->ui->resetDocks, &QAction::triggered, this, reapplyLayout);
	connect(main->ui->resetUI, &QAction::triggered, this, reapplyLayout);

	syncTimer = new QTimer(this);
	syncTimer->setInterval(200);
	connect(syncTimer, &QTimer::timeout, this, &MixishSwitcher::Sync);
	syncTimer->start();

	obs_frontend_add_event_callback(&MixishSwitcher::FrontendEvent, this);
}

MixishSwitcher::~MixishSwitcher()
{
	obs_frontend_remove_event_callback(&MixishSwitcher::FrontendEvent, this);
}

void MixishSwitcher::FrontendEvent(enum obs_frontend_event event, void *data)
{
	auto *self = static_cast<MixishSwitcher *>(data);
	if (!self)
		return;

	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		QMetaObject::invokeMethod(self, &MixishSwitcher::InitializeAfterLoad,
					  Qt::QueuedConnection);
		break;

	case OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN:
		/* Emitted just before OBS clears the scene collection at exit.
		 * Release every libobs object we own here so the collection can
		 * be torn down without any references from the overlay stages. */
		if (QThread::currentThread() == self->thread()) {
			self->PrepareForShutdown();
		} else {
			QMetaObject::invokeMethod(self, &MixishSwitcher::PrepareForShutdown,
						  Qt::BlockingQueuedConnection);
		}
		break;

	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP:
		/* OBS destroys the old collection right after this event, so the
		 * overlay stages and tiles must let go of their sources now. The
		 * event is emitted from the UI thread; only fall back to a
		 * blocking call if that ever changes. */
		if (QThread::currentThread() == self->thread()) {
			self->PrepareForSceneCollectionChange();
		} else {
			QMetaObject::invokeMethod(self, &MixishSwitcher::PrepareForSceneCollectionChange,
						  Qt::BlockingQueuedConnection);
		}
		break;

	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		QMetaObject::invokeMethod(self, &MixishSwitcher::OnSceneCollectionLoaded,
					  Qt::QueuedConnection);
		break;

	default:
		break;
	}
}

void MixishSwitcher::PrepareForShutdown()
{
	/* Drop tile references and tear the overlay stages down while libobs is
	 * still fully alive. ClearSceneData runs right after this event. */
	if (inputBar)
		inputBar->PrepareForSceneCollectionChange();
	if (overlayController)
		overlayController->Shutdown();

	if (syncTimer)
		syncTimer->stop();
}

void MixishSwitcher::PrepareForSceneCollectionChange()
{
	if (inputBar)
		inputBar->PrepareForSceneCollectionChange();
	if (overlayController)
		overlayController->PrepareForSceneCollectionChange();
}

void MixishSwitcher::OnSceneCollectionLoaded()
{
	if (overlayController)
		overlayController->OnSceneCollectionLoaded();
	if (inputBar)
		inputBar->OnSceneCollectionLoaded();

	RefreshOverlayUI();
}

void MixishSwitcher::CreateLayoutMenu()
{
	QMenu *layoutMenu = main->ui->viewMenu->addMenu(tr("Layout"));
	layoutMenu->setObjectName(QStringLiteral("mixishLayoutMenu"));

	auto *group = new QActionGroup(this);
	group->setExclusive(true);

	mixishLayoutAction = layoutMenu->addAction(tr("Mixish Switcher Layout"));
	mixishLayoutAction->setCheckable(true);
	mixishLayoutAction->setToolTip(tr("Preview/Program monitors with a transition column and input tiles"));

	classicLayoutAction = layoutMenu->addAction(tr("Classic Mixish Layout"));
	classicLayoutAction->setCheckable(true);
	classicLayoutAction->setToolTip(tr("Original dock layout, with all docks available"));

	group->addAction(mixishLayoutAction);
	group->addAction(classicLayoutAction);

	connect(mixishLayoutAction, &QAction::triggered, this, [this](bool) { SetMixishLayout(true); });
	connect(classicLayoutAction, &QAction::triggered, this, [this](bool) { SetMixishLayout(false); });
}

void MixishSwitcher::CreatePlaceholders()
{
	transitionPanel = new MixishTransitionPanel(this, main, main->ui->canvasEditor);
	transitionPanel->hide();

	inputBar = new MixishInputBar(this, main, main->ui->centralwidget);
	inputBar->setMinimumHeight(248);

	/* Vertical splitter so the user can resize the monitor area against the
	 * input tile row, like a hardware switcher panel. */
	mainSplitter = new QSplitter(Qt::Vertical, main->ui->centralwidget);
	mainSplitter->setObjectName(QStringLiteral("mixishMainSplitter"));
	mainSplitter->setChildrenCollapsible(false);
	mainSplitter->setHandleWidth(6);
	MixishTheme::Apply(mainSplitter);

	main->ui->verticalLayout->removeWidget(main->ui->canvasEditor);
	mainSplitter->addWidget(main->ui->canvasEditor);
	mainSplitter->addWidget(inputBar);
	mainSplitter->setStretchFactor(0, 1);
	mainSplitter->setStretchFactor(1, 0);
	main->ui->verticalLayout->insertWidget(0, mainSplitter);

	splitterSaveTimer = new QTimer(this);
	splitterSaveTimer->setSingleShot(true);
	splitterSaveTimer->setInterval(300);
	connect(splitterSaveTimer, &QTimer::timeout, this, &MixishSwitcher::SaveSplitterState);
	connect(mainSplitter, &QSplitter::splitterMoved, this, [this](int, int) {
		if (splitterSaveTimer)
			splitterSaveTimer->start();
	});

	inputBar->hide();

	/* Bottom control bar: Overlay panel access and the audio mixer toggle.
	 * The remaining switcher controls join this bar in a later step. */
	bottomBar = new MixishBottomBar(this, main->ui->centralwidget);
	main->ui->verticalLayout->insertWidget(1, bottomBar);
	bottomBar->hide();
}

void MixishSwitcher::InitializeAfterLoad()
{
	if (initialized)
		return;
	initialized = true;

	config_t *config = App()->GetUserConfig();
	const bool hasSetting = config_has_user_value(config, "BasicWindow", "MixishLayout");
	const bool useMixish = hasSetting ? config_get_bool(config, "BasicWindow", "MixishLayout") : true;

	if (!useMixish) {
		/* OBS saves its dock state while the Mixish layout has the docks
		 * hidden. Restore a usable classic layout if they all came back
		 * hidden. */
		const bool anyMainDockVisible = main->ui->scenesDock->isVisible() ||
						 main->ui->sourcesDock->isVisible() ||
						 main->ui->transitionsDock->isVisible() ||
						 main->ui->mixerDock->isVisible() ||
						 (main->controlsDock && main->controlsDock->isVisible());
		if (!anyMainDockVisible)
			main->on_resetDocks_triggered(true);
	}

	SetMixishLayout(useMixish, false);
	RestoreSplitterState();

	if (inputBar)
		inputBar->RefreshAudioMixer();

	if (useMixish) {
		const QString fadeUuid = FadeTransitionUuid();
		if (!fadeUuid.isEmpty() && CurrentTransitionUuid() != fadeUuid)
			main->SetCurrentTransition(fadeUuid);
	}
}

void MixishSwitcher::OnPreviewProgramModeChanged(bool enabled)
{
	if (enabled) {
		QTimer::singleShot(0, this, [this]() {
			if (!mixishMode)
				return;
			SetTransitionColumnVisible(true);
			UpdateHeaders();
		});
	} else if (mixishMode) {
		/* The Program monitor only exists in Studio Mode. Fall back to the
		 * classic layout so OBS remains fully usable. */
		QTimer::singleShot(0, this, [this]() {
			if (mixishMode)
				SetMixishLayout(false);
		});
	}
}

void MixishSwitcher::Sync()
{
	if (overlayController)
		overlayController->Sync();

	if (!mixishMode)
		return;

	if (!main->IsPreviewProgramMode()) {
		SetMixishLayout(false);
		return;
	}

	main->ui->previewLayout->setDirection(QBoxLayout::LeftToRight);

	if (main->programWidget)
		SetTransitionColumnVisible(true);

	if (inputBar)
		inputBar->UpdateStates();

	UpdateHeaders();
}

void MixishSwitcher::SetMixishLayout(bool enabled, bool persist)
{
	if (enabled) {
		if (!main->IsPreviewProgramMode())
			main->SetPreviewProgramMode(true);

		if (!main->IsPreviewProgramMode()) {
			mixishMode = false;
			return;
		}

		if (!dockStateCaptured) {
			CaptureDockState();
			dockStateCaptured = true;
		}

		mixishMode = true;

		main->ui->previewLayout->setDirection(QBoxLayout::LeftToRight);

		if (main->ui->menubar)
			main->ui->menubar->hide();
		if (topBar) {
			main->addToolBar(Qt::TopToolBarArea, topBar);
			topBar->show();
		}

		main->ui->scenesDock->hide();
		main->ui->sourcesDock->hide();
		main->ui->transitionsDock->hide();
		main->ui->mixerDock->hide();
		if (main->controlsDock)
			main->controlsDock->hide();
		if (main->ui->contextContainer)
			main->ui->contextContainer->hide();

		SetTransitionColumnVisible(true);
		SetInputBarVisible(true);
		if (bottomBar)
			bottomBar->show();
		if (mainSplitter)
			RestoreSplitterState();
		UpdateHeaders();
	} else {
		mixishMode = false;

		if (topBar)
			topBar->hide();
		if (bottomBar)
			bottomBar->hide();
		if (main->ui->menubar)
			main->ui->menubar->show();

		SetTransitionColumnVisible(false);
		SetInputBarVisible(false);
		if (dockStateCaptured)
			ApplyDockState();
		ClearHeaderOverrides();
		main->ResetUI();
		main->UpdatePreviewProgramIndicators();
	}

	if (mixishLayoutAction) {
		QSignalBlocker block(mixishLayoutAction);
		mixishLayoutAction->setChecked(mixishMode);
	}
	if (classicLayoutAction) {
		QSignalBlocker block(classicLayoutAction);
		classicLayoutAction->setChecked(!mixishMode);
	}

	if (persist) {
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "MixishLayout", mixishMode);
		config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
	}
}

void MixishSwitcher::SetTransitionColumnVisible(bool visible)
{
	if (!main->programOptions || !main->programWidget) {
		if (transitionPanel)
			transitionPanel->hide();
		if (main->programOptions)
			main->programOptions->setVisible(!visible);
		return;
	}

	QBoxLayout *layout = main->ui->previewLayout;

	if (visible) {
		const int programIndex = layout->indexOf(main->programWidget);
		if (programIndex >= 0 && layout->indexOf(transitionPanel) == -1)
			layout->insertWidget(programIndex, transitionPanel);

		if (transitionPanel)
			transitionPanel->show();

		main->programOptions->hide();

		const int previewIndex = layout->indexOf(main->ui->previewContainer);
		if (previewIndex >= 0)
			layout->setStretch(previewIndex, 1);

		const int panelIndex = layout->indexOf(transitionPanel);
		if (panelIndex >= 0)
			layout->setStretch(panelIndex, 0);

		const int newProgramIndex = layout->indexOf(main->programWidget);
		if (newProgramIndex >= 0)
			layout->setStretch(newProgramIndex, 1);
	} else {
		if (transitionPanel)
			transitionPanel->hide();
		main->programOptions->show();
	}
}

void MixishSwitcher::SetInputBarVisible(bool visible)
{
	if (inputBar)
		inputBar->setVisible(visible);
}

void MixishSwitcher::UpdateHeaders()
{
	if (!main->IsPreviewProgramMode())
		return;

	QLabel *previewLabel = main->ui->previewLabel;
	QLabel *programLabel = main->programLabel;

	if (!previewLabel || !programLabel)
		return;

	previewLabel->setTextFormat(Qt::PlainText);
	programLabel->setTextFormat(Qt::PlainText);

	if (previewLabel->minimumHeight() != MixishTheme::MonitorHeaderHeight)
		previewLabel->setMinimumHeight(MixishTheme::MonitorHeaderHeight);
	if (previewLabel->maximumHeight() != MixishTheme::MonitorHeaderHeight)
		previewLabel->setMaximumHeight(MixishTheme::MonitorHeaderHeight);
	MixishTheme::SetRole(previewLabel, "previewHeader");
	MixishTheme::Apply(previewLabel);
	previewLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	previewLabel->setVisible(true);

	if (programLabel->minimumHeight() != MixishTheme::MonitorHeaderHeight)
		programLabel->setMinimumHeight(MixishTheme::MonitorHeaderHeight);
	if (programLabel->maximumHeight() != MixishTheme::MonitorHeaderHeight)
		programLabel->setMaximumHeight(MixishTheme::MonitorHeaderHeight);
	MixishTheme::SetRole(programLabel, "programHeader");
	MixishTheme::Apply(programLabel);
	programLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	programLabel->setVisible(true);

	OBSSource previewScene = main->GetCurrentSceneSource();
	OBSSource programScene = main->GetProgramSource();

	const QString previewName = previewScene ? QString::fromUtf8(obs_source_get_name(previewScene))
						 : tr("No scene");
	const QString programName = programScene ? QString::fromUtf8(obs_source_get_name(programScene))
						 : tr("No program");

	const QString previewText = tr("PREVIEW   %1").arg(previewName);
	const QString programText = tr("PROGRAM   %1").arg(programName);

	if (previewLabel->text() != previewText)
		previewLabel->setText(previewText);
	if (programLabel->text() != programText)
		programLabel->setText(programText);
}

void MixishSwitcher::ClearHeaderOverrides()
{
	if (main->ui->previewLabel) {
		main->ui->previewLabel->setMinimumHeight(0);
		main->ui->previewLabel->setMaximumHeight(QWIDGETSIZE_MAX);
		main->ui->previewLabel->setProperty("mixishRole", QVariant());
		main->ui->previewLabel->setStyleSheet(QString());
	}

	if (main->programLabel) {
		main->programLabel->setMinimumHeight(0);
		main->programLabel->setMaximumHeight(QWIDGETSIZE_MAX);
		main->programLabel->setProperty("mixishRole", QVariant());
		main->programLabel->setStyleSheet(QString());
	}
}

void MixishSwitcher::CaptureDockState()
{
	scenesDockVisible = main->ui->scenesDock->isVisible();
	sourcesDockVisible = main->ui->sourcesDock->isVisible();
	transitionsDockVisible = main->ui->transitionsDock->isVisible();
	mixerDockVisible = main->ui->mixerDock->isVisible();
	controlsDockVisible = main->controlsDock && main->controlsDock->isVisible();
	contextContainerVisible = main->ui->contextContainer && main->ui->contextContainer->isVisible();
}

void MixishSwitcher::ApplyDockState()
{
	main->ui->scenesDock->setVisible(scenesDockVisible);
	main->ui->sourcesDock->setVisible(sourcesDockVisible);
	main->ui->transitionsDock->setVisible(transitionsDockVisible);
	main->ui->mixerDock->setVisible(mixerDockVisible);
	if (main->controlsDock)
		main->controlsDock->setVisible(controlsDockVisible);
	if (main->ui->contextContainer)
		main->ui->contextContainer->setVisible(contextContainerVisible);
}

void MixishSwitcher::SetAudioMixerVisible(bool visible)
{
	if (!main->ui->mixerDock)
		return;

	main->ui->mixerDock->setVisible(visible);

	QAction *toggle = main->ui->mixerDock->toggleViewAction();
	if (toggle) {
		QSignalBlocker block(toggle);
		toggle->setChecked(visible);
	}

	if (mixishMode)
		mixerDockVisible = visible;
}

bool MixishSwitcher::IsAudioMixerVisible() const
{
	return main->ui->mixerDock && main->ui->mixerDock->isVisible();
}

QStringList MixishSwitcher::TransitionUuids() const
{
	QStringList uuids;
	for (const std::string &uuid : main->transitionUuids)
		uuids.push_back(QString::fromStdString(uuid));
	return uuids;
}

QString MixishSwitcher::TransitionName(const QString &uuid) const
{
	auto it = main->transitions.find(uuid.toStdString());
	if (it == main->transitions.end() || !it->second)
		return QString();
	return QString::fromUtf8(obs_source_get_name(it->second));
}

QString MixishSwitcher::CurrentTransitionUuid() const
{
	return QString::fromStdString(main->currentTransitionUuid);
}

QString MixishSwitcher::CutTransitionUuid() const
{
	if (!main->cutTransition)
		return QString();
	return QString::fromUtf8(obs_source_get_uuid(main->cutTransition));
}

QString MixishSwitcher::FadeTransitionUuid() const
{
	if (!main->fadeTransition)
		return QString();
	return QString::fromUtf8(obs_source_get_uuid(main->fadeTransition));
}

bool MixishSwitcher::FadeToBlackActive() const
{
	return main->prevFTBSource.Get() != nullptr;
}

int MixishSwitcher::TransitionDuration() const
{
	return main->GetTransitionDuration();
}

void MixishSwitcher::SetTransitionDuration(int ms)
{
	main->SetTransitionDuration(ms);
}

void MixishSwitcher::TriggerTransition(const QString &uuid)
{
	if (!main->IsPreviewProgramMode() || uuid.isEmpty())
		return;

	OBSSourceAutoRelease activeTransition = obs_get_output_source(0);
	if (activeTransition && obs_transition_is_active(activeTransition))
		return;

	main->SetCurrentTransition(uuid);
	main->TransitionClicked();
}

void MixishSwitcher::CutPreviewToProgram()
{
	CutToProgram(main->GetCurrentScene());
}

void MixishSwitcher::CutToProgram(OBSScene scene)
{
	if (!scene)
		return;

	OBSSource source = obs_scene_get_source(scene);
	if (!source)
		return;

	if (main->IsPreviewProgramMode()) {
		if (main->GetCurrentTransition() != main->cutTransition) {
			main->OverrideTransition(main->cutTransition);
			main->overridingTransition = true;
		}
		main->TransitionToScene(source, false, true, 0);
	} else {
		main->SetCurrentScene(source, true);
	}
}

void MixishSwitcher::ToggleFadeToBlack()
{
	if (!main->IsPreviewProgramMode())
		return;

	OBSSourceAutoRelease activeTransition = obs_get_output_source(0);
	if (activeTransition && obs_transition_is_active(activeTransition))
		return;

	if (!main->fadeTransition)
		return;

	if (main->GetCurrentTransition() != main->fadeTransition) {
		main->OverrideTransition(main->fadeTransition);
		main->overridingTransition = true;
	}

	main->TransitionToScene(main->GetCurrentSceneSource(), false, true, main->GetTransitionDuration(), true);
}

void MixishSwitcher::AddTransition()
{
	main->on_transitionAdd_clicked();
}

void MixishSwitcher::EditCurrentTransition()
{
	main->on_transitionProps_clicked();
}

void MixishSwitcher::SetTBarPosition(int value)
{
	if (!main->tBar)
		return;

	{
		QSignalBlocker block(main->tBar);
		main->tBar->setValue(value);
	}

	main->TBarChanged(value);
}

void MixishSwitcher::ReleaseTBar()
{
	if (main->tBar)
		main->TBarReleased();
}

int MixishSwitcher::TBarPosition() const
{
	return main->tBar ? main->tBar->value() : 0;
}

bool MixishSwitcher::TBarActive() const
{
	return main->tBarActive;
}

void MixishSwitcher::ResetTBarIfIdle()
{
	if (!main || main->isClosing() || !main->tBar || main->tBarActive)
		return;

	QSignalBlocker block(main->tBar);
	main->tBar->setValue(0);
}

void MixishSwitcher::SelectScene(OBSScene scene)
{
	if (!scene)
		return;

	main->SetCurrentScene(obs_scene_get_source(scene));
}

void MixishSwitcher::AddInput()
{
	OBSBasicSourceSelect dialog(main, main->undo_s);
	MixishAddInputController controller(&dialog, this, main);
	controller.Setup();
	dialog.exec();
}

void MixishSwitcher::AddScene()
{
	main->on_actionAddScene_triggered();
}

void MixishSwitcher::OpenOutputSettings()
{
	main->on_action_Settings_triggered();
}

void MixishSwitcher::OpenRecordingSettings()
{
	MixishRecordingSettings dialog(main, main);
	dialog.exec();
}

void MixishSwitcher::OpenStreamingSettings()
{
	MixishStreamingSettings dialog(main, main);
	dialog.exec();
}

namespace {

struct DeepSearch {
	obs_source_t *needle = nullptr;
	int depth = 0;
	bool found = false;
};

bool DeepSearchItem(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	auto *search = static_cast<DeepSearch *>(param);
	obs_source_t *source = item ? obs_sceneitem_get_source(item) : nullptr;
	if (!source)
		return true;

	if (source == search->needle) {
		search->found = true;
		return false;
	}

	if (search->depth < 32) {
		if (obs_scene_t *nested = obs_scene_from_source(source)) {
			DeepSearch inner;
			inner.needle = search->needle;
			inner.depth = search->depth + 1;
			obs_scene_enum_items(nested, DeepSearchItem, &inner);
			if (inner.found) {
				search->found = true;
				return false;
			}
		}
	}

	return true;
}

bool SceneContainsSource(obs_scene_t *scene, obs_source_t *needle, int depth = 0)
{
	if (!scene || !needle)
		return false;

	DeepSearch search;
	search.needle = needle;
	search.depth = depth;
	obs_scene_enum_items(scene, DeepSearchItem, &search);
	return search.found;
}

} // namespace

void MixishSwitcher::DeleteScene(OBSScene scene)
{
	if (!scene)
		return;

	OBSSourceAutoRelease sceneSource = obs_scene_get_source(scene);
	if (!sceneSource)
		return;

	QListWidget *sceneList = main->ui->scenes;
	int targetRow = -1;

	for (int i = 0; i < sceneList->count(); ++i) {
		QListWidgetItem *item = sceneList->item(i);
		if (!item)
			continue;
		if (GetOBSRef<OBSScene>(item) == scene) {
			targetRow = i;
			break;
		}
	}

	if (targetRow < 0)
		return;

	/* Collect usage warnings: overlay channels and scenes/groups that
	 * contain this input as a layer. */
	QStringList uses;

	if (overlayController) {
		for (int i = 0; i < overlayController->ChannelCount(); ++i) {
			OBSSource assigned = overlayController->InputSource(i);
			if (!assigned)
				continue;
			if (assigned == sceneSource.Get()) {
				uses << tr("overlay OV%1").arg(i + 1);
			} else if (obs_scene_t *assignedScene = obs_scene_from_source(assigned)) {
				if (SceneContainsSource(assignedScene, sceneSource.Get(), 1))
					uses << tr("overlay OV%1").arg(i + 1);
			}
		}
	}

	for (int i = 0; i < sceneList->count(); ++i) {
		QListWidgetItem *item = sceneList->item(i);
		if (!item)
			continue;
		OBSScene other = GetOBSRef<OBSScene>(item);
		if (!other || other == scene)
			continue;
		if (SceneContainsSource(other, sceneSource.Get(), 0))
			uses << tr("layer in \"%1\"")
					.arg(QString::fromUtf8(obs_source_get_name(obs_scene_get_source(other))));
	}

	const QString name = QString::fromUtf8(obs_source_get_name(sceneSource));

	QMessageBox confirm(main);
	confirm.setIcon(QMessageBox::Warning);
	confirm.setWindowTitle(tr("Delete Input"));
	confirm.setText(tr("Delete \"%1\"?").arg(name));

	QString detail;
	if (!uses.isEmpty())
		detail = tr("This input is currently used as: %1.\n\n").arg(uses.join(QStringLiteral(", ")));
	detail += tr("The input and its sources will be removed from the scene collection.");
	confirm.setInformativeText(detail);

	QPushButton *deleteButton = confirm.addButton(tr("Delete"), QMessageBox::DestructiveRole);
	QPushButton *cancelButton = confirm.addButton(tr("No"), QMessageBox::RejectRole);
	confirm.setDefaultButton(cancelButton);
	confirm.setEscapeButton(cancelButton);
	confirm.exec();

	if (confirm.clickedButton() != deleteButton)
		return;

	/* Release overlay channels that used this input before the source goes
	 * away, so no stage keeps a reference to a removed scene. */
	if (overlayController) {
		for (int i = 0; i < overlayController->ChannelCount(); ++i) {
			OBSSource assigned = overlayController->InputSource(i);
			if (!assigned)
				continue;
			if (assigned == sceneSource.Get()) {
				overlayController->Clear(i);
			} else if (obs_scene_t *assignedScene = obs_scene_from_source(assigned)) {
				if (SceneContainsSource(assignedScene, sceneSource.Get(), 1))
					overlayController->Clear(i);
			}
		}
	}

	/* Remove the scene like OBS does, then drop it from the scene list. */
	sceneList->setCurrentRow(targetRow);
	obs_source_remove(sceneSource);

	obs_enum_scenes(
		[](void *, obs_source_t *source) {
			if (strcmp(obs_source_get_id(source), "scene") == 0)
				obs_scene_prune_sources(obs_scene_from_source(source));
			return true;
		},
		nullptr);

	main->RemoveScene(OBSSource(sceneSource.Get()));

	if (inputBar)
		inputBar->UpdateStates();
}

void MixishSwitcher::OpenOverlayPanel()
{
	if (!overlayController)
		return;

	if (!overlayPanel) {
		overlayPanel = new MixishOverlayPanel(this, main, main);
		overlayPanel->setAttribute(Qt::WA_DeleteOnClose, false);
	}

	overlayPanel->Refresh();
	overlayPanel->show();
	overlayPanel->raise();
	overlayPanel->activateWindow();
}

void MixishSwitcher::RefreshOverlayUI()
{
	emit OverlaysChanged();
	if (overlayPanel)
		overlayPanel->Refresh();
}

void MixishSwitcher::RefreshSceneTiles()
{
	if (inputBar)
		inputBar->Rebuild();
}

static OBSSource MixishPrimaryInputSource(OBSScene scene)
{
	if (!scene)
		return nullptr;

	struct SourceSearch {
		OBSSource input;
		OBSSource any;
	} search;

	obs_scene_enum_items(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
			auto *search = static_cast<SourceSearch *>(param);
			obs_source_t *source = obs_sceneitem_get_source(item);
			if (!source)
				return true;

			if (!search->any)
				search->any = source;

			if (obs_source_get_type(source) == OBS_SOURCE_TYPE_INPUT) {
				search->input = source;
				return false;
			}

			return true;
		},
		&search);

	if (search.input)
		return search.input;
	if (search.any)
		return search.any;
	return obs_scene_get_source(scene);
}

void MixishSwitcher::OpenInputProperties(OBSScene scene, int number)
{
	if (!scene || !main)
		return;

	OBSSource source = MixishPrimaryInputSource(scene);
	if (!source)
		return;

	if (inputProperties) {
		inputProperties->close();
		inputProperties = nullptr;
	}

	auto *dialog = new MixishInputProperties(main, main, scene, source, number);
	dialog->setAttribute(Qt::WA_DeleteOnClose, true);
	inputProperties = dialog;
	dialog->show();
	dialog->raise();
	dialog->activateWindow();
}

static std::string MixishUniqueSourceName(const std::string &base)
{
	std::string stem = base.empty() ? "Input" : base;
	std::string candidate = stem;

	for (int suffix = 2;; ++suffix) {
		OBSSourceAutoRelease existing = obs_get_source_by_name(candidate.c_str());
		if (!existing)
			break;
		candidate = stem + " " + std::to_string(suffix);
	}

	return candidate;
}

OBSScene MixishSwitcher::CreateInputScene(OBSSource source, int number)
{
	if (!source)
		return nullptr;

	const std::string baseName = std::string(obs_source_get_name(source));
	const std::string sceneName = MixishUniqueSourceName(baseName);

	OBSSceneAutoRelease scene = obs_scene_create(sceneName.c_str());
	if (!scene)
		return nullptr;

	obs_scene_add(scene, source);
	obs_source_t *sceneSource = obs_scene_get_source(scene);
	if (!sceneSource)
		return nullptr;

	/* Add the scene to the OBS scene list without selecting it. The input
	 * remains hidden until the operator sends it to Preview. Guard against
	 * double-inserting the same scene if the create path is re-entered. */
	bool alreadyListed = false;
	for (int i = 0; i < main->ui->scenes->count(); ++i) {
		QListWidgetItem *listItem = main->ui->scenes->item(i);
		if (!listItem)
			continue;
		if (GetOBSRef<OBSScene>(listItem) == scene.Get()) {
			alreadyListed = true;
			break;
		}
	}

	if (!alreadyListed) {
		main->AddScene(OBSSource(sceneSource));
	} else {
		blog(LOG_WARNING, "Mixish: scene '%s' is already in the scene list", sceneName.c_str());
	}

	if (number >= 1 && number <= 9) {
		int currentIndex = -1;
		for (int i = 0; i < main->ui->scenes->count(); ++i) {
			QListWidgetItem *listItem = main->ui->scenes->item(i);
			if (!listItem)
				continue;
			OBSScene itemScene = GetOBSRef<OBSScene>(listItem);
			if (itemScene == scene.Get()) {
				currentIndex = i;
				break;
			}
		}

		if (currentIndex >= 0) {
			const int targetIndex = std::clamp(number - 1, 0, main->ui->scenes->count() - 1);
			if (targetIndex != currentIndex) {
				QListWidgetItem *item = main->ui->scenes->takeItem(currentIndex);
				main->ui->scenes->insertItem(targetIndex, item);
			}
		}
	}

	main->SaveProject();
	RefreshSceneTiles();

	return OBSScene(scene.Get());
}

void MixishSwitcher::OpenSceneMenu(OBSScene scene, QWidget *anchor)
{
	if (!scene)
		return;

	SelectScene(scene);

	auto *menu = new QMenu(main);
	menu->setAttribute(Qt::WA_DeleteOnClose);

	QAction *properties = menu->addAction(tr("Input Properties..."));
	connect(properties, &QAction::triggered, menu, [this, scene]() { OpenInputProperties(scene); });

	menu->addSeparator();

	QAction *rename = menu->addAction(tr("Rename Scene"));
	connect(rename, &QAction::triggered, main, &OBSBasic::EditSceneName);

	QAction *filters = menu->addAction(tr("Scene Filters..."));
	connect(filters, &QAction::triggered, main, &OBSBasic::OpenSceneFilters);

	QAction *duplicate = menu->addAction(tr("Duplicate Scene"));
	connect(duplicate, &QAction::triggered, main, &OBSBasic::DuplicateSelectedScene);

	menu->addSeparator();

	QAction *deleteAction = menu->addAction(tr("Delete Input…"));
	connect(deleteAction, &QAction::triggered, menu, [this, scene]() { DeleteScene(scene); });

	menu->addSeparator();

	QMenu *colorMenu = menu->addMenu(tr("Color Tag"));
	const auto &palette = MixishInputTile::Palette();
	for (const QColor &color : palette) {
		QPixmap swatch(12, 12);
		swatch.fill(color);
		QAction *colorAction = colorMenu->addAction(QIcon(swatch), color.name());
		connect(colorAction, &QAction::triggered, menu, [this, scene, color]() {
			MixishInputTile::SetSceneColor(scene, color);
			main->SaveProject();
			RefreshSceneTiles();
		});
	}
	colorMenu->addSeparator();
	QAction *clearColor = colorMenu->addAction(tr("Clear"));
	connect(clearColor, &QAction::triggered, menu, [this, scene]() {
		MixishInputTile::SetSceneColor(scene, QColor());
		main->SaveProject();
		RefreshSceneTiles();
	});

	menu->addSeparator();

	/* Scene-transition overrides stay available from OBS's own scene list
	 * menu; keeping them out of the tile menu keeps this menu compact. */

	const QPoint position = anchor ? anchor->mapToGlobal(QPoint(0, anchor->height() + 2))
				       : (QCursor::pos() + QPoint(8, 8));
	QTimer::singleShot(0, menu, [menu, position]() { menu->popup(position); });
}

void MixishSwitcher::SaveSplitterState()
{
	if (!mainSplitter)
		return;

	const QByteArray state = mainSplitter->saveState().toBase64();
	config_set_string(App()->GetUserConfig(), "BasicWindow", "MixishSplitterState", state.constData());
	config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
}

void MixishSwitcher::RestoreSplitterState()
{
	if (!mainSplitter)
		return;

	config_t *config = App()->GetUserConfig();
	const char *stored = config_get_string(config, "BasicWindow", "MixishSplitterState");
	if (stored && *stored) {
		QByteArray state = QByteArray::fromBase64(QByteArray(stored));
		if (mainSplitter->restoreState(state))
			return;
	}

	const int total = std::max(main->height(), 600);
	mainSplitter->setSizes({std::max(360, total - 240), 220});
}



