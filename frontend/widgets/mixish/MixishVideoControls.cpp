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
#include "MixishSwitcher.hpp"
#include "MixishTheme.hpp"

#include <OBSApp.hpp>
#include <components/MediaControls.hpp>
#include <util/config-file.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QSizePolicy>

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
	layout->setSpacing(MixishTheme::Space1);

	controls = new MediaControls(this);
	controls->setObjectName(QStringLiteral("mixishTransport"));
	controls->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	controls->setMinimumWidth(340);
	layout->addWidget(controls, 1);

	/* The Mixish transport shows total duration, not the OBS default
	 * countdown. This only flips the shared preference once. */
	if (config_get_bool(App()->GetUserConfig(), "BasicWindow",
			    "MediaControlsCountdownTimer"))
		QMetaObject::invokeMethod(controls, "on_durationLabel_clicked",
					  Qt::DirectConnection);

	/* MediaControls puts its seek slider next to an expanding spacer, which
	 * leaves the slider without room in a normal widget. Give it the space. */
	QSlider *seek = controls->findChild<QSlider *>(QStringLiteral("slider"));
	if (seek) {
		seek->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		seek->setMinimumWidth(150);
	}

	if (auto *inner = controls->findChild<QHBoxLayout *>(QStringLiteral("horizontalLayout"))) {
		for (int i = 0; i < inner->count(); ++i) {
			if (seek && inner->itemAt(i)->widget() == seek)
				inner->setStretch(i, 1);
		}
	}

	placeholder = new QLabel(tr("Select an input"), this);
	MixishTheme::SetRole(placeholder, "muted");
	placeholder->setVisible(false);
	layout->addWidget(placeholder);

	layout->addStretch(1);

	controls->SetSource(nullptr);
	controls->hide();
	placeholder->show();

	MixishTheme::Apply(this);
}

void MixishVideoControls::SetScene(OBSScene scene)
{
	if (!controls)
		return;

	obs_source_t *media = nullptr;
	if (scene) {
		MediaSearch search;
		obs_scene_enum_items(scene, FindMediaItem, &search);
		media = search.found;
	}

	if (media) {
		controls->SetSource(OBSSource(media));
		controls->setEnabled(true);
		controls->show();
		placeholder->hide();
		return;
	}

	controls->SetSource(nullptr);
	controls->setEnabled(false);

	if (scene) {
		/* Non-media input: keep the transport visible but greyed out. */
		controls->show();
		placeholder->hide();
	} else {
		controls->hide();
		placeholder->show();
	}
}

bool MixishVideoControls::HasMedia() const
{
	return controls && controls->isVisible() && controls->isEnabled();
}
