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

#include <QDialog>
#include <QVector>

class OBSBasic;
class MixishSwitcher;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;

/* Overlay channel panel: assignment, on/off, audio, cut/fade and fade times.
 * Position/size/crop join this panel in the geometry stage. */
class MixishOverlayPanel : public QDialog {
	Q_OBJECT

public:
	MixishOverlayPanel(MixishSwitcher *switcher, OBSBasic *main, QWidget *parent = nullptr);

	void Refresh();

private:
	struct Row {
		QLabel *chip = nullptr;
		QLabel *state = nullptr;
		QComboBox *input = nullptr;
		QPushButton *onAir = nullptr;
		QPushButton *audio = nullptr;
		QComboBox *mode = nullptr;
		QSpinBox *fadeIn = nullptr;
		QSpinBox *fadeOut = nullptr;
	};

	QWidget *BuildRow(int index);
	void PopulateInputs(int index);
	void OnInputSelected(int index);
	MixishSwitcher *switcher = nullptr;
	OBSBasic *main = nullptr;

	QVector<Row> rows;
	QLabel *summary = nullptr;
};
