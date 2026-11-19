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

#include <QObject>
#include <QString>
#include <QVector>

class MixishPlaylistManager;

struct MixishPlaylistItem {
	QString path;
	QString name;
	bool enabled = true;
	bool missing = false;
	double duration = -1.0;
	/* HLS passthrough: the media source opens this path directly instead of
	 * the queue expanding it into segments. streamError is transient and
	 * never saved. */
	bool stream = false;
	bool streamError = false;
};

/* One Mixish playlist backed by a native OBS media source.
 *
 * Stage 1 owns one media source per playlist. Switching items updates that
 * source's local_file and restarts it; the queue is kept in a single
 * controller so a second preloaded media source can be added for gapless v2
 * without changing the public behavior. */
class MixishPlaylistController : public QObject {
	Q_OBJECT

public:
	MixishPlaylistController(MixishPlaylistManager *manager, const QString &sceneUuid,
				 const QString &mediaUuid,
				 const QVector<MixishPlaylistItem> &items);
	~MixishPlaylistController() override;

	const QString &SceneUuid() const { return sceneUuid; }
	const QString &MediaUuid() const { return mediaUuid; }
	const QVector<MixishPlaylistItem> &Items() const { return items; }
	int CurrentIndex() const { return current; }
	bool Continuous() const { return continuous; }
	bool Looping() const { return looping; }
	bool AutoFirst() const { return autoFirst; }
	obs_source_t *MediaSource() const { return media.Get(); }
	OBSScene Scene() const;
	QString SceneName() const;
	QString MediaSourceName() const;
	QString NextUpName() const;
	double LastGapMs() const { return lastGapMs; }

	void SetScene(OBSScene scene, obs_source_t *mediaSource);
	void PlayIndex(int index, bool autoPlay = true);
	void PlayNext();
	void PlayPrevious();
	void PlayFirst();
	void PlayPause();
	void Restart();
	void SetContinuous(bool value);
	void SetLooping(bool value);
	void SetAutoFirst(bool value);
	void AddItems(const QVector<MixishPlaylistItem> &newItems);
	void RemoveItems(const QVector<int> &indexes);
	void ReplaceItem(int index, const MixishPlaylistItem &item);
	void SetItemDuration(int index, double seconds);
	void SetMissing(int index, bool missing);
	void SavePosition();
	void Save(obs_data_t *data) const;
	void Load(obs_data_t *data);
	void RestoreCurrent(bool autoPlay);
	void SetPlaylistVisible(bool visible);
	void StopIfHidden();
	void Stop();

signals:
	void Changed();
	void CurrentChanged(int index);
	void GapMeasured(double milliseconds);

private:
	friend class MixishPlaylistManager;

	void ConnectMedia();
	void DisconnectMedia();
	void ApplyPlaybackSettings();
	void HandleEnded();
	void HandleStarted();
	void AdvanceAfterEnd();
	bool SwitchTo(int index, bool autoPlay);
	int FirstValidIndex() const;
	int NextValidIndex(int from) const;
	int PreviousValidIndex(int from) const;
	static void OnMediaEnded(void *data, calldata_t *calldata);
	static void OnMediaStarted(void *data, calldata_t *calldata);

	MixishPlaylistManager *manager = nullptr;
	QString sceneUuid;
	QString mediaUuid;
	QVector<MixishPlaylistItem> items;
	OBSSource sceneSource;
	OBSSource media;
	signal_handler_t *mediaSignals = nullptr;
	int current = -1;
	bool continuous = true;
	bool looping = false;
	bool autoFirst = false;
	bool playlistVisible = false;
	bool handlingEnd = false;
	double pendingSeek = -1.0;
	double lastGapMs = -1.0;
	int64_t gapStartNs = 0;
	int gapToken = 0;
	int streamToken = 0;
	QString gapFromName;
	bool gapFromEnd = false;
};
