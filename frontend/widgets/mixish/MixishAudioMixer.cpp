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

#include "MixishAudioMixer.hpp"
#include "MixishSwitcher.hpp"
#include "MixishTheme.hpp"

#include <components/VolumeControl.hpp>

#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cstring>
#include <vector>

#include "moc_MixishAudioMixer.cpp"

namespace {

bool IsOutputDevice(obs_source_t *source)
{
	const char *id = obs_source_get_id(source);
	return id && strstr(id, "output_capture") != nullptr;
}

bool IsAudioInput(obs_source_t *source)
{
	if (!source || obs_source_removed(source))
		return false;
	/* Scenes and groups mix their children; listing them would duplicate
	 * every input inside them. */
	if (obs_scene_from_source(source))
		return false;
	return (obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO) != 0;
}

struct AudioScan {
	obs_source_t *master = nullptr;
	std::vector<obs_source_t *> sources;
};

bool ScanSource(void *param, obs_source_t *source)
{
	auto *scan = static_cast<AudioScan *>(param);
	if (!IsAudioInput(source))
		return true;

	if (!scan->master && IsOutputDevice(source)) {
		scan->master = source;
		return true;
	}

	scan->sources.push_back(source);
	return true;
}

void HideCategoryLabels(QWidget *root)
{
	for (QLabel *label : root->findChildren<QLabel *>()) {
		const QString cls = label->property("class").toString();
		if (cls.contains(QStringLiteral("mixer-category")))
			label->hide();
	}
}

} // namespace

MixishAudioMixer::MixishAudioMixer(MixishSwitcher *switcher_, QWidget *parent)
	: QFrame(parent), switcher(switcher_)
{
	setObjectName(QStringLiteral("mixishAudioMixer"));
	setMinimumWidth(280);
	setMaximumWidth(480);

	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(MixishTheme::Space3, MixishTheme::Space2,
				  MixishTheme::Space3, MixishTheme::Space2);
	outer->setSpacing(MixishTheme::Space1);

	auto *titleRow = new QHBoxLayout();
	titleRow->setSpacing(MixishTheme::Space1);

	auto *title = new QLabel(tr("AUDIO MIXER"), this);
	MixishTheme::MakeSectionLabel(title);
	titleRow->addWidget(title);
	titleRow->addStretch(1);
	outer->addLayout(titleRow);

	masterName = new QLabel(tr("MASTER"), this);
	MixishTheme::MakeSectionLabel(masterName);
	outer->addWidget(masterName);

	masterHost = new QWidget(this);
	masterHost->setObjectName(QStringLiteral("mixishMixerMaster"));
	masterHost->setLayout(new QVBoxLayout(masterHost));
	masterHost->layout()->setContentsMargins(0, 0, 0, 2);
	masterHost->layout()->setSpacing(1);
	outer->addWidget(masterHost);

	scrollArea = new QScrollArea(this);
	scrollArea->setObjectName(QStringLiteral("mixishMixerScroll"));
	scrollArea->setFrameShape(QFrame::NoFrame);
	scrollArea->setWidgetResizable(true);
	scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

	rowsHost = new QWidget(scrollArea);
	rowsHost->setObjectName(QStringLiteral("mixishMixerRows"));
	rowLayout = new QVBoxLayout(rowsHost);
	rowLayout->setContentsMargins(0, 0, 0, 0);
	rowLayout->setSpacing(MixishTheme::Space1);
	rowLayout->setAlignment(Qt::AlignTop);
	scrollArea->setWidget(rowsHost);

	outer->addWidget(scrollArea, 1);

	refreshTimer = new QTimer(this);
	refreshTimer->setInterval(1500);
	connect(refreshTimer, &QTimer::timeout, this, &MixishAudioMixer::Refresh);
	refreshTimer->start();

	MixishTheme::Apply(this);

	Refresh();
}

void MixishAudioMixer::showEvent(QShowEvent *event)
{
	QFrame::showEvent(event);
	Refresh();
}

void MixishAudioMixer::Refresh()
{
	RebuildMaster();
	RebuildRows();
}

void MixishAudioMixer::RebuildMaster()
{
	if (!masterHost || !masterHost->layout())
		return;

	AudioScan scan;
	obs_enum_sources(ScanSource, &scan);

	obs_source_t *device = scan.master;
	const QString uuid = device ? QString::fromUtf8(obs_source_get_uuid(device))
				    : QStringLiteral("<none>");

	if (uuid == currentMasterUuid)
		return;

	currentMasterUuid = uuid;

	while (QLayoutItem *item = masterHost->layout()->takeAt(0)) {
		if (QWidget *widget = item->widget())
			widget->deleteLater();
		delete item;
	}
	master = nullptr;

	if (device) {
		masterName->show();
		masterHost->show();

		auto *card = new QFrame(masterHost);
		card->setObjectName(QStringLiteral("mixishMasterCard"));
		auto *cardLayout = new QHBoxLayout(card);
		cardLayout->setContentsMargins(MixishTheme::Space1, MixishTheme::Space1,
					       MixishTheme::Space1, MixishTheme::Space1);
		cardLayout->setSpacing(0);

		master = new VolumeControl(device, card, false);
		master->setObjectName(QStringLiteral("mixishMasterControl"));
		master->setMinimumHeight(40);
		master->setToolTip(tr("Master fader for '%1'")
					   .arg(QString::fromUtf8(obs_source_get_name(device))));
		HideCategoryLabels(master);
		cardLayout->addWidget(master);
		masterHost->layout()->addWidget(card);
	} else {
		/* No output device in this profile: hide the section rather than
		 * leaving an empty fader behind. */
		masterName->hide();
		masterHost->hide();
	}
}

void MixishAudioMixer::RebuildRows()
{
	if (!rowLayout || !rowsHost)
		return;

	AudioScan scan;
	obs_enum_sources(ScanSource, &scan);

	obs_source_t *masterDevice = scan.master;
	std::vector<obs_source_t *> sources;
	for (obs_source_t *source : scan.sources) {
		if (source == masterDevice)
			continue;
		sources.push_back(source);
	}

	std::sort(sources.begin(), sources.end(), [](obs_source_t *a, obs_source_t *b) {
		return strcmp(obs_source_get_name(a), obs_source_get_name(b)) < 0;
	});

	QStringList signature;
	for (obs_source_t *source : sources)
		signature << QString::fromUtf8(obs_source_get_uuid(source));

	if (signature == rowsSignature)
		return;
	rowsSignature = signature;

	while (QLayoutItem *item = rowLayout->takeAt(0)) {
		if (QWidget *widget = item->widget())
			widget->deleteLater();
		delete item;
	}
	emptyLabel = nullptr;

	if (sources.empty()) {
		emptyLabel = new QLabel(tr("No audio inputs"), rowsHost);
		MixishTheme::SetRole(emptyLabel, "muted");
		rowLayout->addWidget(emptyLabel);
		rowLayout->addStretch(1);
		return;
	}

	for (obs_source_t *source : sources) {
		auto *card = new QFrame(rowsHost);
		card->setObjectName(QStringLiteral("mixishMixerCard"));
		auto *cardLayout = new QHBoxLayout(card);
		cardLayout->setContentsMargins(MixishTheme::Space1, MixishTheme::Space1,
					       MixishTheme::Space1, MixishTheme::Space1);
		cardLayout->setSpacing(0);

		auto *control = new VolumeControl(source, card, false);
		control->setObjectName(QStringLiteral("mixishMixerRow"));
		control->setMinimumHeight(40);
		control->setToolTip(QString::fromUtf8(obs_source_get_name(source)));
		HideCategoryLabels(control);
		cardLayout->addWidget(control);
		rowLayout->addWidget(card);
	}

	rowLayout->addStretch(1);
}
