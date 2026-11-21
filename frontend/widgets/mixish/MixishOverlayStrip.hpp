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
#include <QVector>

class MixishSwitcher;
class QPushButton;
class QVBoxLayout;

/* Master overlay strip: one button per overlay channel plus a single
 * All On / All Off switch. Lives in the transition column directly below FTB. */
class MixishOverlayStrip : public QFrame {
	Q_OBJECT

public:
	explicit MixishOverlayStrip(MixishSwitcher *switcher, QWidget *parent = nullptr);

	void Refresh();

private:
	void OnChannelClicked(int index);
	void OnAllClicked();
	bool AnyOnAir() const;

	MixishSwitcher *switcher = nullptr;

	QVector<QPushButton *> channelButtons;
	QPushButton *allButton = nullptr;
};
