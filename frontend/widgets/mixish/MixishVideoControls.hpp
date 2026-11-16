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
#include <QPointer>

class MixishSwitcher;
class MixishMediaTransport;

/* Video transport for the selected input tile.
 *
 * This is a thin adapter over MixishMediaTransport: it finds the first
 * controllable media source in the selected scene and binds it. The Program
 * transport uses the same MixishMediaTransport widget. */
class MixishVideoControls : public QFrame {
	Q_OBJECT

public:
	MixishVideoControls(MixishSwitcher *switcher, QWidget *parent = nullptr);

	/* Picks the first controllable media source inside the scene. */
	void SetScene(OBSScene scene);
	bool HasMedia() const;

	/* The shared transport widget, so the switcher can listen for the
	 * user's own play requests. Defined in the .cpp to keep the QPointer
	 * conversion out of headers that only forward declare the type. */
	MixishMediaTransport *Transport() const;

private:
	MixishSwitcher *switcher = nullptr;
	QPointer<MixishMediaTransport> transport;
};
