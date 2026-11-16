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

#include "MixishPlaylistPanel.hpp"
#include "MixishPlaylistController.hpp"
#include "MixishPlaylistManager.hpp"
#include "MixishSwitcher.hpp"
#include "MixishTheme.hpp"

#include <QComboBox>
#include <QColor>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

#include "moc_MixishPlaylistPanel.cpp"

namespace {

bool PathIsRemote(const QString &path)
{
	return path.contains(QStringLiteral("://"));
}

QString ItemLabel(const MixishPlaylistItem &item)
{
	if (!item.name.isEmpty())
		return item.name;

	const QString fileName = QFileInfo(item.path).fileName();
	return fileName.isEmpty() ? item.path : fileName;
}

QString FormatDuration(double seconds)
{
	if (seconds < 0.0)
		return QStringLiteral("-");

	const int total = int(seconds + 0.5);
	const int hours = total / 3600;
	const int minutes = (total / 60) % 60;
	const int secs = total % 60;
	if (hours > 0)
		return QString::asprintf("%d:%02d:%02d", hours, minutes, secs);
	return QString::asprintf("%d:%02d", minutes, secs);
}

QIcon QueueIcon(bool available)
{
	QPixmap pixmap(12, 12);
	pixmap.fill(Qt::transparent);

	QPainter painter(&pixmap);
	painter.fillRect(1, 1, 10, 10, QColor(available ? QStringLiteral("#1D9E75")
							  : QStringLiteral("#444444")));
	painter.end();

	return QIcon(pixmap);
}

QString FileFilter()
{
	return QObject::tr("Media and Playlist Files (*.mp4 *.mkv *.mov *.avi *.webm *.flv *.ts *.m4v "
			   "*.mp3 *.wav *.flac *.ogg *.m4a *.aac *.m3u *.m3u8);;All Files (*)");
}

} // namespace

MixishPlaylistPanel::MixishPlaylistPanel(MixishSwitcher *switcher_, QWidget *parent)
	: QFrame(parent),
	  switcher(switcher_),
	  manager(switcher_ ? switcher_->GetPlaylistManager() : nullptr)
{
	setObjectName(QStringLiteral("mixishPlaylistPanel"));
	setMinimumWidth(360);

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(MixishTheme::Space2, MixishTheme::Space2,
				   MixishTheme::Space2, MixishTheme::Space2);
	layout->setSpacing(MixishTheme::Space1);

	auto *toolbar = new QHBoxLayout();
	toolbar->setSpacing(MixishTheme::Space1);

	playlistCombo = new QComboBox(this);
	playlistCombo->setObjectName(QStringLiteral("mixishPlaylistCombo"));
	playlistCombo->setMinimumHeight(MixishTheme::CompactHeight);
	MixishTheme::SetControl(playlistCombo);
	toolbar->addWidget(playlistCombo, 1);

	auto makeToolbarButton = [this, toolbar](const QString &text, const QString &name) {
		auto *button = new QPushButton(text, this);
		button->setObjectName(name);
		button->setFixedHeight(MixishTheme::CompactHeight);
		button->setFocusPolicy(Qt::NoFocus);
		MixishTheme::SetVariant(button, "secondary");
		toolbar->addWidget(button);
		return button;
	};

	addButton = makeToolbarButton(tr("Add"), QStringLiteral("mixishPlaylistAdd"));
	editButton = makeToolbarButton(tr("Edit"), QStringLiteral("mixishPlaylistEdit"));
	removeButton = makeToolbarButton(tr("Remove"), QStringLiteral("mixishPlaylistRemove"));

	addButton->setToolTip(tr("Add media or queue files to this playlist"));
	editButton->setToolTip(tr("Replace the selected queue item with another file"));
	removeButton->setToolTip(tr("Remove the selected queue items"));
	layout->addLayout(toolbar);

	list = new QListWidget(this);
	list->setObjectName(QStringLiteral("mixishPlaylistList"));
	list->setSelectionMode(QAbstractItemView::ExtendedSelection);
	list->setUniformItemSizes(true);
	list->setMinimumHeight(96);
	layout->addWidget(list, 1);

	auto *bottom = new QHBoxLayout();
	bottom->setSpacing(MixishTheme::Space1);

	playOutButton = new QPushButton(tr("Play Out"), this);
	playOutButton->setObjectName(QStringLiteral("mixishPlaylistPlayOut"));
	playOutButton->setFixedHeight(MixishTheme::ControlHeight);
	playOutButton->setFocusPolicy(Qt::NoFocus);
	MixishTheme::SetVariant(playOutButton, "primary");
	playOutButton->setToolTip(tr("Play the selected item and send this playlist to Program"));
	bottom->addWidget(playOutButton, 1);

	auto makeToggle = [this, bottom](const QString &text, const QString &name) {
		auto *button = new QPushButton(text, this);
		button->setObjectName(name);
		button->setFixedHeight(MixishTheme::ControlHeight);
		button->setCheckable(true);
		button->setFocusPolicy(Qt::NoFocus);
		MixishTheme::SetVariant(button, "secondary");
		bottom->addWidget(button, 1);
		return button;
	};

	autoNextButton = makeToggle(tr("Auto Next"), QStringLiteral("mixishPlaylistAutoNext"));
	autoFirstButton = makeToggle(tr("Auto First"), QStringLiteral("mixishPlaylistAutoFirst"));
	loopButton = makeToggle(tr("Loop"), QStringLiteral("mixishPlaylistLoop"));

	autoNextButton->setToolTip(tr("Advance to the next item when the current item ends"));
	autoFirstButton->setToolTip(tr("Start from the first item whenever this playlist goes on air"));
	loopButton->setToolTip(tr("Return to the first item after the last item"));

	layout->addLayout(bottom);

	connect(playlistCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
		if (!manager || index < 0 || index >= manager->Playlists().size())
			return;
		controller = manager->Playlists()[index];
		RefreshList();
		UpdateButtons();
	});
	connect(addButton, &QPushButton::clicked, this, &MixishPlaylistPanel::AddItems);
	connect(editButton, &QPushButton::clicked, this, &MixishPlaylistPanel::EditItem);
	connect(removeButton, &QPushButton::clicked, this, &MixishPlaylistPanel::RemoveItems);
	connect(playOutButton, &QPushButton::clicked, this, &MixishPlaylistPanel::PlayOut);
	connect(list, &QListWidget::itemDoubleClicked, this, &MixishPlaylistPanel::PlaySelected);
	connect(list, &QListWidget::itemSelectionChanged, this, &MixishPlaylistPanel::UpdateButtons);
	connect(autoNextButton, &QPushButton::toggled, this, [this](bool checked) {
		if (controller)
			controller->SetContinuous(checked);
	});
	connect(autoFirstButton, &QPushButton::toggled, this, [this](bool checked) {
		if (controller)
			controller->SetAutoFirst(checked);
	});
	connect(loopButton, &QPushButton::toggled, this, [this](bool checked) {
		if (controller)
			controller->SetLooping(checked);
	});

	if (manager)
		connect(manager, &MixishPlaylistManager::PlaylistsChanged, this, &MixishPlaylistPanel::Refresh);

	Refresh();
	MixishTheme::Apply(this);
}

void MixishPlaylistPanel::SetController(MixishPlaylistController *newController)
{
	controller = newController;
	Refresh();
}

void MixishPlaylistPanel::Refresh()
{
	if (controller && manager)
		manager->VerifyDurations(controller);

	RefreshCombo();
	RefreshList();
	UpdateButtons();
}

void MixishPlaylistPanel::RefreshCombo()
{
	if (!playlistCombo)
		return;

	QSignalBlocker block(playlistCombo);
	playlistCombo->clear();

	if (!manager) {
		playlistCombo->setEnabled(false);
		return;
	}

	const QVector<MixishPlaylistController *> &playlists = manager->Playlists();
	for (MixishPlaylistController *playlist : playlists) {
		if (!playlist)
			continue;
		const QString name = playlist->SceneName();
		playlistCombo->addItem(name.isEmpty() ? tr("Playlist") : name);
	}

	playlistCombo->setEnabled(!playlists.isEmpty());
	if (controller) {
		const int index = playlists.indexOf(controller);
		if (index >= 0)
			playlistCombo->setCurrentIndex(index);
	}
}

void MixishPlaylistPanel::RefreshList()
{
	if (!list)
		return;

	QSet<int> selected;
	for (QListWidgetItem *item : list->selectedItems()) {
		if (item)
			selected.insert(item->data(Qt::UserRole).toInt());
	}
	const int previousCurrent = SelectedIndex();

	QSignalBlocker block(list);
	list->clear();

	if (!controller) {
		auto *placeholder = new QListWidgetItem(tr("Select a Playlist input"));
		placeholder->setFlags(Qt::NoItemFlags);
		placeholder->setForeground(QColor(QStringLiteral("#8A8A8A")));
		list->addItem(placeholder);
		return;
	}

	const QVector<MixishPlaylistItem> &items = controller->Items();
	if (items.isEmpty()) {
		auto *placeholder = new QListWidgetItem(tr("No items. Use Add to add media."));
		placeholder->setFlags(Qt::NoItemFlags);
		placeholder->setForeground(QColor(QStringLiteral("#8A8A8A")));
		list->addItem(placeholder);
		return;
	}

	const int current = controller->CurrentIndex();
	for (int i = 0; i < items.size(); ++i) {
		const MixishPlaylistItem &entry = items[i];
		const bool available =
			!entry.missing && (PathIsRemote(entry.path) || QFileInfo::exists(entry.path));

		QString text = ItemLabel(entry);
		if (i == current)
			text.prepend(QStringLiteral("▶  "));
		text += QStringLiteral("   ·   ") + FormatDuration(entry.duration);
		if (!available)
			text += tr(" (missing)");

		auto *item = new QListWidgetItem(QueueIcon(available), text);
		item->setData(Qt::UserRole, i);
		item->setToolTip(tr("%1\nDuration: %2").arg(entry.path, FormatDuration(entry.duration)));
		item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

		if (!available)
			item->setForeground(QColor(QStringLiteral("#777777")));

		if (i == current) {
			QFont font = item->font();
			font.setBold(true);
			item->setFont(font);
		}

		list->addItem(item);
	}

	for (int i = 0; i < list->count(); ++i) {
		QListWidgetItem *item = list->item(i);
		if (item && selected.contains(item->data(Qt::UserRole).toInt()))
			item->setSelected(true);
	}

	if (!selected.isEmpty() && previousCurrent >= 0 && previousCurrent < list->count()) {
		QListWidgetItem *item = list->item(previousCurrent);
		if (item && (item->flags() & Qt::ItemIsSelectable))
			list->setCurrentItem(item);
	} else if (selected.isEmpty()) {
		list->setCurrentRow(-1);
		list->clearSelection();
	}

	if (current >= 0 && current < list->count())
		list->scrollToItem(list->item(current), QAbstractItemView::EnsureVisible);
}

void MixishPlaylistPanel::UpdateButtons()
{
	const bool hasController = controller != nullptr;
	const bool hasSelection = SelectedIndex() >= 0;

	if (addButton)
		addButton->setEnabled(hasController);
	if (editButton)
		editButton->setEnabled(hasController && hasSelection);
	if (removeButton)
		removeButton->setEnabled(hasController && hasSelection);
	if (playOutButton)
		playOutButton->setEnabled(hasController);

	const bool continuous = hasController && controller->Continuous();
	const bool autoFirst = hasController && controller->AutoFirst();
	const bool looping = hasController && controller->Looping();

	if (autoNextButton) {
		QSignalBlocker block(autoNextButton);
		autoNextButton->setEnabled(hasController);
		autoNextButton->setChecked(continuous);
		MixishTheme::SetState(autoNextButton, continuous ? "selected" : "idle");
	}
	if (autoFirstButton) {
		QSignalBlocker block(autoFirstButton);
		autoFirstButton->setEnabled(hasController);
		autoFirstButton->setChecked(autoFirst);
		MixishTheme::SetState(autoFirstButton, autoFirst ? "selected" : "idle");
	}
	if (loopButton) {
		QSignalBlocker block(loopButton);
		loopButton->setEnabled(hasController);
		loopButton->setChecked(looping);
		MixishTheme::SetState(loopButton, looping ? "selected" : "idle");
	}
}

int MixishPlaylistPanel::SelectedIndex() const
{
	if (!list)
		return -1;

	const QList<QListWidgetItem *> selected = list->selectedItems();
	if (selected.isEmpty())
		return -1;

	QListWidgetItem *item = selected.first();
	if (!item || !(item->flags() & Qt::ItemIsSelectable))
		return -1;
	return item->data(Qt::UserRole).toInt();
}

void MixishPlaylistPanel::AddItems()
{
	if (!controller || !manager)
		return;

	const QStringList files =
		QFileDialog::getOpenFileNames(this, tr("Add Playlist Items"), QString(), FileFilter());
	if (files.isEmpty())
		return;

	const QVector<MixishPlaylistItem> items = manager->ExpandFiles(files);
	if (items.isEmpty())
		return;

	controller->AddItems(items);
	if (manager)
		manager->RecheckItems(items);
	Refresh();
}

void MixishPlaylistPanel::EditItem()
{
	if (!controller || !manager)
		return;

	const int index = SelectedIndex();
	if (index < 0)
		return;

	const QString file =
		QFileDialog::getOpenFileName(this, tr("Replace Playlist Item"), QString(), FileFilter());
	if (file.isEmpty())
		return;

	const QVector<MixishPlaylistItem> items = manager->ExpandFiles(QStringList(file));
	if (items.isEmpty())
		return;

	controller->ReplaceItem(index, items.first());
	if (manager)
		manager->RecheckItems(QVector<MixishPlaylistItem>({items.first()}));
	Refresh();
}

void MixishPlaylistPanel::RemoveItems()
{
	if (!controller || !list)
		return;

	QVector<int> indexes;
	for (QListWidgetItem *item : list->selectedItems()) {
		if (!item || !(item->flags() & Qt::ItemIsSelectable))
			continue;
		indexes.push_back(item->data(Qt::UserRole).toInt());
	}
	if (indexes.isEmpty())
		return;

	controller->RemoveItems(indexes);
	Refresh();
}

void MixishPlaylistPanel::PlayOut()
{
	if (!controller)
		return;

	const int index = SelectedIndex();
	if (index >= 0)
		controller->PlayIndex(index, true);

	OBSScene scene = controller->Scene();
	if (scene)
		switcher->CutToProgram(scene);

	Refresh();
}

void MixishPlaylistPanel::PlaySelected(QListWidgetItem *item)
{
	if (!controller || !item || !(item->flags() & Qt::ItemIsSelectable))
		return;

	const int index = item->data(Qt::UserRole).toInt();

	/* PlayIndex() emits PlaylistsChanged, which rebuilds this list. Defer
	 * the work so the QListWidgetItem that emitted doubleClicked is not
	 * deleted inside its own signal handler. */
	QTimer::singleShot(0, this, [this, index]() {
		if (!controller)
			return;

		OBSScene scene = controller->Scene();
		if (scene)
			switcher->SelectScene(scene);
		controller->PlayIndex(index, true);
		Refresh();
	});
}
