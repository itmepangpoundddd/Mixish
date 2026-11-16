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

#include <obs.hpp>

#include <QSet>
#include <QStringList>
#include <QVector>

#include "MixishPlaylistController.hpp"

class MixishSwitcher;
class OBSBasic;
class QTimer;

/* Owns the Mixish playlist queue and its native media sources.
 *
 * Each playlist is a hidden OBS scene containing one native ffmpeg_source.
 * The queue, current item and cached durations are stored in the scene
 * collection's module data. A private muted media source is used to probe
 * durations in the background so the UI never blocks on file metadata. */
class MixishPlaylistManager : public QObject {
	Q_OBJECT

public:
	MixishPlaylistManager(MixishSwitcher *switcher, OBSBasic *main);
	~MixishPlaylistManager() override;

	MixishPlaylistController *CreatePlaylist(const QStringList &files, int number = 0);
	MixishPlaylistController *FindByScene(obs_source_t *sceneSource) const;
	const QVector<MixishPlaylistController *> &Playlists() const { return playlists; }

	/* True for the media sources owned by a playlist. Their controller
	 * manages activation and playback itself, so generic input activity
	 * handling must leave them alone. */
	bool IsPlaylistSource(obs_source_t *source) const;

	void OnSceneCollectionLoaded();
	void PrepareForSceneCollectionChange();
	void PrepareForShutdown();

	void SaveState();
	void ScheduleSave();

	QVector<MixishPlaylistItem> ExpandFiles(const QStringList &files) const;
	void QueueEdited(MixishPlaylistController *controller);
	void VerifyDurations(MixishPlaylistController *controller);
	void RecheckItems(const QVector<MixishPlaylistItem> &items);
	void UpdateVisibility(obs_source_t *previewScene, obs_source_t *programScene);
	void NoteItemDuration(const QString &path, double seconds);

signals:
	void PlaylistsChanged();

private:
	friend class MixishPlaylistController;

	void LoadState();
	void ControllerChanged();

	void StartNextProbe();
	void ProbeTick();
	void FinishProbe(bool success, double seconds);
	void ScheduleProbesForAll();
	void ScheduleProbe(const QString &path, double duration, bool force = false);
	void StopProbe();
	bool EnsureProbeSource(const QString &initialPath);

	static QVector<MixishPlaylistItem> BuildItems(const QStringList &files);
	static QVector<MixishPlaylistItem> ParseQueueFile(const QString &path);
	static bool ItemAvailable(const MixishPlaylistItem &item);

	MixishSwitcher *switcher = nullptr;
	OBSBasic *main = nullptr;

	QVector<MixishPlaylistController *> playlists;

	QTimer *saveTimer = nullptr;
	QTimer *probeTimer = nullptr;
	QStringList probeQueue;
	QSet<QString> probeQueued;
	QSet<QString> probeTried;
	QSet<QString> verifiedPaths;
	OBSSource probeSource;
	QString activeProbePath;
	int64_t probeStartedNs = 0;
	bool probeBusy = false;

	bool loading = false;
	bool collectionChanging = false;
	bool shuttingDown = false;
};
