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

#include "MixishPlaylistController.hpp"
#include "MixishPlaylistManager.hpp"

#include <util/platform.h>

#include <QFileInfo>
#include <QMetaObject>
#include <QTimer>

#include <algorithm>
#include <functional>

#include "moc_MixishPlaylistController.cpp"

namespace {

bool PathIsRemote(const QString &path)
{
	return path.contains(QStringLiteral("://"));
}

bool PathAvailable(const MixishPlaylistItem &item)
{
	if (item.path.isEmpty())
		return false;
	if (PathIsRemote(item.path))
		return true;
	return QFileInfo::exists(item.path);
}

QString ItemLabel(const MixishPlaylistItem &item)
{
	if (!item.name.isEmpty())
		return item.name;

	const QString fileName = QFileInfo(item.path).fileName();
	return fileName.isEmpty() ? item.path : fileName;
}

} // namespace

MixishPlaylistController::MixishPlaylistController(MixishPlaylistManager *manager_,
						   const QString &sceneUuid_,
						   const QString &mediaUuid_,
						   const QVector<MixishPlaylistItem> &items_)
	: QObject(manager_),
	  manager(manager_),
	  sceneUuid(sceneUuid_),
	  mediaUuid(mediaUuid_),
	  items(items_)
{
}

MixishPlaylistController::~MixishPlaylistController()
{
	DisconnectMedia();
}

QString MixishPlaylistController::MediaSourceName() const
{
	return media ? QString::fromUtf8(obs_source_get_name(media.Get())) : QString();
}

OBSScene MixishPlaylistController::Scene() const
{
	if (!sceneSource)
		return OBSScene();

	obs_scene_t *scene = obs_scene_from_source(sceneSource.Get());
	return scene ? OBSScene(scene) : OBSScene();
}

QString MixishPlaylistController::SceneName() const
{
	return sceneSource ? QString::fromUtf8(obs_source_get_name(sceneSource.Get())) : QString();
}

QString MixishPlaylistController::NextUpName() const
{
	int next = NextValidIndex(current);
	if (next < 0 && looping)
		next = FirstValidIndex();
	if (next < 0)
		return QString();
	return ItemLabel(items[next]);
}

void MixishPlaylistController::SetScene(OBSScene scene, obs_source_t *mediaSource)
{
	DisconnectMedia();

	sceneSource = scene ? OBSSource(obs_scene_get_source(scene)) : OBSSource();
	media = OBSSource(mediaSource);

	ConnectMedia();
	emit Changed();
}

void MixishPlaylistController::ConnectMedia()
{
	if (!media || mediaSignals)
		return;

	mediaSignals = obs_source_get_signal_handler(media.Get());
	if (!mediaSignals)
		return;

	signal_handler_connect(mediaSignals, "media_ended", OnMediaEnded, this);
	signal_handler_connect(mediaSignals, "media_started", OnMediaStarted, this);
}

void MixishPlaylistController::DisconnectMedia()
{
	if (playlistVisible && media) {
		obs_source_dec_showing(media.Get());
		playlistVisible = false;
	}

	if (!mediaSignals)
		return;

	signal_handler_disconnect(mediaSignals, "media_ended", OnMediaEnded, this);
	signal_handler_disconnect(mediaSignals, "media_started", OnMediaStarted, this);
	mediaSignals = nullptr;
}

void MixishPlaylistController::SetPlaylistVisible(bool visible)
{
	if (playlistVisible == visible)
		return;

	playlistVisible = visible;
	if (!media)
		return;

	blog(LOG_INFO, "Mixish playlist: '%s' %s", obs_source_get_name(media.Get()),
	     visible ? "on air" : "off air");

	if (visible) {
		/* Preview does not add a show reference for its scene, so the
		 * native media source would not restart there. Holding our own
		 * reference makes Preview and Program behave the same, and
		 * starting explicitly avoids depending on OBS's activate
		 * callback, which is not emitted for AUX_VIEW sources. */
		obs_source_inc_showing(media.Get());

		if (autoFirst) {
			const int first = FirstValidIndex();
			if (first >= 0 && SwitchTo(first, true))
				return;
		}

		obs_source_media_restart(media.Get());
	} else {
		obs_source_dec_showing(media.Get());
		obs_source_media_stop(media.Get());
	}
}

void MixishPlaylistController::ApplyPlaybackSettings()
{
	if (!media)
		return;

	OBSDataAutoRelease settings = obs_source_get_settings(media.Get());
	bool changed = false;

	/* A playlist only runs while it is on air. When the scene leaves
	 * Preview/Program the media source stops, and it restarts the current
	 * item when the scene is shown again. */
	if (!obs_data_get_bool(settings, "restart_on_activate")) {
		obs_data_set_bool(settings, "restart_on_activate", true);
		changed = true;
	}
	if (obs_data_get_bool(settings, "clear_on_media_end")) {
		obs_data_set_bool(settings, "clear_on_media_end", false);
		changed = true;
	}
	if (obs_data_get_bool(settings, "looping")) {
		obs_data_set_bool(settings, "looping", false);
		changed = true;
	}

	if (changed)
		obs_source_update(media.Get(), settings);
}

void MixishPlaylistController::StopIfHidden()
{
	if (!media)
		return;

	if (!obs_source_showing(media.Get()))
		obs_source_media_stop(media.Get());
}

void MixishPlaylistController::OnMediaEnded(void *data, calldata_t *calldata)
{
	UNUSED_PARAMETER(calldata);

	auto *self = static_cast<MixishPlaylistController *>(data);
	if (!self)
		return;

	/* libobs emits this from the media thread. All source updates have to
	 * happen on the UI thread, so hand the work back to the event loop. */
	QMetaObject::invokeMethod(
		self, [self]() { self->HandleEnded(); }, Qt::QueuedConnection);
}

void MixishPlaylistController::OnMediaStarted(void *data, calldata_t *calldata)
{
	UNUSED_PARAMETER(calldata);

	auto *self = static_cast<MixishPlaylistController *>(data);
	if (!self)
		return;

	QMetaObject::invokeMethod(
		self, [self]() { self->HandleStarted(); }, Qt::QueuedConnection);
}

void MixishPlaylistController::HandleEnded()
{
	if (!media || handlingEnd)
		return;

	if (!continuous) {
		emit Changed();
		SavePosition();
		return;
	}

	gapStartNs = os_gettime_ns();
	gapFromEnd = true;
	gapFromName = (current >= 0 && current < items.size()) ? ItemLabel(items[current])
							       : QString();
	++gapToken;

	emit Changed();

	handlingEnd = true;

	/* Let the media thread finish its end-of-file cleanup before the next
	 * obs_source_update. The frame that ended stays on screen because the
	 * media source runs with clear_on_media_end=false. */
	QTimer::singleShot(0, this, [this]() { AdvanceAfterEnd(); });
}

void MixishPlaylistController::HandleStarted()
{
	if (!media)
		return;

	/* Do not cache the source duration here. media_started is emitted
	 * before the media thread necessarily reports the new file's duration,
	 * so a fast file switch can briefly return the previous file's value.
	 * Durations come from #EXTINF or the background probe. */

	if (gapStartNs > 0) {
		const double milliseconds = double(os_gettime_ns() - gapStartNs) / 1000000.0;
		gapStartNs = 0;
		gapToken++;
		lastGapMs = milliseconds;

		if (current >= 0 && current < items.size()) {
			blog(LOG_INFO, "Mixish playlist: %s %.1f ms (%s -> %s)",
			     gapFromEnd ? "auto-advance gap" : "switch start gap", milliseconds,
			     gapFromName.toUtf8().constData(), ItemLabel(items[current]).toUtf8().constData());
		} else {
			blog(LOG_INFO, "Mixish playlist: %s %.1f ms",
			     gapFromEnd ? "auto-advance gap" : "switch start gap", milliseconds);
		}

		gapFromName.clear();
		gapFromEnd = false;

		emit GapMeasured(milliseconds);
		if (manager)
			manager->ControllerChanged();
	}

	emit Changed();
}

void MixishPlaylistController::AdvanceAfterEnd()
{
	handlingEnd = false;

	int next = current;
	while ((next = NextValidIndex(next)) >= 0) {
		if (SwitchTo(next, true))
			return;
	}

	if (looping) {
		const int first = FirstValidIndex();
		if (first >= 0 && SwitchTo(first, true))
			return;
	}

	blog(LOG_INFO, "Mixish playlist: end of queue");
	gapStartNs = 0;
	gapFromEnd = false;
	gapFromName.clear();
	++gapToken;
	SavePosition();
	emit Changed();
}

int MixishPlaylistController::FirstValidIndex() const
{
	for (int i = 0; i < items.size(); ++i) {
		if (items[i].enabled && !items[i].missing && PathAvailable(items[i]))
			return i;
	}
	return -1;
}

int MixishPlaylistController::NextValidIndex(int from) const
{
	for (int i = from + 1; i < items.size(); ++i) {
		if (items[i].enabled && !items[i].missing && PathAvailable(items[i]))
			return i;
	}
	return -1;
}

int MixishPlaylistController::PreviousValidIndex(int from) const
{
	for (int i = from - 1; i >= 0; --i) {
		if (items[i].enabled && !items[i].missing && PathAvailable(items[i]))
			return i;
	}
	return -1;
}

void MixishPlaylistController::PlayIndex(int index, bool autoPlay)
{
	if (index < 0 || index >= items.size())
		return;

	if (index == current && autoPlay) {
		Restart();
		return;
	}

	SwitchTo(index, autoPlay);
}

void MixishPlaylistController::PlayNext()
{
	int next = current;
	if (current < 0) {
		next = FirstValidIndex();
		if (next >= 0)
			SwitchTo(next, true);
		return;
	}

	while ((next = NextValidIndex(next)) >= 0) {
		if (SwitchTo(next, true))
			return;
	}

	if (looping) {
		const int first = FirstValidIndex();
		if (first >= 0)
			SwitchTo(first, true);
	}
}

void MixishPlaylistController::PlayPrevious()
{
	if (current < 0) {
		const int first = FirstValidIndex();
		if (first >= 0)
			SwitchTo(first, true);
		return;
	}

	int previous = current;
	while ((previous = PreviousValidIndex(previous)) >= 0) {
		if (SwitchTo(previous, true))
			return;
	}
}

void MixishPlaylistController::PlayFirst()
{
	const int first = FirstValidIndex();
	if (first >= 0)
		SwitchTo(first, true);
}

void MixishPlaylistController::PlayPause()
{
	if (!media)
		return;

	switch (obs_source_media_get_state(media.Get())) {
	case OBS_MEDIA_STATE_PLAYING:
		obs_source_media_play_pause(media.Get(), true);
		break;
	case OBS_MEDIA_STATE_PAUSED:
		obs_source_media_play_pause(media.Get(), false);
		break;
	case OBS_MEDIA_STATE_STOPPED:
	case OBS_MEDIA_STATE_ENDED:
	case OBS_MEDIA_STATE_NONE:
		Restart();
		break;
	}
}

void MixishPlaylistController::Restart()
{
	if (!media)
		return;

	if (current < 0 || current >= items.size()) {
		const int first = FirstValidIndex();
		if (first < 0)
			return;
		current = first;
		emit CurrentChanged(current);
	}

	gapStartNs = os_gettime_ns();
	gapFromEnd = false;
	gapFromName = ItemLabel(items[current]);
	const int token = ++gapToken;
	QTimer::singleShot(30000, this, [this, token]() {
		if (gapToken == token) {
			gapStartNs = 0;
			gapFromEnd = false;
			gapFromName.clear();
		}
	});

	obs_source_media_restart(media.Get());
	emit Changed();
	SavePosition();
}

bool MixishPlaylistController::SwitchTo(int index, bool autoPlay)
{
	if (!media || index < 0 || index >= items.size())
		return false;

	MixishPlaylistItem &item = items[index];
	if (!item.enabled)
		return false;

	if (!PathAvailable(item)) {
		if (!item.missing) {
			item.missing = true;
			blog(LOG_WARNING, "Mixish playlist: missing file '%s'",
			     item.path.toUtf8().constData());
			if (manager)
				manager->ControllerChanged();
		}
		return false;
	}

	if (item.missing)
		item.missing = false;

	if (autoPlay && gapStartNs == 0) {
		gapStartNs = os_gettime_ns();
		gapFromEnd = false;
		if (current >= 0 && current < items.size())
			gapFromName = ItemLabel(items[current]);
		const int token = ++gapToken;
		QTimer::singleShot(30000, this, [this, token]() {
			if (gapToken == token) {
				gapStartNs = 0;
				gapFromEnd = false;
				gapFromName.clear();
			}
		});
	}

	current = index;

	OBSDataAutoRelease settings = obs_source_get_settings(media.Get());
	obs_data_set_bool(settings, "looping", false);
	obs_data_set_bool(settings, "clear_on_media_end", false);
	obs_data_set_bool(settings, "restart_on_activate", true);

	if (PathIsRemote(item.path)) {
		obs_data_set_bool(settings, "is_local_file", false);
		obs_data_set_string(settings, "input", item.path.toUtf8().constData());
		obs_data_set_string(settings, "input_format", "");
	} else {
		obs_data_set_bool(settings, "is_local_file", true);
		obs_data_set_string(settings, "local_file", item.path.toUtf8().constData());
	}

	obs_source_update(media.Get(), settings);

	if (autoPlay) {
		const obs_media_state state = obs_source_media_get_state(media.Get());
		if (state == OBS_MEDIA_STATE_PAUSED)
			obs_source_media_play_pause(media.Get(), false);
		else if (state != OBS_MEDIA_STATE_PLAYING)
			obs_source_media_restart(media.Get());
	}

	emit CurrentChanged(index);
	emit Changed();

	if (manager)
		manager->ControllerChanged();

	return true;
}

void MixishPlaylistController::SetContinuous(bool value)
{
	if (continuous == value)
		return;

	continuous = value;
	emit Changed();
	SavePosition();
}

void MixishPlaylistController::SetLooping(bool value)
{
	if (looping == value)
		return;

	looping = value;
	emit Changed();
	SavePosition();
}

void MixishPlaylistController::SetAutoFirst(bool value)
{
	if (autoFirst == value)
		return;

	autoFirst = value;
	emit Changed();
	SavePosition();
}

void MixishPlaylistController::AddItems(const QVector<MixishPlaylistItem> &newItems)
{
	if (newItems.isEmpty())
		return;

	items += newItems;

	if (current < 0) {
		current = FirstValidIndex();
		if (current >= 0)
			RestoreCurrent(playlistVisible);
	}

	emit Changed();
	if (manager)
		manager->QueueEdited(this);
}

void MixishPlaylistController::RemoveItems(const QVector<int> &indexes)
{
	if (!media || indexes.isEmpty())
		return;

	QVector<int> sorted;
	for (int index : indexes) {
		if (index >= 0 && index < items.size() && !sorted.contains(index))
			sorted.push_back(index);
	}
	if (sorted.isEmpty())
		return;

	std::sort(sorted.begin(), sorted.end(), std::greater<int>());

	const int oldCurrent = current;
	int anchor = current;
	bool currentRemoved = false;

	for (int index : sorted) {
		if (index == oldCurrent)
			currentRemoved = true;
		else if (current >= 0 && index < current)
			--anchor;

		items.removeAt(index);
	}

	handlingEnd = false;
	gapStartNs = 0;
	gapFromEnd = false;
	gapFromName.clear();
	++gapToken;

	if (items.isEmpty()) {
		current = -1;
		obs_source_media_stop(media.Get());
	} else {
		int candidate = -1;
		const int start = qBound(0, anchor, items.size() - 1);
		for (int i = start; i < items.size(); ++i) {
			if (items[i].enabled && !items[i].missing && PathAvailable(items[i])) {
				candidate = i;
				break;
			}
		}
		if (candidate < 0) {
			for (int i = start - 1; i >= 0; --i) {
				if (items[i].enabled && !items[i].missing && PathAvailable(items[i])) {
					candidate = i;
					break;
				}
			}
		}

		current = candidate;
		if (currentRemoved)
			obs_source_media_stop(media.Get());
		if (current >= 0)
			RestoreCurrent(currentRemoved && playlistVisible);
	}

	emit Changed();
	if (manager)
		manager->QueueEdited(this);
}

void MixishPlaylistController::ReplaceItem(int index, const MixishPlaylistItem &item)
{
	if (index < 0 || index >= items.size())
		return;

	items[index] = item;

	if (index == current && media) {
		if (obs_source_showing(media.Get()))
			SwitchTo(index, true);
		else
			SwitchTo(index, false);
	}

	emit Changed();
	if (manager)
		manager->QueueEdited(this);
}

void MixishPlaylistController::SetItemDuration(int index, double seconds)
{
	if (index < 0 || index >= items.size())
		return;

	items[index].duration = seconds;
}

void MixishPlaylistController::SetMissing(int index, bool missing)
{
	if (index < 0 || index >= items.size() || items[index].missing == missing)
		return;

	items[index].missing = missing;
	emit Changed();
	if (manager)
		manager->ControllerChanged();
}

void MixishPlaylistController::SavePosition()
{
	if (manager)
		manager->ControllerChanged();
}

void MixishPlaylistController::Save(obs_data_t *data) const
{
	if (!data)
		return;

	obs_data_set_string(data, "scene_uuid", sceneUuid.toUtf8().constData());
	obs_data_set_string(data, "media_uuid", mediaUuid.toUtf8().constData());
	obs_data_set_int(data, "current", current);
	obs_data_set_bool(data, "continuous", continuous);
	obs_data_set_bool(data, "looping", looping);
	obs_data_set_bool(data, "auto_first", autoFirst);
	if (lastGapMs >= 0.0)
		obs_data_set_double(data, "last_gap_ms", lastGapMs);

	OBSDataArrayAutoRelease list = obs_data_array_create();
	for (const MixishPlaylistItem &item : items) {
		OBSDataAutoRelease itemData = obs_data_create();
		obs_data_set_string(itemData, "path", item.path.toUtf8().constData());
		obs_data_set_string(itemData, "name", item.name.toUtf8().constData());
		obs_data_set_bool(itemData, "enabled", item.enabled);
		if (item.duration >= 0.0)
			obs_data_set_double(itemData, "duration", item.duration);
		obs_data_array_push_back(list, itemData);
	}

	obs_data_set_array(data, "items", list);
}

void MixishPlaylistController::Load(obs_data_t *data)
{
	if (!data)
		return;

	current = int(obs_data_get_int(data, "current"));
	if (current < 0 || current >= items.size())
		current = items.isEmpty() ? -1 : 0;

	continuous = obs_data_has_user_value(data, "continuous")
			     ? obs_data_get_bool(data, "continuous")
			     : true;

	looping = obs_data_has_user_value(data, "looping")
			  ? obs_data_get_bool(data, "looping")
			  : false;

	autoFirst = obs_data_has_user_value(data, "auto_first")
			    ? obs_data_get_bool(data, "auto_first")
			    : false;

	lastGapMs = obs_data_has_user_value(data, "last_gap_ms")
			    ? obs_data_get_double(data, "last_gap_ms")
			    : -1.0;
}

void MixishPlaylistController::RestoreCurrent(bool autoPlay)
{
	if (!media || items.isEmpty())
		return;

	ApplyPlaybackSettings();

	if (current < 0 || current >= items.size() || !items[current].enabled ||
	    !PathAvailable(items[current])) {
		current = FirstValidIndex();
		if (current < 0) {
			emit Changed();
			return;
		}
	}

	if (autoPlay) {
		SwitchTo(current, true);
		return;
	}

	/* OBS restores the media source's last local_file with the scene
	 * collection. Only touch the source if the queue points somewhere
	 * else, so opening a collection does not restart playback. */
	bool matches = false;
	if (media) {
		OBSDataAutoRelease settings = obs_source_get_settings(media.Get());
		const char *file = obs_data_get_string(settings, "local_file");
		const bool local = obs_data_get_bool(settings, "is_local_file");
		matches = local && file && items[current].path == QString::fromUtf8(file);
	}

	if (!matches)
		SwitchTo(current, false);

	emit CurrentChanged(current);
	emit Changed();
}

void MixishPlaylistController::Stop()
{
	if (!media)
		return;

	obs_source_media_stop(media.Get());
	SavePosition();
}
