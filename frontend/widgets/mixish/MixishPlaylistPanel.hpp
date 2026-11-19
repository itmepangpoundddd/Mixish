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

#include <QFrame>
#include <QPointer>

class MixishPlaylistController;
class MixishPlaylistManager;
class MixishSwitcher;
class QComboBox;
class QListWidget;
class QListWidgetItem;
class QPushButton;

/* Queue editor for the selected Playlist input.
 *
 * Opens as a tab next to the Audio Mixer. The panel edits the same
 * MixishPlaylistController used by Program playback; it does not own a second
 * queue or a second media source. */
class MixishPlaylistPanel : public QFrame {
	Q_OBJECT

public:
	explicit MixishPlaylistPanel(MixishSwitcher *switcher, QWidget *parent = nullptr);

	void SetController(MixishPlaylistController *controller);
	MixishPlaylistController *Controller() const { return controller; }
	void Refresh();

private:
	void RefreshCombo();
	void RefreshList();
	void UpdateButtons();
	int SelectedIndex() const;

	void AddItems();
	void AddUrl();
	void EditItem();
	void RemoveItems();
	void PlayOut();
	void PlaySelected(QListWidgetItem *item);

	MixishSwitcher *switcher = nullptr;
	MixishPlaylistManager *manager = nullptr;
	QPointer<MixishPlaylistController> controller;

	QComboBox *playlistCombo = nullptr;
	QListWidget *list = nullptr;
	QPushButton *addButton = nullptr;
	QPushButton *addUrlButton = nullptr;
	QPushButton *editButton = nullptr;
	QPushButton *removeButton = nullptr;
	QPushButton *playOutButton = nullptr;
	QPushButton *autoNextButton = nullptr;
	QPushButton *autoFirstButton = nullptr;
	QPushButton *loopButton = nullptr;
};
