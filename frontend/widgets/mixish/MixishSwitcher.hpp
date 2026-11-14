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

#pragma once

#include <obs-frontend-api.h>

#include <obs.hpp>

#include "MixishBottomBar.hpp"
#include "MixishInputBar.hpp"
#include "MixishInputProperties.hpp"
#include "MixishOverlayController.hpp"
#include "MixishOverlayPanel.hpp"
#include "MixishTopBar.hpp"
#include "MixishTransitionPanel.hpp"

#include <QObject>
#include <QPointer>
#include <QStringList>

class OBSBasic;
class QAction;
class QActionGroup;
class QFrame;
class QMenu;
class QSplitter;
class QTimer;

/* Mixish main-window switcher shell.
 *
 * This class adapts the existing OBS Studio preview/program widgets and
 * Studio Mode layout into a vMix-style switcher layout without duplicating
 * the OBS scene or transition engines.
 */
class MixishSwitcher : public QObject {
	Q_OBJECT

public:
	explicit MixishSwitcher(OBSBasic *main);
	~MixishSwitcher() override;

	bool IsMixishLayout() const { return mixishMode; }
	void SetMixishLayout(bool enabled, bool persist = true);

	void SetAudioMixerVisible(bool visible);
	bool IsAudioMixerVisible() const;

	/* Transition system helpers used by MixishTransitionPanel. */
	QStringList TransitionUuids() const;
	QString TransitionName(const QString &uuid) const;
	QString CurrentTransitionUuid() const;
	QString CutTransitionUuid() const;
	QString FadeTransitionUuid() const;
	bool FadeToBlackActive() const;
	int TransitionDuration() const;
	void SetTransitionDuration(int ms);
	void TriggerTransition(const QString &uuid);
	void CutPreviewToProgram();
	void CutToProgram(OBSScene scene);
	void ToggleFadeToBlack();
	void AddTransition();
	void EditCurrentTransition();
	void SetTBarPosition(int value);
	void ReleaseTBar();
	int TBarPosition() const;
	bool TBarActive() const;
	void ResetTBarIfIdle();

	/* Scene helpers used by the input tiles. */
	void SelectScene(OBSScene scene);
	void AddInput();
	void AddScene();
	void OpenOutputSettings();
	void OpenRecordingSettings();
	void OpenStreamingSettings();
	void DeleteScene(OBSScene scene);
	void OpenSceneMenu(OBSScene scene, QWidget *anchor);
	void OpenInputProperties(OBSScene scene, int number = 0);
	void RefreshSceneTiles();
	OBSScene CreateInputScene(OBSSource source, int number = 0);
	OBSBasic *GetMain() const { return main; }

	/* Overlay channels. */
	MixishOverlayController *GetOverlays() const { return overlayController; }
	void OpenOverlayPanel();
	void RefreshOverlayUI();

	MixishTransitionPanel *GetTransitionPanel() const { return transitionPanel; }
	MixishInputBar *GetInputBar() const { return inputBar; }

signals:
	void OverlaysChanged();

private slots:
	void InitializeAfterLoad();
	void OnPreviewProgramModeChanged(bool enabled);
	void Sync();
	void PrepareForSceneCollectionChange();
	void OnSceneCollectionLoaded();
	void PrepareForShutdown();

private:
	void CreateLayoutMenu();
	void CreatePlaceholders();
	void SetTransitionColumnVisible(bool visible);
	void SetInputBarVisible(bool visible);
	void UpdateHeaders();
	void ClearHeaderOverrides();
	void CaptureDockState();
	void ApplyDockState();
	void SaveSplitterState();
	void RestoreSplitterState();

	static void FrontendEvent(enum obs_frontend_event event, void *data);

	OBSBasic *main = nullptr;

	bool mixishMode = false;
	bool initialized = false;
	bool dockStateCaptured = false;

	bool scenesDockVisible = true;
	bool sourcesDockVisible = true;
	bool transitionsDockVisible = true;
	bool mixerDockVisible = true;
	bool controlsDockVisible = true;
	bool contextContainerVisible = true;

	QPointer<MixishTopBar> topBar;
	QSplitter *mainSplitter = nullptr;
	QPointer<MixishBottomBar> bottomBar;
	QPointer<MixishOverlayController> overlayController;
	QPointer<MixishOverlayPanel> overlayPanel;
	QPointer<MixishTransitionPanel> transitionPanel;
	QPointer<MixishInputBar> inputBar;
	QPointer<MixishInputProperties> inputProperties;
	QPointer<QTimer> syncTimer;
	QPointer<QTimer> splitterSaveTimer;

	QAction *mixishLayoutAction = nullptr;
	QAction *classicLayoutAction = nullptr;
};
