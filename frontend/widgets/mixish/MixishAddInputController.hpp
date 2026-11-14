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

#include <QObject>
#include <QPointer>
#include <QStringList>

class MixishSwitcher;
class OBSBasic;
class OBSBasicSourceSelect;
class QFrame;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QSpinBox;
class QWidget;

/* Decorates OBS Studio's existing Add Source dialog for the Mixish "Add Input"
 * flow. The source type list and source widgets are not rebuilt here. */
class MixishAddInputController : public QObject {
	Q_OBJECT

public:
	MixishAddInputController(OBSBasicSourceSelect *dialog, MixishSwitcher *switcher, OBSBasic *main);
	void Setup();

private slots:
	void OnTypeChanged(QListWidgetItem *current, QListWidgetItem *previous);
	void CreateInput();
	void BrowseMedia();
	void OnRecentMediaClicked(QListWidgetItem *item);

private:
	void HideObsOnlyControls();
	void RemoveUnsupportedTypeItems();
	void BuildRecentTypesRow();
	void BuildMediaPanel();
	void BuildTypeInfoPanel();
	void BuildNumberField();
	void UpdateMediaPanel();
	void UpdateTypeInfoPanel();
	void RefreshMediaList();
	void ApplyMixishStyle();

	void LoadRecentLists();
	void SaveRecentTypes();
	void SaveRecentMedia();
	void AddRecentType(const QString &type);
	void AddRecentMedia(const QString &path);

	QString SelectedTypeId() const;
	bool IsMediaType(const QString &type) const;
	QString TypeDescription(const QString &type) const;
	QListWidgetItem *FindTypeItem(const QString &type) const;

	OBSBasicSourceSelect *dialog = nullptr;
	MixishSwitcher *switcher = nullptr;
	OBSBasic *main = nullptr;

	QListWidget *typeList = nullptr;
	QPushButton *createButton = nullptr;
	QListWidget *mediaList = nullptr;
	QSpinBox *numberSpin = nullptr;
	QPointer<QFrame> recentTypesFrame;
	QPointer<QFrame> mediaPanel;
	QPointer<QFrame> typeInfoPanel;
	QPointer<QWidget> numberWidget;
	QLabel *typeInfoIcon = nullptr;
	QLabel *typeInfoName = nullptr;
	QLabel *typeInfoDescription = nullptr;

	QStringList recentTypes;
	QStringList recentMedia;
	QString selectedMediaFile;
};
