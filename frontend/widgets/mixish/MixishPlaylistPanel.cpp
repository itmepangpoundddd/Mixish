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
#include "MixishOverlayController.hpp"
#include "MixishSwitcher.hpp"
#include "MixishTheme.hpp"

#include <obs-frontend-api.h>

#include <QComboBox>
#include <QCheckBox>
#include <QColor>
#include <QGridLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
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

QString UrlItemName(const QString &url)
{
	const QUrl parsed(url);
	const QString fileName = QFileInfo(parsed.path()).fileName();
	if (!fileName.isEmpty())
		return fileName;
	return parsed.host().isEmpty() ? url : parsed.host();
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
	setMinimumWidth(300);
	setMinimumHeight(300);

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

	auto makeCompactButton = [this](QHBoxLayout *row, const QString &text, const QString &name) {
		auto *button = new QPushButton(text, this);
		button->setObjectName(name);
		button->setFixedHeight(MixishTheme::CompactHeight);
		button->setFocusPolicy(Qt::NoFocus);
		MixishTheme::SetVariant(button, "secondary");
		row->addWidget(button);
		return button;
	};

	newButton = makeCompactButton(toolbar, tr("New"), QStringLiteral("mixishPlaylistNew"));
	deleteButton =
		makeCompactButton(toolbar, tr("Delete"), QStringLiteral("mixishPlaylistDelete"));

	newButton->setToolTip(tr("Create an empty playlist input"));
	deleteButton->setToolTip(tr("Delete this playlist input and its queue"));
	layout->addLayout(toolbar);

	auto *editRow = new QHBoxLayout();
	editRow->setSpacing(MixishTheme::Space1);

	addButton = makeCompactButton(editRow, tr("Add"), QStringLiteral("mixishPlaylistAdd"));
	addUrlButton =
		makeCompactButton(editRow, tr("Add URL…"), QStringLiteral("mixishPlaylistAddUrl"));
	editButton = makeCompactButton(editRow, tr("Edit"), QStringLiteral("mixishPlaylistEdit"));
	removeButton =
		makeCompactButton(editRow, tr("Remove"), QStringLiteral("mixishPlaylistRemove"));

	addButton->setToolTip(tr("Add media or queue files to this playlist"));
	addUrlButton->setToolTip(tr("Add an HLS stream URL to this playlist"));
	editButton->setToolTip(tr("Replace the selected queue item with another file"));
	removeButton->setToolTip(tr("Remove the selected queue items"));
	layout->addLayout(editRow);

	auto *middle = new QHBoxLayout();
	middle->setSpacing(MixishTheme::Space1);

	list = new QListWidget(this);
	list->setObjectName(QStringLiteral("mixishPlaylistList"));
	list->setSelectionMode(QAbstractItemView::ExtendedSelection);
	list->setUniformItemSizes(true);
	list->setMinimumHeight(80);
	middle->addWidget(list, 1);

	auto makeOrderButton = [this](QVBoxLayout *column, const QString &text, const QString &name) {
		auto *button = new QPushButton(text, this);
		button->setObjectName(name);
		button->setFixedSize(MixishTheme::IconButtonSize, MixishTheme::IconButtonSize);
		button->setFocusPolicy(Qt::NoFocus);
		MixishTheme::SetVariant(button, "secondary");
		column->addWidget(button);
		return button;
	};

	auto *orderColumn = new QVBoxLayout();
	orderColumn->setSpacing(MixishTheme::Space1);
	orderColumn->addStretch(1);
	upButton = makeOrderButton(orderColumn, QStringLiteral("▲"),
				   QStringLiteral("mixishPlaylistUp"));
	downButton = makeOrderButton(orderColumn, QStringLiteral("▼"),
				     QStringLiteral("mixishPlaylistDown"));
	orderColumn->addStretch(1);
	upButton->setToolTip(tr("Move the selected item up the queue"));
	downButton->setToolTip(tr("Move the selected item down the queue"));
	middle->addLayout(orderColumn);

	layout->addLayout(middle, 1);

	/* One grid for every transport/option button: the panel is narrow, so
	 * equal columns keep the buttons from overlapping each other. */
	auto *buttonsGrid = new QGridLayout();
	buttonsGrid->setContentsMargins(0, 0, 0, 0);
	buttonsGrid->setHorizontalSpacing(MixishTheme::Space1);
	buttonsGrid->setVerticalSpacing(MixishTheme::Space1);

	for (int column = 0; column < 4; ++column)
		buttonsGrid->setColumnStretch(column, 1);

	auto addGridButton = [this, buttonsGrid](int row, int column, const QString &text,
						const QString &name, bool primary, bool checkable) {
		auto *button = new QPushButton(text, this);
		button->setObjectName(name);
		button->setFixedHeight(MixishTheme::ControlHeight);
		button->setFocusPolicy(Qt::NoFocus);
		button->setCheckable(checkable);
		button->setMinimumWidth(0);
		button->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
		MixishTheme::SetVariant(button, primary ? "primary" : "secondary");
		buttonsGrid->addWidget(button, row, column);
		return button;
	};

	playOutButton = addGridButton(0, 0, tr("Play Out"),
				      QStringLiteral("mixishPlaylistPlayOut"), true, false);
	stopButton = addGridButton(0, 1, tr("Stop"), QStringLiteral("mixishPlaylistStop"),
				   false, false);
	prevButton = addGridButton(0, 2, tr("Prev"), QStringLiteral("mixishPlaylistPrev"),
				   false, false);
	nextButton = addGridButton(0, 3, tr("Next"), QStringLiteral("mixishPlaylistNext"),
				   false, false);

	autoNextButton = addGridButton(1, 0, tr("Auto Next"),
				       QStringLiteral("mixishPlaylistAutoNext"), false, true);
	autoFirstButton = addGridButton(1, 1, tr("Auto First"),
					QStringLiteral("mixishPlaylistAutoFirst"), false, true);
	loopButton = addGridButton(1, 2, tr("Loop"), QStringLiteral("mixishPlaylistLoop"),
				   false, true);

	playOutButton->setToolTip(tr("Play the selected item and send this playlist to Program"));
	stopButton->setToolTip(tr("Stop the current item"));
	prevButton->setToolTip(tr("Play the previous item in the queue"));
	nextButton->setToolTip(tr("Play the next item in the queue"));
	autoNextButton->setToolTip(tr("Advance to the next item when the current item ends"));
	autoFirstButton->setToolTip(tr("Start from the first item whenever this playlist goes on air"));
	loopButton->setToolTip(tr("Return to the first item after the last item"));

	layout->addLayout(buttonsGrid);

	connect(playlistCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
		if (!manager || index < 0 || index >= manager->Playlists().size())
			return;
		controller = manager->Playlists()[index];
		RefreshList();
		UpdateButtons();
	});
	connect(newButton, &QPushButton::clicked, this, &MixishPlaylistPanel::NewPlaylist);
	connect(deleteButton, &QPushButton::clicked, this, &MixishPlaylistPanel::DeletePlaylist);
	connect(upButton, &QPushButton::clicked, this, [this]() { MoveSelected(-1); });
	connect(downButton, &QPushButton::clicked, this, [this]() { MoveSelected(1); });
	connect(stopButton, &QPushButton::clicked, this, &MixishPlaylistPanel::StopPlayback);
	connect(prevButton, &QPushButton::clicked, this, &MixishPlaylistPanel::PlayPreviousItem);
	connect(nextButton, &QPushButton::clicked, this, &MixishPlaylistPanel::PlayNextItem);
	connect(addButton, &QPushButton::clicked, this, &MixishPlaylistPanel::AddItems);
	connect(addUrlButton, &QPushButton::clicked, this, &MixishPlaylistPanel::AddUrl);
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

		QString durationText = FormatDuration(entry.duration);
		if (entry.stream)
			durationText = entry.streamError ? tr("ERROR") : tr("LIVE");

		text += QStringLiteral("   ·   ") + durationText;
		if (!available)
			text += tr(" (missing)");

		auto *item = new QListWidgetItem(QueueIcon(available), text);
		item->setData(Qt::UserRole, i);
		item->setToolTip(tr("%1\nDuration: %2").arg(entry.path, durationText));
		item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

		if (entry.streamError)
			item->setForeground(QColor(QStringLiteral("#E5484D")));
		else if (!available)
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
	const int index = SelectedIndex();
	const int itemCount = hasController ? controller->Items().size() : 0;

	if (newButton)
		newButton->setEnabled(manager != nullptr);
	if (deleteButton)
		deleteButton->setEnabled(hasController);
	if (upButton)
		upButton->setEnabled(index > 0);
	if (downButton)
		downButton->setEnabled(index >= 0 && index < itemCount - 1);
	if (stopButton)
		stopButton->setEnabled(hasController);
	if (prevButton)
		prevButton->setEnabled(hasController);
	if (nextButton)
		nextButton->setEnabled(hasController);

	if (addButton)
		addButton->setEnabled(hasController);
	if (addUrlButton)
		addUrlButton->setEnabled(hasController);
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

void MixishPlaylistPanel::NewPlaylist()
{
	if (!manager)
		return;

	MixishPlaylistController *created = manager->CreatePlaylist(QStringList());
	if (!created)
		return;

	controller = created;
	Refresh();

	if (playlistCombo) {
		const int index = manager->Playlists().indexOf(created);
		if (index >= 0)
			playlistCombo->setCurrentIndex(index);
	}
}

void MixishPlaylistPanel::DeletePlaylist()
{
	if (!manager || !controller)
		return;

	/* DeletePlaylist removes the controller and then asks before deleting
	 * the playlist's scene. */
	manager->DeletePlaylist(controller);
	controller = nullptr;
	Refresh();
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

void MixishPlaylistPanel::AddUrl()
{
	if (!controller || !manager)
		return;

	bool ok = false;
	QString url = QInputDialog::getText(this, tr("Add Stream URL"),
					    tr("HLS playlist URL:"), QLineEdit::Normal,
					    QString(), &ok);
	if (!ok)
		return;

	url = url.trimmed();
	if (url.isEmpty())
		return;

	const QString scheme = QUrl(url).scheme().toLower();
	if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https"))
		return;

	MixishPlaylistItem item;
	item.path = url;
	item.name = UrlItemName(url);
	item.duration = -1.0;
	item.missing = false;
	item.stream = true;

	const QVector<MixishPlaylistItem> items({item});
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

void MixishPlaylistPanel::MoveSelected(int delta)
{
	if (!controller || !list)
		return;

	const int index = SelectedIndex();
	if (index < 0)
		return;

	const int target = index + delta;
	if (target < 0 || target >= controller->Items().size())
		return;

	controller->MoveItem(index, target);
	Refresh();

	for (int i = 0; i < list->count(); ++i) {
		QListWidgetItem *item = list->item(i);
		if (item && item->data(Qt::UserRole).toInt() == target) {
			list->setCurrentItem(item);
			break;
		}
	}

	UpdateButtons();
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

void MixishPlaylistPanel::StopPlayback()
{
	if (!controller)
		return;

	controller->Stop();
	Refresh();
}

void MixishPlaylistPanel::PlayPreviousItem()
{
	if (!controller)
		return;

	controller->PlayPrevious();
	Refresh();
}

void MixishPlaylistPanel::PlayNextItem()
{
	if (!controller)
		return;

	controller->PlayNext();
	Refresh();
}

/* ---------------------------------------------------------------- *
 * MixishPlaylistWindow: the playlist manager opened from the bottom bar.
 * ---------------------------------------------------------------- */

namespace {

constexpr const char *kStorageKey = "mixish_playlist_manager";

} // namespace

MixishPlaylistWindow::MixishPlaylistWindow(MixishSwitcher *switcher_, obs_data_t *storage,
					   QWidget *parent)
	: QFrame(parent), switcher(switcher_), storageData(storage)
{
	setObjectName(QStringLiteral("mixishPlaylistWindow"));
	setWindowFlags(Qt::Window);
	setWindowTitle(tr("PlayList"));
	resize(900, 540);
	setMinimumSize(640, 420);

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(MixishTheme::Space2, MixishTheme::Space2,
				   MixishTheme::Space2, MixishTheme::Space2);
	layout->setSpacing(MixishTheme::Space1);

	auto *topRow = new QHBoxLayout();
	topRow->setSpacing(MixishTheme::Space1);

	newButton = new QPushButton(tr("New PlayList"), this);
	newButton->setObjectName(QStringLiteral("mixishPlaylistWindowNew"));
	newButton->setFixedHeight(MixishTheme::ControlHeight);
	newButton->setFocusPolicy(Qt::NoFocus);
	newButton->setMinimumWidth(0);
	newButton->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
	MixishTheme::SetVariant(newButton, "secondary");
	topRow->addWidget(newButton, 1);

	deleteButton = new QPushButton(tr("Delete PlayList"), this);
	deleteButton->setObjectName(QStringLiteral("mixishPlaylistWindowDelete"));
	deleteButton->setFixedHeight(MixishTheme::ControlHeight);
	deleteButton->setFocusPolicy(Qt::NoFocus);
	deleteButton->setMinimumWidth(0);
	deleteButton->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
	MixishTheme::SetVariant(deleteButton, "secondary");
	topRow->addWidget(deleteButton, 1);

	topRow->addStretch(1);

	nameCombo = new QComboBox(this);
	nameCombo->setObjectName(QStringLiteral("mixishPlaylistWindowName"));
	nameCombo->setEditable(true);
	nameCombo->setMinimumWidth(220);
	nameCombo->setFixedHeight(MixishTheme::ControlHeight);
	MixishTheme::SetControl(nameCombo);
	topRow->addWidget(nameCombo);

	layout->addLayout(topRow);

	auto *middle = new QHBoxLayout();
	middle->setSpacing(MixishTheme::Space2);

	auto *availableColumn = new QVBoxLayout();
	availableColumn->setSpacing(MixishTheme::Space1);
	auto *availableLabel = new QLabel(tr("Available Inputs"), this);
	availableLabel->setObjectName(QStringLiteral("mixishPlaylistWindowCaption"));
	availableColumn->addWidget(availableLabel);

	availableList = new QListWidget(this);
	availableList->setObjectName(QStringLiteral("mixishPlaylistWindowAvailable"));
	availableList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	availableColumn->addWidget(availableList, 1);
	middle->addLayout(availableColumn, 1);

	auto *moveColumn = new QVBoxLayout();
	moveColumn->setSpacing(MixishTheme::Space1);
	moveColumn->addStretch(1);

	auto makeSmallButton = [this, moveColumn](const QString &text, const QString &name) {
		auto *button = new QPushButton(text, this);
		button->setObjectName(name);
		button->setFixedSize(MixishTheme::IconButtonSize, MixishTheme::IconButtonSize);
		button->setFocusPolicy(Qt::NoFocus);
		MixishTheme::SetVariant(button, "secondary");
		moveColumn->addWidget(button);
		return button;
	};

	addButton = makeSmallButton(QStringLiteral(">"), QStringLiteral("mixishPlaylistWindowAdd"));
	removeButton =
		makeSmallButton(QStringLiteral("<"), QStringLiteral("mixishPlaylistWindowRemove"));
	addButton->setToolTip(tr("Add the selected inputs to the playlist"));
	removeButton->setToolTip(tr("Remove the selected items from the playlist"));
	moveColumn->addStretch(1);
	middle->addLayout(moveColumn);

	auto *queueColumn = new QVBoxLayout();
	queueColumn->setSpacing(MixishTheme::Space1);
	auto *queueLabel = new QLabel(tr("Playlist"), this);
	queueLabel->setObjectName(QStringLiteral("mixishPlaylistWindowCaption"));
	queueColumn->addWidget(queueLabel);

	auto *queueRow = new QHBoxLayout();
	queueRow->setSpacing(MixishTheme::Space1);

	queueList = new QListWidget(this);
	queueList->setObjectName(QStringLiteral("mixishPlaylistWindowQueue"));
	queueList->setSelectionMode(QAbstractItemView::SingleSelection);
	queueRow->addWidget(queueList, 1);

	auto *orderColumn = new QVBoxLayout();
	orderColumn->setSpacing(MixishTheme::Space1);
	orderColumn->addStretch(1);

	auto makeOrderButton = [this, orderColumn](const QString &text, const QString &name) {
		auto *button = new QPushButton(text, this);
		button->setObjectName(name);
		button->setFixedSize(MixishTheme::IconButtonSize, MixishTheme::IconButtonSize);
		button->setFocusPolicy(Qt::NoFocus);
		MixishTheme::SetVariant(button, "secondary");
		orderColumn->addWidget(button);
		return button;
	};

	upButton = makeOrderButton(QStringLiteral("▲"), QStringLiteral("mixishPlaylistWindowUp"));
	downButton =
		makeOrderButton(QStringLiteral("▼"), QStringLiteral("mixishPlaylistWindowDown"));
	upButton->setToolTip(tr("Move the selected item up"));
	downButton->setToolTip(tr("Move the selected item down"));
	orderColumn->addStretch(1);
	queueRow->addLayout(orderColumn);

	queueColumn->addLayout(queueRow, 1);
	middle->addLayout(queueColumn, 2);

	layout->addLayout(middle, 1);

	auto *optionsRow = new QHBoxLayout();
	optionsRow->setSpacing(MixishTheme::Space2);

	auto makeCheck = [this, optionsRow](const QString &text, const QString &name) {
		auto *check = new QCheckBox(text, this);
		check->setObjectName(name);
		check->setFocusPolicy(Qt::NoFocus);
		optionsRow->addWidget(check);
		return check;
	};

	loopCheck = makeCheck(tr("Loop"), QStringLiteral("mixishPlaylistWindowLoop"));
	beginCheck = makeCheck(tr("Begin from selected item"),
			       QStringLiteral("mixishPlaylistWindowBegin"));
	clearOverlayCheck = makeCheck(tr("Clear Overlays"),
				      QStringLiteral("mixishPlaylistWindowClearOverlays"));
	manualCheck = makeCheck(tr("Manual Mode"), QStringLiteral("mixishPlaylistWindowManual"));
	optionsRow->addStretch(1);
	layout->addLayout(optionsRow);

	auto *buttonRow = new QHBoxLayout();
	buttonRow->setSpacing(MixishTheme::Space1);

	auto addButtonToRow = [this, buttonRow](const QString &text, const QString &name) {
		auto *button = new QPushButton(text, this);
		button->setObjectName(name);
		button->setFixedHeight(MixishTheme::ControlHeight);
		button->setFocusPolicy(Qt::NoFocus);
		button->setMinimumWidth(0);
		button->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
		MixishTheme::SetVariant(button, "secondary");
		buttonRow->addWidget(button, 1);
		return button;
	};

	stopButton = addButtonToRow(tr("Stop"), QStringLiteral("mixishPlaylistWindowStop"));
	prevButton = addButtonToRow(tr("Previous"), QStringLiteral("mixishPlaylistWindowPrev"));
	nextButton = addButtonToRow(tr("Next"), QStringLiteral("mixishPlaylistWindowNext"));
	closeButton = addButtonToRow(tr("Close"), QStringLiteral("mixishPlaylistWindowClose"));
	layout->addLayout(buttonRow);

	connect(newButton, &QPushButton::clicked, this, &MixishPlaylistWindow::NewList);
	connect(deleteButton, &QPushButton::clicked, this, &MixishPlaylistWindow::DeleteList);
	connect(addButton, &QPushButton::clicked, this, &MixishPlaylistWindow::AddSelected);
	connect(removeButton, &QPushButton::clicked, this, &MixishPlaylistWindow::RemoveSelected);
	connect(upButton, &QPushButton::clicked, this, [this]() { MoveSelected(-1); });
	connect(downButton, &QPushButton::clicked, this, [this]() { MoveSelected(1); });
	connect(stopButton, &QPushButton::clicked, this, &MixishPlaylistWindow::StopPlayback);
	connect(prevButton, &QPushButton::clicked, this, &MixishPlaylistWindow::PlayPrevious);
	connect(nextButton, &QPushButton::clicked, this, &MixishPlaylistWindow::PlayNext);
	connect(closeButton, &QPushButton::clicked, this, [this]() { hide(); });

	connect(availableList, &QListWidget::itemDoubleClicked, this,
		[this](QListWidgetItem *) { AddSelected(); });
	connect(queueList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
		if (!item)
			return;
		PlayIndex(item->data(Qt::UserRole).toInt(), true);
	});
	connect(queueList, &QListWidget::itemSelectionChanged, this,
		&MixishPlaylistWindow::UpdateButtons);
	connect(availableList, &QListWidget::itemSelectionChanged, this,
		&MixishPlaylistWindow::UpdateButtons);

	connect(nameCombo, &QComboBox::currentTextChanged, this, [this](const QString &text) {
		if (text.isEmpty() || text == activeName)
			return;
		ApplyName();
	});

	connect(loopCheck, &QCheckBox::toggled, this, [this](bool) { Save(); });
	connect(beginCheck, &QCheckBox::toggled, this, [this](bool) { Save(); });
	connect(clearOverlayCheck, &QCheckBox::toggled, this, [this](bool) { Save(); });
	connect(manualCheck, &QCheckBox::toggled, this, [this](bool) { Save(); });

	advanceTimer = new QTimer(this);
	advanceTimer->setInterval(400);
	connect(advanceTimer, &QTimer::timeout, this, &MixishPlaylistWindow::CheckAutoAdvance);
	advanceTimer->start();

	Load();
	Refresh();
	MixishTheme::Apply(this);
}

void MixishPlaylistWindow::Load()
{
	queues.clear();
	activeName = QStringLiteral("Default");

	if (storageData) {
		const char *raw = obs_data_get_string(storageData, kStorageKey);
		if (raw && *raw) {
			const QJsonDocument document = QJsonDocument::fromJson(QByteArray(raw));
			const QJsonObject root = document.object();

			const QJsonObject lists = root.value(QStringLiteral("lists")).toObject();
			for (auto it = lists.begin(); it != lists.end(); ++it) {
				QVector<Entry> entries;
				for (const QJsonValue &value : it.value().toArray()) {
					const QJsonObject object = value.toObject();
					Entry entry;
					entry.uuid = object.value(QStringLiteral("uuid")).toString();
					entry.label = object.value(QStringLiteral("label")).toString();
					if (!entry.uuid.isEmpty())
						entries.push_back(entry);
				}
				queues.insert(it.key(), entries);
			}

			const QString active = root.value(QStringLiteral("active")).toString();
			if (!active.isEmpty())
				activeName = active;

			const QJsonObject options = root.value(QStringLiteral("options")).toObject();
			loopCheck->setChecked(options.value(QStringLiteral("loop")).toBool());
			beginCheck->setChecked(options.value(QStringLiteral("begin")).toBool());
			clearOverlayCheck->setChecked(
				options.value(QStringLiteral("clearOverlays")).toBool());
			manualCheck->setChecked(options.value(QStringLiteral("manual")).toBool());
		}
	}

	if (!queues.contains(activeName))
		queues.insert(activeName, QVector<Entry>());
	current = -1;
}

void MixishPlaylistWindow::Save()
{
	if (!storageData)
		return;

	QJsonObject lists;
	for (auto it = queues.begin(); it != queues.end(); ++it) {
		QJsonArray array;
		for (const Entry &entry : it.value()) {
			QJsonObject object;
			object.insert(QStringLiteral("uuid"), entry.uuid);
			object.insert(QStringLiteral("label"), entry.label);
			array.append(object);
		}
		lists.insert(it.key(), array);
	}

	QJsonObject options;
	options.insert(QStringLiteral("loop"), loopCheck && loopCheck->isChecked());
	options.insert(QStringLiteral("begin"), beginCheck && beginCheck->isChecked());
	options.insert(QStringLiteral("clearOverlays"),
		       clearOverlayCheck && clearOverlayCheck->isChecked());
	options.insert(QStringLiteral("manual"), manualCheck && manualCheck->isChecked());

	QJsonObject root;
	root.insert(QStringLiteral("active"), activeName);
	root.insert(QStringLiteral("lists"), lists);
	root.insert(QStringLiteral("options"), options);

	const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Compact);
	obs_data_set_string(storageData, kStorageKey, json.constData());
}

QVector<MixishPlaylistWindow::Entry> &MixishPlaylistWindow::ActiveQueue()
{
	return queues[activeName];
}

void MixishPlaylistWindow::SetActive(const QString &name)
{
	if (name.isEmpty())
		return;

	activeName = name;
	if (!queues.contains(activeName))
		queues.insert(activeName, QVector<Entry>());
	current = -1;
	playing = nullptr;

	Refresh();
	Save();
}

void MixishPlaylistWindow::ApplyName()
{
	if (!nameCombo)
		return;

	QString name = nameCombo->currentText().trimmed();
	if (name.isEmpty()) {
		nameCombo->setEditText(activeName);
		return;
	}

	if (name != activeName) {
		const QVector<Entry> moved = queues.value(activeName);
		queues.remove(activeName);
		queues.insert(name, moved);
	}

	SetActive(name);
}

void MixishPlaylistWindow::Refresh()
{
	if (nameCombo) {
		QSignalBlocker block(nameCombo);
		QStringList names = queues.keys();
		names.sort(Qt::CaseInsensitive);
		nameCombo->clear();
		nameCombo->addItems(names);
		nameCombo->setCurrentText(activeName);
	}

	RefreshAvailable();
	RefreshQueue();
	UpdateButtons();
}

void MixishPlaylistWindow::RefreshAvailable()
{
	if (!availableList)
		return;

	QSet<QString> selected;
	for (QListWidgetItem *item : availableList->selectedItems()) {
		if (item)
			selected.insert(item->data(Qt::UserRole).toString());
	}

	QSignalBlocker block(availableList);
	availableList->clear();

	obs_frontend_source_list sources = {};
	obs_frontend_get_scenes(&sources);
	for (size_t i = 0; i < sources.sources.num; ++i) {
		obs_source_t *source = sources.sources.array[i];
		if (!source)
			continue;

		const char *uuid = obs_source_get_uuid(source);
		const char *name = obs_source_get_name(source);
		if (!uuid || !name)
			continue;

		auto *item = new QListWidgetItem(QString::fromUtf8(name), availableList);
		item->setData(Qt::UserRole, QString::fromUtf8(uuid));
		item->setSelected(selected.contains(QString::fromUtf8(uuid)));
	}
	obs_frontend_source_list_free(&sources);
}

void MixishPlaylistWindow::RefreshQueue()
{
	if (!queueList)
		return;

	QSignalBlocker block(queueList);
	queueList->clear();

	const QVector<Entry> &entries = ActiveQueue();
	for (int i = 0; i < entries.size(); ++i) {
		const Entry &entry = entries[i];
		OBSSourceAutoRelease source = obs_get_source_by_uuid(entry.uuid.toUtf8().constData());
		const bool available = !!source;

		const QString label = available ? QString::fromUtf8(obs_source_get_name(source))
						: tr("%1 (missing)").arg(entry.label);
		QString text = label;
		if (i == current)
			text.prepend(QStringLiteral("▶  "));

		auto *item = new QListWidgetItem(text, queueList);
		item->setData(Qt::UserRole, i);
		if (i == current) {
			QFont font = item->font();
			font.setBold(true);
			item->setFont(font);
		}
		if (!available)
			item->setForeground(QColor(QStringLiteral("#777777")));
	}

	if (current >= 0 && current < queueList->count())
		queueList->setCurrentRow(current);
}

void MixishPlaylistWindow::UpdateButtons()
{
	const int index = QueueSelection();
	const int count = ActiveQueue().size();
	const bool hasSelection = index >= 0;

	if (addButton)
		addButton->setEnabled(!availableList->selectedItems().isEmpty());
	if (removeButton)
		removeButton->setEnabled(hasSelection);
	if (upButton)
		upButton->setEnabled(index > 0);
	if (downButton)
		downButton->setEnabled(hasSelection && index < count - 1);
	if (nextButton)
		nextButton->setEnabled(count > 0);
	if (prevButton)
		prevButton->setEnabled(count > 0);
	if (stopButton)
		stopButton->setEnabled(current >= 0);
}

int MixishPlaylistWindow::QueueSelection() const
{
	if (!queueList)
		return -1;
	QListWidgetItem *item = queueList->currentItem();
	if (!item)
		return -1;
	return item->data(Qt::UserRole).toInt();
}

void MixishPlaylistWindow::NewList()
{
	QString name = tr("Playlist %1").arg(queues.size() + 1);
	int suffix = 2;
	while (queues.contains(name))
		name = tr("Playlist %1").arg(queues.size() + suffix++);

	queues.insert(name, QVector<Entry>());
	SetActive(name);
}

void MixishPlaylistWindow::DeleteList()
{
	if (queues.size() <= 1) {
		/* Keep one list so the window always has somewhere to put items. */
		ActiveQueue().clear();
		current = -1;
		playing = nullptr;
		Refresh();
		Save();
		return;
	}

	queues.remove(activeName);
	current = -1;
	playing = nullptr;
	SetActive(queues.keys().first());
}

void MixishPlaylistWindow::AddSelected()
{
	if (!availableList)
		return;

	const QList<QListWidgetItem *> selected = availableList->selectedItems();
	if (selected.isEmpty())
		return;

	QVector<Entry> &queue = ActiveQueue();
	for (QListWidgetItem *item : selected) {
		if (!item)
			continue;
		Entry entry;
		entry.uuid = item->data(Qt::UserRole).toString();
		entry.label = item->text();
		if (!entry.uuid.isEmpty())
			queue.push_back(entry);
	}

	RefreshQueue();
	UpdateButtons();
	Save();
}

void MixishPlaylistWindow::RemoveSelected()
{
	const int index = QueueSelection();
	if (index < 0)
		return;

	QVector<Entry> &queue = ActiveQueue();
	if (index >= queue.size())
		return;

	queue.remove(index);
	if (current > index)
		--current;
	else if (current == index)
		current = -1;

	RefreshQueue();
	UpdateButtons();
	Save();
}

void MixishPlaylistWindow::MoveSelected(int delta)
{
	const int index = QueueSelection();
	if (index < 0)
		return;

	QVector<Entry> &queue = ActiveQueue();
	const int target = index + delta;
	if (target < 0 || target >= queue.size())
		return;

	queue.move(index, target);
	if (current == index)
		current = target;
	else if (current == target)
		current = index;

	RefreshQueue();
	if (target < queueList->count())
		queueList->setCurrentRow(target);
	UpdateButtons();
	Save();
}

void MixishPlaylistWindow::ClearOverlayChannels()
{
	if (switcher && switcher->GetOverlays())
		switcher->GetOverlays()->AllOff();
}

void MixishPlaylistWindow::PlayIndex(int index, bool restart)
{
	const QVector<Entry> &queue = ActiveQueue();
	if (index < 0 || index >= queue.size())
		return;

	const Entry &entry = queue[index];
	OBSSourceAutoRelease source = obs_get_source_by_uuid(entry.uuid.toUtf8().constData());
	if (!source) {
		RefreshQueue();
		return;
	}

	OBSScene scene = obs_scene_from_source(source);
	if (!scene) {
		/* Not an input scene: nothing to cut to Program. */
		return;
	}

	if (clearOverlayCheck && clearOverlayCheck->isChecked())
		ClearOverlayChannels();

	if (restart)
		obs_source_media_restart(source);

	if (switcher)
		switcher->CutToProgram(scene);

	playing = OBSGetWeakRef(source);
	current = index;
	RefreshQueue();
	UpdateButtons();
}

void MixishPlaylistWindow::PlayNext()
{
	const int count = ActiveQueue().size();
	if (count == 0)
		return;

	int next = current < 0 ? (beginCheck && beginCheck->isChecked() ? QueueSelection() : 0)
			       : current + 1;
	if (next < 0)
		next = 0;

	if (next >= count) {
		if (loopCheck && loopCheck->isChecked())
			next = 0;
		else {
			current = -1;
			playing = nullptr;
			RefreshQueue();
			UpdateButtons();
			return;
		}
	}

	PlayIndex(next, true);
}

void MixishPlaylistWindow::PlayPrevious()
{
	const int count = ActiveQueue().size();
	if (count == 0)
		return;

	int previous = current < 0 ? 0 : current - 1;
	if (previous < 0)
		previous = loopCheck && loopCheck->isChecked() ? count - 1 : 0;

	PlayIndex(previous, true);
}

void MixishPlaylistWindow::StopPlayback()
{
	playing = nullptr;
	current = -1;
	RefreshQueue();
	UpdateButtons();
}

void MixishPlaylistWindow::CheckAutoAdvance()
{
	if (manualCheck && manualCheck->isChecked())
		return;
	if (!playing || current < 0)
		return;

	OBSSourceAutoRelease source = OBSGetStrongRef(playing);
	if (!source)
		return;

	if (obs_source_media_get_state(source) == OBS_MEDIA_STATE_ENDED)
		PlayNext();
}
