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

#include <QColor>
#include <QFrame>
#include <QVector>

class MixishSwitcher;
class QLabel;
class QPushButton;
class ThumbnailView;

class MixishInputTile : public QFrame {
	Q_OBJECT

public:
	MixishInputTile(MixishSwitcher *switcher, OBSScene scene, int number, QWidget *parent = nullptr);

	int Number() const { return number; }
	OBSScene Scene() const;

	void SetNumber(int number);
	void SetState(bool preview, bool program);
	void SetSelected(bool selected);
	void RefreshName();
	void UpdateColorTag();
	void RequestThumbnail();
	void RefreshOverlayButtons();
	void RefreshAudioState();

	static const QVector<QColor> &Palette();
	static void SetSceneColor(OBSScene scene, const QColor &color);

signals:
	void clicked(MixishInputTile *tile);
	void doubleClicked(MixishInputTile *tile);

protected:
	void mousePressEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	void enterEvent(QEnterEvent *event) override;
	void contextMenuEvent(QContextMenuEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	void UpdateThumbnail(const QPixmap &pixmap);
	void ApplyState();
	void OnOverlayClicked(int index);
	OBSSource PrimaryAudioSource() const;

	MixishSwitcher *switcher = nullptr;
	OBSWeakSource weakScene;
	int number = 1;
	bool isPreview = false;
	bool isProgram = false;
	bool isSelected = false;

	QLabel *numberLabel = nullptr;
	QLabel *nameLabel = nullptr;
	QLabel *thumbnailLabel = nullptr;
	QLabel *previewBadge = nullptr;
	QLabel *programBadge = nullptr;
	QFrame *colorTag = nullptr;
	QFrame *header = nullptr;
	QPushButton *cutButton = nullptr;
	QPushButton *audioButton = nullptr;
	QVector<QPushButton *> overlayButtons;
	QPushButton *settingsButton = nullptr;
	QPushButton *menuButton = nullptr;
	ThumbnailView *thumbnailView = nullptr;
};
