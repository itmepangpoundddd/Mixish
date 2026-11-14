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

#pragma once

#include <QFrame>
#include <QPointer>
#include <QStringList>

class MixishSwitcher;
class VolumeControl;
class QLabel;
class QScrollArea;
class QTimer;
class QVBoxLayout;
class QWidget;

/* Docked audio mixer for the Mixish input area.
 *
 * Each row is OBS's own VolumeControl (fader, mute, level meter, monitor
 * toggle) and the source list comes from the running scene collection. The
 * master row controls the Desktop Audio output device when one is configured.
 * Rows are laid out horizontally so they fit the height of the tile row. */
class MixishAudioMixer : public QFrame {
	Q_OBJECT

public:
	explicit MixishAudioMixer(MixishSwitcher *switcher, QWidget *parent = nullptr);

	/* Re-reads the master device and the audio source list. */
	void Refresh();

protected:
	void showEvent(QShowEvent *event) override;

private:
	void RebuildMaster();
	void RebuildRows();

	MixishSwitcher *switcher = nullptr;

	QPointer<QWidget> masterHost = nullptr;
	QPointer<VolumeControl> master = nullptr;
	QPointer<QLabel> masterName = nullptr;

	QPointer<QScrollArea> scrollArea = nullptr;
	QPointer<QWidget> rowsHost = nullptr;
	QPointer<QVBoxLayout> rowLayout = nullptr;
	QPointer<QLabel> emptyLabel = nullptr;

	QPointer<QTimer> refreshTimer;
	QString currentMasterUuid;
	QStringList rowsSignature;
};
