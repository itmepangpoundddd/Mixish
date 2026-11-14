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

#include "MixishStreamingSettings.hpp"
#include "MixishTheme.hpp"

#include <obs-frontend-api.h>
#include <widgets/OBSBasic.hpp>

#include <util/config-file.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>
#include <climits>
#include <cstdlib>

#include "moc_MixishStreamingSettings.cpp"

namespace {

struct QualityPreset {
	const char *label;
	int videoBitrate;
	int audioBitrate;
};

const QualityPreset kQualities[] = {
	{"H264 720p 3 Mbps", 3000, 128},
	{"H264 1080p 6 Mbps", 6000, 160},
	{"H264 1080p 8 Mbps", 8000, 192},
	{"H264 1080p 12 Mbps", 12000, 192},
};

QString StreamKey(int profile, const char *name)
{
	return QStringLiteral("StreamProfile%1%2").arg(profile + 1).arg(QLatin1String(name));
}

int ClosestQualityIndex(int bitrate)
{
	int best = 0;
	int bestDelta = INT_MAX;
	for (int i = 0; i < int(sizeof(kQualities) / sizeof(kQualities[0])); ++i) {
		const int delta = std::abs(kQualities[i].videoBitrate - bitrate);
		if (delta < bestDelta) {
			bestDelta = delta;
			best = i;
		}
	}
	return best;
}

} // namespace

MixishStreamingSettings::MixishStreamingSettings(OBSBasic *main_, QWidget *parent)
	: QDialog(parent), main(main_)
{
	setObjectName(QStringLiteral("mixishStreamingSettings"));
	setWindowTitle(tr("Streaming Settings"));
	setMinimumWidth(760);
	setModal(true);

	config_t *cfg = main->Config();
	slotCount = int(config_get_int(cfg, "Mixish", "StreamProfileCount"));
	if (slotCount < 1)
		slotCount = 1;
	if (slotCount > 5)
		slotCount = 5;

	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(MixishTheme::Space3, MixishTheme::Space3,
				  MixishTheme::Space3, MixishTheme::Space3);
	outer->setSpacing(MixishTheme::Space2);

	auto *profileRow = new QHBoxLayout();
	profileRow->setSpacing(MixishTheme::Space1);
	auto *profileLabel = new QLabel(tr("Profile"), this);
	MixishTheme::SetRole(profileLabel, "body");
	profileRow->addWidget(profileLabel);
	profileCombo = new QComboBox(this);
	MixishTheme::SetControl(profileCombo);
	profileCombo->setMinimumWidth(220);
	profileRow->addWidget(profileCombo);
	profileRow->addStretch(1);

	auto *addProfile = new QPushButton(tr("Add"), this);
	MixishTheme::SetVariant(addProfile, "secondary");
	addProfile->setFixedHeight(28);
	connect(addProfile, &QPushButton::clicked, this, [this]() {
		if (slotCount >= 5)
			return;
		StoreProfile(activeProfile);
		config_t *cfg = main->Config();
		for (const char *key : {"Server", "Key", "Encoder", "Name"}) {
			const std::string from = StreamKey(activeProfile, key).toUtf8().constData();
			const std::string to = StreamKey(slotCount, key).toUtf8().constData();
			config_set_string(cfg, "Mixish", to.c_str(),
					  config_get_string(cfg, "Mixish", from.c_str()));
		}
		config_set_int(cfg, "Mixish", StreamKey(slotCount, "VBitrate").toUtf8().constData(),
			       config_get_int(cfg, "Mixish",
					      StreamKey(activeProfile, "VBitrate").toUtf8().constData()));
		config_set_int(cfg, "Mixish", StreamKey(slotCount, "ABitrate").toUtf8().constData(),
			       config_get_int(cfg, "Mixish",
					      StreamKey(activeProfile, "ABitrate").toUtf8().constData()));
		slotCount++;
		config_set_int(cfg, "Mixish", "StreamProfileCount", slotCount);
		activeProfile = slotCount - 1;
		RebuildProfileButtons();
		LoadProfile(activeProfile);
	});
	profileRow->addWidget(addProfile);

	auto *deleteProfile = new QPushButton(tr("Delete"), this);
	MixishTheme::SetVariant(deleteProfile, "secondary");
	deleteProfile->setFixedHeight(28);
	connect(deleteProfile, &QPushButton::clicked, this, [this]() {
		if (slotCount <= 1)
			return;
		config_t *cfg = main->Config();
		for (int i = activeProfile; i < slotCount - 1; ++i) {
			for (const char *key : {"Server", "Key", "Encoder", "Name"}) {
				const std::string from = StreamKey(i + 1, key).toUtf8().constData();
				const std::string to = StreamKey(i, key).toUtf8().constData();
				config_set_string(cfg, "Mixish", to.c_str(),
						  config_get_string(cfg, "Mixish", from.c_str()));
			}
			config_set_int(cfg, "Mixish", StreamKey(i, "VBitrate").toUtf8().constData(),
				       config_get_int(cfg, "Mixish",
						      StreamKey(i + 1, "VBitrate").toUtf8().constData()));
			config_set_int(cfg, "Mixish", StreamKey(i, "ABitrate").toUtf8().constData(),
				       config_get_int(cfg, "Mixish",
						      StreamKey(i + 1, "ABitrate").toUtf8().constData()));
		}
		slotCount--;
		config_set_int(cfg, "Mixish", "StreamProfileCount", slotCount);
		activeProfile = std::min(activeProfile, slotCount - 1);
		RebuildProfileButtons();
		LoadProfile(activeProfile);
	});
	profileRow->addWidget(deleteProfile);
	outer->addLayout(profileRow);

	profileButtonRow = new QHBoxLayout();
	profileButtonRow->setSpacing(MixishTheme::Space1);
	outer->addLayout(profileButtonRow);

	auto *form = new QWidget(this);
	auto *grid = new QGridLayout(form);
	grid->setContentsMargins(0, MixishTheme::Space2, 0, 0);
	grid->setHorizontalSpacing(MixishTheme::Space2);
	grid->setVerticalSpacing(MixishTheme::Space2);
	grid->setColumnStretch(1, 1);
	grid->setColumnMinimumWidth(0, 110);

	int row = 0;
	auto addLabel = [&](const QString &text) {
		auto *label = new QLabel(text, form);
		label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		MixishTheme::SetRole(label, "body");
		grid->addWidget(label, row, 0);
	};

	addLabel(tr("Destination"));
	destination = new QComboBox(form);
	MixishTheme::SetControl(destination);
	destination->addItem(tr("Custom RTMP Server"));
	destination->setEnabled(false);
	grid->addWidget(destination, row, 1);
	++row;

	addLabel(tr("URL"));
	server = new QLineEdit(form);
	MixishTheme::SetControl(server);
	server->setPlaceholderText(QStringLiteral("rtmp://localhost/live"));
	grid->addWidget(server, row, 1);
	++row;

	addLabel(tr("Stream Name or Key"));
	streamKey = new QLineEdit(form);
	MixishTheme::SetControl(streamKey);
	grid->addWidget(streamKey, row, 1);
	++row;

	addLabel(tr("Quality"));
	quality = new QComboBox(form);
	MixishTheme::SetControl(quality);
	for (const QualityPreset &preset : kQualities)
		quality->addItem(QString::fromUtf8(preset.label), preset.videoBitrate);
	grid->addWidget(quality, row, 1);
	++row;

	addLabel(tr("Application"));
	application = new QComboBox(form);
	MixishTheme::SetControl(application);
	application->addItem(QStringLiteral("FFMPEG"));
	application->setEnabled(false);
	grid->addWidget(application, row, 1);
	++row;

	hardwareEncoder = new QCheckBox(tr("Use hardware encoder"), form);
	MixishTheme::SetRole(hardwareEncoder, "body");
	grid->addWidget(hardwareEncoder, row, 1);
	++row;

	grid->setRowStretch(row, 1);
	outer->addWidget(form, 1);

	auto *footer = new QHBoxLayout();
	footer->setSpacing(MixishTheme::Space1);

	auto *stopAll = new QPushButton(tr("Stop All"), this);
	MixishTheme::SetVariant(stopAll, "danger");
	stopAll->setFixedHeight(32);
	connect(stopAll, &QPushButton::clicked, this, [this]() {
		if (obs_frontend_streaming_active())
			obs_frontend_streaming_stop();
	});
	footer->addWidget(stopAll);

	auto *stopOne = new QPushButton(tr("Stop 1"), this);
	MixishTheme::SetVariant(stopOne, "danger");
	stopOne->setFixedHeight(32);
	connect(stopOne, &QPushButton::clicked, this, [this]() {
		if (obs_frontend_streaming_active())
			obs_frontend_streaming_stop();
	});
	footer->addWidget(stopOne);

	footer->addStretch(1);

	auto *status = new QPushButton(tr("View Status"), this);
	MixishTheme::SetVariant(status, "secondary");
	status->setFixedHeight(32);
	connect(status, &QPushButton::clicked, this, [this]() { main->on_stats_triggered(); });
	footer->addWidget(status);

	auto *save = new QPushButton(tr("Save and Close"), this);
	MixishTheme::SetVariant(save, "primary");
	save->setFixedHeight(32);
	connect(save, &QPushButton::clicked, this, [this]() {
		if (obs_frontend_streaming_active()) {
			QMessageBox::warning(this, tr("Streaming Settings"),
					     tr("Stop streaming before changing the streaming settings."));
			return;
		}
		StoreProfile(activeProfile);
		Apply();
		accept();
	});
	footer->addWidget(save);
	outer->addLayout(footer);

	connect(quality, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		[this](int index) {
			if (index >= 0 && index < int(sizeof(kQualities) / sizeof(kQualities[0])))
				hardwareEncoder->setChecked(hardwareEncoder->isChecked());
		});
	connect(profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
		if (loadingProfile || index < 0 || index == activeProfile)
			return;
		StoreProfile(activeProfile);
		activeProfile = index;
		LoadProfile(index);
	});

	/* Seed profile 1 from OBS on first use. */
	if (!config_has_user_value(cfg, "Mixish", StreamKey(0, "Server").toUtf8().constData())) {
		QString currentServer;
		QString currentKey;
		OBSService service = main->GetService();
		if (service) {
			OBSDataAutoRelease settings = obs_service_get_settings(service);
			currentServer = QString::fromUtf8(obs_data_get_string(settings, "server"));
			currentKey = QString::fromUtf8(obs_data_get_string(settings, "key"));
		}
		const int bitrate = int(config_get_int(cfg, "SimpleOutput", "VBitrate"));
		const char *enc = config_get_string(cfg, "SimpleOutput", "StreamEncoder");
		for (int i = 0; i < 5; ++i) {
			config_set_string(cfg, "Mixish", StreamKey(i, "Name").toUtf8().constData(),
					  QStringLiteral("Profile %1").arg(i + 1).toUtf8().constData());
			config_set_string(cfg, "Mixish", StreamKey(i, "Server").toUtf8().constData(),
					  i == 0 ? currentServer.toUtf8().constData() : "");
			config_set_string(cfg, "Mixish", StreamKey(i, "Key").toUtf8().constData(),
					  i == 0 ? currentKey.toUtf8().constData() : "");
			config_set_string(cfg, "Mixish", StreamKey(i, "Encoder").toUtf8().constData(),
					  enc && *enc ? enc : "x264");
			config_set_int(cfg, "Mixish", StreamKey(i, "VBitrate").toUtf8().constData(),
				       bitrate);
			config_set_int(cfg, "Mixish", StreamKey(i, "ABitrate").toUtf8().constData(),
				       int(config_get_int(cfg, "SimpleOutput", "ABitrate")));
		}
	}

	RebuildProfileButtons();
	LoadProfile(0);
	MixishTheme::Apply(this);
}

void MixishStreamingSettings::RebuildProfileButtons()
{
	for (QPushButton *button : profileButtons)
		button->deleteLater();
	profileButtons.clear();

	for (int i = 0; i < slotCount; ++i) {
		auto *button = new QPushButton(QString::number(i + 1), this);
		MixishTheme::SetVariant(button, "secondary");
		button->setCheckable(true);
		button->setFixedSize(34, 28);
		button->setFocusPolicy(Qt::NoFocus);
		connect(button, &QPushButton::clicked, this, [this, i]() {
			if (activeProfile == i || loadingProfile)
				return;
			StoreProfile(activeProfile);
			activeProfile = i;
			LoadProfile(i);
		});
		profileButtonRow->addWidget(button);
		profileButtons.push_back(button);
	}
	profileButtonRow->addStretch(1);

	QSignalBlocker block(profileCombo);
	profileCombo->clear();
	for (int i = 0; i < slotCount; ++i)
		profileCombo->addItem(tr("Profile %1").arg(i + 1));
}

void MixishStreamingSettings::SyncProfileCombo()
{
	QSignalBlocker block(profileCombo);
	profileCombo->setCurrentIndex(activeProfile);
	for (int i = 0; i < profileButtons.size(); ++i) {
		profileButtons[i]->setChecked(i == activeProfile);
		MixishTheme::SetState(profileButtons[i], i == activeProfile ? "selected" : "idle");
	}
}

void MixishStreamingSettings::LoadProfile(int index)
{
	loadingProfile = true;
	config_t *cfg = main->Config();

	server->setText(QString::fromUtf8(
		config_get_string(cfg, "Mixish", StreamKey(index, "Server").toUtf8().constData())));
	streamKey->setText(QString::fromUtf8(
		config_get_string(cfg, "Mixish", StreamKey(index, "Key").toUtf8().constData())));
	const int bitrate = int(config_get_int(cfg, "Mixish",
					      StreamKey(index, "VBitrate").toUtf8().constData()));
	quality->setCurrentIndex(ClosestQualityIndex(bitrate));
	const std::string enc =
		config_get_string(cfg, "Mixish", StreamKey(index, "Encoder").toUtf8().constData());
	hardwareEncoder->setChecked(enc == "nvenc");
	SyncProfileCombo();
	loadingProfile = false;
}

void MixishStreamingSettings::StoreProfile(int index)
{
	config_t *cfg = main->Config();
	config_set_string(cfg, "Mixish", StreamKey(index, "Server").toUtf8().constData(),
			  server->text().toUtf8().constData());
	config_set_string(cfg, "Mixish", StreamKey(index, "Key").toUtf8().constData(),
			  streamKey->text().toUtf8().constData());
	const int q = ClosestQualityIndex(quality->currentData().toInt());
	config_set_int(cfg, "Mixish", StreamKey(index, "VBitrate").toUtf8().constData(),
		       kQualities[q].videoBitrate);
	config_set_int(cfg, "Mixish", StreamKey(index, "ABitrate").toUtf8().constData(),
		       kQualities[q].audioBitrate);
	config_set_string(cfg, "Mixish", StreamKey(index, "Encoder").toUtf8().constData(),
			  hardwareEncoder->isChecked() ? "nvenc" : "x264");
}

void MixishStreamingSettings::Apply()
{
	config_t *cfg = main->Config();
	const int q = ClosestQualityIndex(quality->currentData().toInt());

	OBSService service = main->GetService();
	if (service) {
		OBSDataAutoRelease settings = obs_service_get_settings(service);
		obs_data_set_string(settings, "server", server->text().toUtf8().constData());
		obs_data_set_string(settings, "key", streamKey->text().toUtf8().constData());
		obs_service_update(service, settings);
	}

	config_set_string(cfg, "SimpleOutput", "StreamEncoder",
			  hardwareEncoder->isChecked() ? "nvenc" : "x264");
	config_set_int(cfg, "SimpleOutput", "VBitrate", kQualities[q].videoBitrate);
	config_set_int(cfg, "SimpleOutput", "ABitrate", kQualities[q].audioBitrate);
	config_set_string(cfg, "SimpleOutput", "StreamAudioEncoder", "aac");
	config_set_bool(cfg, "SimpleOutput", "UseAdvanced", false);

	config_save(cfg);
	main->ResetOutputs();
	main->SaveProject();
}
