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

class OBSBasic;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;

/* vMix-style Recording Setup.
 *
 * The dialog edits OBS's Simple Output recording settings (path, container,
 * encoder, quality and audio bitrate) and applies them by rebuilding the
 * output handler through OBSBasic::ResetOutputs(). */
class MixishRecordingSettings : public QDialog {
	Q_OBJECT

public:
	explicit MixishRecordingSettings(OBSBasic *main, QWidget *parent = nullptr);

private:
	void LoadProfile(int index);
	void StoreProfile(int index);
	void Apply();
	void UpdateSummary();

	OBSBasic *main = nullptr;
	int activeProfile = 0;
	bool loadingProfile = false;

	QPushButton *profileButtons[2] = {};

	QListWidget *formats = nullptr;
	QLineEdit *path = nullptr;
	QComboBox *encoder = nullptr;
	QComboBox *quality = nullptr;
	QSpinBox *videoBitrate = nullptr;
	QComboBox *audioBitrate = nullptr;
	QComboBox *audioTrack = nullptr;
	QCheckBox *hardwareEncoder = nullptr;
	QLabel *sizeValue = nullptr;
	QLabel *fpsValue = nullptr;
};
