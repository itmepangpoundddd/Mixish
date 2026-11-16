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

class MediaControls;
class QLabel;
class QPushButton;

/* One reusable media transport, built on OBS's MediaControls.
 *
 * Used by the INPUTS transport today and by the Program transport below the
 * Program monitor. The Program transport additionally shows the previous /
 * next buttons for a Mixish playlist queue; the INPUTS transport leaves them
 * to OBS's native media playlist behavior. Keeping this in one widget avoids
 * two transport implementations that would drift apart. */
class MixishMediaTransport : public QFrame {
	Q_OBJECT

public:
	explicit MixishMediaTransport(QWidget *parent = nullptr);

	/* hasInput=false shows only the placeholder. hasInput=true with no
	 * source keeps the controls visible but disabled. */
	void SetSource(obs_source_t *source, bool hasInput = true);
	void SetPlaceholder(const QString &text);
	void SetQueueNavigationVisible(bool visible);
	bool HasSource() const { return source != nullptr; }
	MediaControls *Controls() const { return controls; }

signals:
	void previousRequested();
	void nextRequested();

	/* The user pressed the transport's play / pause / restart button. The
	 * switcher uses this to tell an intentional play from a media source
	 * that started on its own (for example when it becomes visible), which
	 * it parks again while the input is off air. */
	void playRequested(obs_source_t *source);

private:
	void UpdateQueueButtons();

	MediaControls *controls = nullptr;
	QLabel *placeholder = nullptr;
	QPushButton *previousButton = nullptr;
	QPushButton *nextButton = nullptr;
	OBSSource source;
	bool queueNavigation = false;
	bool queueSignalsConnected = false;
};
