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
class QCheckBox;
class QComboBox;
class QHBoxLayout;
class QLineEdit;
class QPushButton;

/* vMix-style Streaming Settings.
 *
 * Five profiles are stored in the Mixish user config. Saving writes the
 * selected profile into OBS's service (server/key) and Simple Output encoder
 * settings, then rebuilds the output handler. */
class MixishStreamingSettings : public QDialog {
	Q_OBJECT

public:
	explicit MixishStreamingSettings(OBSBasic *main, QWidget *parent = nullptr);

private:
	void LoadProfile(int index);
	void StoreProfile(int index);
	void Apply();
	void RebuildProfileButtons();
	void SyncProfileCombo();

	OBSBasic *main = nullptr;
	int activeProfile = 0;
	int slotCount = 1;
	bool loadingProfile = false;

	QVector<QPushButton *> profileButtons;
	QHBoxLayout *profileButtonRow = nullptr;
	QComboBox *profileCombo = nullptr;

	QComboBox *destination = nullptr;
	QLineEdit *server = nullptr;
	QLineEdit *streamKey = nullptr;
	QComboBox *quality = nullptr;
	QComboBox *application = nullptr;
	QCheckBox *hardwareEncoder = nullptr;
};
