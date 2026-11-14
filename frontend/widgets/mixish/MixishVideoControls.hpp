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

#include <obs.hpp>

#include <QFrame>

class MixishSwitcher;
class MediaControls;
class QLabel;

/* Video transport for the selected input tile.
 *
 * Wraps OBS's own MediaControls component, so seeking, time display and
 * play/pause/restart are the same code paths the rest of OBS uses. When the
 * selected input has no controllable media source the bar stays visible but
 * disabled. */
class MixishVideoControls : public QFrame {
	Q_OBJECT

public:
	MixishVideoControls(MixishSwitcher *switcher, QWidget *parent = nullptr);

	/* Picks the first controllable media source inside the scene. */
	void SetScene(OBSScene scene);
	bool HasMedia() const;

private:
	MixishSwitcher *switcher = nullptr;
	MediaControls *controls = nullptr;
	QLabel *placeholder = nullptr;
};
