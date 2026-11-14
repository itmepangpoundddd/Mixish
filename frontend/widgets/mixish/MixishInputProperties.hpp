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

#include <QDialog>

class OBSBasic;
class OBSPropertiesView;
class OBSQTDisplay;
class QCloseEvent;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;

/* Mixish input properties window.
 *
 * Reuses OBS's properties view for the source contained in an input scene,
 * presented with a vMix-style sidebar, white settings area and live preview.
 */
class MixishInputProperties : public QDialog {
	Q_OBJECT

public:
	MixishInputProperties(QWidget *parent, OBSBasic *main, OBSScene scene, OBSSource source, int number);
	~MixishInputProperties() override;

protected:
	void closeEvent(QCloseEvent *event) override;

private slots:
	void PopulateSections();
	void UpdateTitle();

private:
	void BuildUi();
	void ScrollToSection(QListWidgetItem *item);
	void RestoreDefaults();
	void RevertChanges();
	void CommitSettings();

	static void SourceRemoved(void *data, calldata_t *params);
	static void SourceRenamed(void *data, calldata_t *params);
	static void UpdateProperties(void *data, calldata_t *params);
	static void DrawPreview(void *data, uint32_t cx, uint32_t cy);

	OBSBasic *main = nullptr;
	OBSSource sceneSource;
	OBSSource source;
	int number = 0;

	OBSSignal sceneRemovedSignal;
	OBSSignal sceneRenamedSignal;
	OBSSignal sourceRemovedSignal;
	OBSSignal sourceRenamedSignal;
	OBSSignal updatePropertiesSignal;

	OBSData oldSettings;

	OBSPropertiesView *view = nullptr;
	OBSQTDisplay *preview = nullptr;
	QListWidget *sectionList = nullptr;
	QLabel *tabLabel = nullptr;
	QLabel *titleLabel = nullptr;
	QLabel *subtitleLabel = nullptr;
	QLabel *emptyLabel = nullptr;
	QPushButton *defaultsButton = nullptr;
	QPushButton *okButton = nullptr;
	QPushButton *cancelButton = nullptr;

	bool discardChanges = false;
};
