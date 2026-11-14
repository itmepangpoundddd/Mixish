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

#include "MixishOverlayPanel.hpp"
#include "MixishOverlayController.hpp"
#include "MixishSwitcher.hpp"
#include "MixishTheme.hpp"

#include <widgets/OBSBasic.hpp>

#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include "moc_MixishOverlayPanel.cpp"

namespace {

} // namespace

MixishOverlayPanel::MixishOverlayPanel(MixishSwitcher *switcher_, OBSBasic *main_, QWidget *parent)
	: QDialog(parent), switcher(switcher_), main(main_)
{
	setObjectName(QStringLiteral("mixishOverlayPanel"));
	setWindowTitle(tr("Overlays"));
	setWindowFlag(Qt::Tool, true);
	setAttribute(Qt::WA_DeleteOnClose, false);

	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(MixishTheme::Space3, MixishTheme::Space3,
				  MixishTheme::Space3, MixishTheme::Space3);
	outer->setSpacing(MixishTheme::Space2);

	auto *title = new QLabel(tr("Overlay Channels"), this);
	MixishTheme::MakeTitleLabel(title);
	outer->addWidget(title);

	MixishOverlayController *overlays = switcher->GetOverlays();
	const int count = overlays ? overlays->ChannelCount() : 0;

	for (int i = 0; i < count; ++i) {
		if (QWidget *row = BuildRow(i))
			outer->addWidget(row);
	}

	summary = new QLabel(this);
	MixishTheme::SetRole(summary, "muted");
	outer->addWidget(summary);

	auto *footer = new QHBoxLayout();
	footer->setSpacing(MixishTheme::Space1);

	auto *allOff = new QPushButton(tr("All Off"), this);
	MixishTheme::SetVariant(allOff, "secondary");
	allOff->setFixedHeight(28);
	connect(allOff, &QPushButton::clicked, this, [this]() {
		if (MixishOverlayController *o = switcher->GetOverlays())
			o->AllOff();
	});
	footer->addWidget(allOff);

	footer->addStretch(1);

	auto *close = new QPushButton(tr("Close"), this);
	MixishTheme::SetVariant(close, "secondary");
	close->setFixedHeight(28);
	connect(close, &QPushButton::clicked, this, &QDialog::close);
	footer->addWidget(close);

	outer->addLayout(footer);

	if (overlays) {
		connect(overlays, &MixishOverlayController::ChannelsChanged, this,
			&MixishOverlayPanel::Refresh);
		connect(switcher, &MixishSwitcher::OverlaysChanged, this, &MixishOverlayPanel::Refresh);
	}

	MixishTheme::Apply(this);

	Refresh();
}

QWidget *MixishOverlayPanel::BuildRow(int index)
{
	MixishOverlayController *overlays = switcher->GetOverlays();
	Row row;

	auto *frame = new QFrame(this);
	frame->setObjectName(QStringLiteral("mixishOverlayRow"));

	auto *layout = new QHBoxLayout(frame);
	layout->setContentsMargins(MixishTheme::Space2, MixishTheme::Space1,
				   MixishTheme::Space2, MixishTheme::Space1);
	layout->setSpacing(MixishTheme::Space2);

	row.chip = new QLabel(MixishOverlayController::ChannelLabel(index), frame);
	row.chip->setObjectName(QStringLiteral("mixishOverlayChip"));
	row.chip->setFixedWidth(34);
	row.chip->setAlignment(Qt::AlignCenter);
	layout->addWidget(row.chip);

	row.input = new QComboBox(frame);
	MixishTheme::SetControl(row.input);
	row.input->setMinimumWidth(190);
	row.input->setToolTip(tr("Input assigned to this overlay channel"));
	layout->addWidget(row.input, 1);

	row.state = new QLabel(frame);
	row.state->setObjectName(QStringLiteral("mixishOverlayState"));
	row.state->setFixedWidth(56);
	row.state->setAlignment(Qt::AlignCenter);
	layout->addWidget(row.state);

	row.onAir = new QPushButton(tr("On air"), frame);
	MixishTheme::SetVariant(row.onAir, "secondary");
	row.onAir->setCheckable(true);
	row.onAir->setFixedWidth(80);
	row.onAir->setFixedHeight(28);
	layout->addWidget(row.onAir);

	row.audio = new QPushButton(tr("Audio"), frame);
	MixishTheme::SetVariant(row.audio, "secondary");
	row.audio->setCheckable(true);
	row.audio->setChecked(true);
	row.audio->setFixedWidth(72);
	row.audio->setFixedHeight(28);
	row.audio->setToolTip(tr("Include this overlay's audio in the main mix"));
	layout->addWidget(row.audio);

	row.mode = new QComboBox(frame);
	MixishTheme::SetControl(row.mode);
	row.mode->addItem(tr("Cut"));
	row.mode->addItem(tr("Fade"));
	row.mode->setFixedWidth(72);
	row.mode->setToolTip(tr("How this channel turns on and off"));
	layout->addWidget(row.mode);

	row.fadeIn = new QSpinBox(frame);
	MixishTheme::SetControl(row.fadeIn);
	row.fadeIn->setRange(0, 10000);
	row.fadeIn->setSingleStep(50);
	row.fadeIn->setSuffix(tr(" ms"));
	row.fadeIn->setFixedWidth(84);
	row.fadeIn->setToolTip(tr("Fade in time"));
	layout->addWidget(row.fadeIn);

	row.fadeOut = new QSpinBox(frame);
	MixishTheme::SetControl(row.fadeOut);
	row.fadeOut->setRange(0, 10000);
	row.fadeOut->setSingleStep(50);
	row.fadeOut->setSuffix(tr(" ms"));
	row.fadeOut->setFixedWidth(84);
	row.fadeOut->setToolTip(tr("Fade out time"));
	layout->addWidget(row.fadeOut);

	rows.push_back(row);

	/* Initialise before connecting: the fade-time handlers read both spin
	 * boxes, so a partially initialised row would write 0 into the model. */
	{
		QSignalBlocker blockIn(row.fadeIn);
		QSignalBlocker blockOut(row.fadeOut);
		row.fadeIn->setValue(overlays ? overlays->FadeInMs(index) : 300);
		row.fadeOut->setValue(overlays ? overlays->FadeOutMs(index) : 300);
	}

	connect(row.input, QOverload<int>::of(&QComboBox::activated), this,
		[this, index](int) { OnInputSelected(index); });

	connect(row.onAir, &QPushButton::toggled, this, [this, index](bool checked) {
		if (MixishOverlayController *o = switcher->GetOverlays())
			o->SetOnAir(index, checked);
	});

	connect(row.audio, &QPushButton::toggled, this, [this, index](bool checked) {
		if (MixishOverlayController *o = switcher->GetOverlays())
			o->SetAudioEnabled(index, checked);
	});

	connect(row.mode, QOverload<int>::of(&QComboBox::activated), this, [this, index](int mode) {
		if (MixishOverlayController *o = switcher->GetOverlays())
			o->SetFade(index, mode == 1);
	});

	connect(row.fadeIn, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, index](int) {
		MixishOverlayController *o = switcher->GetOverlays();
		if (!o)
			return;
		o->SetFadeTimes(index, rows[index].fadeIn->value(), rows[index].fadeOut->value());
	});
	connect(row.fadeOut, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, index](int) {
		MixishOverlayController *o = switcher->GetOverlays();
		if (!o)
			return;
		o->SetFadeTimes(index, rows[index].fadeIn->value(), rows[index].fadeOut->value());
	});

	/* Keep the default state sane before the first Refresh. */
	row.fadeIn->setValue(overlays ? overlays->FadeInMs(index) : 300);
	row.fadeOut->setValue(overlays ? overlays->FadeOutMs(index) : 300);

	return frame;
}

void MixishOverlayPanel::PopulateInputs(int index)
{
	if (index < 0 || index >= rows.size())
		return;

	MixishOverlayController *overlays = switcher->GetOverlays();
	Row &row = rows[index];

	QSignalBlocker block(row.input);
	row.input->clear();
	row.input->addItem(tr("— Empty —"), QString());

	QString currentUuid;
	if (overlays) {
		if (OBSSource source = overlays->InputSource(index))
			currentUuid = QString::fromUtf8(obs_source_get_uuid(source));
	}

	int currentIndex = 0;
	for (int i = 0; i < main->ui->scenes->count(); ++i) {
		QListWidgetItem *item = main->ui->scenes->item(i);
		if (!item)
			continue;

		OBSScene scene = GetOBSRef<OBSScene>(item);
		if (!scene)
			continue;

		obs_source_t *source = obs_scene_get_source(scene);
		if (!source || obs_source_removed(source))
			continue;

		const QString uuid = QString::fromUtf8(obs_source_get_uuid(source));
		const QString name = QString::fromUtf8(obs_source_get_name(source));
		row.input->addItem(name, uuid);

		if (!currentUuid.isEmpty() && uuid == currentUuid)
			currentIndex = row.input->count() - 1;
	}

	row.input->setCurrentIndex(currentIndex);
}

void MixishOverlayPanel::OnInputSelected(int index)
{
	if (index < 0 || index >= rows.size())
		return;

	MixishOverlayController *overlays = switcher->GetOverlays();
	if (!overlays)
		return;

	Row &row = rows[index];
	const QString uuid = row.input->currentData().toString();

	if (uuid.isEmpty()) {
		overlays->Clear(index);
		return;
	}

	OBSSourceAutoRelease source = obs_get_source_by_uuid(uuid.toUtf8().constData());
	if (!source) {
		QMessageBox::warning(this, tr("Overlay"),
				     tr("That input no longer exists."));
		Refresh();
		return;
	}

	OBSScene scene = obs_scene_from_source(source);
	if (!scene) {
		QMessageBox::warning(this, tr("Overlay"),
				     tr("Only scenes can be assigned to an overlay channel."));
		Refresh();
		return;
	}

	if (!overlays->CanAssign(index, scene)) {
		QMessageBox::warning(this, tr("Overlay"),
				     tr("This input cannot be assigned to %1 because it would "
					"create a source cycle.")
					     .arg(MixishOverlayController::ChannelTitle(index)));
		Refresh();
		return;
	}

	overlays->Assign(index, scene, false);
}

void MixishOverlayPanel::Refresh()
{
	MixishOverlayController *overlays = switcher->GetOverlays();
	if (!overlays)
		return;

	if (rows.size() != overlays->ChannelCount()) {
		/* Channel count is fixed for the session; nothing to rebuild. */
		return;
	}

	for (int i = 0; i < rows.size(); ++i) {
		Row &row = rows[i];
		const bool assigned = overlays->IsAssigned(i);
		const bool onAir = assigned && overlays->IsOnAir(i);

		PopulateInputs(i);

		row.onAir->setEnabled(assigned);
		row.audio->setEnabled(assigned);
		row.mode->setEnabled(assigned);
		row.fadeIn->setEnabled(assigned && overlays->IsFade(i));
		row.fadeOut->setEnabled(assigned && overlays->IsFade(i));

		{
			QSignalBlocker block(row.onAir);
			row.onAir->setChecked(onAir);
		}
		MixishTheme::SetState(row.onAir, onAir ? "onAir" : "idle");

		{
			QSignalBlocker block(row.audio);
			row.audio->setChecked(overlays->IsAudioEnabled(i));
		}
		MixishTheme::SetState(row.audio,
				      overlays->IsAudioEnabled(i) ? "idle" : "empty");

		{
			QSignalBlocker block(row.mode);
			row.mode->setCurrentIndex(overlays->IsFade(i) ? 1 : 0);
		}

		{
			QSignalBlocker blockIn(row.fadeIn);
			QSignalBlocker blockOut(row.fadeOut);
			row.fadeIn->setValue(overlays->FadeInMs(i));
			row.fadeOut->setValue(overlays->FadeOutMs(i));
		}

		row.state->setText(!assigned ? tr("Empty") : onAir ? tr("On air") : tr("Ready"));
		MixishTheme::SetState(row.state, onAir ? "onAir" : "idle");
	}

	QStringList live;
	for (int i = 0; i < overlays->ChannelCount(); ++i) {
		if (overlays->IsAssigned(i) && overlays->IsOnAir(i))
			live << MixishOverlayController::ChannelLabel(i);
	}

	summary->setText(live.isEmpty()
				 ? tr("No overlay is on air. Overlays render above Program and are "
				      "not affected by Cut, Fade or the T-bar.")
				 : tr("On air: %1").arg(live.join(QStringLiteral(", "))));
}
