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
#include <QHash>
#include <QPair>
#include <QPointer>
#include <QString>
#include <QVector>

#include <obs.hpp>

class MixishPlaylistController;
class MixishPlaylistManager;
class MixishSwitcher;
class QCheckBox;
class QComboBox;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTimer;

/* Playlist manager: a window where a queue is built out of the collection's
 * inputs. Left side lists every input, the right side is the queue; the
 * queue is played by cutting each input to Program and advances on its own
 * when the running item's media ends (unless Manual Mode is on). */
class MixishPlaylistWindow : public QFrame {
	Q_OBJECT

public:
	MixishPlaylistWindow(MixishSwitcher *switcher, obs_data_t *storage,
			     QWidget *parent = nullptr);

	void Refresh();

private:
	struct Entry {
		QString uuid;
		QString label;
	};

	void Load();
	void Save();
	void SetActive(const QString &name);
	void ApplyName();
	void RefreshAvailable();
	void RefreshQueue();
	void UpdateButtons();
	void NewList();
	void DeleteList();
	void AddSelected();
	void RemoveSelected();
	void MoveSelected(int delta);
	void PlayIndex(int index, bool restart);
	void PlayNext();
	void PlayPrevious();
	void StopPlayback();
	void CheckAutoAdvance();
	void ClearOverlayChannels();
	OBSScene SceneForEntry(const Entry &entry) const;
	int QueueSelection() const;
	QVector<Entry> &ActiveQueue();

	MixishSwitcher *switcher = nullptr;
	OBSDataAutoRelease storageData;
	QHash<QString, QVector<Entry>> queues;
	QString activeName;
	int current = -1;
	OBSWeakSource playing;

	QPushButton *newButton = nullptr;
	QPushButton *deleteButton = nullptr;
	QComboBox *nameCombo = nullptr;
	QListWidget *availableList = nullptr;
	QListWidget *queueList = nullptr;
	QPushButton *addButton = nullptr;
	QPushButton *removeButton = nullptr;
	QPushButton *upButton = nullptr;
	QPushButton *downButton = nullptr;
	QCheckBox *loopCheck = nullptr;
	QCheckBox *beginCheck = nullptr;
	QCheckBox *clearOverlayCheck = nullptr;
	QCheckBox *manualCheck = nullptr;
	QPushButton *stopButton = nullptr;
	QPushButton *prevButton = nullptr;
	QPushButton *nextButton = nullptr;
	QPushButton *closeButton = nullptr;
	QTimer *advanceTimer = nullptr;
};

/* Queue editor for the selected Playlist input.
 *
 * It runs in its own window, opened from the PlayList button in the INPUTS
 * header, and edits the same MixishPlaylistController used by Program
 * playback; it does not own a second queue or a second media source. */
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

	void NewPlaylist();
	void DeletePlaylist();
	void AddItems();
	void AddUrl();
	void EditItem();
	void RemoveItems();
	void MoveSelected(int delta);
	void PlayOut();
	void PlaySelected(QListWidgetItem *item);
	void StopPlayback();
	void PlayPreviousItem();
	void PlayNextItem();

	MixishSwitcher *switcher = nullptr;
	MixishPlaylistManager *manager = nullptr;
	QPointer<MixishPlaylistController> controller;

	QComboBox *playlistCombo = nullptr;
	QPushButton *newButton = nullptr;
	QPushButton *deleteButton = nullptr;
	QListWidget *list = nullptr;
	QPushButton *addButton = nullptr;
	QPushButton *addUrlButton = nullptr;
	QPushButton *editButton = nullptr;
	QPushButton *removeButton = nullptr;
	QPushButton *upButton = nullptr;
	QPushButton *downButton = nullptr;
	QPushButton *playOutButton = nullptr;
	QPushButton *stopButton = nullptr;
	QPushButton *prevButton = nullptr;
	QPushButton *nextButton = nullptr;
	QPushButton *autoNextButton = nullptr;
	QPushButton *autoFirstButton = nullptr;
	QPushButton *loopButton = nullptr;
};
