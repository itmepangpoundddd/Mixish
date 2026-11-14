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

#include <QToolBar>

class OBSBasic;
class MixishSwitcher;
class QMenu;
class QToolButton;

/* vMix-style top bar. The original OBS menu bar is hidden while this bar is
 * active, and every original OBS menu remains reachable from the Preset menu. */
class MixishTopBar : public QToolBar {
	Q_OBJECT

public:
	MixishTopBar(MixishSwitcher *switcher, OBSBasic *main, QWidget *parent = nullptr);

private:
	QToolButton *MakeMenuButton(const QString &text, QMenu *menu);
	void BuildPresetMenu();

	MixishSwitcher *switcher = nullptr;
	OBSBasic *main = nullptr;
	QMenu *presetMenu = nullptr;
};
