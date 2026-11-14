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

#include "MixishInputTile.hpp"
#include "MixishSwitcher.hpp"
#include "MixishTheme.hpp"

#include <utility/ThumbnailView.hpp>
#include <widgets/OBSBasic.hpp>

#include <QFont>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPushButton>
#include <QSizePolicy>
#include <QStyle>
#include <QVBoxLayout>

#include "moc_MixishInputTile.cpp"

#include <algorithm>

static const QVector<QColor> &ScenePalette()
{
	static const QVector<QColor> palette = {
		QColor(QStringLiteral("#1D9E75")), QColor(QStringLiteral("#D85A30")),
		QColor(QStringLiteral("#EF9F27")), QColor(QStringLiteral("#3D8BD4")),
		QColor(QStringLiteral("#805FD3")), QColor(QStringLiteral("#E03E84")),
		QColor(QStringLiteral("#37D247")), QColor(QStringLiteral("#8A8A80")),
	};
	return palette;
}

MixishInputTile::MixishInputTile(MixishSwitcher *switcher_, OBSScene scene_, int number_, QWidget *parent)
	: QFrame(parent),
	  switcher(switcher_),
	  number(number_)
{
	setObjectName(QStringLiteral("mixishInputTile"));
	setFixedSize(208, 196);
	setCursor(Qt::PointingHandCursor);
	setFocusPolicy(Qt::NoFocus);

	auto *root = new QHBoxLayout(this);
	root->setContentsMargins(0, 0, 0, 0);
	root->setSpacing(0);

	colorTag = new QFrame(this);
	colorTag->setFixedWidth(5);
	colorTag->setAttribute(Qt::WA_TransparentForMouseEvents);
	root->addWidget(colorTag);

	auto *content = new QWidget(this);
	auto *contentLayout = new QVBoxLayout(content);
	contentLayout->setContentsMargins(4, 4, 4, 4);
	contentLayout->setSpacing(2);

	header = new QFrame(content);
	header->setObjectName(QStringLiteral("mixishTileHeader"));
	header->setFixedHeight(26);
	header->setAttribute(Qt::WA_TransparentForMouseEvents);

	auto *headerLayout = new QHBoxLayout(header);
	headerLayout->setContentsMargins(5, 2, 5, 2);
	headerLayout->setSpacing(MixishTheme::Space1);

	numberLabel = new QLabel(header);
	numberLabel->setObjectName(QStringLiteral("mixishTileNumber"));
	numberLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
	numberLabel->setFixedWidth(14);

	nameLabel = new QLabel(header);
	nameLabel->setObjectName(QStringLiteral("mixishTileName"));
	nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
	nameLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

	previewBadge = new QLabel(QStringLiteral("PVW"), header);
	previewBadge->setObjectName(QStringLiteral("mixishPvwBadge"));
	previewBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
	previewBadge->hide();

	programBadge = new QLabel(QStringLiteral("PGM"), header);
	programBadge->setObjectName(QStringLiteral("mixishPgmBadge"));
	programBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
	programBadge->hide();

	headerLayout->addWidget(numberLabel);
	headerLayout->addWidget(nameLabel, 1);
	headerLayout->addWidget(previewBadge);
	headerLayout->addWidget(programBadge);

	thumbnailLabel = new QLabel(content);
	thumbnailLabel->setObjectName(QStringLiteral("mixishThumb"));
	thumbnailLabel->setFixedSize(190, 102);
	thumbnailLabel->setAlignment(Qt::AlignCenter);
	thumbnailLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

	auto *footer = new QWidget(content);
	auto *footerLayout = new QVBoxLayout(footer);
	footerLayout->setContentsMargins(0, 0, 0, 0);
	footerLayout->setSpacing(MixishTheme::Space1);

	auto *actionRow = new QHBoxLayout();
	actionRow->setContentsMargins(0, 0, 0, 0);
	actionRow->setSpacing(MixishTheme::Space1);

	cutButton = new QPushButton(tr("Cut"), footer);
	MixishTheme::SetVariant(cutButton, "secondary");
	MixishTheme::SetRole(cutButton, "tileAction");
	cutButton->setFixedHeight(MixishTheme::CompactHeight);
	cutButton->setToolTip(tr("Cut this scene directly to Program"));
	actionRow->addWidget(cutButton);
	actionRow->addStretch(1);

	MixishOverlayController *overlays = switcher ? switcher->GetOverlays() : nullptr;
	const int overlayCount = overlays ? overlays->ChannelCount() : 0;
	for (int i = 0; i < overlayCount; ++i) {
		auto *overlayButton = new QPushButton(QString::number(i + 1), footer);
		MixishTheme::SetVariant(overlayButton, "secondary");
		MixishTheme::SetRole(overlayButton, "tileOverlay");
		overlayButton->setFixedSize(MixishTheme::CompactHeight, MixishTheme::CompactHeight);
		overlayButton->setFocusPolicy(Qt::NoFocus);
		overlayButton->setToolTip(tr("Assign this input to overlay %1").arg(i + 1));
		connect(overlayButton, &QPushButton::clicked, this, [this, i]() { OnOverlayClicked(i); });
		actionRow->addWidget(overlayButton);
		overlayButtons.push_back(overlayButton);
	}

	auto *utilRow = new QHBoxLayout();
	utilRow->setContentsMargins(0, 0, 0, 0);
	utilRow->setSpacing(MixishTheme::Space1);

	audioButton = new QPushButton(tr("Audio"), footer);
	MixishTheme::SetVariant(audioButton, "secondary");
	MixishTheme::SetRole(audioButton, "tileAction");
	audioButton->setFixedHeight(MixishTheme::CompactHeight);
	audioButton->setToolTip(tr("Mute or unmute this input"));
	connect(audioButton, &QPushButton::clicked, this, [this]() {
		OBSSource source = PrimaryAudioSource();
		if (!source)
			return;
		obs_source_set_muted(source, !obs_source_muted(source));
		RefreshAudioState();
	});
	utilRow->addWidget(audioButton);

	settingsButton = new QPushButton(tr("Settings"), footer);
	MixishTheme::SetVariant(settingsButton, "secondary");
	MixishTheme::SetRole(settingsButton, "tileAction");
	settingsButton->setFixedHeight(MixishTheme::CompactHeight);
	settingsButton->setToolTip(tr("Input properties (right-click for scene menu)"));
	settingsButton->setAccessibleName(tr("Input properties"));

	menuButton = new QPushButton(QStringLiteral("\u22ee"), footer);
	menuButton->setObjectName(QStringLiteral("mixishTileMenuButton"));
	MixishTheme::SetVariant(menuButton, "secondary");
	MixishTheme::SetRole(menuButton, "tileAction");
	menuButton->setFixedSize(MixishTheme::CompactHeight, MixishTheme::CompactHeight);
	menuButton->setToolTip(tr("Scene menu"));
	menuButton->setAccessibleName(tr("Scene menu"));

	utilRow->addStretch(1);
	utilRow->addWidget(settingsButton);
	utilRow->addWidget(menuButton);

	footerLayout->addLayout(actionRow);
	footerLayout->addLayout(utilRow);

	contentLayout->addWidget(header);
	contentLayout->addWidget(thumbnailLabel, 0, Qt::AlignHCenter);
	contentLayout->addWidget(footer);
	root->addWidget(content, 1);

	OBSSource source = obs_scene_get_source(scene_);
	weakScene = OBSGetWeakRef(source);
	if (source) {
		thumbnailView = App()->thumbnails()->createView(this, source);
		connect(thumbnailView, &ThumbnailView::updated, this, &MixishInputTile::UpdateThumbnail);
		UpdateThumbnail(thumbnailView->getPixmap());
		thumbnailView->requestUpdate();
	}

	connect(cutButton, &QPushButton::clicked, this, [this]() {
		if (switcher)
			switcher->CutToProgram(Scene());
	});
	connect(settingsButton, &QPushButton::clicked, this, [this]() {
		if (switcher)
			switcher->OpenInputProperties(Scene(), number);
	});
	connect(menuButton, &QPushButton::clicked, this, [this]() {
		if (switcher)
			switcher->OpenSceneMenu(Scene(), menuButton);
	});

	if (switcher)
		connect(switcher, &MixishSwitcher::OverlaysChanged, this,
			&MixishInputTile::RefreshOverlayButtons);

	SetNumber(number);
	RefreshName();
	UpdateColorTag();
	MixishTheme::Apply(this);
	ApplyState();
	RefreshOverlayButtons();
	RefreshAudioState();

	for (QWidget *child : findChildren<QWidget *>())
		child->installEventFilter(this);
}

const QVector<QColor> &MixishInputTile::Palette()
{
	return ScenePalette();
}

OBSScene MixishInputTile::Scene() const
{
	OBSSource source = OBSGetStrongRef(weakScene);
	if (!source)
		return OBSScene();
	return OBSScene(obs_scene_from_source(source));
}

void MixishInputTile::SetSceneColor(OBSScene scene, const QColor &color)
{
	if (!scene)
		return;

	OBSSource source = obs_scene_get_source(scene);
	if (!source)
		return;

	OBSDataAutoRelease settings = obs_source_get_private_settings(source);
	if (color.isValid())
		obs_data_set_string(settings, "mixish_color", color.name().toUtf8().constData());
	else
		obs_data_set_string(settings, "mixish_color", "");
}

void MixishInputTile::SetNumber(int number_)
{
	number = number_;
	if (numberLabel)
		numberLabel->setText(QString::number(number));
	UpdateColorTag();
	RefreshName();
}

void MixishInputTile::RefreshName()
{
	OBSScene currentScene = Scene();
	OBSSource source = currentScene ? obs_scene_get_source(currentScene) : nullptr;
	const QString name = source ? QString::fromUtf8(obs_source_get_name(source)) : QString();

	if (nameLabel)
		nameLabel->setText(name);
	setToolTip(tr("%1  %2").arg(number).arg(name));
	setAccessibleName(name);
}

void MixishInputTile::UpdateColorTag()
{
	if (!colorTag)
		return;

	QColor color;
	OBSScene currentScene = Scene();
	OBSSource source = currentScene ? obs_scene_get_source(currentScene) : nullptr;
	if (source) {
		OBSDataAutoRelease settings = obs_source_get_private_settings(source);
		const char *stored = obs_data_get_string(settings, "mixish_color");
		if (stored && *stored)
			color = QColor(QString::fromUtf8(stored));
	}

	if (!color.isValid()) {
		const auto &palette = ScenePalette();
		color = palette[(std::max(number, 1) - 1) % palette.size()];
	}

	QPalette palette = colorTag->palette();
	palette.setColor(QPalette::Window, color);
	colorTag->setAutoFillBackground(true);
	colorTag->setPalette(palette);
}

void MixishInputTile::SetState(bool preview, bool program)
{
	isPreview = preview;
	isProgram = program;
	ApplyState();
}

void MixishInputTile::SetSelected(bool selected)
{
	isSelected = selected;
	ApplyState();
}

void MixishInputTile::ApplyState()
{
	const QString state = isProgram ? QStringLiteral("program")
			      : isPreview ? QStringLiteral("preview")
			      : isSelected ? QStringLiteral("selected")
					   : QStringLiteral("idle");

	MixishTheme::SetState(this, state.toUtf8().constData());

	if (previewBadge)
		previewBadge->setVisible(isPreview);
	if (programBadge)
		programBadge->setVisible(isProgram);
}

void MixishInputTile::RequestThumbnail()
{
	if (thumbnailView)
		thumbnailView->requestUpdate();
}

void MixishInputTile::UpdateThumbnail(const QPixmap &pixmap)
{
	if (!thumbnailLabel || pixmap.isNull())
		return;

	const QSize target = thumbnailLabel->size();
	if (target.isEmpty())
		return;

	QPixmap scaled = pixmap.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);

	/* Draw a checkerboard behind the thumbnail so transparent inputs
	 * (images with alpha, text) are distinguishable from black. */
	QPixmap canvas(target);
	canvas.fill(QColor(0x18, 0x18, 0x18));

	QPainter painter(&canvas);
	const int cell = 8;
	for (int y = 0; y < target.height(); y += cell) {
		for (int x = 0; x < target.width(); x += cell) {
			if (((x / cell) + (y / cell)) % 2 == 0)
				continue;
			painter.fillRect(x, y, cell, cell, QColor(0x26, 0x26, 0x26));
		}
	}

	painter.drawPixmap((target.width() - scaled.width()) / 2,
			   (target.height() - scaled.height()) / 2, scaled);
	painter.end();

	thumbnailLabel->setPixmap(canvas);
}

void MixishInputTile::RefreshOverlayButtons()
{
	MixishOverlayController *overlays = switcher ? switcher->GetOverlays() : nullptr;
	OBSScene scene = Scene();
	obs_source_t *sceneSource = scene ? obs_scene_get_source(scene) : nullptr;

	for (int i = 0; i < overlayButtons.size(); ++i) {
		QPushButton *button = overlayButtons[i];
		if (!button)
			continue;

		const bool assignedHere =
			overlays && sceneSource && overlays->InputSource(i) == sceneSource;
		const bool onAir = assignedHere && overlays->IsOnAir(i);

		MixishTheme::SetState(button, onAir ? "onAir" : (assignedHere ? "assigned" : "idle"));

		if (assignedHere) {
			button->setToolTip(tr("%1: this input — click to %2")
						   .arg(MixishOverlayController::ChannelTitle(i),
							onAir ? tr("turn off") : tr("turn on")));
		} else {
			button->setToolTip(tr("Assign this input to %1")
						   .arg(MixishOverlayController::ChannelTitle(i)));
		}
	}
}

namespace {

struct AudioSourceSearch {
	obs_source_t *found = nullptr;
	int depth = 0;
};

bool FindAudioItem(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	auto *search = static_cast<AudioSourceSearch *>(param);
	if (search->found || !item)
		return false;

	obs_source_t *source = obs_sceneitem_get_source(item);
	if (!source)
		return true;

	if (!obs_scene_from_source(source)) {
		if (obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO) {
			search->found = source;
			return false;
		}
		return true;
	}

	if (search->depth < 4) {
		search->depth++;
		obs_scene_enum_items(obs_scene_from_source(source), FindAudioItem, search);
		search->depth--;
	}

	return search->found == nullptr;
}

} // namespace

OBSSource MixishInputTile::PrimaryAudioSource() const
{
	OBSScene scene = Scene();
	if (!scene)
		return nullptr;

	AudioSourceSearch search;
	obs_scene_enum_items(scene, FindAudioItem, &search);
	return search.found;
}

void MixishInputTile::RefreshAudioState()
{
	if (!audioButton)
		return;

	OBSSource source = PrimaryAudioSource();
	const bool available = source != nullptr;
	audioButton->setEnabled(available);

	if (!available) {
		MixishTheme::SetState(audioButton, "idle");
		return;
	}

	const bool muted = obs_source_muted(source);
	MixishTheme::SetState(audioButton, muted ? "empty" : "idle");
	audioButton->setText(muted ? tr("Muted") : tr("Audio"));
	audioButton->setToolTip(muted ? tr("Unmute this input") : tr("Mute this input"));
}

void MixishInputTile::OnOverlayClicked(int index)
{
	MixishOverlayController *overlays = switcher ? switcher->GetOverlays() : nullptr;
	OBSScene scene = Scene();
	if (!overlays || !scene)
		return;

	obs_source_t *sceneSource = obs_scene_get_source(scene);
	if (!sceneSource)
		return;

	if (overlays->InputSource(index) == sceneSource)
		overlays->Toggle(index);
	else if (overlays->CanAssign(index, scene))
		overlays->Assign(index, scene, true);
}

void MixishInputTile::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton)
		emit clicked(this);
	QFrame::mousePressEvent(event);
}

void MixishInputTile::mouseDoubleClickEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton)
		emit doubleClicked(this);
	QFrame::mouseDoubleClickEvent(event);
}

void MixishInputTile::enterEvent(QEnterEvent *event)
{
	RequestThumbnail();
	QFrame::enterEvent(event);
}

void MixishInputTile::contextMenuEvent(QContextMenuEvent *event)
{
	if (switcher)
		switcher->OpenSceneMenu(Scene(), nullptr);
	event->accept();
}

bool MixishInputTile::eventFilter(QObject *watched, QEvent *event)
{
	if (event->type() == QEvent::ContextMenu) {
		if (switcher)
			switcher->OpenSceneMenu(Scene(), nullptr);
		return true;
	}

	return QFrame::eventFilter(watched, event);
}
