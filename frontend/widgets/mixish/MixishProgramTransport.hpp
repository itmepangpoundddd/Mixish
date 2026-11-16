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

#include <obs.h>

#include <QFrame>
#include <QPointer>

class QLabel;
class MixishMediaTransport;

/* Reusable transport for the active Program media.
 *
 * Currently bound to the Program Playlist's media source. It is deliberately
 * a small, generic wrapper around MixishMediaTransport so the upcoming
 * live-operation batch can extend it and then remove the old INPUTS
 * transport. Previous/Next are emitted as signals for the playlist queue. */
class MixishProgramTransport : public QFrame {
	Q_OBJECT

public:
	explicit MixishProgramTransport(QWidget *parent = nullptr);

	void SetSource(obs_source_t *source);
	void SetPlaceholder(const QString &text);
	void SetQueueInfo(const QString &text);
	bool HasSource() const;

	/* Shared transport underneath, see MixishVideoControls::Transport(). */
	MixishMediaTransport *Transport() const;

signals:
	void previousRequested();
	void nextRequested();

private:
	QPointer<MixishMediaTransport> transport;
	QLabel *queueInfo = nullptr;
};
