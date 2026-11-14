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
class MixishSwitcher;
class MixishVideoControls;
class MixishAudioMixer;
class OBSBasic;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QScrollArea;
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

	/* Drops every tile (and the strong scene references they hold) before
	 * OBS clears the current scene collection, so no tile outlives the
	 * sources it points at. */
	void PrepareForSceneCollectionChange();

	/* Restarts thumbnails and rebuilds tiles once a collection is loaded. */
	void OnSceneCollectionLoaded();

	/* Re-reads the audio mixer master device and source list. */
	void RefreshAudioMixer();

protected:
	bool eventFilter(QObject *object, QEvent *event) override;

private slots:
	void ScheduleRebuild();
	void SelectTile(MixishInputTile *tile);
	void CutTile(MixishInputTile *tile);
	void RequestNextThumbnail();

private:
	void ClearTiles();
	void UpdateVideoControls();
	void SetMixerVisible(bool visible);

	MixishSwitcher *switcher = nullptr;
	OBSBasic *main = nullptr;

	QPointer<QHBoxLayout> tileLayout;
	QPointer<QScrollArea> scrollArea;
	QPointer<QLabel> emptyLabel;
	QPointer<MixishVideoControls> videoControls;
	QPointer<MixishAudioMixer> audioMixer;
	QPointer<QPushButton> mixerToggle;
	QVector<MixishInputTile *> tiles;
	QPointer<QTimer> rebuildTimer;
	QPointer<QTimer> thumbnailTimer;
	QPointer<MixishInputTile> colorTarget;
	QPointer<MixishInputTile> selectedTile;

	int thumbnailIndex = 0;
};
