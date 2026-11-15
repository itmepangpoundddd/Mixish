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

#include <QFrame>
#include <QHash>
#include <QPointer>
#include <QVector>

class OBSBasic;
class MixishSwitcher;
class MixishTBar;
class QLabel;
class QPushButton;
class QScrollArea;
class QShowEvent;
class QSpinBox;
class QTimer;
class QVBoxLayout;


class MixishTransitionPanel : public QFrame {
	Q_OBJECT

public:
	MixishTransitionPanel(MixishSwitcher *switcher, OBSBasic *main, QWidget *parent = nullptr);

	void RefreshTransitions();

protected:
	void showEvent(QShowEvent *event) override;

private slots:
	void OnCutClicked();
	void OnFadeClicked();
	void OnFtbClicked();
	void OnAddClicked();
	void OnConfigureClicked();
	void OnDurationChanged(int ms);
	void OnDurationChangedExternally(int ms);
	void OnTBarChanged(int value);
	void OnTBarDragFinished(int value);
	void OnTBarReachedEnd(bool programSide);
	void OnTBarCancel();
	void OnTBarAnimationFinished();
	void UpdateTransitionProgress();
	void OnCurrentTransitionChanged(const QString &uuid);
	void OnTransitionsChanged();
	void Tick();

private:
	void AddTransitionButton(const QString &uuid, const QString &name);
	void ClearTransitionButtons();
	void SetButtonActive(QPushButton *button, bool active);
	void UpdateActiveState();
	void UpdateBusyState();
	void UpdateDurationPresets();
	void UpdateFtbState();
	void BeginTransitionFollow();
	void EndTransitionFollow();
	void CompleteTBar(bool toProgram);
	void FinishTBarCommit();

	MixishSwitcher *switcher = nullptr;
	OBSBasic *main = nullptr;

	QPointer<QVBoxLayout> customLayout;
	QPointer<QScrollArea> customScroll;
	QPointer<QScrollArea> transitionScroll;
	QPointer<QPushButton> cutButton;
	QPointer<QPushButton> fadeButton;
	QPointer<QPushButton> ftbButton;
	QPointer<MixishTBar> tBar;
	QPointer<QSpinBox> durationSpin;
	QPointer<QTimer> tickTimer;
	QPointer<QTimer> progressTimer;
	QHash<QString, QPointer<QPushButton>> transitionButtons;
	QVector<QPointer<QPushButton>> durationButtons;
	bool followingTransition = false;
	bool sawTransitionProgress = false;
	int followTicks = 0;
	bool committingTBar = false;
	bool commitToProgram = false;
	qint64 followSuppressedUntil = 0;
};
