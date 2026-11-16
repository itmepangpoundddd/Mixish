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

#include "MixishVideoControls.hpp"
#include "MixishMediaTransport.hpp"
#include "MixishSwitcher.hpp"
#include "MixishTheme.hpp"

#include <QHBoxLayout>

#include "moc_MixishVideoControls.cpp"

namespace {

bool IsMediaSource(obs_source_t *source)
{
	if (!source)
		return false;
	return (obs_source_get_output_flags(source) & OBS_SOURCE_CONTROLLABLE_MEDIA) != 0;
}

struct MediaSearch {
	obs_source_t *found = nullptr;
	int depth = 0;
};

bool FindMediaItem(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	UNUSED_PARAMETER(scene);

	auto *search = static_cast<MediaSearch *>(param);
	if (search->found || !item)
		return false;

	obs_source_t *source = obs_sceneitem_get_source(item);
	if (IsMediaSource(source)) {
		search->found = source;
		return false;
	}

	if (search->depth < 4) {
		if (obs_scene_t *nested = obs_scene_from_source(source)) {
			search->depth++;
			obs_scene_enum_items(nested, FindMediaItem, search);
			search->depth--;
		}
	}

	return search->found == nullptr;
}

} // namespace

MixishVideoControls::MixishVideoControls(MixishSwitcher *switcher_, QWidget *parent)
	: QFrame(parent), switcher(switcher_)
{
	setObjectName(QStringLiteral("mixishVideoControls"));
	setFixedHeight(32);

	auto *layout = new QHBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	transport = new MixishMediaTransport(this);
	transport->SetPlaceholder(tr("Select an input"));
	layout->addWidget(transport, 1);

	SetScene(OBSScene());

	MixishTheme::Apply(this);
}

MixishMediaTransport *MixishVideoControls::Transport() const
{
	return transport;
}

void MixishVideoControls::SetScene(OBSScene scene)
{
	if (!transport)
		return;

	obs_source_t *media = nullptr;
	if (scene) {
		MediaSearch search;
		obs_scene_enum_items(scene, FindMediaItem, &search);
		media = search.found;
	}

	if (media) {
		transport->SetSource(media, true);
	} else if (scene) {
		/* Non-media input: keep the transport visible but disabled. */
		transport->SetSource(nullptr, true);
	} else {
		transport->SetPlaceholder(tr("Select an input"));
		transport->SetSource(nullptr, false);
	}
}

bool MixishVideoControls::HasMedia() const
{
	return transport && transport->HasSource();
}
