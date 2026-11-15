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

#include <QElapsedTimer>
#include <QWidget>

class QTimer;

/* Manual transition T-bar.
 *
 * A custom-painted control that maps 0..1023 onto a thick track. The filled
 * portion reveals a Preview-green to Program-red gradient, the handle is a
 * large rounded grip, and the widget owns its own drag, wheel, keyboard and
 * animation behavior so the transition panel only has to translate values to
 * OBS. Manual input is disabled while a button transition is being followed
 * (setLocked) and while the widget is animating a snap or a return. */
class MixishTBar : public QWidget {
	Q_OBJECT

public:
	explicit MixishTBar(QWidget *parent = nullptr);

	int maximum() const { return MaxValue; }
	int value() const { return currentValue; }
	bool isDragging() const { return dragging; }
	bool isLocked() const { return locked; }
	bool isAnimating() const { return motion != Motion::None; }

	/* Programmatic sync. Never emits valueChanged(). */
	void setValue(int value);
	/* Follow a transition that runs from a button (0..1 progress). */
	void setExternalProgress(float progress);
	/* Ignore manual drag/wheel/key input until unlocked. */
	void setLocked(bool locked);
	/* Animate to a value; emitChanges drives the transition through OBS. */
	void animateTo(int target, int durationMs, bool emitChanges);
	/* Visual-only 0 -> 100 -> 0 sweep used for Cut. */
	void playPulse();
	void resetToStart();

signals:
	void valueChanged(int value);
	void dragFinished(int value);
	void reachedEnd(bool programSide);
	void cancelRequested();
	void animationFinished();

protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void enterEvent(QEnterEvent *event) override;
	void leaveEvent(QEvent *event) override;
	void changeEvent(QEvent *event) override;

private:
	enum class Motion { None, Animate, Pulse };

	QRectF trackRect() const;
	QRectF handleRect() const;
	qreal handleCenterX() const;
	int valueForX(qreal x) const;
	void setVisualValue(int value);
	void applyUserValue(int value);
	void finishDrag();
	void stopMotion();
	void startMotion(Motion type, int durationMs, int target, bool emitChanges);
	void onMotionTick();
	void updateCursorFor(const QPoint &pos);

	static constexpr int MaxValue = 1023;
	static constexpr int HandleWidth = 26;
	static constexpr int HandleHeight = 36;
	static constexpr int WheelStep = 25;
	static constexpr int KeyStep = 10;
	static constexpr int PageStep = 100;

	int currentValue = 0;
	bool dragging = false;
	bool dragMoved = false;
	bool endNotified = false;
	bool locked = false;
	bool handleHovered = false;
	bool pressed = false;
	qreal pressOffset = 0.0;

	Motion motion = Motion::None;
	QTimer *motionTimer = nullptr;
	QElapsedTimer motionClock;
	int motionFrom = 0;
	int motionTo = 0;
	int motionDuration = 0;
	bool motionEmits = false;
};
