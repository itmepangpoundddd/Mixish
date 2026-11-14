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

#include "MixishRecordingSettings.hpp"
#include "MixishTheme.hpp"

#include <obs-frontend-api.h>
#include <widgets/OBSBasic.hpp>

#include <util/config-file.h>

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <climits>
#include <cstdlib>

#include "moc_MixishRecordingSettings.cpp"

namespace {

struct FormatEntry {
	const char *value;
	const char *label;
};

const FormatEntry kFormats[] = {
	{"mp4", "MP4"},
	{"mkv", "MKV"},
	{"mov", "MOV"},
	{"flv", "FLV"},
	{"ts", "MPEG-TS"},
};

QString ProfileKey(int profile, const char *name)
{
	return QStringLiteral("RecProfile%1%2").arg(profile + 1).arg(QLatin1String(name));
}

} // namespace

MixishRecordingSettings::MixishRecordingSettings(OBSBasic *main_, QWidget *parent)
	: QDialog(parent), main(main_)
{
	setObjectName(QStringLiteral("mixishRecordingSettings"));
	setWindowTitle(tr("Recording Setup"));
	setMinimumWidth(720);
	setModal(true);

	config_t *cfg = main->Config();

	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(MixishTheme::Space3, MixishTheme::Space3,
				  MixishTheme::Space3, MixishTheme::Space3);
	outer->setSpacing(MixishTheme::Space2);

	auto *profileRow = new QHBoxLayout();
	profileRow->setSpacing(MixishTheme::Space1);
	for (int i = 0; i < 2; ++i) {
		profileButtons[i] = new QPushButton(QString::number(i + 1), this);
		MixishTheme::SetVariant(profileButtons[i], "secondary");
		profileButtons[i]->setCheckable(true);
		profileButtons[i]->setFixedSize(34, 28);
		profileButtons[i]->setFocusPolicy(Qt::NoFocus);
		connect(profileButtons[i], &QPushButton::clicked, this, [this, i]() {
			if (activeProfile == i || loadingProfile)
				return;
			StoreProfile(activeProfile);
			activeProfile = i;
			LoadProfile(i);
		});
		profileRow->addWidget(profileButtons[i]);
	}
	profileRow->addStretch(1);
	outer->addLayout(profileRow);

	auto *body = new QHBoxLayout();
	body->setSpacing(MixishTheme::Space3);

	formats = new QListWidget(this);
	formats->setObjectName(QStringLiteral("mixishRecordingFormats"));
	formats->setFixedWidth(150);
	for (const FormatEntry &entry : kFormats) {
		auto *item = new QListWidgetItem(QString::fromUtf8(entry.label), formats);
		item->setData(Qt::UserRole, QString::fromUtf8(entry.value));
	}
	body->addWidget(formats);

	auto *form = new QWidget(this);
	auto *grid = new QGridLayout(form);
	grid->setContentsMargins(0, 0, 0, 0);
	grid->setHorizontalSpacing(MixishTheme::Space2);
	grid->setVerticalSpacing(MixishTheme::Space2);
	grid->setColumnStretch(1, 1);

	int row = 0;
	auto addLabel = [&](const QString &text) {
		auto *label = new QLabel(text, form);
		label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		MixishTheme::SetRole(label, "body");
		grid->addWidget(label, row, 0);
	};
	auto nextRow = [&]() { ++row; };

	addLabel(tr("Filename"));
	auto *pathRow = new QHBoxLayout();
	pathRow->setSpacing(MixishTheme::Space1);
	path = new QLineEdit(form);
	MixishTheme::SetControl(path);
	pathRow->addWidget(path, 1);
	auto *browse = new QPushButton(tr("Browse"), form);
	MixishTheme::SetVariant(browse, "secondary");
	browse->setFixedHeight(28);
	connect(browse, &QPushButton::clicked, this, [this]() {
		const QString dir = QFileDialog::getExistingDirectory(this, tr("Recording folder"), path->text());
		if (!dir.isEmpty())
			path->setText(dir);
	});
	pathRow->addWidget(browse);
	grid->addLayout(pathRow, row, 1);
	nextRow();

	addLabel(tr("Size"));
	sizeValue = new QLabel(form);
	MixishTheme::SetRole(sizeValue, "muted");
	grid->addWidget(sizeValue, row, 1);
	nextRow();

	addLabel(tr("Frame Rate"));
	fpsValue = new QLabel(form);
	MixishTheme::SetRole(fpsValue, "muted");
	grid->addWidget(fpsValue, row, 1);
	nextRow();

	addLabel(tr("Bit Rate"));
	auto *bitrateRow = new QHBoxLayout();
	bitrateRow->setSpacing(MixishTheme::Space1);
	videoBitrate = new QSpinBox(form);
	MixishTheme::SetControl(videoBitrate);
	videoBitrate->setRange(500, 50000);
	videoBitrate->setSingleStep(500);
	videoBitrate->setSuffix(tr(" Kbps"));
	bitrateRow->addWidget(videoBitrate, 1);
	quality = new QComboBox(form);
	MixishTheme::SetControl(quality);
	quality->addItem(tr("Use stream bitrate"), QStringLiteral("Stream"));
	quality->addItem(tr("Higher"), QStringLiteral("Higher"));
	quality->addItem(tr("Lossless"), QStringLiteral("Lossless"));
	bitrateRow->addWidget(quality, 1);
	grid->addLayout(bitrateRow, row, 1);
	nextRow();

	addLabel(tr("Encoder"));
	encoder = new QComboBox(form);
	MixishTheme::SetControl(encoder);
	encoder->addItem(tr("x264 (CPU)"), QStringLiteral("x264"));
	encoder->addItem(tr("NVIDIA NVENC"), QStringLiteral("nvenc"));
	grid->addWidget(encoder, row, 1);
	nextRow();

	hardwareEncoder = new QCheckBox(tr("Use hardware encoder"), form);
	MixishTheme::SetRole(hardwareEncoder, "body");
	grid->addWidget(hardwareEncoder, row, 1);
	nextRow();

	addLabel(tr("Audio"));
	audioTrack = new QComboBox(form);
	MixishTheme::SetControl(audioTrack);
	audioTrack->addItem(tr("Master"), 0);
	audioTrack->setEnabled(false);
	grid->addWidget(audioTrack, row, 1);
	nextRow();

	addLabel(tr("Audio Bit Rate"));
	audioBitrate = new QComboBox(form);
	MixishTheme::SetControl(audioBitrate);
	for (int value : {96, 128, 160, 192, 256, 320})
		audioBitrate->addItem(QStringLiteral("%1 Kbps").arg(value), value);
	grid->addWidget(audioBitrate, row, 1);
	nextRow();

	addLabel(tr("New File Every"));
	auto *split = new QComboBox(form);
	MixishTheme::SetControl(split);
	split->addItem(tr("None"));
	split->setEnabled(false);
	split->setToolTip(tr("File splitting is available in Mixish advanced output mode"));
	grid->addWidget(split, row, 1);
	nextRow();

	grid->setRowStretch(row, 1);
	body->addWidget(form, 1);
	outer->addLayout(body, 1);

	auto *footer = new QHBoxLayout();
	footer->setSpacing(MixishTheme::Space1);

	auto *wavButton = new QPushButton(tr("WAV File Record"), this);
	MixishTheme::SetVariant(wavButton, "secondary");
	wavButton->setFixedHeight(32);
	wavButton->setEnabled(false);
	wavButton->setToolTip(tr("Audio-only recording is configured in Mixish settings"));
	footer->addWidget(wavButton);
	footer->addStretch(1);

	auto *cancel = new QPushButton(tr("Cancel"), this);
	MixishTheme::SetVariant(cancel, "secondary");
	cancel->setFixedHeight(32);
	connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
	footer->addWidget(cancel);

	auto *ok = new QPushButton(tr("OK"), this);
	MixishTheme::SetVariant(ok, "primary");
	ok->setFixedHeight(32);
	connect(ok, &QPushButton::clicked, this, [this]() {
		if (obs_frontend_recording_active()) {
			QMessageBox::warning(this, tr("Recording Setup"),
					     tr("Stop recording before changing the recording settings."));
			return;
		}
		StoreProfile(activeProfile);
		Apply();
		accept();
	});
	footer->addWidget(ok);
	outer->addLayout(footer);

	connect(encoder, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
		const bool hw = encoder->currentData().toString() == QLatin1String("nvenc");
		QSignalBlocker block(hardwareEncoder);
		hardwareEncoder->setChecked(hw);
	});
	connect(hardwareEncoder, &QCheckBox::toggled, this, [this](bool checked) {
		const int index = encoder->findData(checked ? QStringLiteral("nvenc") : QStringLiteral("x264"));
		if (index >= 0) {
			QSignalBlocker block(encoder);
			encoder->setCurrentIndex(index);
		}
	});
	connect(quality, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
		videoBitrate->setEnabled(quality->currentData().toString() == QLatin1String("Stream"));
	});

	/* Seed both profiles from the current OBS configuration on first use. */
	if (!config_has_user_value(cfg, "Mixish", ProfileKey(0, "Format").toUtf8().constData())) {
		const char *format = config_get_string(cfg, "SimpleOutput", "RecFormat2");
		const char *enc = config_get_string(cfg, "SimpleOutput", "RecEncoder");
		const char *qual = config_get_string(cfg, "SimpleOutput", "RecQuality");
		const char *filePath = config_get_string(cfg, "SimpleOutput", "FilePath");
		for (int i = 0; i < 2; ++i) {
			config_set_string(cfg, "Mixish", ProfileKey(i, "Format").toUtf8().constData(),
					  format && *format ? format : "mkv");
			config_set_string(cfg, "Mixish", ProfileKey(i, "Encoder").toUtf8().constData(),
					  enc && *enc ? enc : "x264");
			config_set_string(cfg, "Mixish", ProfileKey(i, "Quality").toUtf8().constData(),
					  qual && *qual ? qual : "Stream");
			config_set_string(cfg, "Mixish", ProfileKey(i, "Path").toUtf8().constData(),
					  filePath ? filePath : "");
			config_set_int(cfg, "Mixish", ProfileKey(i, "VBitrate").toUtf8().constData(),
				       config_get_int(cfg, "SimpleOutput", "VBitrate"));
			config_set_int(cfg, "Mixish", ProfileKey(i, "ABitrate").toUtf8().constData(),
				       config_get_int(cfg, "SimpleOutput", "ABitrate"));
		}
	}

	LoadProfile(0);
	profileButtons[0]->setChecked(true);
	UpdateSummary();
	MixishTheme::Apply(this);
}

void MixishRecordingSettings::UpdateSummary()
{
	obs_video_info ovi = {};
	if (obs_get_video_info(&ovi)) {
		sizeValue->setText(QStringLiteral("%1 x %2").arg(ovi.base_width).arg(ovi.base_height));
		const double fps = ovi.fps_den ? double(ovi.fps_num) / double(ovi.fps_den) : 0.0;
		fpsValue->setText(QStringLiteral("%1 fps").arg(fps, 0, 'f', fps < 10.0 ? 2 : 0));
	}
}

void MixishRecordingSettings::LoadProfile(int index)
{
	loadingProfile = true;
	config_t *cfg = main->Config();

	const std::string format =
		config_get_string(cfg, "Mixish", ProfileKey(index, "Format").toUtf8().constData());
	const std::string enc =
		config_get_string(cfg, "Mixish", ProfileKey(index, "Encoder").toUtf8().constData());
	const std::string qual =
		config_get_string(cfg, "Mixish", ProfileKey(index, "Quality").toUtf8().constData());

	path->setText(QString::fromUtf8(
		config_get_string(cfg, "Mixish", ProfileKey(index, "Path").toUtf8().constData())));
	videoBitrate->setValue(int(config_get_int(cfg, "Mixish",
						  ProfileKey(index, "VBitrate").toUtf8().constData())));
	audioBitrate->setCurrentIndex(audioBitrate->findData(
		int(config_get_int(cfg, "Mixish", ProfileKey(index, "ABitrate").toUtf8().constData()))));

	for (int i = 0; i < formats->count(); ++i) {
		if (formats->item(i)->data(Qt::UserRole).toString() == QString::fromStdString(format)) {
			formats->setCurrentRow(i);
			break;
		}
	}

	encoder->setCurrentIndex(
		std::max(0, encoder->findData(QString::fromStdString(enc.empty() ? "x264" : enc))));
	const int q = quality->findData(QString::fromStdString(qual.empty() ? "Stream" : qual));
	quality->setCurrentIndex(std::max(0, q));
	videoBitrate->setEnabled(quality->currentData().toString() == QLatin1String("Stream"));
	hardwareEncoder->setChecked(encoder->currentData().toString() == QLatin1String("nvenc"));
	for (int i = 0; i < 2; ++i)
		MixishTheme::SetState(profileButtons[i], i == index ? "selected" : "idle");
	loadingProfile = false;
}

void MixishRecordingSettings::StoreProfile(int index)
{
	config_t *cfg = main->Config();
	const QString format = formats->currentItem() ? formats->currentItem()->data(Qt::UserRole).toString()
						      : QStringLiteral("mkv");

	config_set_string(cfg, "Mixish", ProfileKey(index, "Format").toUtf8().constData(),
			  format.toUtf8().constData());
	config_set_string(cfg, "Mixish", ProfileKey(index, "Path").toUtf8().constData(),
			  path->text().toUtf8().constData());
	config_set_string(cfg, "Mixish", ProfileKey(index, "Encoder").toUtf8().constData(),
			  encoder->currentData().toString().toUtf8().constData());
	config_set_string(cfg, "Mixish", ProfileKey(index, "Quality").toUtf8().constData(),
			  quality->currentData().toString().toUtf8().constData());
	config_set_int(cfg, "Mixish", ProfileKey(index, "VBitrate").toUtf8().constData(),
		       videoBitrate->value());
	config_set_int(cfg, "Mixish", ProfileKey(index, "ABitrate").toUtf8().constData(),
		       audioBitrate->currentData().toInt());
}

void MixishRecordingSettings::Apply()
{
	config_t *cfg = main->Config();
	const QString format = formats->currentItem() ? formats->currentItem()->data(Qt::UserRole).toString()
						      : QStringLiteral("mkv");

	config_set_string(cfg, "SimpleOutput", "FilePath", path->text().toUtf8().constData());
	config_set_string(cfg, "SimpleOutput", "RecFormat2", format.toUtf8().constData());
	config_set_string(cfg, "SimpleOutput", "RecEncoder",
			  encoder->currentData().toString().toUtf8().constData());
	config_set_string(cfg, "SimpleOutput", "RecQuality",
			  quality->currentData().toString().toUtf8().constData());
	config_set_int(cfg, "SimpleOutput", "VBitrate", videoBitrate->value());
	config_set_int(cfg, "SimpleOutput", "ABitrate", audioBitrate->currentData().toInt());
	config_set_string(cfg, "SimpleOutput", "StreamAudioEncoder", "aac");
	config_set_string(cfg, "SimpleOutput", "RecAudioEncoder", "aac");

	config_save(cfg);
	main->ResetOutputs();
}
