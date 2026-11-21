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

#include <QElapsedTimer>
#include <QFrame>

class MixishSwitcher;
class QAction;
class QMenu;
class QPushButton;
class QTimer;
class QToolButton;

/* Bottom control bar for the Mixish layout.
 *
 * Add Input (with the legacy "Add Scene" action in its menu), recording and
 * streaming controls. Recording and streaming reuse OBS's frontend API; the
 * buttons only reflect state, they do not reimplement output handling. */
class MixishBottomBar : public QFrame {
	Q_OBJECT

public:
	explicit MixishBottomBar(MixishSwitcher *switcher, QWidget *parent = nullptr);

	void Refresh();

private:
	static QString FormatDuration(qint64 ms);

	MixishSwitcher *switcher = nullptr;

	QToolButton *addInputButton = nullptr;
	QMenu *addInputMenu = nullptr;

	QPushButton *playlistButton = nullptr;

	QToolButton *recordSettingsButton = nullptr;
	QPushButton *recordButton = nullptr;

	QToolButton *streamSettingsButton = nullptr;
	QPushButton *streamButton = nullptr;

	QTimer *stateTimer = nullptr;
	QElapsedTimer recordClock;
	QElapsedTimer streamClock;
	bool recordActive = false;
	bool streamActive = false;
};
