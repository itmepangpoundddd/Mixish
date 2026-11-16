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

#include <QWidget>

class QLabel;
class MixishSplashProgress;

/* Startup splash.
 *
 * Shown by OBSApp before the main window exists and closed once the main
 * window is on screen, so the first thing the user sees is Mixish instead of
 * an empty desktop. Styling follows MIXISH_DESIGN_SYSTEM.md (tokens copied
 * from the design sheet, no new colours). */
class MixishSplash : public QWidget {
	Q_OBJECT

public:
	explicit MixishSplash(QWidget *parent = nullptr);

	void SetStatus(const QString &text);

	/* Fades the splash out and deletes it. Safe to call more than once. */
	void Finish();

protected:
	void paintEvent(QPaintEvent *event) override;
	void showEvent(QShowEvent *event) override;

private:
	QLabel *statusLabel = nullptr;
	QLabel *versionLabel = nullptr;
	MixishSplashProgress *progress = nullptr;
	bool finishing = false;
};

/* One splash for the whole process. Show() is a no-op when one is already
 * visible or the process has no GUI. */
namespace MixishSplashScreen {
void Show();
void SetStatus(const QString &text);
void Finish();
} // namespace MixishSplashScreen
