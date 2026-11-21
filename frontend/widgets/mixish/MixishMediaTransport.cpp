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

#include "MixishMediaTransport.hpp"
#include "MixishTheme.hpp"

#include <OBSApp.hpp>
#include <components/MediaControls.hpp>
#include <util/config-file.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSizePolicy>
#include <QTimer>

#include "moc_MixishMediaTransport.cpp"

MixishMediaTransport::MixishMediaTransport(QWidget *parent) : QFrame(parent)
{
	setObjectName(QStringLiteral("mixishMediaTransport"));
	setMinimumHeight(28);

	auto *layout = new QHBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(MixishTheme::Space1);

	controls = new MediaControls(this);
	controls->setObjectName(QStringLiteral("mixishTransport"));
	controls->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	controls->setMinimumWidth(220);
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
	seekSlider = seek;
	if (seek) {
		seek->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		seek->setMinimumWidth(120);
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

	previousButton = controls->findChild<QPushButton *>(QStringLiteral("previousButton"));
	nextButton = controls->findChild<QPushButton *>(QStringLiteral("nextButton"));

	/* The timecode and the seek bar turn into the warning colour in the last
	 * seconds of a clip, so the operator sees an item about to end without
	 * watching the monitor. */
	timerLabel = controls->findChild<QLabel *>(QStringLiteral("timerLabel"));
	durationLabel = controls->findChild<QLabel *>(QStringLiteral("durationLabel"));

	endingTimer = new QTimer(this);
	endingTimer->setInterval(200);
	connect(endingTimer, &QTimer::timeout, this, &MixishMediaTransport::UpdateEndingState);

	/* MediaControls owns the play button. Watch it in parallel so an
	 * intentional play is never mistaken for automatic playback. */
	if (auto *play = controls->findChild<QPushButton *>(QStringLiteral("playPauseButton"))) {
		connect(play, &QPushButton::clicked, this, [this]() {
			if (source)
				emit playRequested(source.Get());
		});
	}

	controls->SetSource(nullptr);
	controls->hide();
	placeholder->show();

	MixishTheme::Apply(this);
}

void MixishMediaTransport::SetSource(obs_source_t *newSource, bool hasInput)
{
	if (source.Get() != newSource) {
		source = OBSSource(newSource);
		if (controls)
			controls->SetSource(newSource ? OBSSource(newSource) : OBSSource());
	}

	if (!controls)
		return;

	if (newSource) {
		controls->setEnabled(true);
		controls->show();
		if (placeholder)
			placeholder->hide();
	} else {
		controls->setEnabled(false);
		if (hasInput) {
			controls->show();
			if (placeholder)
				placeholder->hide();
		} else {
			controls->hide();
			if (placeholder)
				placeholder->show();
		}
	}

	UpdateQueueButtons();

	if (newSource) {
		UpdateEndingState();
		if (endingTimer)
			endingTimer->start();
	} else {
		if (endingTimer)
			endingTimer->stop();
		/* Clearing happens inside UpdateEndingState(), otherwise the
		 * labels would keep the warning colour after unbinding. */
		UpdateEndingState();
	}
}

/* Marks both timecode labels and the seek bar while the bound media is within
 * ten seconds of its end. Re-polishing only happens when the state flips, not
 * per tick. */
void MixishMediaTransport::UpdateEndingState()
{
	bool warn = false;

	if (source) {
		const int64_t duration = obs_source_media_get_duration(source);
		if (duration > 0) {
			const int64_t time = obs_source_media_get_time(source);
			const int64_t remaining = duration - time;
			warn = remaining > 0 && remaining <= 10000;
		}
	}

	if (warn == ending)
		return;

	ending = warn;

	for (QLabel *label : {timerLabel, durationLabel}) {
		if (!label)
			continue;
		label->setProperty("mixishEnding", warn);
		MixishTheme::Repolish(label);
	}

	if (seekSlider) {
		seekSlider->setProperty("mixishEnding", warn);
		MixishTheme::Repolish(seekSlider);
	}
}

void MixishMediaTransport::SetPlaceholder(const QString &text)
{
	if (placeholder)
		placeholder->setText(text);
}

void MixishMediaTransport::SetQueueNavigationVisible(bool visible)
{
	queueNavigation = visible;
	UpdateQueueButtons();
}

void MixishMediaTransport::UpdateQueueButtons()
{
	if (!queueNavigation || !previousButton || !nextButton)
		return;

	if (!queueSignalsConnected) {
		/* MediaControls only shows these for sources with an internal
		 * playlist. For a Mixish queue they are repurposed, so drop its
		 * native handlers and expose Mixish signals instead. */
		previousButton->disconnect();
		nextButton->disconnect();

		connect(previousButton, &QPushButton::clicked, this,
			&MixishMediaTransport::previousRequested);
		connect(nextButton, &QPushButton::clicked, this,
			&MixishMediaTransport::nextRequested);

		previousButton->setToolTip(tr("Previous playlist item"));
		nextButton->setToolTip(tr("Next playlist item"));
		queueSignalsConnected = true;
	}

	const bool visible = source != nullptr;
	previousButton->setVisible(visible);
	nextButton->setVisible(visible);
}
