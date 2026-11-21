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

#include "MixishTransitionPanel.hpp"
#include "MixishOverlayStrip.hpp"
#include "MixishSwitcher.hpp"
#include "MixishTBar.hpp"
#include "MixishTheme.hpp"

#include <widgets/OBSBasic.hpp>

#include <QFont>
#include <QCoreApplication>
#include <QDateTime>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLayoutItem>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "moc_MixishTransitionPanel.cpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr float MeterMinDb = -60.0f;
constexpr float MeterWarningDb = -12.0f;
constexpr float MeterErrorDb = -3.0f;

float MeterToDb(float peak)
{
	if (peak <= 0.0000001f)
		return MeterMinDb;
	const float db = 20.0f * std::log10(peak);
	return std::max(MeterMinDb, std::min(0.0f, db));
}

/* libobs only renders sources when at least one output mix is connected, so
 * the program meter needs a silent sink even if the audio mixer tab was never
 * opened. The mixer connects its own sink; either one is enough. */
void KeepProgramMixAlive(void *, size_t, struct audio_data *) {}

void EnsureProgramMixAlive()
{
	static bool connected = false;
	if (connected)
		return;

	audio_t *audio = obs_get_audio();
	if (!audio)
		return;

	connected = audio_output_connect(audio, 0, nullptr, KeepProgramMixAlive, nullptr);
}

/* Narrow vertical program level meter shown beside the transition column.
 * Stereo bars with green/yellow/red zones and a peak-hold line, drawn to
 * match OBS's source meters. */
class MixishVuMeter : public QWidget {
public:
	explicit MixishVuMeter(QWidget *parent) : QWidget(parent)
	{
		setObjectName(QStringLiteral("mixishTransitionMeter"));
		setFixedWidth(18);
		setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
		setMinimumHeight(80);

		for (int i = 0; i < Channels; i++) {
			displayDb[i] = MeterMinDb;
			holdDb[i] = MeterMinDb;
			holdUntil[i] = 0;
		}
	}

	void SetLevels(float left, float right)
	{
		const float peaks[Channels] = {left, right};
		const qint64 now = QDateTime::currentMSecsSinceEpoch();
		bool changed = false;

		for (int i = 0; i < Channels; i++) {
			const float db = MeterToDb(peaks[i]);

			/* Instant attack, gentle release. */
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
		}

		if (changed)
			update();
	}

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter painter(this);

		const int barWidth = 5;
		const int barGap = 2;
		const int barsWidth = Channels * barWidth + (Channels - 1) * barGap;
		const int barLeft = (width() - barsWidth) / 2;

		const int meterTop = 6;
		const int meterHeight = std::max(1, height() - 12);

		auto yForDb = [meterTop, meterHeight](float db) {
			db = std::max(MeterMinDb, std::min(0.0f, db));
			return meterTop + int(std::lround(-db * meterHeight / -MeterMinDb));
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
		const QColor holdColor(0xE8, 0xE8, 0xE8);

		for (int ch = 0; ch < Channels; ch++) {
			const int x = barLeft + ch * (barWidth + barGap);

			painter.fillRect(barRect(x, MeterMinDb, MeterWarningDb), bgGreen);
			painter.fillRect(barRect(x, MeterWarningDb, MeterErrorDb), bgYellow);
			painter.fillRect(barRect(x, MeterErrorDb, 0.0f), bgRed);

			const float level = displayDb[ch];
			if (level > MeterMinDb) {
				const float greenTop = std::min(level, MeterWarningDb);
				if (greenTop > MeterMinDb)
					painter.fillRect(barRect(x, MeterMinDb, greenTop), fgGreen);

				if (level > MeterWarningDb) {
					const float yellowTop = std::min(level, MeterErrorDb);
					painter.fillRect(barRect(x, MeterWarningDb, yellowTop), fgYellow);
				}

				if (level > MeterErrorDb)
					painter.fillRect(barRect(x, MeterErrorDb, level), fgRed);
			}

			if (holdDb[ch] > MeterMinDb + 0.5f) {
				const int y = yForDb(holdDb[ch]);
				painter.fillRect(QRect(x, y, barWidth, 2), holdColor);
			}
		}
	}

private:
	static constexpr int Channels = 2;

	float displayDb[Channels];
	float holdDb[Channels];
	qint64 holdUntil[Channels];
};

} // namespace

MixishTransitionPanel::MixishTransitionPanel(MixishSwitcher *switcher_, OBSBasic *main_, QWidget *parent)
	: QFrame(parent),
	  switcher(switcher_),
	  main(main_)
{
	setObjectName(QStringLiteral("mixishTransitionPanel"));
	setAttribute(Qt::WA_StyledBackground, true);
	setFixedWidth(250);

	/* The column is taller than a 768p screen once the overlay strip is
	 * added. Keeping the content inside a scroll area gives the panel a
	 * small minimum height so the monitor area and the bottom bar always
	 * fit; on large screens nothing scrolls. */
	auto *panelLayout = new QHBoxLayout(this);
	panelLayout->setContentsMargins(0, 0, 0, 0);
	panelLayout->setSpacing(0);

	/* The transition controls keep their fixed width; the program VU meter
	 * is a narrow strip beside them so the operator can read the audio level
	 * without opening the mixer. */
	auto *columnHost = new QWidget(this);
	columnHost->setObjectName(QStringLiteral("mixishTransitionColumn"));
	columnHost->setFixedWidth(232);
	auto *columnLayout = new QVBoxLayout(columnHost);
	columnLayout->setContentsMargins(0, 0, 0, 0);
	columnLayout->setSpacing(0);
	panelLayout->addWidget(columnHost);

	auto *vuMeter = new MixishVuMeter(this);
	vuMeter->setToolTip(tr("Program audio level"));
	panelLayout->addWidget(vuMeter);

	auto *meterTimer = new QTimer(this);
	meterTimer->setInterval(50);
	connect(meterTimer, &QTimer::timeout, this, [vuMeter]() {
		EnsureProgramMixAlive();
		vuMeter->SetLevels(obs_get_master_peak_channel(0),
				    obs_get_master_peak_channel(1));
	});
	meterTimer->start();

	auto *panelScroll = new QScrollArea(this);
	panelScroll->setObjectName(QStringLiteral("mixishTransitionScroll"));
	panelScroll->setFrameShape(QFrame::NoFrame);
	panelScroll->setWidgetResizable(true);
	panelScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	panelScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	transitionScroll = panelScroll;

	auto *content = new QWidget(panelScroll);
	content->setObjectName(QStringLiteral("mixishTransitionContent"));
	panelScroll->setWidget(content);
	columnLayout->addWidget(panelScroll);

	/* The manual T-bar is pinned below the scrolling transition list so it
	 * is always visible and reachable at 1366x768. */
	auto *tBarHost = new QWidget(this);
	tBarHost->setObjectName(QStringLiteral("mixishTBarHost"));
	auto *tBarLayout = new QVBoxLayout(tBarHost);
	tBarLayout->setContentsMargins(MixishTheme::Space2, MixishTheme::Space1,
				       MixishTheme::Space2, 0);
	tBarLayout->setSpacing(0);
	tBar = new MixishTBar(tBarHost);
	tBarLayout->addWidget(tBar);
	columnLayout->addWidget(tBarHost);

	auto *outer = new QVBoxLayout(content);
	outer->setContentsMargins(MixishTheme::Space2, MixishTheme::Space2,
				  MixishTheme::Space2, MixishTheme::Space2);
	outer->setSpacing(MixishTheme::Space1);

	auto *headerRow = new QHBoxLayout();
	headerRow->setSpacing(MixishTheme::Space1);

	auto *title = new QLabel(tr("TRANSITIONS"), this);
	MixishTheme::MakeSectionLabel(title);

	auto *addButton = new QPushButton(this);
	addButton->setObjectName(QStringLiteral("mixishAddTransition"));
	addButton->setIcon(QIcon(QStringLiteral(":/res/images/plus.svg")));
	MixishTheme::SetVariant(addButton, "icon");
	addButton->setFixedSize(MixishTheme::IconButtonSize, MixishTheme::IconButtonSize);
	addButton->setToolTip(tr("Add transition"));
	addButton->setAccessibleName(tr("Add transition"));

	auto *configureButton = new QPushButton(this);
	configureButton->setObjectName(QStringLiteral("mixishConfigureTransition"));
	configureButton->setIcon(QIcon(QStringLiteral(":/res/images/cogs.svg")));
	MixishTheme::SetVariant(configureButton, "icon");
	configureButton->setFixedSize(MixishTheme::IconButtonSize, MixishTheme::IconButtonSize);
	configureButton->setToolTip(tr("Configure current transition"));
	configureButton->setAccessibleName(tr("Configure current transition"));

	headerRow->addWidget(title);
	headerRow->addStretch(1);
	headerRow->addWidget(addButton);
	headerRow->addWidget(configureButton);
	outer->addLayout(headerRow);

	cutButton = new QPushButton(tr("CUT"), this);
	cutButton->setObjectName(QStringLiteral("mixishCutButton"));
	MixishTheme::SetVariant(cutButton, "secondary");
	cutButton->setFixedHeight(MixishTheme::ControlHeight);
	cutButton->setToolTip(tr("Cut Preview to Program"));
	outer->addWidget(cutButton);

	fadeButton = new QPushButton(tr("FADE"), this);
	fadeButton->setObjectName(QStringLiteral("mixishFadeButton"));
	MixishTheme::SetVariant(fadeButton, "secondary");
	fadeButton->setFixedHeight(MixishTheme::ControlHeight);
	fadeButton->setToolTip(tr("Fade Preview to Program"));
	outer->addWidget(fadeButton);

	auto *scrollArea = new QScrollArea(this);
	customScroll = scrollArea;
	scrollArea->setObjectName(QStringLiteral("mixishTransitionList"));
	scrollArea->setFrameShape(QFrame::NoFrame);
	scrollArea->setWidgetResizable(true);
	scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	scrollArea->setMinimumHeight(0);

	auto *customWidget = new QWidget(scrollArea);
	customWidget->setObjectName(QStringLiteral("mixishTransitionListContent"));
	customLayout = new QVBoxLayout(customWidget);
	customLayout->setContentsMargins(0, 0, 0, 0);
	customLayout->setSpacing(MixishTheme::Space1);
	customLayout->setAlignment(Qt::AlignTop);
	scrollArea->setWidget(customWidget);
	outer->addWidget(scrollArea);

	ftbButton = new QPushButton(tr("FTB"), this);
	ftbButton->setObjectName(QStringLiteral("mixishFtbButton"));
	MixishTheme::SetVariant(ftbButton, "secondary");
	ftbButton->setFixedHeight(MixishTheme::ControlHeight);
	ftbButton->setToolTip(tr("Fade to Black"));
	outer->addWidget(ftbButton);

	/* Overlay channel strip is pinned to the bottom of the column so the
	 * overlay keys stay reachable even when the transition list scrolls. */
	auto *overlayStrip = new MixishOverlayStrip(switcher, this);
	columnLayout->addWidget(overlayStrip);

	auto *presetRow = new QHBoxLayout();
	presetRow->setSpacing(MixishTheme::Space1);

	const int presetValues[] = {250, 500, 1000, 2000};
	for (const int value : presetValues) {
		auto *preset = new QPushButton(QString::number(value), this);
		MixishTheme::SetVariant(preset, "secondary");
		MixishTheme::SetRole(preset, "preset");
		preset->setFixedHeight(MixishTheme::CompactHeight);
		preset->setToolTip(tr("Set transition duration to %1 ms").arg(value));
		connect(preset, &QPushButton::clicked, this, [this, value]() { switcher->SetTransitionDuration(value); });
		presetRow->addWidget(preset);
		durationButtons.push_back(preset);
	}
	outer->addLayout(presetRow);

	durationSpin = new QSpinBox(this);
	MixishTheme::SetControl(durationSpin);
	durationSpin->setRange(50, 20000);
	durationSpin->setSingleStep(50);
	durationSpin->setValue(switcher->TransitionDuration());
	durationSpin->setFixedWidth(62);
	durationSpin->setFixedHeight(MixishTheme::CompactHeight);
	durationSpin->setToolTip(tr("Transition duration in milliseconds"));
	headerRow->insertWidget(2, durationSpin);

	outer->addStretch(1);

	connect(cutButton, &QPushButton::clicked, this, &MixishTransitionPanel::OnCutClicked);
	connect(fadeButton, &QPushButton::clicked, this, &MixishTransitionPanel::OnFadeClicked);
	connect(ftbButton, &QPushButton::clicked, this, &MixishTransitionPanel::OnFtbClicked);
	connect(addButton, &QPushButton::clicked, this, &MixishTransitionPanel::OnAddClicked);
	connect(configureButton, &QPushButton::clicked, this, &MixishTransitionPanel::OnConfigureClicked);
	connect(durationSpin, &QSpinBox::valueChanged, this, &MixishTransitionPanel::OnDurationChanged);
	connect(tBar, &MixishTBar::valueChanged, this, &MixishTransitionPanel::OnTBarChanged);
	connect(tBar, &MixishTBar::dragFinished, this, &MixishTransitionPanel::OnTBarDragFinished);
	connect(tBar, &MixishTBar::reachedEnd, this, &MixishTransitionPanel::OnTBarReachedEnd);
	connect(tBar, &MixishTBar::cancelRequested, this, &MixishTransitionPanel::OnTBarCancel);
	connect(tBar, &MixishTBar::animationFinished, this,
		&MixishTransitionPanel::OnTBarAnimationFinished);

	connect(main, &OBSBasic::CurrentTransitionChanged, this, &MixishTransitionPanel::OnCurrentTransitionChanged);
	connect(main, &OBSBasic::TransitionAdded, this, &MixishTransitionPanel::OnTransitionsChanged);
	connect(main, &OBSBasic::TransitionRemoved, this, &MixishTransitionPanel::OnTransitionsChanged);
	connect(main, &OBSBasic::TransitionRenamed, this, &MixishTransitionPanel::OnTransitionsChanged);
	connect(main, &OBSBasic::TransitionsCleared, this, &MixishTransitionPanel::OnTransitionsChanged);
	connect(main, &OBSBasic::TransitionDurationChanged, this, &MixishTransitionPanel::OnDurationChangedExternally);
	connect(main, &OBSBasic::PreviewProgramModeChanged, this, &MixishTransitionPanel::OnTransitionsChanged);

	tickTimer = new QTimer(this);
	tickTimer->setInterval(150);
	connect(tickTimer, &QTimer::timeout, this, &MixishTransitionPanel::Tick);

	progressTimer = new QTimer(this);
	progressTimer->setInterval(33);
	connect(progressTimer, &QTimer::timeout, this,
		&MixishTransitionPanel::UpdateTransitionProgress);

	connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
		if (tickTimer)
			tickTimer->stop();
		if (progressTimer)
			progressTimer->stop();
	});
	tickTimer->start();

	MixishTheme::Apply(this);

	RefreshTransitions();
}

void MixishTransitionPanel::showEvent(QShowEvent *event)
{
	QFrame::showEvent(event);

	/* At 768p the column scrolls; always open on Cut/Fade rather than on a
	 * previously scrolled part of the list. */
	if (transitionScroll)
		transitionScroll->verticalScrollBar()->setValue(0);
}

void MixishTransitionPanel::RefreshTransitions()
{
	const QString cutUuid = switcher->CutTransitionUuid();
	const QString fadeUuid = switcher->FadeTransitionUuid();

	if (fadeButton) {
		fadeButton->setProperty("mixishActive", false);
		fadeButton->setEnabled(!fadeUuid.isEmpty());
	}

	ClearTransitionButtons();

	const QStringList uuids = switcher->TransitionUuids();
	for (const QString &uuid : uuids) {
		if (uuid == cutUuid || uuid == fadeUuid)
			continue;

		const QString name = switcher->TransitionName(uuid);
		AddTransitionButton(uuid, name.isEmpty() ? tr("Transition") : name);
	}

	const int count = transitionButtons.size();
	const int buttonHeight = MixishTheme::ControlHeight;
	const int spacing = MixishTheme::Space1;
	int height = count > 0 ? count * buttonHeight + (count - 1) * spacing : 0;

	if (customScroll) {
		/* The outer column scrolls; this list must not create a second
		 * nested scrollbar. Size it to its content. */
		customScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		customScroll->setFixedHeight(height);
		customScroll->setVisible(count > 0);
	}

	UpdateActiveState();
	UpdateDurationPresets();
	UpdateFtbState();
}

void MixishTransitionPanel::ClearTransitionButtons()
{
	if (!customLayout)
		return;

	transitionButtons.clear();

	while (QLayoutItem *item = customLayout->takeAt(0)) {
		if (QWidget *widget = item->widget())
			widget->deleteLater();
		delete item;
	}
}

void MixishTransitionPanel::AddTransitionButton(const QString &uuid, const QString &name)
{
	if (!customLayout)
		return;

	auto *button = new QPushButton(name, customLayout->parentWidget());
	MixishTheme::SetVariant(button, "secondary");
	button->setFixedHeight(MixishTheme::ControlHeight);
	button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	button->setToolTip(tr("Transition to Program using %1").arg(name));
	connect(button, &QPushButton::clicked, this, [this, uuid]() {
		BeginTransitionFollow();
		switcher->TriggerTransition(uuid);
	});

	customLayout->addWidget(button);
	transitionButtons.insert(uuid, button);
}

void MixishTransitionPanel::SetButtonActive(QPushButton *button, bool active)
{
	if (!button)
		return;

	MixishTheme::SetState(button, active ? "selected" : "idle");
}

void MixishTransitionPanel::UpdateActiveState()
{
	const QString current = switcher->CurrentTransitionUuid();
	const QString fadeUuid = switcher->FadeTransitionUuid();

	SetButtonActive(fadeButton, !fadeUuid.isEmpty() && current == fadeUuid);

	for (auto it = transitionButtons.constBegin(); it != transitionButtons.constEnd(); ++it)
		SetButtonActive(it.value(), it.key() == current);
}

void MixishTransitionPanel::UpdateFtbState()
{
	SetButtonActive(ftbButton, switcher->FadeToBlackActive());
}

void MixishTransitionPanel::UpdateDurationPresets()
{
	const int current = switcher->TransitionDuration();

	for (const QPointer<QPushButton> &button : durationButtons) {
		if (!button)
			continue;

		bool ok = false;
		const int value = button->text().toInt(&ok);
		SetButtonActive(button, ok && value == current);
	}
}

void MixishTransitionPanel::UpdateBusyState()
{
	if (!switcher || !main || main->isClosing())
		return;

	OBSSourceAutoRelease transition = obs_get_output_source(0);
	const bool transitioning = transition && obs_transition_is_active(transition);
	const bool enabled = !transitioning;

	if (cutButton)
		cutButton->setEnabled(enabled);
	if (fadeButton)
		fadeButton->setEnabled(enabled && !switcher->FadeTransitionUuid().isEmpty());
	if (ftbButton)
		ftbButton->setEnabled(enabled);

	for (auto it = transitionButtons.constBegin(); it != transitionButtons.constEnd(); ++it) {
		if (it.value())
			it.value()->setEnabled(enabled);
	}

	if (tBar) {
		const bool usable = main->IsPreviewProgramMode();
		if (tBar->isEnabled() != usable)
			tBar->setEnabled(usable);
	}

	if (tBar && !followingTransition && !committingTBar && !tBar->isDragging() &&
	    !tBar->isAnimating()) {
		if (!transitioning && !switcher->TBarActive()) {
			if (tBar->value() != 0)
				tBar->resetToStart();
			switcher->ResetTBarIfIdle();
		} else if (switcher->TBarActive()) {
			const int position = switcher->TBarPosition();
			if (tBar->value() != position)
				tBar->setValue(position);
		}
	}
}

void MixishTransitionPanel::Tick()
{
	if (!main || main->isClosing())
		return;

	UpdateBusyState();
	UpdateActiveState();
	UpdateFtbState();

	/* Transitions triggered from a hotkey or another OBS window still get
	 * the handle animation; button clicks start following immediately.
	 * A finished manual transition reports time 0.0 or 1.0 while its
	 * source is still active, so only follow real progress. */
	if (!followingTransition && !committingTBar && tBar && !tBar->isDragging() &&
	    !tBar->isAnimating() &&
	    QDateTime::currentMSecsSinceEpoch() >= followSuppressedUntil) {
		OBSSourceAutoRelease transition = obs_get_output_source(0);
		if (transition && obs_transition_is_active(transition) &&
		    !switcher->TBarActive()) {
			const float time = obs_transition_get_time(transition);
			if (time > 0.001f && time < 0.999f)
				BeginTransitionFollow();
		}
	}
}

void MixishTransitionPanel::OnCutClicked()
{
	switcher->CutPreviewToProgram();
	if (tBar)
		tBar->playPulse();
}

void MixishTransitionPanel::OnFadeClicked()
{
	const QString fadeUuid = switcher->FadeTransitionUuid();
	if (fadeUuid.isEmpty())
		return;

	BeginTransitionFollow();
	switcher->TriggerTransition(fadeUuid);
}

void MixishTransitionPanel::OnFtbClicked()
{
	BeginTransitionFollow();
	switcher->ToggleFadeToBlack();
}

void MixishTransitionPanel::OnAddClicked()
{
	switcher->AddTransition();
}

void MixishTransitionPanel::OnConfigureClicked()
{
	switcher->EditCurrentTransition();
}

void MixishTransitionPanel::OnDurationChanged(int ms)
{
	switcher->SetTransitionDuration(ms);
	UpdateDurationPresets();
}

void MixishTransitionPanel::OnDurationChangedExternally(int ms)
{
	if (!durationSpin)
		return;

	QSignalBlocker block(durationSpin);
	durationSpin->setValue(ms);
	UpdateDurationPresets();
}

void MixishTransitionPanel::OnTBarChanged(int value)
{
	if (switcher)
		switcher->SetTBarPosition(value);
}

void MixishTransitionPanel::OnTBarDragFinished(int value)
{
	if (!tBar || followingTransition || committingTBar)
		return;

	/* Release never leaves a manual transition half-applied: snap to the
	 * nearest end and complete or cancel it. */
	CompleteTBar(value >= tBar->maximum() / 2);
}

void MixishTransitionPanel::OnTBarReachedEnd(bool programSide)
{
	if (followingTransition || committingTBar)
		return;

	CompleteTBar(programSide);
}

void MixishTransitionPanel::OnTBarCancel()
{
	if (followingTransition || committingTBar)
		return;

	CompleteTBar(false);
}

void MixishTransitionPanel::OnTBarAnimationFinished()
{
	if (committingTBar)
		FinishTBarCommit();
}

void MixishTransitionPanel::CompleteTBar(bool toProgram)
{
	if (!tBar || committingTBar || followingTransition)
		return;

	const int target = toProgram ? tBar->maximum() : 0;

	if (!toProgram && tBar->value() == 0 && !switcher->TBarActive()) {
		tBar->resetToStart();
		return;
	}

	committingTBar = true;
	commitToProgram = toProgram;
	tBar->setLocked(true);

	if (tBar->value() != target)
		tBar->animateTo(target, 130, true);
	else
		FinishTBarCommit();
}

void MixishTransitionPanel::FinishTBarCommit()
{
	if (!tBar)
		return;

	const bool toProgram = commitToProgram;
	committingTBar = false;

	if (toProgram) {
		switcher->SetTBarPosition(tBar->maximum());
		switcher->ReleaseTBar();
	} else if (switcher->TBarActive() || tBar->value() > 0) {
		switcher->SetTBarPosition(0);
		switcher->ReleaseTBar();
	}

	tBar->resetToStart();
	tBar->setLocked(false);
	followSuppressedUntil = QDateTime::currentMSecsSinceEpoch() + 700;
	UpdateBusyState();
}

void MixishTransitionPanel::BeginTransitionFollow()
{
	if (!tBar || followingTransition)
		return;

	followingTransition = true;
	sawTransitionProgress = false;
	followTicks = 0;
	tBar->setLocked(true);
	/* Move focus off the clicked transition button before it is disabled;
	 * otherwise Qt walks the focus chain and scrolls the list. */
	tBar->setFocus(Qt::OtherFocusReason);
	if (progressTimer)
		progressTimer->start();
}

void MixishTransitionPanel::EndTransitionFollow()
{
	if (progressTimer)
		progressTimer->stop();

	followingTransition = false;
	sawTransitionProgress = false;
	followTicks = 0;

	if (tBar) {
		tBar->setLocked(false);
		tBar->resetToStart();
	}
	followSuppressedUntil = QDateTime::currentMSecsSinceEpoch() + 400;
	UpdateBusyState();
}

void MixishTransitionPanel::UpdateTransitionProgress()
{
	if (!followingTransition)
		return;

	if (!main || main->isClosing()) {
		EndTransitionFollow();
		return;
	}

	OBSSourceAutoRelease transition = obs_get_output_source(0);
	if (transition && obs_transition_is_active(transition)) {
		const float time = std::clamp(obs_transition_get_time(transition), 0.0f, 1.0f);
		tBar->setExternalProgress(time);
		sawTransitionProgress = true;
		followTicks = 0;
		return;
	}

	/* Give the transition a tick to register, then unlock. Cut has no
	 * active period, so it uses the visual pulse instead. */
	if (sawTransitionProgress || ++followTicks > 12)
		EndTransitionFollow();
}

void MixishTransitionPanel::OnCurrentTransitionChanged(const QString &)
{
	UpdateActiveState();
}

void MixishTransitionPanel::OnTransitionsChanged()
{
	RefreshTransitions();
}
