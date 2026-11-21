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

#include "MixishPlaylistManager.hpp"
#include "MixishSwitcher.hpp"

#include <widgets/OBSBasic.hpp>

#include <util/platform.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QUrl>

#include "moc_MixishPlaylistManager.cpp"

namespace {

constexpr int kProbeIntervalMs = 200;
constexpr int kProbeTimeoutMs = 15000;
constexpr int kSaveDelayMs = 500;
constexpr int kHlsSniffBytes = 64 * 1024;

bool PathIsRemote(const QString &path)
{
	return path.contains(QStringLiteral("://"));
}

bool PlaylistSuffix(const QString &path)
{
	const QString suffix = QFileInfo(path).suffix().toLower();
	return suffix == QStringLiteral("m3u") || suffix == QStringLiteral("m3u8");
}

/* A remote m3u/m3u8 URL is a stream endpoint. The query string is ignored
 * when looking at the suffix. */
bool IsRemoteStreamUrl(const QString &path)
{
	return PathIsRemote(path) && PlaylistSuffix(QUrl(path).path());
}

/* A real HLS playlist carries #EXT-X- tags. A Mixish queue file only has
 * #EXTM3U/#EXTINF, so it still expands into its individual entries. */
bool IsLocalHlsPlaylist(const QString &path)
{
	if (PathIsRemote(path) || !PlaylistSuffix(path))
		return false;

	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
		return false;

	const QByteArray head = file.read(kHlsSniffBytes);
	for (const QByteArray &line : head.split('\n')) {
		if (line.trimmed().startsWith("#EXT-X-"))
			return true;
	}

	return false;
}

QString UrlStreamName(const QString &url)
{
	const QUrl parsed(url);
	const QString fileName = QFileInfo(parsed.path()).fileName();
	if (!fileName.isEmpty())
		return fileName;
	return parsed.host().isEmpty() ? url : parsed.host();
}

struct SceneSourceSearch {
	obs_source_t *target = nullptr;
	bool found = false;
};

bool FindSceneSource(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	UNUSED_PARAMETER(scene);

	auto *search = static_cast<SceneSourceSearch *>(param);
	if (!search || !item || search->found)
		return false;

	obs_source_t *source = obs_sceneitem_get_source(item);
	if (!source)
		return true;

	if (source == search->target) {
		search->found = true;
		return false;
	}

	if (obs_scene_t *nested = obs_scene_from_source(source)) {
		obs_scene_enum_items(nested, FindSceneSource, search);
	}

	return !search->found;
}

bool SourceNameExists(const QString &name)
{
	OBSSourceAutoRelease source = obs_get_source_by_name(name.toUtf8().constData());
	return source != nullptr;
}

QString ResolveQueuePath(const QDir &baseDir, const QString &path)
{
	if (PathIsRemote(path) || QDir::isAbsolutePath(path))
		return QDir::cleanPath(path);
	return QDir::cleanPath(baseDir.filePath(path));
}

} // namespace

MixishPlaylistManager::MixishPlaylistManager(MixishSwitcher *switcher_, OBSBasic *main_)
	: QObject(switcher_),
	  switcher(switcher_),
	  main(main_)
{
	saveTimer = new QTimer(this);
	saveTimer->setSingleShot(true);
	saveTimer->setInterval(kSaveDelayMs);
	connect(saveTimer, &QTimer::timeout, this, [this]() {
		if (shuttingDown || collectionChanging)
			return;

		SaveState();
		if (main)
			main->SaveProject();
	});

	probeTimer = new QTimer(this);
	probeTimer->setInterval(kProbeIntervalMs);
	connect(probeTimer, &QTimer::timeout, this, &MixishPlaylistManager::ProbeTick);
}

MixishPlaylistManager::~MixishPlaylistManager()
{
	PrepareForShutdown();
}

MixishPlaylistController *MixishPlaylistManager::CreatePlaylist(const QStringList &files, int number)
{
	if (shuttingDown || collectionChanging || !main || !switcher)
		return nullptr;

	QVector<MixishPlaylistItem> items = BuildItems(files);

	int first = -1;
	for (int i = 0; i < items.size(); ++i) {
		if (items[i].enabled && ItemAvailable(items[i])) {
			first = i;
			break;
		}
	}
	QString base;
	for (int suffix = 1;; ++suffix) {
		const QString candidate =
			suffix == 1 ? tr("Playlist") : tr("Playlist %1").arg(suffix);
		if (!SourceNameExists(candidate) && !SourceNameExists(candidate + tr(" Media"))) {
			base = candidate;
			break;
		}
	}

	const QString mediaName = base + tr(" Media");
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_bool(settings, "looping", false);
	obs_data_set_bool(settings, "clear_on_media_end", false);
	obs_data_set_bool(settings, "restart_on_activate", true);
	obs_data_set_bool(settings, "close_when_inactive", false);

	/* New Playlist creates an empty input; the operator fills the queue
	 * from the panel. */
	if (first >= 0) {
		if (PathIsRemote(items[first].path)) {
			obs_data_set_bool(settings, "is_local_file", false);
			obs_data_set_string(settings, "input", items[first].path.toUtf8().constData());
		} else {
			obs_data_set_bool(settings, "is_local_file", true);
			obs_data_set_string(settings, "local_file",
					    items[first].path.toUtf8().constData());
		}
	}

	OBSSourceAutoRelease source = obs_source_create("ffmpeg_source", mediaName.toUtf8().constData(),
							settings, nullptr);
	if (!source) {
		blog(LOG_WARNING, "Mixish playlist: unable to create the media source");
		return nullptr;
	}

	OBSScene scene = switcher->CreateInputScene(OBSSource(source.Get()), number, base);
	if (!scene)
		return nullptr;

	obs_source_t *sceneSource = obs_scene_get_source(scene);
	if (!sceneSource)
		return nullptr;

	const QString sceneUuid = QString::fromUtf8(obs_source_get_uuid(sceneSource));
	const QString mediaUuid = QString::fromUtf8(obs_source_get_uuid(source.Get()));

	auto *controller =
		new MixishPlaylistController(this, sceneUuid, mediaUuid, items);
	controller->SetScene(scene, source.Get());
	controller->RestoreCurrent(false);
	controller->StopIfHidden();
	playlists.push_back(controller);

	SaveState();
	ScheduleSave();
	ScheduleProbesForAll();
	emit PlaylistsChanged();

	blog(LOG_INFO, "Mixish playlist: created '%s' with %d items",
	     sceneUuid.toUtf8().constData(), int(items.size()));

	return controller;
}

void MixishPlaylistManager::DeletePlaylist(MixishPlaylistController *controller)
{
	if (!controller || shuttingDown || collectionChanging)
		return;

	OBSScene scene = controller->Scene();

	/* Take the playlist off the queue first so nothing looks the controller
	 * up while the scene is being removed. */
	controller->SetPlaylistVisible(false);
	controller->Stop();
	controller->DisconnectMedia();

	playlists.removeOne(controller);
	delete controller;

	/* DeleteScene confirms before removing the input and its sources. */
	if (scene && switcher)
		switcher->DeleteScene(scene);

	if (shuttingDown)
		return;

	SaveState();
	ScheduleSave();
	emit PlaylistsChanged();
}

MixishPlaylistController *MixishPlaylistManager::FindByScene(obs_source_t *sceneSource) const
{
	if (!sceneSource)
		return nullptr;

	const char *uuid = obs_source_get_uuid(sceneSource);
	if (!uuid || !*uuid)
		return nullptr;

	for (MixishPlaylistController *controller : playlists) {
		if (controller && controller->SceneUuid() == QString::fromUtf8(uuid))
			return controller;
	}

	return nullptr;
}

bool MixishPlaylistManager::IsPlaylistSource(obs_source_t *source) const
{
	if (!source)
		return false;

	for (MixishPlaylistController *controller : playlists) {
		if (controller && controller->MediaSource() == source)
			return true;
	}

	return false;
}

void MixishPlaylistManager::OnSceneCollectionLoaded()
{
	if (shuttingDown)
		return;

	collectionChanging = false;
	LoadState();
}

void MixishPlaylistManager::PrepareForSceneCollectionChange()
{
	if (collectionChanging || shuttingDown)
		return;

	collectionChanging = true;

	if (saveTimer)
		saveTimer->stop();

	probeQueue.clear();
	probeQueued.clear();
	probeTried.clear();
	verifiedPaths.clear();
	probeBusy = false;
	StopProbe();

	for (MixishPlaylistController *controller : playlists) {
		if (controller)
			controller->DisconnectMedia();
	}
	qDeleteAll(playlists);
	playlists.clear();
}

void MixishPlaylistManager::PrepareForShutdown()
{
	if (shuttingDown)
		return;

	shuttingDown = true;
	collectionChanging = true;

	if (saveTimer)
		saveTimer->stop();
	if (probeTimer)
		probeTimer->stop();

	probeQueue.clear();
	probeQueued.clear();
	probeTried.clear();
	verifiedPaths.clear();
	probeBusy = false;
	StopProbe();

	for (MixishPlaylistController *controller : playlists) {
		if (controller)
			controller->DisconnectMedia();
	}
	qDeleteAll(playlists);
	playlists.clear();
}

void MixishPlaylistManager::LoadState()
{
	if (shuttingDown || collectionChanging)
		return;

	loading = true;

	for (MixishPlaylistController *controller : playlists) {
		if (controller)
			controller->DisconnectMedia();
	}
	qDeleteAll(playlists);
	playlists.clear();

	probeQueue.clear();
	probeQueued.clear();
	probeTried.clear();
	verifiedPaths.clear();
	probeBusy = false;
	StopProbe();

	if (main && main->collectionModuleData) {
		OBSDataAutoRelease root = obs_data_get_obj(main->collectionModuleData, "mixish_playlist");
		if (root) {
			OBSDataArrayAutoRelease records = obs_data_get_array(root, "playlists");
			if (records) {
				const size_t count = obs_data_array_count(records);

				for (size_t n = 0; n < count; ++n) {
					OBSDataAutoRelease record = obs_data_array_item(records, n);

					const char *sceneUuidText =
						obs_data_get_string(record, "scene_uuid");
					const char *mediaUuidText =
						obs_data_get_string(record, "media_uuid");
					if (!sceneUuidText || !*sceneUuidText || !mediaUuidText ||
					    !*mediaUuidText)
						continue;

					const QString sceneUuid = QString::fromUtf8(sceneUuidText);
					const QString mediaUuid = QString::fromUtf8(mediaUuidText);

					OBSSourceAutoRelease sceneSource =
						obs_get_source_by_uuid(sceneUuid.toUtf8().constData());
					if (!sceneSource)
						continue;

					OBSScene scene = obs_scene_from_source(sceneSource.Get());
					if (!scene)
						continue;

					OBSSourceAutoRelease media =
						obs_get_source_by_uuid(mediaUuid.toUtf8().constData());
					if (!media)
						continue;

					SceneSourceSearch search;
					search.target = media.Get();
					obs_scene_enum_items(scene, FindSceneSource, &search);
					if (!search.found) {
						blog(LOG_WARNING,
						     "Mixish playlist: media source is not in its scene, skipping");
						continue;
					}

					QVector<MixishPlaylistItem> items;
					OBSDataArrayAutoRelease itemArray =
						obs_data_get_array(record, "items");
					if (itemArray) {
						const size_t itemCount =
							obs_data_array_count(itemArray);
						for (size_t i = 0; i < itemCount; ++i) {
							OBSDataAutoRelease itemData =
								obs_data_array_item(itemArray, i);

							MixishPlaylistItem item;
							const char *pathText = obs_data_get_string(
								itemData, "path");
							if (!pathText || !*pathText)
								continue;
							item.path = QString::fromUtf8(pathText);
							const char *nameText = obs_data_get_string(
								itemData, "name");
							if (nameText)
								item.name = QString::fromUtf8(nameText);
							item.enabled = obs_data_has_user_value(
										       itemData, "enabled")
										       ? obs_data_get_bool(itemData,
											   "enabled")
										       : true;
							if (obs_data_has_user_value(itemData, "duration"))
								item.duration = obs_data_get_double(
									itemData, "duration");
							/* Missing in 0.0.2 collections, and
							 * obs_data_get_bool() defaults to false. */
							item.stream =
								obs_data_get_bool(itemData, "stream");
							item.missing = !ItemAvailable(item);

							items.push_back(item);
						}
					}

					if (items.isEmpty()) {
						blog(LOG_WARNING,
						     "Mixish playlist: empty queue record, skipping");
						continue;
					}

					auto *controller = new MixishPlaylistController(
						this, sceneUuid, mediaUuid, items);
					controller->SetScene(scene, media.Get());
					controller->Load(record);
					playlists.push_back(controller);
				}
			}
		}
	}

	loading = false;

	for (MixishPlaylistController *controller : playlists) {
		if (controller) {
			controller->RestoreCurrent(false);
			controller->StopIfHidden();
		}
	}

	emit PlaylistsChanged();
	ScheduleProbesForAll();
}

void MixishPlaylistManager::SaveState()
{
	if (!main || loading || shuttingDown || collectionChanging)
		return;

	obs_data_t *modules = main->collectionModuleData;
	if (!modules) {
		modules = obs_data_create();
		main->collectionModuleData = modules;
	}

	OBSDataAutoRelease root = obs_data_create();
	obs_data_set_int(root, "version", 1);

	OBSDataArrayAutoRelease records = obs_data_array_create();
	for (MixishPlaylistController *controller : playlists) {
		if (!controller || !controller->MediaSource())
			continue;

		OBSDataAutoRelease record = obs_data_create();
		controller->Save(record);
		obs_data_array_push_back(records, record);
	}

	obs_data_set_array(root, "playlists", records);
	obs_data_set_obj(modules, "mixish_playlist", root);
}

void MixishPlaylistManager::ScheduleSave()
{
	if (shuttingDown || collectionChanging || loading || !saveTimer)
		return;

	saveTimer->start();
}

void MixishPlaylistManager::ControllerChanged()
{
	if (shuttingDown || collectionChanging || loading)
		return;

	ScheduleSave();
	emit PlaylistsChanged();
}

QVector<MixishPlaylistItem> MixishPlaylistManager::ExpandFiles(const QStringList &files) const
{
	return BuildItems(files);
}

void MixishPlaylistManager::QueueEdited(MixishPlaylistController *controller)
{
	if (!controller || shuttingDown || collectionChanging)
		return;

	ScheduleProbesForAll();
	ControllerChanged();
}

void MixishPlaylistManager::VerifyDurations(MixishPlaylistController *controller)
{
	if (!controller || shuttingDown || collectionChanging)
		return;

	const QVector<MixishPlaylistItem> &items = controller->Items();
	for (const MixishPlaylistItem &item : items) {
		if (item.path.isEmpty() || item.stream || PathIsRemote(item.path) ||
		    !QFileInfo::exists(item.path))
			continue;
		if (verifiedPaths.contains(item.path))
			continue;

		verifiedPaths.insert(item.path);
		if (item.path == activeProbePath)
			continue;

		ScheduleProbe(item.path, item.duration, true);
	}

	StartNextProbe();
}

void MixishPlaylistManager::RecheckItems(const QVector<MixishPlaylistItem> &items)
{
	if (shuttingDown || collectionChanging)
		return;

	for (const MixishPlaylistItem &item : items) {
		if (item.path.isEmpty() || item.stream)
			continue;
		verifiedPaths.remove(item.path);
		probeTried.remove(item.path);
	}

	StartNextProbe();
}

void MixishPlaylistManager::UpdateVisibility(obs_source_t *previewScene, obs_source_t *programScene)
{
	if (shuttingDown || collectionChanging)
		return;

	const char *previewUuid = previewScene ? obs_source_get_uuid(previewScene) : nullptr;
	const char *programUuid = programScene ? obs_source_get_uuid(programScene) : nullptr;

	for (MixishPlaylistController *controller : playlists) {
		if (!controller)
			continue;

		const QString &sceneUuid = controller->SceneUuid();
		const bool preview = previewUuid && sceneUuid == QString::fromUtf8(previewUuid);
		const bool program = programUuid && sceneUuid == QString::fromUtf8(programUuid);
		controller->SetPlaylistVisible(preview || program);
	}
}

void MixishPlaylistManager::NoteItemDuration(const QString &path, double seconds)
{
	if (path.isEmpty() || seconds <= 0.0)
		return;

	bool changed = false;
	for (MixishPlaylistController *controller : playlists) {
		if (!controller)
			continue;

		const QVector<MixishPlaylistItem> &items = controller->Items();
		for (int i = 0; i < items.size(); ++i) {
			if (items[i].path == path && items[i].duration != seconds) {
				controller->SetItemDuration(i, seconds);
				changed = true;
			}
		}
	}

	if (changed) {
		ScheduleSave();
		emit PlaylistsChanged();
	}
}

void MixishPlaylistManager::ScheduleProbesForAll()
{
	for (MixishPlaylistController *controller : playlists) {
		if (!controller)
			continue;

		const QVector<MixishPlaylistItem> &items = controller->Items();
		for (const MixishPlaylistItem &item : items) {
			if (item.enabled && !item.stream && item.duration < 0.0)
				ScheduleProbe(item.path, item.duration);
		}
	}

	StartNextProbe();
}

void MixishPlaylistManager::ScheduleProbe(const QString &path, double duration, bool force)
{
	if (path.isEmpty() || (!force && duration >= 0.0) || PathIsRemote(path) ||
	    !QFileInfo::exists(path))
		return;
	if ((!force && probeTried.contains(path)) || probeQueued.contains(path))
		return;

	if (force)
		probeTried.remove(path);

	probeQueue.push_back(path);
	probeQueued.insert(path);
}

bool MixishPlaylistManager::EnsureProbeSource(const QString &initialPath)
{
	if (probeSource)
		return true;

	if (shuttingDown || collectionChanging)
		return false;

	QString name = QStringLiteral("Mixish Duration Probe");
	for (int suffix = 2; SourceNameExists(name); ++suffix)
		name = QStringLiteral("Mixish Duration Probe %1").arg(suffix);

	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_bool(settings, "is_local_file", true);
	obs_data_set_string(settings, "local_file", initialPath.toUtf8().constData());
	obs_data_set_bool(settings, "looping", false);
	obs_data_set_bool(settings, "clear_on_media_end", false);
	obs_data_set_bool(settings, "restart_on_activate", false);
	obs_data_set_bool(settings, "close_when_inactive", false);

	/* Private: the probe is a Mixish implementation detail. A public source
	 * would be saved into the scene collection and would remain registered
	 * at shutdown, where OBS reports it as an uncleared source. */
	obs_source_t *probe = obs_source_create_private("ffmpeg_source", name.toUtf8().constData(), settings);
	if (!probe) {
		blog(LOG_WARNING, "Mixish playlist: unable to create the duration probe source");
		return false;
	}

	/* OBSSource adds its own reference; release the creation reference so
	 * the probe is actually destroyed when probeSource is cleared. */
	probeSource = probe;
	obs_source_release(probe);

	obs_source_set_muted(probeSource.Get(), true);
	obs_source_inc_showing(probeSource.Get());
	return true;
}

void MixishPlaylistManager::StartNextProbe()
{
	if (probeBusy || shuttingDown || collectionChanging || !probeTimer)
		return;

	QString path;
	while (!probeQueue.isEmpty()) {
		path = probeQueue.takeFirst();
		probeQueued.remove(path);
		if (QFileInfo::exists(path))
			break;
		probeTried.insert(path);
		path.clear();
	}

	if (path.isEmpty())
		return;

	if (!EnsureProbeSource(path))
		return;

	probeBusy = true;
	activeProbePath = path;
	probeStartedNs = os_gettime_ns();

	OBSDataAutoRelease settings = obs_source_get_settings(probeSource.Get());
	obs_data_set_bool(settings, "is_local_file", true);
	obs_data_set_string(settings, "local_file", path.toUtf8().constData());
	obs_data_set_bool(settings, "looping", false);
	obs_data_set_bool(settings, "clear_on_media_end", false);
	obs_data_set_bool(settings, "restart_on_activate", false);
	obs_source_update(probeSource.Get(), settings);

	probeTimer->start();
}

void MixishPlaylistManager::ProbeTick()
{
	if (!probeBusy || !probeSource || !probeTimer) {
		if (probeTimer)
			probeTimer->stop();
		return;
	}

	const int64_t duration = obs_source_media_get_duration(probeSource.Get());
	if (duration > 0) {
		FinishProbe(true, double(duration) / 1000.0);
		return;
	}

	const int64_t elapsedNs = os_gettime_ns() - probeStartedNs;
	if (elapsedNs >= int64_t(kProbeTimeoutMs) * 1000000LL) {
		FinishProbe(false, -1.0);
	}
}

void MixishPlaylistManager::FinishProbe(bool success, double seconds)
{
	if (probeTimer)
		probeTimer->stop();

	const QString path = activeProbePath;
	probeBusy = false;
	activeProbePath.clear();

	if (probeSource)
		obs_source_media_stop(probeSource.Get());

	if (!path.isEmpty()) {
		probeTried.insert(path);
		if (success && seconds > 0.0) {
			blog(LOG_INFO, "Mixish playlist: probed duration %.3f s for '%s'", seconds,
			     path.toUtf8().constData());
			NoteItemDuration(path, seconds);
		} else {
			blog(LOG_WARNING, "Mixish playlist: duration probe failed for '%s'",
			     path.toUtf8().constData());
		}
	}

	QTimer::singleShot(0, this, [this]() { StartNextProbe(); });
}

void MixishPlaylistManager::StopProbe()
{
	if (probeTimer)
		probeTimer->stop();

	if (probeSource) {
		obs_source_media_stop(probeSource.Get());
		obs_source_dec_showing(probeSource.Get());
		probeSource = nullptr;
	}

	probeBusy = false;
	activeProbePath.clear();
}

QVector<MixishPlaylistItem> MixishPlaylistManager::BuildItems(const QStringList &files)
{
	QVector<MixishPlaylistItem> items;

	for (const QString &file : files) {
		const QFileInfo info(file);
		const QString suffix = info.suffix().toLower();

		if (IsRemoteStreamUrl(file)) {
			MixishPlaylistItem item;
			item.path = file;
			item.name = UrlStreamName(file);
			item.duration = -1.0;
			item.missing = false;
			item.stream = true;
			items.push_back(item);
			continue;
		}

		if (!PathIsRemote(file) &&
		    (suffix == QStringLiteral("m3u") || suffix == QStringLiteral("m3u8"))) {
			if (IsLocalHlsPlaylist(file)) {
				MixishPlaylistItem item;
				item.path = info.absoluteFilePath();
				item.name = info.fileName();
				item.duration = -1.0;
				item.missing = !QFileInfo::exists(item.path);
				item.stream = true;
				items.push_back(item);
				continue;
			}

			const QVector<MixishPlaylistItem> parsed = ParseQueueFile(file);
			if (!parsed.isEmpty()) {
				items += parsed;
				continue;
			}
		}

		MixishPlaylistItem item;
		item.path = file;
		item.name = info.fileName();
		item.missing = !ItemAvailable(item);
		items.push_back(item);
	}

	return items;
}

QVector<MixishPlaylistItem> MixishPlaylistManager::ParseQueueFile(const QString &path)
{
	QVector<MixishPlaylistItem> items;

	QFile file(path);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return items;

	const QDir baseDir = QFileInfo(path).absoluteDir();
	double pendingDuration = -1.0;
	QString pendingTitle;

	while (!file.atEnd()) {
		const QString line = QString::fromUtf8(file.readLine()).trimmed();
		if (line.isEmpty())
			continue;

		if (line.startsWith(QStringLiteral("#EXTINF:"))) {
			const QString payload = line.mid(8);
			const int comma = payload.indexOf(',');
			const QString durationText = (comma >= 0 ? payload.left(comma) : payload).trimmed();
			pendingDuration = durationText.toDouble();
			if (pendingDuration <= 0.0)
				pendingDuration = -1.0;
			pendingTitle = comma >= 0 ? payload.mid(comma + 1).trimmed() : QString();
			continue;
		}

		if (line.startsWith('#'))
			continue;

		MixishPlaylistItem item;
		item.path = ResolveQueuePath(baseDir, line);
		item.name = pendingTitle.isEmpty() ? QFileInfo(item.path).fileName()
						   : pendingTitle;
		item.duration = pendingDuration;
		item.missing = !ItemAvailable(item);
		items.push_back(item);

		pendingDuration = -1.0;
		pendingTitle.clear();
	}

	return items;
}

bool MixishPlaylistManager::ItemAvailable(const MixishPlaylistItem &item)
{
	if (item.path.isEmpty())
		return false;
	if (PathIsRemote(item.path))
		return true;
	return QFileInfo::exists(item.path);
}
