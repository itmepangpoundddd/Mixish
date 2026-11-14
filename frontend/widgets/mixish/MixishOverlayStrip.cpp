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

#include "MixishOverlayStrip.hpp"
#include "MixishOverlayController.hpp"
#include "MixishSwitcher.hpp"
#include "MixishTheme.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "moc_MixishOverlayStrip.cpp"

MixishOverlayStrip::MixishOverlayStrip(MixishSwitcher *switcher_, QWidget *parent)
	: QFrame(parent), switcher(switcher_)
{
	setObjectName(QStringLiteral("mixishOverlayStrip"));

	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(MixishTheme::Space2, MixishTheme::Space1,
				  MixishTheme::Space2, MixishTheme::Space1);
	outer->setSpacing(MixishTheme::Space1);

	auto *titleRow = new QHBoxLayout();
	titleRow->setContentsMargins(0, 0, 0, 0);
	titleRow->setSpacing(MixishTheme::Space1);

	auto *title = new QLabel(tr("OVERLAYS"), this);
	MixishTheme::MakeSectionLabel(title);
	titleRow->addWidget(title);
	titleRow->addStretch(1);

	auto *editButton = new QPushButton(tr("Edit"), this);
	editButton->setObjectName(QStringLiteral("mixishOverlayEdit"));
	MixishTheme::SetVariant(editButton, "secondary");
	editButton->setFixedHeight(MixishTheme::CompactHeight);
	editButton->setFocusPolicy(Qt::NoFocus);
	editButton->setToolTip(tr("Open the overlay editor"));
	connect(editButton, &QPushButton::clicked, this, [this]() { switcher->OpenOverlayPanel(); });
	titleRow->addWidget(editButton);

	outer->addLayout(titleRow);

	auto *row = new QHBoxLayout();
	row->setSpacing(MixishTheme::Space1);
	row->setContentsMargins(0, 0, 0, 0);

	MixishOverlayController *overlays = switcher->GetOverlays();
	const int count = overlays ? overlays->ChannelCount() : 0;

	for (int i = 0; i < count; ++i) {
		auto *button = new QPushButton(QString::number(i + 1), this);
		button->setObjectName(QStringLiteral("mixishOverlayChannel%1").arg(i + 1));
		MixishTheme::SetVariant(button, "secondary");
		MixishTheme::SetRole(button, "overlayToggle");
		button->setFixedSize(MixishTheme::OverlayToggleWidth,
				     MixishTheme::OverlayToggleHeight);
		button->setFocusPolicy(Qt::NoFocus);
		connect(button, &QPushButton::clicked, this, [this, i]() { OnChannelClicked(i); });
		row->addWidget(button, 0);
		channelButtons.push_back(button);
	}

	row->addStretch(1);

	outer->addLayout(row);

	allOffButton = new QPushButton(tr("All Off"), this);
	allOffButton->setObjectName(QStringLiteral("mixishOverlayAllOff"));
	MixishTheme::SetVariant(allOffButton, "secondary");
	allOffButton->setFixedHeight(MixishTheme::CompactHeight);
	allOffButton->setFocusPolicy(Qt::NoFocus);
	allOffButton->setToolTip(tr("Turn every overlay channel off"));
	connect(allOffButton, &QPushButton::clicked, this, [this]() {
		if (MixishOverlayController *overlays = switcher->GetOverlays())
			overlays->AllOff();
	});
	/* Kept in the header row so the strip stays compact and the transition
	 * column never needs to scroll for it. */
	titleRow->insertWidget(titleRow->count() - 1, allOffButton);

	connect(switcher, &MixishSwitcher::OverlaysChanged, this, &MixishOverlayStrip::Refresh);

	MixishTheme::Apply(this);

	Refresh();
}

void MixishOverlayStrip::Refresh()
{
	MixishOverlayController *overlays = switcher ? switcher->GetOverlays() : nullptr;
	if (!overlays)
		return;

	for (int i = 0; i < channelButtons.size(); ++i) {
		QPushButton *button = channelButtons[i];
		if (!button)
			continue;

		const bool assigned = overlays->IsAssigned(i);
		const bool onAir = assigned && overlays->IsOnAir(i);

		MixishTheme::SetState(button, onAir ? "onAir" : (assigned ? "assigned" : "empty"));

		if (assigned) {
			button->setToolTip(tr("%1: %2 — click to %3")
						   .arg(MixishOverlayController::ChannelTitle(i),
							overlays->InputName(i),
							onAir ? tr("turn off") : tr("turn on")));
		} else {
			button->setToolTip(tr("%1 is empty — click to assign an input")
						   .arg(MixishOverlayController::ChannelTitle(i)));
		}
	}
}

void MixishOverlayStrip::OnChannelClicked(int index)
{
	MixishOverlayController *overlays = switcher->GetOverlays();
	if (!overlays)
		return;

	if (overlays->IsAssigned(index))
		overlays->Toggle(index);
	else
		switcher->OpenOverlayPanel();
}
