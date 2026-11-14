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

#include "MixishBottomBar.hpp"
#include "MixishSwitcher.hpp"
#include "MixishTheme.hpp"

#include <obs-frontend-api.h>

#include <QAction>
#include <QHBoxLayout>
#include <QIcon>
#include <QMenu>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolButton>

#include "moc_MixishBottomBar.cpp"

#include <algorithm>

MixishBottomBar::MixishBottomBar(MixishSwitcher *switcher_, QWidget *parent)
	: QFrame(parent), switcher(switcher_)
{
	setObjectName(QStringLiteral("mixishBottomBar"));
	setFixedHeight(MixishTheme::BottomBarHeight);

	auto *layout = new QHBoxLayout(this);
	layout->setContentsMargins(MixishTheme::Space3, MixishTheme::Space1,
				   MixishTheme::Space3, MixishTheme::Space1);
	layout->setSpacing(MixishTheme::Space1);

	addInputButton = new QToolButton(this);
	addInputButton->setObjectName(QStringLiteral("mixishAddInputButton"));
	MixishTheme::SetVariant(addInputButton, "secondary");
	addInputButton->setText(tr("Add Input"));
	addInputButton->setPopupMode(QToolButton::MenuButtonPopup);
	addInputButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	addInputButton->setFixedHeight(MixishTheme::ControlHeight);
	addInputButton->setMinimumWidth(120);
	addInputButton->setFocusPolicy(Qt::NoFocus);
	addInputButton->setToolTip(tr("Add a new input, or open the menu for a legacy scene"));

	addInputMenu = new QMenu(addInputButton);
	addInputMenu->addAction(tr("Add Input…"), this, [this]() { switcher->AddInput(); });
	addInputMenu->addAction(tr("Add Scene (legacy)"), this, [this]() { switcher->AddScene(); });
	addInputButton->setMenu(addInputMenu);
	connect(addInputButton, &QToolButton::clicked, this, [this]() { switcher->AddInput(); });
	layout->addWidget(addInputButton);

	layout->addStretch(1);

	/* Record group: the gear configures the button immediately next to it. */
	auto *recordGroup = new QHBoxLayout();
	recordGroup->setSpacing(MixishTheme::Space1);

	recordSettingsButton = new QToolButton(this);
	recordSettingsButton->setObjectName(QStringLiteral("mixishRecordSettings"));
	MixishTheme::SetVariant(recordSettingsButton, "icon");
	recordSettingsButton->setIcon(QIcon(QStringLiteral(":/res/images/cogs.svg")));
	recordSettingsButton->setIconSize(QSize(16, 16));
	recordSettingsButton->setFixedSize(MixishTheme::ControlHeight, MixishTheme::ControlHeight);
	recordSettingsButton->setFocusPolicy(Qt::NoFocus);
	recordSettingsButton->setToolTip(tr("Recording settings"));
	connect(recordSettingsButton, &QToolButton::clicked, this,
		[this]() { switcher->OpenRecordingSettings(); });
	recordGroup->addWidget(recordSettingsButton);

	recordButton = new QPushButton(tr("Record"), this);
	recordButton->setObjectName(QStringLiteral("mixishRecordButton"));
	MixishTheme::SetVariant(recordButton, "secondary");
	recordButton->setFixedHeight(MixishTheme::ControlHeight);
	recordButton->setMinimumWidth(128);
	recordButton->setFocusPolicy(Qt::NoFocus);
	recordButton->setToolTip(tr("Start or stop recording"));
	connect(recordButton, &QPushButton::clicked, this, [this]() {
		if (obs_frontend_recording_active())
			obs_frontend_recording_stop();
		else
			obs_frontend_recording_start();
		Refresh();
	});
	recordGroup->addWidget(recordButton);
	layout->addLayout(recordGroup);

	layout->addSpacing(MixishTheme::Space4);

	/* Stream group. */
	auto *streamGroup = new QHBoxLayout();
	streamGroup->setSpacing(MixishTheme::Space1);

	streamSettingsButton = new QToolButton(this);
	streamSettingsButton->setObjectName(QStringLiteral("mixishStreamSettings"));
	MixishTheme::SetVariant(streamSettingsButton, "icon");
	streamSettingsButton->setIcon(QIcon(QStringLiteral(":/res/images/cogs.svg")));
	streamSettingsButton->setIconSize(QSize(16, 16));
	streamSettingsButton->setFixedSize(MixishTheme::ControlHeight, MixishTheme::ControlHeight);
	streamSettingsButton->setFocusPolicy(Qt::NoFocus);
	streamSettingsButton->setToolTip(tr("Stream settings"));
	connect(streamSettingsButton, &QToolButton::clicked, this,
		[this]() { switcher->OpenStreamingSettings(); });
	streamGroup->addWidget(streamSettingsButton);

	streamButton = new QToolButton(this);
	streamButton->setObjectName(QStringLiteral("mixishStreamButton"));
	MixishTheme::SetVariant(streamButton, "secondary");
	streamButton->setText(tr("Stream"));
	streamButton->setPopupMode(QToolButton::MenuButtonPopup);
	streamButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	streamButton->setFixedHeight(MixishTheme::ControlHeight);
	streamButton->setMinimumWidth(128);
	streamButton->setFocusPolicy(Qt::NoFocus);
	streamButton->setToolTip(tr("Start or stop streaming"));

	streamMenu = new QMenu(streamButton);
	streamSettingsAction = streamMenu->addAction(tr("Stream Settings…"));
	connect(streamSettingsAction, &QAction::triggered, this,
		[this]() { switcher->OpenOutputSettings(); });
	streamMenu->addSeparator();
	stopStreamAction = streamMenu->addAction(tr("Stop Streaming"));
	stopStreamAction->setEnabled(false);
	connect(stopStreamAction, &QAction::triggered, this, [this]() {
		if (obs_frontend_streaming_active())
			obs_frontend_streaming_stop();
		Refresh();
	});
	streamButton->setMenu(streamMenu);

	connect(streamButton, &QToolButton::clicked, this, [this]() {
		if (obs_frontend_streaming_active())
			obs_frontend_streaming_stop();
		else
			obs_frontend_streaming_start();
		Refresh();
	});
	streamGroup->addWidget(streamButton);
	layout->addLayout(streamGroup);

	stateTimer = new QTimer(this);
	stateTimer->setInterval(500);
	connect(stateTimer, &QTimer::timeout, this, &MixishBottomBar::Refresh);
	stateTimer->start();

	MixishTheme::Apply(this);

	Refresh();
}

QString MixishBottomBar::FormatDuration(qint64 ms)
{
	const qint64 totalSeconds = std::max<qint64>(0, ms / 1000);
	const qint64 hours = totalSeconds / 3600;
	const qint64 minutes = (totalSeconds / 60) % 60;
	const qint64 seconds = totalSeconds % 60;

	return QStringLiteral("%1:%2:%3")
		.arg(hours, 2, 10, QLatin1Char('0'))
		.arg(minutes, 2, 10, QLatin1Char('0'))
		.arg(seconds, 2, 10, QLatin1Char('0'));
}

void MixishBottomBar::Refresh()
{
	const bool recording = obs_frontend_recording_active();
	if (recording && !recordClock.isValid())
		recordClock.start();
	if (!recording)
		recordClock.invalidate();
	recordActive = recording;

	const bool streaming = obs_frontend_streaming_active();
	if (streaming && !streamClock.isValid())
		streamClock.start();
	if (!streaming)
		streamClock.invalidate();
	streamActive = streaming;

	if (recordButton) {
		recordButton->setText(recording ? tr("REC  %1").arg(FormatDuration(recordClock.elapsed()))
						: tr("Record"));
		MixishTheme::SetState(recordButton, recording ? "onAir" : "idle");
	}

	if (streamButton) {
		streamButton->setText(streaming ? tr("LIVE  %1").arg(FormatDuration(streamClock.elapsed()))
						: tr("Stream"));
		MixishTheme::SetState(streamButton, streaming ? "onAir" : "idle");
	}

	if (stopStreamAction) {
		QSignalBlocker block(stopStreamAction);
		stopStreamAction->setEnabled(streamActive);
	}
}
