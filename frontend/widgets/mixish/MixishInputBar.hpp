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

#include <QColor>
#include <QPointer>
#include <QVector>
#include <QWidget>

class MixishInputTile;
class MixishPlaylistController;
class MixishPlaylistPanel;
class MixishPlaylistWindow;
class MixishSwitcher;
class MixishVideoControls;
class MixishAudioMixer;
class OBSBasic;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QScrollArea;
class QSplitter;
class QTimer;

class MixishInputBar : public QWidget {
	Q_OBJECT

public:
	MixishInputBar(MixishSwitcher *switcher, OBSBasic *main, QWidget *parent = nullptr);
	~MixishInputBar() override;

	void Rebuild();
	void UpdateStates();
	int TileCount() const { return tiles.size(); }
	bool HandleNumberKey(int number);

	/* The INPUTS transport. The switcher listens to it so a play the user
	 * starts in Preview is not parked again by the off-air rule. */
	MixishVideoControls *VideoControls() const;

	/* Drops every tile (and the strong scene references they hold) before
	 * OBS clears the current scene collection, so no tile outlives the
	 * sources it points at. */
	void PrepareForSceneCollectionChange();

	/* Releases the remaining strong libobs references owned by the input
	 * bar (the video transport binding and the playlist controller). Called
	 * during shutdown, after PrepareForSceneCollectionChange(), so nothing
	 * here can keep a source alive past ClearSceneData. */
	void ClearOBSRefs();

	/* Restarts thumbnails and rebuilds tiles once a collection is loaded. */
	void OnSceneCollectionLoaded();

	/* Re-reads the audio mixer master device and source list. */
	void RefreshAudioMixer();

	/* Shows the playlist queue panel (the List tab). */
	void ShowPlaylistPanel();

	/* Opens the PlayList manager window (the bottom bar's PlayList button). */
	void ShowPlaylistManager();

protected:
	bool eventFilter(QObject *object, QEvent *event) override;

private slots:
	void ScheduleRebuild();
	void SelectTile(MixishInputTile *tile);
	void CutTile(MixishInputTile *tile);
	void RequestNextThumbnail();

private:
	enum class InputPanelMode { None, Playlist, Mixer };

	void ClearTiles();
	void UpdateVideoControls();
	void UpdatePlaylistPanel();
	MixishInputTile *SelectionTile() const;
	void SetMixerVisible(bool visible);
	void SetPlaylistVisible(bool visible);
	void ShowPanel(InputPanelMode mode);

	MixishSwitcher *switcher = nullptr;
	OBSBasic *main = nullptr;

	QPointer<QHBoxLayout> tileLayout;
	QPointer<QScrollArea> scrollArea;
	QPointer<QSplitter> contentSplitter;
	QPointer<QLabel> emptyLabel;
	QPointer<MixishVideoControls> videoControls;
	QPointer<MixishAudioMixer> audioMixer;
	QPointer<MixishPlaylistPanel> playlistPanel;
	QPointer<MixishPlaylistWindow> playlistWindow;
	QPointer<QPushButton> mixerToggle;
	QPointer<QPushButton> playlistToggle;
	QPointer<MixishPlaylistController> panelController;
	QVector<MixishInputTile *> tiles;
	QPointer<QTimer> rebuildTimer;
	QPointer<QTimer> thumbnailTimer;
	QPointer<MixishInputTile> colorTarget;
	QPointer<MixishInputTile> selectedTile;

	InputPanelMode panelMode = InputPanelMode::None;
	int thumbnailIndex = 0;
};
