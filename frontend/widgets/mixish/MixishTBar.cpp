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

#include "MixishTBar.hpp"

#include <QEnterEvent>
#include <QCursor>
#include <QEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

#include "moc_MixishTBar.cpp"

namespace {

const QColor PreviewColor(0x1D, 0x9E, 0x75);
const QColor ProgramColor(0xC0, 0x39, 0x2B);
const QColor AccentColor(0x4C, 0x8D, 0xFF);
const QColor TrackColor(0x2A, 0x2A, 0x2A);
const QColor BorderColor(0x3A, 0x3A, 0x3A);
const QColor HandleColor(0xF1, 0xF1, 0xF1);
const QColor HandleHotColor(0xFF, 0xFF, 0xFF);
const QColor HandlePressedColor(0xE2, 0xE2, 0xE2);
const QColor HandleLockedColor(0x8A, 0x8A, 0x8A);
const QColor HandleDisabledColor(0x44, 0x44, 0x44);
const QColor GripColor(0x8C, 0x8C, 0x8C);
const QColor MutedColor(0x8C, 0x8C, 0x8C);
const QColor DisabledTextColor(0x5E, 0x5E, 0x5E);

static QPointF mousePosition(QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	return event->position();
#else
	return event->localPos();
#endif
}

} // namespace

MixishTBar::MixishTBar(QWidget *parent) : QWidget(parent)
{
	setObjectName(QStringLiteral("mixishTBar"));
	setFixedHeight(64);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	setFocusPolicy(Qt::StrongFocus);
	setMouseTracking(true);
	setCursor(Qt::ArrowCursor);
	setAccessibleName(tr("T-bar"));
	setAccessibleDescription(tr("Manual transition between Preview and Program"));
	setToolTip(tr("Drag to mix Preview into Program.\nMouse wheel or arrow keys nudge, Esc cancels."));

	motionTimer = new QTimer(this);
	motionTimer->setInterval(16);
	connect(motionTimer, &QTimer::timeout, this, &MixishTBar::onMotionTick);
}

QRectF MixishTBar::trackRect() const
{
	const qreal margin = HandleWidth / 2.0 + 2.0;
	return QRectF(margin, 26.0, std::max(1.0, width() - margin * 2.0), 12.0);
}

qreal MixishTBar::handleCenterX() const
{
	const QRectF track = trackRect();
	return track.left() + track.width() * (currentValue / qreal(MaxValue));
}

QRectF MixishTBar::handleRect() const
{
	const QRectF track = trackRect();
	const qreal centerX = handleCenterX();
	return QRectF(centerX - HandleWidth / 2.0, track.center().y() - HandleHeight / 2.0,
		      HandleWidth, HandleHeight);
}

int MixishTBar::valueForX(qreal x) const
{
	const QRectF track = trackRect();
	if (track.width() <= 0.0)
		return 0;

	qreal t = (x - track.left()) / track.width();
	t = std::clamp(t, 0.0, 1.0);

	int value = int(std::lround(t * MaxValue));
	if (t <= 0.02)
		value = 0;
	else if (t >= 0.98)
		value = MaxValue;
	return value;
}

void MixishTBar::setVisualValue(int value)
{
	currentValue = std::clamp(value, 0, MaxValue);
	update();
}

void MixishTBar::setValue(int value)
{
	stopMotion();
	setVisualValue(value);
	endNotified = false;
}

void MixishTBar::setExternalProgress(float progress)
{
	stopMotion();
	if (dragging) {
		dragging = false;
		pressed = false;
		if (mouseGrabber() == this)
			releaseMouse();
	}
	setVisualValue(int(std::lround(std::clamp(progress, 0.0f, 1.0f) * MaxValue)));
	endNotified = false;
}

void MixishTBar::setLocked(bool value)
{
	if (locked == value)
		return;

	locked = value;
	if (locked && dragging) {
		dragging = false;
		pressed = false;
		if (mouseGrabber() == this)
			releaseMouse();
	}

	if (locked) {
		handleHovered = false;
		setCursor(Qt::ArrowCursor);
	}
	update();
}

void MixishTBar::applyUserValue(int value)
{
	const int clamped = std::clamp(value, 0, MaxValue);
	if (clamped == currentValue)
		return;

	setVisualValue(clamped);
	emit valueChanged(clamped);

	if (clamped >= MaxValue && !endNotified) {
		endNotified = true;
		emit reachedEnd(true);
	} else if (clamped <= 0 && !endNotified) {
		endNotified = true;
		emit reachedEnd(false);
	}

	update();
}

void MixishTBar::finishDrag()
{
	if (!dragging)
		return;

	dragging = false;
	pressed = false;
	if (mouseGrabber() == this)
		releaseMouse();
	setCursor(Qt::ArrowCursor);
	update();
}

void MixishTBar::stopMotion()
{
	if (motionTimer)
		motionTimer->stop();
	motion = Motion::None;
}

void MixishTBar::startMotion(Motion type, int durationMs, int target, bool emitChanges)
{
	stopMotion();

	motion = type;
	motionFrom = currentValue;
	motionTo = target;
	motionDuration = std::max(1, durationMs);
	motionEmits = emitChanges;
	motionClock.restart();
	motionTimer->start();
	update();
}

void MixishTBar::animateTo(int target, int durationMs, bool emitChanges)
{
	if (!isEnabled() || motion == Motion::Pulse)
		return;

	target = std::clamp(target, 0, MaxValue);
	if (durationMs <= 0 || currentValue == target) {
		setVisualValue(target);
		if (emitChanges)
			emit valueChanged(target);
		emit animationFinished();
		return;
	}

	startMotion(Motion::Animate, durationMs, target, emitChanges);
}

void MixishTBar::playPulse()
{
	if (!isEnabled() || locked || motion != Motion::None)
		return;

	startMotion(Motion::Pulse, 260, 0, false);
}

void MixishTBar::resetToStart()
{
	stopMotion();
	dragging = false;
	pressed = false;
	dragMoved = false;
	endNotified = false;
	if (mouseGrabber() == this)
		releaseMouse();
	setVisualValue(0);
	setCursor(Qt::ArrowCursor);
	update();
}

void MixishTBar::onMotionTick()
{
	if (motion == Motion::None) {
		motionTimer->stop();
		return;
	}

	const qreal elapsed = motionClock.elapsed();

	if (motion == Motion::Animate) {
		const qreal t = std::min(1.0, elapsed / qreal(motionDuration));
		const qreal eased = 1.0 - std::pow(1.0 - t, 3.0);
		const int value = motionFrom + int(std::lround((motionTo - motionFrom) * eased));

		if (value != currentValue) {
			setVisualValue(value);
			if (motionEmits)
				emit valueChanged(value);
		}

		if (t >= 1.0) {
			if (currentValue != motionTo) {
				setVisualValue(motionTo);
				if (motionEmits)
					emit valueChanged(motionTo);
			}
			stopMotion();
			emit animationFinished();
		}
		return;
	}

	/* Pulse: a visual-only 0 -> 100 -> 0 sweep used for Cut. */
	const qreal t = std::min(1.0, elapsed / qreal(motionDuration));
	const qreal triangle = t < 0.5 ? t * 2.0 : (1.0 - t) * 2.0;
	setVisualValue(int(std::lround(MaxValue * triangle)));

	if (t >= 1.0) {
		setVisualValue(0);
		stopMotion();
		emit animationFinished();
	}
}

void MixishTBar::updateCursorFor(const QPoint &pos)
{
	if (!isEnabled() || locked) {
		if (handleHovered) {
			handleHovered = false;
			update();
		}
		setCursor(Qt::ArrowCursor);
		return;
	}

	if (dragging) {
		setCursor(Qt::ClosedHandCursor);
		return;
	}

	const QRectF hit = handleRect().adjusted(-12.0, -4.0, 12.0, 4.0);
	const bool over = hit.contains(pos);
	if (over != handleHovered) {
		handleHovered = over;
		update();
	}
	setCursor(over ? Qt::SizeHorCursor : Qt::ArrowCursor);
}

void MixishTBar::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing, true);
	if (!isEnabled())
		painter.setOpacity(0.55);

	const bool enabled = isEnabled();
	const bool interactive = enabled && !locked;
	const QRectF track = trackRect();
	const qreal radius = track.height() / 2.0;

	QPainterPath trackPath;
	trackPath.addRoundedRect(track, radius, radius);

	/* Track. */
	const QColor trackBorder = (handleHovered && interactive) ? QColor(0x55, 0x55, 0x55)
								  : BorderColor;
	painter.setPen(QPen(trackBorder, 1.0));
	painter.setBrush(TrackColor);
	painter.drawPath(trackPath);

	/* Filled portion: reveal the Preview -> Program gradient up to the grip. */
	const qreal fillRight = handleCenterX();
	if (fillRight > track.left() + 1.0) {
		painter.save();
		painter.setClipPath(trackPath);
		painter.setClipRect(QRectF(track.left(), track.top(), fillRight - track.left(),
					   track.height()),
				    Qt::IntersectClip);

		QLinearGradient gradient(track.left(), 0.0, track.right(), 0.0);
		gradient.setColorAt(0.0, PreviewColor);
		gradient.setColorAt(1.0, ProgramColor);
		painter.fillRect(track, gradient);
		painter.restore();
	}

	/* Tick marks along the track. */
	painter.save();
	painter.setClipPath(trackPath);
	painter.setPen(QPen(QColor(255, 255, 255, 50), 1.0));
	for (int i = 0; i <= 10; i++) {
		const qreal x = track.left() + track.width() * (i / 10.0);
		const qreal length = (i % 5 == 0) ? 8.0 : 4.0;
		painter.drawLine(QPointF(x, track.center().y() - length / 2.0),
				 QPointF(x, track.center().y() + length / 2.0));
	}
	painter.restore();

	/* Header row: T-BAR + drag percentage. */
	QFont labelFont = font();
	labelFont.setPixelSize(9);
	labelFont.setBold(true);
	painter.setFont(labelFont);
	painter.setPen(enabled ? MutedColor : DisabledTextColor);
	painter.drawText(QRectF(track.left(), 0.0, width() / 2.0, 13.0),
			 Qt::AlignLeft | Qt::AlignVCenter, tr("T-BAR"));

	if (dragging) {
		QFont percentFont = font();
		percentFont.setPixelSize(10);
		percentFont.setBold(true);
		painter.setFont(percentFont);
		painter.setPen(interactive ? HandleColor : HandleLockedColor);
		const int percent = int(std::lround(currentValue * 100.0 / MaxValue));
		painter.drawText(QRectF(width() / 2.0, 0.0, width() - width() / 2.0 - track.left(), 13.0),
				 Qt::AlignRight | Qt::AlignVCenter,
				 QStringLiteral("%1%").arg(percent));
	}

	/* End labels. */
	QFont endFont = font();
	endFont.setPixelSize(10);
	endFont.setBold(true);
	painter.setFont(endFont);

	QColor previewLabel = enabled ? PreviewColor : DisabledTextColor;
	QColor programLabel = enabled ? ProgramColor : DisabledTextColor;
	if (locked && enabled) {
		previewLabel.setAlpha(190);
		programLabel.setAlpha(190);
	}
	painter.setPen(previewLabel);
	painter.drawText(QRectF(track.left(), height() - 13.0, 48.0, 13.0),
			 Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("PVW"));
	painter.setPen(programLabel);
	painter.drawText(QRectF(track.right() - 48.0, height() - 13.0, 48.0, 13.0),
			 Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("PGM"));

	/* Handle. */
	const QRectF handle = handleRect();
	const bool hot = handleHovered && interactive && !dragging;
	const bool active = pressed || dragging;

	QColor fill = HandleColor;
	QColor border = BorderColor;
	QColor grip = GripColor;

	if (!enabled) {
		fill = HandleDisabledColor;
		border = BorderColor;
		grip = DisabledTextColor;
	} else if (locked) {
		fill = HandleLockedColor;
		border = QColor(0x55, 0x55, 0x55);
		grip = QColor(0x6A, 0x6A, 0x6A);
	} else if (active) {
		fill = HandlePressedColor;
		border = AccentColor;
		grip = AccentColor;
	} else if (hot) {
		fill = HandleHotColor;
		border = AccentColor;
		grip = AccentColor;
	}

	/* Soft shadow under the grip. */
	painter.setPen(Qt::NoPen);
	painter.setBrush(QColor(0, 0, 0, 70));
	painter.drawRoundedRect(handle.translated(0.0, 1.0), 6.0, 6.0);

	painter.setPen(QPen(border, 1.0));
	painter.setBrush(fill);
	painter.drawRoundedRect(handle.adjusted(0.5, 0.5, -0.5, -0.5), 6.0, 6.0);

	painter.setPen(QPen(grip, 2.0, Qt::SolidLine, Qt::RoundCap));
	const qreal centerX = handle.center().x();
	const qreal centerY = handle.center().y();
	painter.drawLine(QPointF(centerX, centerY - 5.0), QPointF(centerX, centerY + 5.0));

	/* Keyboard focus ring. */
	if (hasFocus() && interactive) {
		painter.setPen(QPen(QColor(0x4C, 0x8D, 0xFF, 170), 1.0));
		painter.setBrush(Qt::NoBrush);
		painter.drawRoundedRect(QRectF(1.5, 1.5, width() - 3.0, height() - 3.0), 4.0, 4.0);
	}
}

void MixishTBar::mousePressEvent(QMouseEvent *event)
{
	if (!isEnabled() || locked || motion != Motion::None) {
		event->ignore();
		return;
	}
	if (event->button() != Qt::LeftButton) {
		QWidget::mousePressEvent(event);
		return;
	}

	const QPointF pos = mousePosition(event);
	const QRectF hit = handleRect().adjusted(-12.0, -4.0, 12.0, 4.0);
	if (!hit.contains(pos)) {
		/* Never jump the transition from a track click: only the grip
		 * starts a drag. */
		event->ignore();
		return;
	}

	dragging = true;
	dragMoved = false;
	endNotified = false;
	pressed = true;
	pressOffset = pos.x() - handleCenterX();
	setFocus(Qt::MouseFocusReason);
	grabMouse();
	setCursor(Qt::ClosedHandCursor);
	update();
	event->accept();
}

void MixishTBar::mouseMoveEvent(QMouseEvent *event)
{
	if (!dragging) {
		updateCursorFor(mousePosition(event).toPoint());
		event->ignore();
		return;
	}

	const QPointF pos = mousePosition(event);
	const int value = valueForX(pos.x() - pressOffset);
	if (value != currentValue) {
		dragMoved = true;
		setVisualValue(value);
		emit valueChanged(value);

		if (value >= MaxValue && !endNotified) {
			endNotified = true;
			emit reachedEnd(true);
			finishDrag();
			return;
		}
		if (value <= 0 && !endNotified) {
			endNotified = true;
			emit reachedEnd(false);
			finishDrag();
			return;
		}
	}

	update();
	event->accept();
}

void MixishTBar::mouseReleaseEvent(QMouseEvent *event)
{
	if (!dragging) {
		QWidget::mouseReleaseEvent(event);
		return;
	}
	if (event->button() != Qt::LeftButton) {
		event->ignore();
		return;
	}

	const int value = currentValue;
	const bool moved = dragMoved;
	finishDrag();

	if (moved)
		emit dragFinished(value);

	update();
	event->accept();
}

void MixishTBar::wheelEvent(QWheelEvent *event)
{
	if (!isEnabled() || locked || motion != Motion::None) {
		event->ignore();
		return;
	}

	const int delta = event->angleDelta().y();
	if (delta == 0) {
		event->ignore();
		return;
	}

	applyUserValue(currentValue + (delta > 0 ? WheelStep : -WheelStep));
	event->accept();
}

void MixishTBar::keyPressEvent(QKeyEvent *event)
{
	if (!isEnabled() || locked || motion != Motion::None) {
		event->ignore();
		return;
	}

	switch (event->key()) {
	case Qt::Key_Left:
	case Qt::Key_Down:
		applyUserValue(currentValue - KeyStep);
		break;
	case Qt::Key_Right:
	case Qt::Key_Up:
		applyUserValue(currentValue + KeyStep);
		break;
	case Qt::Key_PageDown:
		applyUserValue(currentValue - PageStep);
		break;
	case Qt::Key_PageUp:
		applyUserValue(currentValue + PageStep);
		break;
	case Qt::Key_Home:
		applyUserValue(0);
		break;
	case Qt::Key_End:
		applyUserValue(MaxValue);
		break;
	case Qt::Key_Escape:
		if (dragging)
			finishDrag();
		emit cancelRequested();
		break;
	default:
		QWidget::keyPressEvent(event);
		return;
	}

	event->accept();
}

void MixishTBar::enterEvent(QEnterEvent *event)
{
	updateCursorFor(mapFromGlobal(QCursor::pos()));
	update();
	QWidget::enterEvent(event);
}

void MixishTBar::leaveEvent(QEvent *event)
{
	handleHovered = false;
	if (!dragging)
		setCursor(Qt::ArrowCursor);
	update();
	QWidget::leaveEvent(event);
}

void MixishTBar::changeEvent(QEvent *event)
{
	if (event->type() == QEvent::EnabledChange) {
		if (!isEnabled()) {
			if (dragging) {
				dragging = false;
				pressed = false;
				if (mouseGrabber() == this)
					releaseMouse();
			}
			handleHovered = false;
			setCursor(Qt::ArrowCursor);
		}
		update();
	}
	QWidget::changeEvent(event);
}
