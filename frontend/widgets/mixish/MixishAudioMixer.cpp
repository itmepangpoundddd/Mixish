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

#include <OBSApp.hpp>
#include <util/config-file.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QScrollArea>
#include <QShowEvent>
#include <QSlider>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "moc_MixishAudioMixer.cpp"

namespace {

constexpr int DbSteps = 10;
constexpr float MasterMinDb = -60.0f;
constexpr float MasterMaxDb = 6.0f;

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

bool ScanSource(void *param, obs_source_t *source)
{
	auto *sources = static_cast<std::vector<obs_source_t *> *>(param);
	if (IsAudioInput(source))
		sources->push_back(source);
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

QString FormatDb(float db)
{
	if (db <= MasterMinDb + 0.05f)
		return QStringLiteral("-∞ dB");
	QString text = QString::number(db, 'f', 1);
	if (db > 0.0f)
		text.prepend(QLatin1Char('+'));
	return text + QStringLiteral(" dB");
}

/* libobs only renders sources when at least one output mix is connected.
 * This silent sink keeps the program mix running so the master meter and the
 * per-input meters stay live while nothing is recording or streaming. */
void KeepProgramMixAlive(void *, size_t, struct audio_data *) {}

} // namespace

/* Horizontal peak meter for the master bus. The audio thread updates a
 * single float, so the widget polls it and applies a fast attack / slow
 * release ballistics of its own. */
class MixishMasterMeter : public QWidget {
public:
	explicit MixishMasterMeter(QWidget *parent) : QWidget(parent)
	{
		setObjectName(QStringLiteral("mixishMasterMeter"));
		setFixedSize(64, 8);
	}

	void SetLevel(float peak)
	{
		float db = MasterMinDb;
		if (peak > 0.000001f) {
			db = 20.0f * std::log10(peak);
			db = std::max(MasterMinDb, std::min(0.0f, db));
		}

		displayDb = db > displayDb ? db : std::max(db, displayDb - 3.0f);
		if (std::fabs(displayDb - lastPainted) < 0.05f)
			return;

		lastPainted = displayDb;
		update();
	}

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setPen(Qt::NoPen);

		const QRectF area = QRectF(rect()).adjusted(0.0, 0.0, -1.0, -1.0);
		painter.setBrush(QColor(0x2A, 0x2A, 0x2A));
		painter.drawRoundedRect(area, 2.0, 2.0);

		const float fraction = (displayDb - MasterMinDb) / -MasterMinDb;
		if (fraction <= 0.0f)
			return;

		QColor color(0x37, 0xC8, 0x6B);
		if (displayDb > -1.0f)
			color = QColor(0xE7, 0x4C, 0x3C);
		else if (displayDb > -8.0f)
			color = QColor(0xE6, 0xB8, 0x3C);

		QRectF fill = area;
		fill.setWidth(area.width() * std::min(1.0f, fraction));
		painter.setBrush(color);
		painter.drawRoundedRect(fill, 2.0, 2.0);
	}

private:
	float displayDb = MasterMinDb;
	float lastPainted = MasterMinDb - 1.0f;
};

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

	BuildMaster();
	Refresh();
}

void MixishAudioMixer::showEvent(QShowEvent *event)
{
	QFrame::showEvent(event);
	Refresh();
}

void MixishAudioMixer::Refresh()
{
	EnsureProgramMixAlive();
	RebuildRows();
}

void MixishAudioMixer::EnsureProgramMixAlive()
{
	if (keepAliveConnected)
		return;

	audio_t *audio = obs_get_audio();
	if (!audio)
		return;

	keepAliveConnected = audio_output_connect(audio, 0, nullptr, KeepProgramMixAlive, nullptr);
	if (keepAliveConnected)
		blog(LOG_INFO, "Mixish: program mix keepalive connected");
}

void MixishAudioMixer::BuildMaster()
{
	if (masterBuilt || !masterHost || !masterHost->layout())
		return;

	masterBuilt = true;

	config_t *config = App()->GetUserConfig();
	float db = float(config_get_double(config, "Mixish", "MasterVolumeDb"));
	db = std::max(MasterMinDb, std::min(MasterMaxDb, db));
	const bool muted = config_get_bool(config, "Mixish", "MasterMuted");

	auto *card = new QFrame(masterHost);
	card->setObjectName(QStringLiteral("mixishMasterCard"));
	auto *cardLayout = new QHBoxLayout(card);
	cardLayout->setContentsMargins(MixishTheme::Space1, MixishTheme::Space1,
				       MixishTheme::Space1, MixishTheme::Space1);
	cardLayout->setSpacing(MixishTheme::Space2);

	masterMute = new QToolButton(card);
	masterMute->setObjectName(QStringLiteral("mixishMasterMute"));
	masterMute->setText(QStringLiteral("M"));
	masterMute->setCheckable(true);
	masterMute->setChecked(muted);
	masterMute->setFixedSize(24, 24);
	masterMute->setToolTip(tr("Mute the master output (stream and recording)"));
	cardLayout->addWidget(masterMute);

	masterSlider = new QSlider(Qt::Horizontal, card);
	masterSlider->setObjectName(QStringLiteral("mixishMasterSlider"));
	masterSlider->setRange(int(MasterMinDb * DbSteps), int(MasterMaxDb * DbSteps));
	masterSlider->setSingleStep(1);
	masterSlider->setPageStep(DbSteps);
	masterSlider->setValue(int(std::lround(db * DbSteps)));
	masterSlider->setToolTip(tr("Master gain applied to the final program mix"));
	cardLayout->addWidget(masterSlider, 1);

	masterValue = new QLabel(card);
	masterValue->setObjectName(QStringLiteral("volLabel"));
	masterValue->setMinimumWidth(58);
	masterValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	cardLayout->addWidget(masterValue);

	masterMeter = new MixishMasterMeter(card);
	cardLayout->addWidget(masterMeter);

	masterHost->layout()->addWidget(card);

	connect(masterSlider, &QSlider::valueChanged, this, [this](int) {
		UpdateMasterGain();
		if (!masterSlider->isSliderDown())
			SaveMasterConfig();
	});
	connect(masterSlider, &QSlider::sliderReleased, this,
		&MixishAudioMixer::SaveMasterConfig);
	connect(masterMute, &QToolButton::toggled, this, [this](bool) {
		UpdateMasterGain();
		SaveMasterConfig();
	});

	auto *meterTimer = new QTimer(this);
	meterTimer->setInterval(50);
	connect(meterTimer, &QTimer::timeout, this, [this]() {
		auto *meter = static_cast<MixishMasterMeter *>(masterMeter.data());
		if (meter)
			meter->SetLevel(obs_get_master_peak());
	});
	meterTimer->start();

	UpdateMasterGain();
}

void MixishAudioMixer::UpdateMasterGain()
{
	if (!masterSlider)
		return;

	lastMasterDb = float(masterSlider->value()) / DbSteps;
	const bool muted = masterMute && masterMute->isChecked();
	obs_set_master_volume_db(muted ? -96.0f : lastMasterDb);

	if (masterValue)
		masterValue->setText(FormatDb(lastMasterDb));

	if (auto *meter = static_cast<MixishMasterMeter *>(masterMeter.data()))
		meter->SetLevel(0.0f);
}

void MixishAudioMixer::SaveMasterConfig()
{
	config_t *config = App()->GetUserConfig();
	config_set_double(config, "Mixish", "MasterVolumeDb", double(lastMasterDb));
	config_set_bool(config, "Mixish", "MasterMuted", masterMute && masterMute->isChecked());
	config_save_safe(config, "tmp", nullptr);
}

void MixishAudioMixer::RebuildRows()
{
	if (!rowLayout || !rowsHost)
		return;

	std::vector<obs_source_t *> sources;
	obs_enum_sources(ScanSource, &sources);

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
