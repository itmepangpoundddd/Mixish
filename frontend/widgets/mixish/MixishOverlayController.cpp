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

#include "MixishOverlayController.hpp"
#include "MixishSwitcher.hpp"

#include <widgets/OBSBasic.hpp>

#include <QApplication>
#include <QTimer>

#include "moc_MixishOverlayController.cpp"

#include <algorithm>

namespace {

const QColor kChannelColors[MixishOverlayController::MaxChannels] = {
	QColor(0x4C, 0x8D, 0xFF),
	QColor(0x39, 0xC7, 0x7A),
	QColor(0xF2, 0xA9, 0x3B),
	QColor(0xA9, 0x7B, 0xFF),
	QColor(0xE5, 0x5C, 0x73),
};

struct ContainsSearch {
	obs_source_t *needle = nullptr;
	int depth = 0;
	bool found = false;
};

bool SourceTreeContains(obs_source_t *haystack, obs_source_t *needle, int depth);

bool ContainsItem(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	UNUSED_PARAMETER(scene);

	auto *search = static_cast<ContainsSearch *>(param);
	obs_source_t *source = item ? obs_sceneitem_get_source(item) : nullptr;

	if (SourceTreeContains(source, search->needle, search->depth + 1)) {
		search->found = true;
		return false;
	}

	return true;
}

/* Walks scene items, groups and nested scenes looking for `needle`. */
bool SourceTreeContains(obs_source_t *haystack, obs_source_t *needle, int depth)
{
	if (!haystack || !needle || depth > 32)
		return false;
	if (haystack == needle)
		return true;

	obs_scene_t *scene = obs_scene_from_source(haystack);
	if (!scene)
		return false;

	ContainsSearch search;
	search.needle = needle;
	search.depth = depth;
	obs_scene_enum_items(scene, ContainsItem, &search);
	return search.found;
}

QRectF CanvasRect()
{
	obs_video_info ovi = {};
	if (!obs_get_video_info(&ovi) || ovi.base_width == 0 || ovi.base_height == 0)
		return QRectF(0.0, 0.0, 1920.0, 1080.0);

	return QRectF(0.0, 0.0, double(ovi.base_width), double(ovi.base_height));
}

} // namespace

MixishOverlayController::MixishOverlayController(MixishSwitcher *switcher_, OBSBasic *main_)
	: QObject(switcher_), switcher(switcher_), main(main_)
{
	setObjectName(QStringLiteral("mixishOverlayController"));

	connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() { Shutdown(); });

	for (int i = 0; i < ActiveChannels; ++i)
		EnsureStage(i);
}

MixishOverlayController::~MixishOverlayController()
{
	Shutdown();
}

QColor MixishOverlayController::ChannelColor(int index)
{
	if (index < 0 || index >= MaxChannels)
		return QColor(0x80, 0x80, 0x80);
	return kChannelColors[index];
}

QString MixishOverlayController::ChannelLabel(int index)
{
	return QStringLiteral("OV%1").arg(index + 1);
}

QString MixishOverlayController::ChannelTitle(int index)
{
	return tr("Overlay %1").arg(index + 1);
}

bool MixishOverlayController::IsValidIndex(int index) const
{
	return index >= 0 && index < ActiveChannels;
}

MixishOverlayController::Channel *MixishOverlayController::Get(int index)
{
	return IsValidIndex(index) ? &channels[index] : nullptr;
}

const MixishOverlayController::Channel *MixishOverlayController::Get(int index) const
{
	return IsValidIndex(index) ? &channels[index] : nullptr;
}

void MixishOverlayController::EnsureStage(int index)
{
	if (shutdown || collectionChanging || !IsValidIndex(index))
		return;

	Channel &channel = channels[index];
	if (channel.stage) {
		/* Stage scenes survive scene-collection changes; put the existing
		 * stage back on its output channel after OBS cleared it. */
		obs_source_t *stageSource = obs_scene_get_source(channel.stage);
		if (stageSource) {
			OBSSourceAutoRelease current = obs_get_output_source(FirstVideoChannel + index);
			if (current.Get() != stageSource)
				obs_set_output_source(FirstVideoChannel + index, stageSource);
		}
		return;
	}

	const QString name = QStringLiteral("Mixish %1").arg(ChannelTitle(index));
	OBSSceneAutoRelease stage = obs_scene_create_private(name.toUtf8().constData());
	if (!stage) {
		blog(LOG_WARNING, "Mixish: failed to create overlay stage scene for %s",
		     name.toUtf8().constData());
		return;
	}

	channel.stage = stage.Get();
	channel.rect = CanvasRect();
	channel.hasGeometry = false;

	/* Keep the (possibly empty) stage on the overlay channel so state and
	 * ordering are stable for the whole session. */
	obs_set_output_source(FirstVideoChannel + index, obs_scene_get_source(channel.stage));

	blog(LOG_INFO, "Mixish: overlay %d stage on video channel %d", index + 1,
	     FirstVideoChannel + index);
}

void MixishOverlayController::Shutdown()
{
	if (shutdown)
		return;
	shutdown = true;

	blog(LOG_INFO, "Mixish: overlay shutdown (libobs initialized: %s)",
	     obs_initialized() ? "yes" : "no");
	for (int i = 0; i < MaxChannels; ++i) {
		Channel &channel = channels[i];

		DisconnectInput(i);

		if (channel.item) {
			DetachTransition(i);
			obs_sceneitem_remove(channel.item.Get());
			channel.item = nullptr;
		}

		if (channel.stage) {
			obs_set_output_source(FirstVideoChannel + i, nullptr);
			channel.stage = nullptr;
		}

		channel.fade = nullptr;
		channel.onAir = false;
		channel.uuid.clear();
		channel.inputName.clear();
	}

	stateLoaded = false;

	/* Scene destruction happens on the OBS task thread. Waiting here makes
	 * sure the stage scenes (and the items referencing their inputs) are
	 * gone before libobs begins tearing down the source graph. */
	if (obs_initialized()) {
		bool pending = obs_wait_for_destroy_queue();
		blog(LOG_INFO, "Mixish: overlay shutdown destroy queue drained (more pending: %s)",
		     pending ? "yes" : "no");
	}
}

bool MixishOverlayController::IsAssigned(int index) const
{
	const Channel *channel = Get(index);
	return channel && channel->item != nullptr;
}

bool MixishOverlayController::IsOnAir(int index) const
{
	const Channel *channel = Get(index);
	return channel && channel->onAir;
}

bool MixishOverlayController::IsAudioEnabled(int index) const
{
	const Channel *channel = Get(index);
	return channel && channel->audio;
}

bool MixishOverlayController::IsFade(int index) const
{
	const Channel *channel = Get(index);
	return channel && channel->fadeMode;
}

int MixishOverlayController::FadeInMs(int index) const
{
	const Channel *channel = Get(index);
	return channel ? channel->fadeInMs : 300;
}

int MixishOverlayController::FadeOutMs(int index) const
{
	const Channel *channel = Get(index);
	return channel ? channel->fadeOutMs : 300;
}

OBSScene MixishOverlayController::StageScene(int index) const
{
	const Channel *channel = Get(index);
	return channel ? channel->stage : nullptr;
}

OBSSource MixishOverlayController::InputSource(int index) const
{
	const Channel *channel = Get(index);
	if (!channel || !channel->item)
		return nullptr;

	return obs_sceneitem_get_source(channel->item.Get());
}

QString MixishOverlayController::InputName(int index) const
{
	OBSSource source = InputSource(index);
	if (!source)
		return QString();
	return QString::fromUtf8(obs_source_get_name(source));
}

QRectF MixishOverlayController::Rect(int index) const
{
	const Channel *channel = Get(index);
	return channel ? channel->rect : QRectF();
}

obs_sceneitem_crop MixishOverlayController::Crop(int index) const
{
	const Channel *channel = Get(index);
	if (!channel)
		return obs_sceneitem_crop{};
	return channel->crop;
}

bool MixishOverlayController::CanAssign(int index, OBSScene input) const
{
	if (shutdown || collectionChanging || !IsValidIndex(index) || !input)
		return false;

	const Channel *channel = Get(index);
	if (!channel || !channel->stage)
		return false;

	obs_source_t *inputSource = obs_scene_get_source(input);
	obs_source_t *stageSource = obs_scene_get_source(channel->stage);
	if (!inputSource || !stageSource)
		return false;

	if (obs_source_removed(inputSource))
		return false;

	/* Assigning an input that already contains the stage scene (directly or
	 * through groups/layers) would create a source cycle. */
	if (SourceTreeContains(inputSource, stageSource, 0))
		return false;

	return true;
}

bool MixishOverlayController::Assign(int index, OBSScene input, bool turnOn)
{
	if (!CanAssign(index, input)) {
		blog(LOG_WARNING, "Mixish: refusing overlay %d assignment (cycle or invalid input)",
		     index + 1);
		return false;
	}

	Channel &channel = channels[index];

	DisconnectInput(index);

	if (channel.item) {
		DetachTransition(index);
		obs_sceneitem_remove(channel.item.Get());
		channel.item = nullptr;
	}

	channel.onAir = false;

	OBSSceneItem item = obs_scene_add(channel.stage, obs_scene_get_source(input));
	if (!item) {
		blog(LOG_WARNING, "Mixish: failed to add input to overlay %d", index + 1);
		channel.uuid.clear();
		channel.inputName.clear();
		Notify(index);
		return false;
	}

	channel.item = item;
	channel.hasGeometry = false;
	channel.rect = CanvasRect();
	channel.crop = obs_sceneitem_crop{};

	obs_source_t *inputSource = obs_sceneitem_get_source(item.Get());
	channel.uuid = QString::fromUtf8(obs_source_get_uuid(inputSource));
	channel.inputName = QString::fromUtf8(obs_source_get_name(inputSource));

	if (!channel.fade) {
		const QString fadeName = QStringLiteral("Mixish Overlay Fade %1").arg(index + 1);
		obs_source_t *fade =
			obs_source_create_private("fade_transition", fadeName.toUtf8().constData(), nullptr);
		/* OBSSource adds its own reference; release the creation reference
		 * so the fade is destroyed when the channel lets go of it. */
		channel.fade = fade;
		obs_source_release(fade);
	}

	/* The transition is attached only while a fade runs (see SetOnAir):
	 * a transition left attached to a scene item that references a scene
	 * keeps that scene alive past the scene collection teardown. */
	obs_sceneitem_set_transition(item.Get(), true, nullptr);
	obs_sceneitem_set_transition(item.Get(), false, nullptr);

	obs_sceneitem_set_visible(item.Get(), false);
	ApplyGeometry(index);
	ApplyAudio(index);
	ConnectInput(index);

	blog(LOG_INFO, "Mixish: overlay %d assigned to '%s'", index + 1,
	     channel.inputName.toUtf8().constData());

	Notify(index);

	if (turnOn)
		SetOnAir(index, true);

	return true;
}

void MixishOverlayController::Clear(int index)
{
	Channel *channel = Get(index);
	if (shutdown || collectionChanging || !channel)
		return;

	DisconnectInput(index);

	if (channel->item) {
		DetachTransition(index);
		obs_sceneitem_remove(channel->item.Get());
		channel->item = nullptr;
	}

	channel->onAir = false;
	channel->uuid.clear();
	channel->inputName.clear();
	channel->hasGeometry = false;
	channel->rect = CanvasRect();
	channel->crop = obs_sceneitem_crop{};

	blog(LOG_INFO, "Mixish: overlay %d cleared", index + 1);

	Notify(index);
}

void MixishOverlayController::Toggle(int index)
{
	if (!IsAssigned(index))
		return;
	SetOnAir(index, !IsOnAir(index));
}

void MixishOverlayController::SetOnAir(int index, bool onAir)
{
	Channel *channel = Get(index);
	if (!channel || !channel->item || shutdown)
		return;
	if (channel->onAir == onAir)
		return;

	obs_sceneitem_t *item = channel->item.Get();
	const int duration = onAir ? channel->fadeInMs : channel->fadeOutMs;

	if (channel->fadeMode && channel->fade) {
		/* Fade: attach OBS's own scene-item transition just for the
		 * duration of the fade, then detach it again. */
		obs_sceneitem_set_transition(item, onAir, channel->fade.Get());
		obs_sceneitem_set_transition_duration(item, onAir, uint32_t(std::max(0, duration)));
		obs_sceneitem_set_visible(item, onAir);

		QTimer::singleShot(std::max(0, duration) + 120, this, [this, index]() {
			if (!shutdown && !collectionChanging)
				DetachTransition(index);
		});
	} else {
		/* Cut: no transition attached means the visibility change is
		 * applied immediately. */
		obs_sceneitem_set_transition(item, true, nullptr);
		obs_sceneitem_set_transition(item, false, nullptr);
		obs_sceneitem_set_visible(item, onAir);
	}

	channel->onAir = onAir;

	blog(LOG_INFO, "Mixish: overlay %d %s (%s, %d ms)", index + 1, onAir ? "ON" : "OFF",
	     channel->fadeMode ? "fade" : "cut", duration);

	Notify(index);
}

void MixishOverlayController::DetachTransition(int index)
{
	Channel *channel = Get(index);
	if (!channel || !channel->item || !channel->fade)
		return;

	obs_sceneitem_t *item = channel->item.Get();
	if (obs_sceneitem_get_transition(item, true) == channel->fade.Get())
		obs_sceneitem_set_transition(item, true, nullptr);
	if (obs_sceneitem_get_transition(item, false) == channel->fade.Get())
		obs_sceneitem_set_transition(item, false, nullptr);
}

void MixishOverlayController::SetAudioEnabled(int index, bool enabled)
{
	Channel *channel = Get(index);
	if (!channel)
		return;
	if (channel->audio == enabled)
		return;

	channel->audio = enabled;
	ApplyAudio(index);
	Notify(index);
}

void MixishOverlayController::SetFade(int index, bool fade)
{
	Channel *channel = Get(index);
	if (!channel || channel->fadeMode == fade)
		return;

	channel->fadeMode = fade;

	/* Switching to Cut while a fade is running should stop it immediately. */
	if (!fade && channel->item) {
		obs_sceneitem_t *item = channel->item.Get();
		obs_source_t *show = obs_sceneitem_get_transition(item, true);
		obs_source_t *hide = obs_sceneitem_get_transition(item, false);
		if (show) {
			obs_transition_force_stop(show);
			obs_sceneitem_set_transition(item, true, nullptr);
		}
		if (hide) {
			obs_transition_force_stop(hide);
			obs_sceneitem_set_transition(item, false, nullptr);
		}
	}

	Notify(index);
}

void MixishOverlayController::SetFadeTimes(int index, int fadeInMs, int fadeOutMs)
{
	Channel *channel = Get(index);
	if (!channel)
		return;

	fadeInMs = std::clamp(fadeInMs, 0, 10000);
	fadeOutMs = std::clamp(fadeOutMs, 0, 10000);
	if (channel->fadeInMs == fadeInMs && channel->fadeOutMs == fadeOutMs)
		return;

	channel->fadeInMs = fadeInMs;
	channel->fadeOutMs = fadeOutMs;
	Notify(index);
}

void MixishOverlayController::SetRect(int index, const QRectF &rect)
{
	Channel *channel = Get(index);
	if (!channel)
		return;

	channel->rect =
		QRectF(rect.x(), rect.y(), std::max(1.0, rect.width()), std::max(1.0, rect.height()));
	channel->hasGeometry = true;
	ApplyGeometry(index);
	Notify(index);
}

void MixishOverlayController::SetCrop(int index, const obs_sceneitem_crop &crop)
{
	Channel *channel = Get(index);
	if (!channel)
		return;

	channel->crop = crop;
	channel->hasGeometry = true;
	ApplyGeometry(index);
	Notify(index);
}

void MixishOverlayController::ResetRect(int index)
{
	Channel *channel = Get(index);
	if (!channel)
		return;

	channel->rect = CanvasRect();
	channel->crop = obs_sceneitem_crop{};
	channel->hasGeometry = true;
	ApplyGeometry(index);
	Notify(index);
}

void MixishOverlayController::AllOff()
{
	for (int i = 0; i < ActiveChannels; ++i) {
		if (IsAssigned(i) && IsOnAir(i))
			SetOnAir(i, false);
	}
}

void MixishOverlayController::AllOn()
{
	for (int i = 0; i < ActiveChannels; ++i) {
		if (IsAssigned(i) && !IsOnAir(i))
			SetOnAir(i, true);
	}
}

void MixishOverlayController::ApplyGeometry(int index)
{
	Channel *channel = Get(index);
	if (!channel || !channel->item)
		return;

	obs_sceneitem_t *item = channel->item.Get();
	const QRectF rect = channel->rect;

	struct vec2 pos;
	vec2_set(&pos, float(rect.x()), float(rect.y()));
	obs_sceneitem_set_pos(item, &pos);

	struct vec2 bounds;
	vec2_set(&bounds, float(rect.width()), float(rect.height()));
	obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_STRETCH);
	obs_sceneitem_set_bounds_alignment(item, OBS_ALIGN_TOP | OBS_ALIGN_LEFT);
	obs_sceneitem_set_bounds(item, &bounds);

	obs_sceneitem_set_crop(item, &channel->crop);
}

void MixishOverlayController::ApplyAudio(int index)
{
	Channel *channel = Get(index);
	if (!channel || !channel->stage)
		return;

	obs_source_set_muted(obs_scene_get_source(channel->stage), !channel->audio);
}

void MixishOverlayController::ConnectInput(int index)
{
	Channel *channel = Get(index);
	if (!channel || !channel->item || hooks[size_t(index)])
		return;

	obs_source_t *source = obs_sceneitem_get_source(channel->item.Get());
	if (!source)
		return;

	auto *hook = new ChannelHook();
	hook->self = this;
	hook->index = index;

	signal_handler_t *handler = obs_source_get_signal_handler(source);
	if (handler) {
		signal_handler_connect(handler, "remove", &MixishOverlayController::InputRemoved, hook);
		hooks[size_t(index)] = hook;
	} else {
		delete hook;
	}
}

void MixishOverlayController::DisconnectInput(int index)
{
	if (!IsValidIndex(index))
		return;

	Channel &channel = channels[index];
	ChannelHook *hook = hooks[size_t(index)];
	if (!hook)
		return;

	hooks[size_t(index)] = nullptr;

	/* Resolve the source through the scene item instead of the weak
	 * reference: obs_weak_source_get_source() adds a reference, and doing
	 * that while OBS is removing a source resurrects it for a second
	 * destroy. The item is still alive at every call site. */
	obs_source_t *source = channel.item ? obs_sceneitem_get_source(channel.item.Get()) : nullptr;
	if (source) {
		signal_handler_t *handler = obs_source_get_signal_handler(source);
		if (handler)
			signal_handler_disconnect(handler, "remove",
						  &MixishOverlayController::InputRemoved, hook);
	}

	delete hook;
}

void MixishOverlayController::InputRemoved(void *data, calldata_t *params)
{
	UNUSED_PARAMETER(params);

	auto *hook = static_cast<ChannelHook *>(data);
	if (!hook || !hook->self)
		return;

	MixishOverlayController *self = hook->self;
	const int index = hook->index;

	/* The remove signal can arrive off the UI thread and disconnecting the
	 * handler from inside it is unsafe, so clear the channel later. */
	QMetaObject::invokeMethod(self, [self, index]() { self->OnInputRemoved(index); },
				  Qt::QueuedConnection);
}

void MixishOverlayController::OnInputRemoved(int index)
{
	if (shutdown || collectionChanging)
		return;

	Channel *channel = Get(index);
	if (!channel || !channel->item)
		return;

	blog(LOG_WARNING, "Mixish: input '%s' removed while on overlay %d; clearing channel",
	     channel->inputName.toUtf8().constData(), index + 1);

	/* Disconnect while the scene item still holds a reference to the input,
	 * then drop the item so nothing can render a removed source. */
	DisconnectInput(index);
	DetachTransition(index);
	obs_sceneitem_remove(channel->item.Get());
	channel->item = nullptr;

	channel->onAir = false;
	channel->uuid.clear();
	channel->inputName.clear();
	channel->hasGeometry = false;
	channel->rect = CanvasRect();
	channel->crop = obs_sceneitem_crop{};

	Notify(index);
}

void MixishOverlayController::ReloadChannels()
{
	for (int i = 0; i < ActiveChannels; ++i) {
		Channel &channel = channels[i];
		if (!channel.item)
			continue;

		obs_source_t *source = obs_sceneitem_get_source(channel.item.Get());
		if (source && !obs_source_removed(source))
			continue;

		OnInputRemoved(i);
	}
}

void MixishOverlayController::PrepareForSceneCollectionChange()
{
	if (shutdown || collectionChanging)
		return;

	collectionChanging = true;
	stateLoaded = false;

	/* OBS has already called obs_source_remove() on every source when this
	 * runs, but none of them are destroyed until the destroy queue drains.
	 * Detach everything now, without addref'ing anything, so the stage
	 * scenes no longer hold references that would outlive the collection.
	 *
	 * The (private, empty) stage scenes themselves are kept alive: destroying
	 * them here would queue more scene teardown in the middle of OBS's own
	 * destroy-queue drain, which can tear down an input scene before the
	 * stage item that still references it. They are released in Shutdown(),
	 * with the queue drained before libobs shuts down. */
	for (int i = 0; i < MaxChannels; ++i) {
		Channel &channel = channels[i];

		DisconnectInput(i);

		if (channel.item) {
			obs_source_t *assigned = obs_sceneitem_get_source(channel.item.Get());
			blog(LOG_INFO, "Mixish: overlay %d detaching '%s' (removed=%d)", i + 1,
			     assigned ? obs_source_get_name(assigned) : "(null)",
			     assigned ? int(obs_source_removed(assigned)) : -1);

			DetachTransition(i);
			obs_sceneitem_remove(channel.item.Get());
			channel.item = nullptr;
		}

		if (channel.stage) {
			obs_set_output_source(FirstVideoChannel + i, nullptr);
			channel.stage = nullptr;
		}

		channel.fade = nullptr;
		channel.onAir = false;
		channel.uuid.clear();
		channel.inputName.clear();
		channel.hasGeometry = false;
		channel.rect = CanvasRect();
		channel.crop = obs_sceneitem_crop{};
	}

	/* Drain our own scene/transition teardown here, while the collection is
	 * still intact, instead of leaving it pending across OBS's destroy pass. */
	if (obs_initialized())
		obs_wait_for_destroy_queue();

	blog(LOG_INFO, "Mixish: overlay stages released for scene collection change");

	for (int i = 0; i < ActiveChannels; ++i)
		Notify(i);
}

void MixishOverlayController::OnSceneCollectionLoaded()
{
	if (shutdown)
		return;

	collectionChanging = false;

	for (int i = 0; i < ActiveChannels; ++i)
		EnsureStage(i);

	RestoreState();

	for (int i = 0; i < ActiveChannels; ++i)
		Notify(i);

	ReloadChannels();
}

void MixishOverlayController::Sync()
{
	if (shutdown || collectionChanging)
		return;

	for (int i = 0; i < ActiveChannels; ++i) {
		Channel &channel = channels[i];

		if (!channel.stage) {
			EnsureStage(i);
			continue;
		}

		obs_source_t *stageSource = obs_scene_get_source(channel.stage);
		if (!stageSource)
			continue;

		OBSSourceAutoRelease current = obs_get_output_source(FirstVideoChannel + i);
		if (current.Get() != stageSource)
			obs_set_output_source(FirstVideoChannel + i, stageSource);
	}

	ReloadChannels();
}

void MixishOverlayController::SaveState()
{
	if (shutdown || collectionChanging || restoringState || !main)
		return;

	obs_data_t *modules = main->collectionModuleData;
	if (!modules) {
		modules = obs_data_create();
		main->collectionModuleData = modules;
	}

	OBSDataAutoRelease root = obs_data_create();
	obs_data_set_int(root, "version", 1);

	OBSDataArrayAutoRelease channelsData = obs_data_array_create();
	for (int i = 0; i < ActiveChannels; ++i) {
		Channel &channel = channels[i];
		obs_source_t *source =
			channel.item ? obs_sceneitem_get_source(channel.item.Get()) : nullptr;
		if (!source)
			continue;

		OBSDataAutoRelease data = obs_data_create();
		obs_data_set_int(data, "channel", i);
		obs_data_set_string(data, "uuid", obs_source_get_uuid(source));
		obs_data_set_bool(data, "audio", channel.audio);
		obs_data_set_bool(data, "fade", channel.fadeMode);
		obs_data_set_int(data, "fadeIn", channel.fadeInMs);
		obs_data_set_int(data, "fadeOut", channel.fadeOutMs);
		obs_data_set_bool(data, "geometry", channel.hasGeometry);

		if (channel.hasGeometry) {
			obs_data_set_double(data, "x", channel.rect.x());
			obs_data_set_double(data, "y", channel.rect.y());
			obs_data_set_double(data, "w", channel.rect.width());
			obs_data_set_double(data, "h", channel.rect.height());
			obs_data_set_int(data, "cropLeft", channel.crop.left);
			obs_data_set_int(data, "cropTop", channel.crop.top);
			obs_data_set_int(data, "cropRight", channel.crop.right);
			obs_data_set_int(data, "cropBottom", channel.crop.bottom);
		}

		obs_data_array_push_back(channelsData, data);
	}

	obs_data_set_array(root, "channels", channelsData);
	obs_data_set_obj(modules, "mixish_overlay", root);
}

void MixishOverlayController::RestoreState()
{
	if (shutdown || collectionChanging || stateLoaded)
		return;

	stateLoaded = true;

	if (!main || !main->collectionModuleData)
		return;

	OBSDataAutoRelease root = obs_data_get_obj(main->collectionModuleData, "mixish_overlay");
	if (!root)
		return;

	OBSDataArrayAutoRelease list = obs_data_get_array(root, "channels");
	if (!list)
		return;

	restoringState = true;
	const size_t count = obs_data_array_count(list);

	for (size_t n = 0; n < count; ++n) {
		OBSDataAutoRelease data = obs_data_array_item(list, n);
		const int index = int(obs_data_get_int(data, "channel"));
		if (!IsValidIndex(index) || channels[index].item)
			continue;

		const char *uuid = obs_data_get_string(data, "uuid");
		if (!uuid || !*uuid)
			continue;

		OBSSourceAutoRelease source = obs_get_source_by_uuid(uuid);
		if (!source)
			continue;

		OBSScene scene = obs_scene_from_source(source);
		if (!scene || !CanAssign(index, scene))
			continue;

		if (!Assign(index, scene, false))
			continue;

		SetAudioEnabled(index, obs_data_get_bool(data, "audio"));
		SetFade(index, obs_data_get_bool(data, "fade"));
		SetFadeTimes(index, int(obs_data_get_int(data, "fadeIn")),
			     int(obs_data_get_int(data, "fadeOut")));

		if (obs_data_get_bool(data, "geometry")) {
			QRectF rect(obs_data_get_double(data, "x"), obs_data_get_double(data, "y"),
				    obs_data_get_double(data, "w"),
				    obs_data_get_double(data, "h"));
			SetRect(index, rect);

			obs_sceneitem_crop crop = {};
			crop.left = int(obs_data_get_int(data, "cropLeft"));
			crop.top = int(obs_data_get_int(data, "cropTop"));
			crop.right = int(obs_data_get_int(data, "cropRight"));
			crop.bottom = int(obs_data_get_int(data, "cropBottom"));
			SetCrop(index, crop);
		}

		blog(LOG_INFO, "Mixish: overlay %d restored from scene collection", index + 1);
	}

	restoringState = false;
}

void MixishOverlayController::Notify(int index)
{
	SaveState();
	emit ChannelChanged(index);
	emit ChannelsChanged();
}
