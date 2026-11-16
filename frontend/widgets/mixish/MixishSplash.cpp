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

#include "MixishSplash.hpp"
#include "MixishTheme.hpp"

#include <OBSApp.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPropertyAnimation>
#include <QRadialGradient>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

#include "moc_MixishSplash.cpp"

namespace {

/* Design tokens, see MIXISH_DESIGN_SYSTEM.md. */
const QColor BgTop("#1C1C1C");
const QColor BgBottom("#141414");
const QColor BorderColor("#3A3A3A");
const QColor TextPrimary("#F1F1F1");
const QColor TextSecondary("#B8B8B8");
const QColor TextMuted("#8C8C8C");
const QColor TextDisabled("#5E5E5E");
const QColor Accent("#4C8DFF");
const QColor Track("#2A2A2A");

constexpr int SplashWidth = 480;
constexpr int SplashHeight = 300;
constexpr int PanelRadius = 4;
constexpr int ContentMargin = 32;

/* Keep the splash on screen long enough to be read; OBS can be ready in
 * under a second on a warm start. */
constexpr int MinimumVisibleMs = 1100;
constexpr int FadeOutMs = 220;

} // namespace

/* Indeterminate progress sweep. Drawn by hand so the animation matches the
 * accent token exactly instead of borrowing an OBS style. */
class MixishSplashProgress : public QWidget {
public:
	explicit MixishSplashProgress(QWidget *parent) : QWidget(parent)
	{
		setFixedHeight(3);
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

		connect(&timer, &QTimer::timeout, this, [this]() {
			phase += 0.0075;
			if (phase > 1.0)
				phase -= 1.0;
			update();
		});
		timer.start(16);
	}

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing, true);

		const QRectF trackRect(0, 0, width(), height());
		QPainterPath trackPath;
		trackPath.addRoundedRect(trackRect, height() / 2.0, height() / 2.0);
		painter.fillPath(trackPath, Track);

		const qreal chunk = width() * 0.32;
		const qreal travel = width() + chunk;
		const qreal x = -chunk + phase * travel;

		QPainterPath chunkPath;
		chunkPath.addRoundedRect(QRectF(x, 0, chunk, height()), height() / 2.0, height() / 2.0);

		QLinearGradient fade(x, 0, x + chunk, 0);
		QColor left = Accent;
		QColor right = Accent;
		left.setAlpha(80);
		right.setAlpha(80);
		fade.setColorAt(0.0, left);
		fade.setColorAt(0.5, Accent);
		fade.setColorAt(1.0, right);
		painter.fillPath(chunkPath, fade);
	}

private:
	QTimer timer;
	qreal phase = 0.0;
};

MixishSplash::MixishSplash(QWidget *parent) : QWidget(parent)
{
	/* SplashScreen: frameless and no taskbar entry, but not always on top,
	 * so a modal OBS dialog can still appear above it. */
	setWindowFlags(Qt::SplashScreen);
	/* Opaque on purpose. A translucent window is invisible until the first
	 * paint actually reaches the screen, and OBS runs a long synchronous
	 * init right after this point, so on a cold start that paint can be
	 * delayed past the whole splash. Opaque windows fall back to their
	 * native background instead of vanishing. Rounded corners come from a
	 * mask so the design still matches the app. */
	setAttribute(Qt::WA_TranslucentBackground, false);
	setAttribute(Qt::WA_OpaquePaintEvent, true);
	setFixedSize(SplashWidth, SplashHeight);
	setObjectName(QStringLiteral("mixishSplash"));
	setWindowTitle(QStringLiteral("Mixish"));

	QPainterPath shape;
	shape.addRoundedRect(QRectF(rect()), PanelRadius, PanelRadius);
	setMask(QRegion(shape.toFillPolygon().toPolygon()));

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(ContentMargin, 0, ContentMargin, 30);
	layout->setSpacing(0);

	layout->addStretch(3);

	auto *logo = new QLabel(this);
	logo->setPixmap(QIcon(QStringLiteral(":/res/images/mixish-icon.svg")).pixmap(64, 64));
	logo->setFixedSize(64, 64);
	layout->addWidget(logo, 0, Qt::AlignHCenter);

	layout->addSpacing(16);

	auto *wordmark = new QLabel(QStringLiteral("MIXISH"), this);
	wordmark->setAlignment(Qt::AlignHCenter);
	QFont wordmarkFont = wordmark->font();
	wordmarkFont.setPixelSize(30);
	wordmarkFont.setWeight(QFont::Black);
	wordmarkFont.setLetterSpacing(QFont::AbsoluteSpacing, 9.0);
	wordmark->setFont(wordmarkFont);
	wordmark->setStyleSheet(
		QStringLiteral("color: %1; padding-left: 9px;").arg(TextPrimary.name()));
	layout->addWidget(wordmark);

	layout->addSpacing(6);

	auto *tagline = new QLabel(tr("LIVE PRODUCTION SWITCHER"), this);
	tagline->setAlignment(Qt::AlignHCenter);
	QFont taglineFont = tagline->font();
	taglineFont.setPixelSize(10);
	taglineFont.setWeight(QFont::Bold);
	taglineFont.setLetterSpacing(QFont::AbsoluteSpacing, 3.0);
	tagline->setFont(taglineFont);
	tagline->setStyleSheet(QStringLiteral("color: %1; padding-left: 3px;").arg(TextMuted.name()));
	layout->addWidget(tagline);

	layout->addStretch(4);

	progress = new MixishSplashProgress(this);
	layout->addWidget(progress);

	layout->addSpacing(14);

	auto *footer = new QHBoxLayout();
	footer->setContentsMargins(0, 0, 0, 0);
	footer->setSpacing(8);

	statusLabel = new QLabel(tr("Starting Mixish..."), this);
	statusLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(TextSecondary.name()));
	footer->addWidget(statusLabel, 1);

	versionLabel = new QLabel(App() ? QString::fromStdString(App()->GetVersionString(false)) : QString(), this);
	versionLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(TextDisabled.name()));
	versionLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	footer->addWidget(versionLabel, 0);

	layout->addLayout(footer);

	MixishTheme::Apply(this);
}

void MixishSplash::SetStatus(const QString &text)
{
	if (!statusLabel || text.isEmpty())
		return;

	statusLabel->setText(text);
	QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void MixishSplash::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);

	/* No fade-in on purpose: OBS startup runs a long synchronous init right
	 * after this, so an opacity animation would never get to run and the
	 * splash would stay invisible. Appear fully opaque and fade out later. */
	setWindowOpacity(1.0);
	repaint();
}

void MixishSplash::Finish()
{
	if (finishing)
		return;
	finishing = true;

	auto *fade = new QPropertyAnimation(this, "windowOpacity", this);
	fade->setDuration(FadeOutMs);
	fade->setStartValue(windowOpacity());
	fade->setEndValue(0.0);
	fade->setEasingCurve(QEasingCurve::InCubic);
	connect(fade, &QPropertyAnimation::finished, this, [this]() { deleteLater(); });
	fade->start(QAbstractAnimation::DeleteWhenStopped);
}

void MixishSplash::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing, true);

	const QRectF panel = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);

	/* Paint every pixel: the window is opaque and must never show an
	 * unpainted frame while startup is blocking the event loop. */
	painter.fillRect(rect(), BgBottom);

	/* Soft accent glow behind the wordmark, very low alpha so the splash
	 * stays dark and does not fight the app's palette. */
	QRadialGradient glow(QPointF(width() / 2.0, 96.0), 150.0);
	QColor glowStart = Accent;
	glowStart.setAlpha(34);
	QColor glowEnd = Accent;
	glowEnd.setAlpha(0);
	glow.setColorAt(0.0, glowStart);
	glow.setColorAt(1.0, glowEnd);
	painter.fillRect(rect(), glow);

	QPainterPath panelPath;
	panelPath.addRoundedRect(panel, PanelRadius, PanelRadius);

	QLinearGradient background(0, 0, 0, height());
	background.setColorAt(0.0, BgTop);
	background.setColorAt(1.0, BgBottom);
	painter.fillPath(panelPath, background);

	painter.setPen(QPen(BorderColor, 1.0));
	painter.drawPath(panelPath);
}

namespace MixishSplashScreen {

namespace {
QPointer<MixishSplash> splash;
QElapsedTimer shownAt;
} // namespace

void Show()
{
	if (splash)
		return;

	if (!qApp || !QApplication::instance())
		return;

	splash = new MixishSplash();
	shownAt.start();

	/* Centre on the primary screen, like the main window. */
	const QRect available = splash->screen() ? splash->screen()->availableGeometry()
						 : QRect(0, 0, SplashWidth, SplashHeight);
	const QPoint topLeft(available.center().x() - SplashWidth / 2,
			     available.center().y() - SplashHeight / 2);
	splash->move(topLeft);

	splash->show();
	splash->raise();
	/* Force the first frame out now: OBS startup runs a long series of
	 * synchronous init steps and the event loop may not spin for a while. */
	QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
	splash->repaint();
}

void SetStatus(const QString &text)
{
	if (!splash)
		return;

	splash->SetStatus(text);
}

void Finish()
{
	if (!splash)
		return;

	QPointer<MixishSplash> pending = splash;
	splash = nullptr;

	const qint64 shown = shownAt.isValid() ? shownAt.elapsed() : MinimumVisibleMs;
	const int remaining = int(MinimumVisibleMs - shown);
	if (remaining > 0) {
		QTimer::singleShot(remaining, pending, [pending]() {
			if (pending)
				pending->Finish();
		});
		return;
	}

	pending->Finish();
}

} // namespace MixishSplashScreen
