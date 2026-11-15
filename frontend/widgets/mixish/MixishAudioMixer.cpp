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

#include <QDateTime>
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

/* Post-gain master meter drawn to match OBS's source meters: stereo bars,
 * green/yellow/red zones, 6 dB scale and peak-hold markers. The audio thread
 * publishes per-channel peaks; this widget polls them at 50 ms and applies
 * fast attack / slow release ballistics. */
class MixishMasterMeter : public QWidget {
public:
	explicit MixishMasterMeter(QWidget *parent) : QWidget(parent)
	{
		setObjectName(QStringLiteral("mixishMasterMeter"));
		setFixedWidth(50);
		setMinimumHeight(80);
		setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

		for (int i = 0; i < Channels; i++) {
			displayDb[i] = MasterMinDb;
			holdDb[i] = MasterMinDb;
			holdUntil[i] = 0;
		}
	}

	void SetLevels(float left, float right)
	{
		const float peaks[Channels] = {left, right};
		const qint64 now = QDateTime::currentMSecsSinceEpoch();
		bool changed = false;

		for (int i = 0; i < Channels; i++) {
			const float db = ToDb(peaks[i]);

			const float next = db > displayDb[i] ? db : std::max(db, displayDb[i] - 3.0f);
			if (std::fabs(next - displayDb[i]) >= 0.05f)
				changed = true;
			displayDb[i] = next;

			if (db >= holdDb[i]) {
				holdDb[i] = db;
				holdUntil[i] = now + 1500;
				changed = true;
			} else if (now >= holdUntil[i]) {
				const float hold = std::max(db, holdDb[i] - 3.0f);
				if (std::fabs(hold - holdDb[i]) >= 0.05f)
					changed = true;
				holdDb[i] = hold;
			}

			if (peaks[i] >= 1.0f) {
				clipped[i] = true;
				clipUntil[i] = now + 1000;
				changed = true;
			} else if (clipped[i] && now >= clipUntil[i]) {
				clipped[i] = false;
				changed = true;
			}
		}

		if (changed)
			update();
	}

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter painter(this);

		const int height = this->height();
		const int barWidth = 4;
		const int barGap = 2;
		const int barsWidth = Channels * barWidth + (Channels - 1) * barGap;
		const int tickX = barsWidth + 2;
		const int labelX = barsWidth + 5;

		QFont font = this->font();
		font.setPixelSize(8);
		painter.setFont(font);

		const int meterTop = 4;
		const int meterHeight = height - 12;

		auto yForDb = [meterTop, meterHeight](float db) {
			db = std::max(MasterMinDb, std::min(0.0f, db));
			return meterTop + int(std::lround(-db * meterHeight / -MasterMinDb));
		};

		auto barRect = [&](int x, float fromDb, float toDb) {
			const int top = yForDb(toDb);
			const int bottom = yForDb(fromDb);
			return QRect(x, top, barWidth, std::max(1, bottom - top));
		};

		const QColor bgGreen(0x14, 0x52, 0x26);
		const QColor bgYellow(0x6E, 0x52, 0x0D);
		const QColor bgRed(0x7D, 0x12, 0x24);
		const QColor fgGreen(0x37, 0xD2, 0x47);
		const QColor fgYellow(0xE8, 0xC1, 0x3A);
		const QColor fgRed(0xE3, 0x3B, 0x57);
		const QColor tickColor(0x3A, 0x3A, 0x3A);
		const QColor labelColor(0xB8, 0xB8, 0xB8);

		for (int ch = 0; ch < Channels; ch++) {
			const int x = ch * (barWidth + barGap);

			painter.fillRect(barRect(x, MasterMinDb, WarningDb), bgGreen);
			painter.fillRect(barRect(x, WarningDb, ErrorDb), bgYellow);
			painter.fillRect(barRect(x, ErrorDb, 0.0f), bgRed);

			const float level = displayDb[ch];
			if (level > MasterMinDb) {
				const float greenTop = std::min(level, WarningDb);
				if (greenTop > MasterMinDb)
					painter.fillRect(barRect(x, MasterMinDb, greenTop), fgGreen);

				if (level > WarningDb) {
					const float yellowTop = std::min(level, ErrorDb);
					painter.fillRect(barRect(x, WarningDb, yellowTop), fgYellow);
				}

				if (level > ErrorDb)
					painter.fillRect(barRect(x, ErrorDb, level), fgRed);
			}

			if (clipped[ch])
				painter.fillRect(x, 0, barWidth, 2, QColor(0xFF, 0xFF, 0xFF));

			if (holdDb[ch] > MasterMinDb + 0.5f) {
				const int y = yForDb(holdDb[ch]);
				painter.fillRect(x, y, barWidth, 1, QColor(0xF1, 0xF1, 0xF1));
			}
		}

		for (int db = 0; db >= int(MasterMinDb); db -= 6) {
			const int y = yForDb(float(db));

			painter.setPen(tickColor);
			painter.drawLine(tickX, y, tickX + 2, y);

			const int labelTop = std::max(0, std::min(height - 10, y - 5));
			painter.setPen(labelColor);
			painter.drawText(QRect(labelX, labelTop, width() - labelX, 10),
					 Qt::AlignLeft | Qt::AlignVCenter, QString::number(db));
		}
	}

private:
	static constexpr int Channels = 2;
	static constexpr float WarningDb = -20.0f;
	static constexpr float ErrorDb = -9.0f;

	static float ToDb(float peak)
	{
		if (peak <= 0.000001f)
			return MasterMinDb;

		const float db = 20.0f * std::log10(peak);
		return std::max(MasterMinDb, std::min(0.0f, db));
	}

	float displayDb[Channels];
	float holdDb[Channels];
	qint64 holdUntil[Channels];
	qint64 clipUntil[Channels] = {0, 0};
	bool clipped[Channels] = {false, false};
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

	auto *contentRow = new QHBoxLayout();
	contentRow->setContentsMargins(0, 0, 0, 0);
	contentRow->setSpacing(MixishTheme::Space2);

	scrollArea = new QScrollArea(this);
	scrollArea->setObjectName(QStringLiteral("mixishMixerScroll"));
	scrollArea->setFrameShape(QFrame::NoFrame);
	scrollArea->setWidgetResizable(true);
	scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	rowsHost = new QWidget(scrollArea);
	rowsHost->setObjectName(QStringLiteral("mixishMixerRows"));
	rowLayout = new QHBoxLayout(rowsHost);
	rowLayout->setContentsMargins(0, 0, 0, 0);
	rowLayout->setSpacing(MixishTheme::Space1);
	rowLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
	scrollArea->setWidget(rowsHost);

	contentRow->addWidget(scrollArea, 1);

	masterHost = new QWidget(this);
	masterHost->setObjectName(QStringLiteral("mixishMixerMaster"));
	masterHost->setFixedWidth(88);
	masterHost->setLayout(new QVBoxLayout(masterHost));
	masterHost->layout()->setContentsMargins(0, 0, 0, 0);
	masterHost->layout()->setSpacing(0);
	contentRow->addWidget(masterHost, 0);

	outer->addLayout(contentRow, 1);

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
	card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	auto *cardLayout = new QVBoxLayout(card);
	cardLayout->setContentsMargins(MixishTheme::Space1, 6, MixishTheme::Space1,
				       MixishTheme::Space1);
	cardLayout->setSpacing(2);

	auto *name = new QLabel(tr("MASTER"), card);
	name->setAlignment(Qt::AlignCenter);
	name->setToolTip(tr("Master output (stream and recording)"));
	cardLayout->addWidget(name);

	masterValue = new QLabel(card);
	masterValue->setObjectName(QStringLiteral("volLabel"));
	masterValue->setAlignment(Qt::AlignCenter);
	cardLayout->addWidget(masterValue);

	auto *faderRow = new QHBoxLayout();
	faderRow->setContentsMargins(0, 0, 0, 0);
	faderRow->setSpacing(2);
	faderRow->setAlignment(Qt::AlignHCenter);

	masterSlider = new QSlider(Qt::Vertical, card);
	masterSlider->setObjectName(QStringLiteral("mixishMasterSlider"));
	masterSlider->setRange(int(MasterMinDb * DbSteps), int(MasterMaxDb * DbSteps));
	masterSlider->setSingleStep(1);
	masterSlider->setPageStep(DbSteps);
	masterSlider->setValue(int(std::lround(db * DbSteps)));
	masterSlider->setFixedWidth(28);
	masterSlider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
	masterSlider->setToolTip(tr("Master gain applied to the final program mix"));
	faderRow->addWidget(masterSlider, 0);

	masterMeter = new MixishMasterMeter(card);
	faderRow->addWidget(masterMeter, 0);

	cardLayout->addLayout(faderRow, 1);

	masterMute = new QToolButton(card);
	masterMute->setObjectName(QStringLiteral("mixishMasterMute"));
	masterMute->setText(QStringLiteral("M"));
	masterMute->setCheckable(true);
	masterMute->setChecked(muted);
	masterMute->setFixedHeight(22);
	masterMute->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	masterMute->setToolTip(tr("Mute the master output (stream and recording)"));
	cardLayout->addWidget(masterMute);

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
			meter->SetLevels(obs_get_master_peak_channel(0),
					 obs_get_master_peak_channel(1));
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
		meter->SetLevels(0.0f, 0.0f);
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
		card->setFixedWidth(80);
		card->setMinimumHeight(140);
		card->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

		auto *cardLayout = new QVBoxLayout(card);
		cardLayout->setContentsMargins(MixishTheme::Space1, MixishTheme::Space1,
					       MixishTheme::Space1, MixishTheme::Space1);
		cardLayout->setSpacing(0);

		auto *control = new VolumeControl(source, card, true);
		control->setObjectName(QStringLiteral("mixishMixerRow"));
		control->setMinimumHeight(140);
		control->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
		control->setToolTip(QString::fromUtf8(obs_source_get_name(source)));
		HideCategoryLabels(control);
		cardLayout->addWidget(control);
		rowLayout->addWidget(card);
	}

	rowLayout->addStretch(1);
}
