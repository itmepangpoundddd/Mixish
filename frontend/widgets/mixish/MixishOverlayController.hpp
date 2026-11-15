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

#include <QColor>
#include <QObject>
#include <QRectF>
#include <QString>

#include <array>

class OBSBasic;
class MixishSwitcher;

/* Overlay channels for the Mixish switcher.
 *
 * Each channel owns a private stage scene that is placed on its own OBS video
 * output channel above channel 0 (Program). OBS renders all output channels in
 * ascending order into the same main texture, so the stage scenes composite on
 * top of Program in the Program monitor, the stream and the recording without
 * touching the transition system or libobs.
 *
 * The assigned Input (an OBS scene) is a single scene item inside the stage,
 * which gives every channel its own position/size/crop and the option to use
 * OBS's own scene-item show/hide transitions for fades.
 */
class MixishOverlayController : public QObject {
	Q_OBJECT

public:
	static constexpr int MaxChannels = 4;
	static constexpr int FirstVideoChannel = 16;

	/* All four channels are active since stage (b) of the overlay work. */
	static constexpr int ActiveChannels = MaxChannels;

	MixishOverlayController(MixishSwitcher *switcher, OBSBasic *main);
	~MixishOverlayController() override;

	/* Releases channels and stage scenes. Safe to call more than once and
	 * from aboutToQuit so nothing outlives libobs. */
	void Shutdown();

	static QColor ChannelColor(int index);
	static QString ChannelLabel(int index); /* "OV1" */
	static QString ChannelTitle(int index); /* "Overlay 1" */

	int ChannelCount() const { return ActiveChannels; }

	bool IsAssigned(int index) const;
	bool IsOnAir(int index) const;
	bool IsAudioEnabled(int index) const;
	bool IsFade(int index) const;
	int FadeInMs(int index) const;
	int FadeOutMs(int index) const;

	OBSScene StageScene(int index) const;
	OBSSource InputSource(int index) const;
	QString InputName(int index) const;

	QRectF Rect(int index) const;
	obs_sceneitem_crop Crop(int index) const;

	bool CanAssign(int index, OBSScene input) const;
	bool Assign(int index, OBSScene input, bool turnOn = false);
	void Clear(int index);
	void Toggle(int index);
	void SetOnAir(int index, bool onAir);
	void SetAudioEnabled(int index, bool enabled);
	void SetFade(int index, bool fade);
	void SetFadeTimes(int index, int fadeInMs, int fadeOutMs);
	void SetRect(int index, const QRectF &rect);
	void SetCrop(int index, const obs_sceneitem_crop &crop);
	void ResetRect(int index);
	void AllOff();

	/* Re-resolve assignments after a scene collection switch. */
	void ReloadChannels();

	/* Called before OBS destroys the sources of the current collection:
	 * detaches every overlay without touching weak references, so no
	 * source is addref'd while it is being destroyed. */
	void PrepareForSceneCollectionChange();

	/* Called after a collection finished loading: rebuilds the stage
	 * scenes and clears the "collection changing" guard. */
	void OnSceneCollectionLoaded();

	/* Restores the saved per-collection overlay state (assignments, audio,
	 * fade settings and geometry). ON/OFF is intentionally not restored. */
	void RestoreState();

	/* Periodic safety net: keeps the stage scenes on their video channels
	 * and drops assignments whose input disappeared. */
	void Sync();

signals:
	void ChannelChanged(int index);
	void ChannelsChanged();

private:
	struct Channel {
		OBSScene stage;
		OBSSceneItem item;
		OBSSource fade; /* private fade_transition shared by show/hide */

		QString uuid;
		QString inputName;

		QRectF rect;
		obs_sceneitem_crop crop = {};
		int fadeInMs = 300;
		int fadeOutMs = 300;

		bool audio = true;
		bool onAir = false;
		bool fadeMode = false;
		bool hasGeometry = false;
	};

	struct ChannelHook {
		MixishOverlayController *self = nullptr;
		int index = 0;
	};

	static void InputRemoved(void *data, calldata_t *params);
	void OnInputRemoved(int index);

	void EnsureStage(int index);
	void ConnectInput(int index);
	void DisconnectInput(int index);
	void ApplyGeometry(int index);
	void ApplyAudio(int index);
	void DetachTransition(int index);
	void SaveState();
	void Notify(int index);

	bool IsValidIndex(int index) const;
	Channel *Get(int index);
	const Channel *Get(int index) const;

	MixishSwitcher *switcher = nullptr;
	OBSBasic *main = nullptr;

	std::array<Channel, MaxChannels> channels;
	std::array<ChannelHook *, MaxChannels> hooks = {};

	bool shutdown = false;
	bool collectionChanging = false;
	bool restoringState = false;
	bool stateLoaded = false;
};
