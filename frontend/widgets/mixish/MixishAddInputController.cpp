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

#include "MixishAddInputController.hpp"
#include "MixishSwitcher.hpp"
#include "MixishTheme.hpp"

#include <dialogs/OBSBasicSourceSelect.hpp>
#include <widgets/OBSBasic.hpp>

#include <QBoxLayout>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLayout>
#include <QLayoutItem>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QVBoxLayout>

#include <string>

#include "moc_MixishAddInputController.cpp"

static constexpr int kUnversionedIdRole = Qt::UserRole + 1;
static constexpr int kRecentTypeLimit = 5;
static constexpr int kRecentMediaLimit = 10;

static QString RecentConfigString(const char *key)
{
	const char *value = config_get_string(App()->GetUserConfig(), "BasicWindow", key);
	return value ? QString::fromUtf8(value) : QString();
}

static void SaveRecentConfigString(const char *key, const QString &value)
{
	config_set_string(App()->GetUserConfig(), "BasicWindow", key, value.toUtf8().constData());
	config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
}

MixishAddInputController::MixishAddInputController(OBSBasicSourceSelect *dialog_, MixishSwitcher *switcher_,
						   OBSBasic *main_)
	: dialog(dialog_),
	  switcher(switcher_),
	  main(main_)
{
}

void MixishAddInputController::Setup()
{
	if (!dialog)
		return;

	typeList = dialog->findChild<QListWidget *>(QStringLiteral("sourceTypeList"));
	createButton = dialog->findChild<QPushButton *>(QStringLiteral("createNewSource"));
	if (!typeList || !createButton)
		return;

	dialog->setWindowTitle(tr("Add Input"));

	LoadRecentLists();
	HideObsOnlyControls();

	/* The original dialog's New Source button and double-click both add to the
	 * current scene. Replace those paths for the Mixish flow. */
	QObject::disconnect(createButton, nullptr, dialog, nullptr);
	QObject::disconnect(typeList, &QListWidget::itemDoubleClicked, dialog, nullptr);

	connect(typeList, &QListWidget::currentItemChanged, this, &MixishAddInputController::OnTypeChanged);
	connect(createButton, &QPushButton::clicked, this, &MixishAddInputController::CreateInput);
	connect(typeList, &QListWidget::itemDoubleClicked, this,
		[this](QListWidgetItem *) { CreateInput(); });

	createButton->hide();

	if (auto *box = dialog->findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"))) {
		auto *createBottom = new QPushButton(tr("Create Input"), box);
		createBottom->setObjectName(QStringLiteral("mixishCreateInputButton"));
		box->addButton(createBottom, QDialogButtonBox::AcceptRole);
		connect(createBottom, &QPushButton::clicked, this, &MixishAddInputController::CreateInput);
	}

	RemoveUnsupportedTypeItems();

	BuildRecentTypesRow();
	BuildMediaPanel();
	BuildTypeInfoPanel();
	BuildNumberField();
	ApplyMixishStyle();

	auto *title = dialog->findChild<QLabel *>(QStringLiteral("sourceSelectTitle"));
	if (title)
		title->setText(tr("Add Input"));

	auto *description = dialog->findChild<QLabel *>(QStringLiteral("sourceSelectDescription"));
	if (description)
		description->setText(
			tr("Create a new input in its own hidden scene. The current Preview and Program scenes are not changed."));

	createButton->setText(tr("Create Input"));
	OnTypeChanged(typeList->currentItem(), nullptr);
}

void MixishAddInputController::HideObsOnlyControls()
{
	for (const char *name : {"addExistingContainer", "existingScrollArea", "noExistingLabel", "sourceVisible"}) {
		if (QWidget *widget = dialog->findChild<QWidget *>(QString::fromLatin1(name)))
			widget->hide();
	}

	if (auto *box = dialog->findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"))) {
		if (QPushButton *close = box->button(QDialogButtonBox::Close))
			close->setText(tr("Cancel"));
	}
}

void MixishAddInputController::RemoveUnsupportedTypeItems()
{
	const QString sceneId = QStringLiteral("scene");
	const QString recentId = QStringLiteral("_recent");

	QListWidgetItem *firstValid = nullptr;

	{
		QSignalBlocker block(typeList);

		for (int i = typeList->count() - 1; i >= 0; --i) {
			QListWidgetItem *item = typeList->item(i);
			if (!item)
				continue;

			const QString type = item->data(kUnversionedIdRole).toString();
			if (type == sceneId || type == recentId)
				delete typeList->takeItem(i);
		}

		for (int i = 0; i < typeList->count(); ++i) {
			QListWidgetItem *item = typeList->item(i);
			if (!item || !(item->flags() & Qt::ItemIsEnabled))
				continue;

			const QString type = item->data(kUnversionedIdRole).toString();
			if (!type.isEmpty() && type != sceneId && type != recentId) {
				firstValid = item;
				break;
			}
		}
	}

	if (firstValid)
		typeList->setCurrentItem(firstValid);
}

void MixishAddInputController::BuildRecentTypesRow()
{
	auto *mainLayout = dialog->findChild<QVBoxLayout *>(QStringLiteral("verticalLayout_7"));
	if (!mainLayout)
		return;

	if (!recentTypesFrame) {
		recentTypesFrame = new QFrame(dialog);
		recentTypesFrame->setObjectName(QStringLiteral("mixishRecentTypesFrame"));
		auto *layout = new QHBoxLayout(recentTypesFrame);
		layout->setContentsMargins(8, 6, 8, 6);
		layout->setSpacing(6);
		mainLayout->insertWidget(0, recentTypesFrame);
	}

	QLayout *layout = recentTypesFrame->layout();
	while (QLayoutItem *item = layout->takeAt(0)) {
		if (item->widget())
			item->widget()->deleteLater();
		delete item;
	}

	auto *label = new QLabel(tr("Recent:"), recentTypesFrame);
	MixishTheme::SetRole(label, "muted");
	layout->addWidget(label);

	int added = 0;
	for (const QString &type : recentTypes) {
		QListWidgetItem *item = FindTypeItem(type);
		if (!item)
			continue;

		auto *button = new QPushButton(item->icon(), item->text(), recentTypesFrame);
		button->setToolTip(tr("Use the %1 input type").arg(item->text()));
		button->setMaximumWidth(180);
		connect(button, &QPushButton::clicked, this, [this, item]() { typeList->setCurrentItem(item); });
		layout->addWidget(button);
		added++;
	}

	if (auto *box = qobject_cast<QBoxLayout *>(layout))
		box->addStretch(1);
	recentTypesFrame->setVisible(added > 0);
}

void MixishAddInputController::BuildMediaPanel()
{
	auto *selectSourceFrame = dialog->findChild<QWidget *>(QStringLiteral("selectSourceFrame"));
	if (!selectSourceFrame)
		return;

	if (mediaPanel)
		return;

	mediaPanel = new QFrame(dialog);
	mediaPanel->setObjectName(QStringLiteral("mixishMediaPanel"));
	mediaPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	auto *layout = new QVBoxLayout(mediaPanel);
	layout->setContentsMargins(8, 8, 8, 8);
	layout->setSpacing(6);

	auto *title = new QLabel(tr("Media / Video"), mediaPanel);
	MixishTheme::SetRole(title, "body");
	QFont titleFont = title->font();
	titleFont.setBold(true);
	title->setFont(titleFont);

	auto *hint = new QLabel(tr("Recent files"), mediaPanel);
	MixishTheme::SetRole(hint, "muted");

	mediaList = new QListWidget(mediaPanel);
	mediaList->setObjectName(QStringLiteral("mixishMediaList"));
	mediaList->setMinimumHeight(160);

	auto *browseButton = new QPushButton(tr("Browse..."), mediaPanel);
	browseButton->setObjectName(QStringLiteral("mixishBrowseMedia"));
	MixishTheme::SetVariant(browseButton, "secondary");

	auto *propertiesHint = new QLabel(
		tr("Alpha and interlace options are available in the source properties."), mediaPanel);
	propertiesHint->setWordWrap(true);
	MixishTheme::SetRole(propertiesHint, "muted");

	layout->addWidget(title);
	layout->addWidget(hint);
	layout->addWidget(mediaList, 1);
	layout->addWidget(browseButton);
	layout->addWidget(propertiesHint);

	if (auto *box = qobject_cast<QBoxLayout *>(selectSourceFrame->layout())) {
		box->insertWidget(0, mediaPanel);
		box->setStretchFactor(mediaPanel, 1);
	}

	connect(mediaList, &QListWidget::itemClicked, this, &MixishAddInputController::OnRecentMediaClicked);
	connect(browseButton, &QPushButton::clicked, this, &MixishAddInputController::BrowseMedia);

	RefreshMediaList();
	mediaPanel->hide();
}

void MixishAddInputController::BuildTypeInfoPanel()
{
	auto *selectSourceFrame = dialog->findChild<QWidget *>(QStringLiteral("selectSourceFrame"));
	if (!selectSourceFrame || typeInfoPanel)
		return;

	typeInfoPanel = new QFrame(dialog);
	typeInfoPanel->setObjectName(QStringLiteral("mixishTypeInfoPanel"));
	typeInfoPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	auto *layout = new QVBoxLayout(typeInfoPanel);
	layout->setContentsMargins(24, 24, 24, 24);
	layout->setSpacing(10);

	layout->addStretch(1);

	typeInfoIcon = new QLabel(typeInfoPanel);
	typeInfoIcon->setAlignment(Qt::AlignCenter);
	typeInfoIcon->setFixedHeight(104);

	typeInfoName = new QLabel(typeInfoPanel);
	typeInfoName->setAlignment(Qt::AlignCenter);
	typeInfoName->setWordWrap(true);
	MixishTheme::MakeTitleLabel(typeInfoName);

	typeInfoDescription = new QLabel(typeInfoPanel);
	typeInfoDescription->setAlignment(Qt::AlignCenter);
	typeInfoDescription->setWordWrap(true);
	typeInfoDescription->setFixedWidth(420);
	typeInfoDescription->setMinimumHeight(38);
	MixishTheme::SetRole(typeInfoDescription, "body");

	auto *hint = new QLabel(tr("The new input is added to the input bar without changing Preview or Program."),
				typeInfoPanel);
	hint->setAlignment(Qt::AlignCenter);
	hint->setWordWrap(true);
	hint->setFixedWidth(420);
	hint->setMinimumHeight(28);
	MixishTheme::SetRole(hint, "muted");

	layout->addWidget(typeInfoIcon);
	layout->addWidget(typeInfoName);
	layout->addWidget(typeInfoDescription, 0, Qt::AlignHCenter);
	layout->addWidget(hint, 0, Qt::AlignHCenter);
	layout->addStretch(1);

	if (auto *box = qobject_cast<QBoxLayout *>(selectSourceFrame->layout())) {
		box->insertWidget(0, typeInfoPanel);
		box->setStretchFactor(typeInfoPanel, 1);
	}
}

void MixishAddInputController::UpdateTypeInfoPanel()
{
	if (!typeInfoPanel || !typeList)
		return;

	QListWidgetItem *item = typeList->currentItem();
	if (!item) {
		typeInfoPanel->hide();
		return;
	}

	const QString type = item->data(kUnversionedIdRole).toString();
	if (typeInfoName)
		typeInfoName->setText(item->text());
	if (typeInfoDescription)
		typeInfoDescription->setText(TypeDescription(type));

	const QIcon icon = item->icon();
	if (typeInfoIcon && !icon.isNull()) {
		typeInfoIcon->setPixmap(icon.pixmap(96, 96));
		typeInfoIcon->show();
	} else if (typeInfoIcon) {
		typeInfoIcon->clear();
		typeInfoIcon->hide();
	}

	typeInfoPanel->show();
}

void MixishAddInputController::BuildNumberField()
{
	auto *footer = dialog->findChild<QWidget *>(QStringLiteral("footer"));
	QBoxLayout *layout = nullptr;
	if (footer)
		layout = qobject_cast<QBoxLayout *>(footer->layout());
	if (!layout) {
		if (auto *selectSourceFrame = dialog->findChild<QWidget *>(QStringLiteral("selectSourceFrame")))
			layout = qobject_cast<QBoxLayout *>(selectSourceFrame->layout());
	}
	if (!layout)
		return;

	numberWidget = new QWidget(dialog);
	auto *row = new QHBoxLayout(numberWidget);
	row->setContentsMargins(0, 0, 0, 0);
	row->setSpacing(6);

	auto *label = new QLabel(tr("Input Number"), numberWidget);
	numberSpin = new QSpinBox(numberWidget);
	numberSpin->setRange(0, 9);
	numberSpin->setSpecialValueText(tr("Auto"));
	numberSpin->setValue(0);
	numberSpin->setToolTip(tr("Optional input slot 1-9. This also sets the number-key shortcut."));

	row->addWidget(label);
	row->addWidget(numberSpin);

	layout->insertWidget(0, numberWidget);
}

void MixishAddInputController::UpdateMediaPanel()
{
	const bool media = IsMediaType(SelectedTypeId());

	if (mediaPanel)
		mediaPanel->setVisible(media);
	if (typeInfoPanel)
		typeInfoPanel->setVisible(!media);

	if (media)
		RefreshMediaList();
	else
		UpdateTypeInfoPanel();
}

void MixishAddInputController::RefreshMediaList()
{
	if (!mediaList)
		return;

	QSignalBlocker block(mediaList);
	mediaList->clear();

	for (const QString &path : recentMedia) {
		const QFileInfo info(path);
		QString label = info.fileName();
		if (!info.exists())
			label += tr(" (missing)");

		auto *item = new QListWidgetItem(label, mediaList);
		item->setData(Qt::UserRole, path);
		item->setToolTip(path);
		if (path == selectedMediaFile)
			mediaList->setCurrentItem(item);
	}

	if (mediaList->count() == 0) {
		auto *empty = new QListWidgetItem(tr("No recent media files"), mediaList);
		empty->setFlags(empty->flags() & ~Qt::ItemIsEnabled);
	}
}

void MixishAddInputController::ApplyMixishStyle()
{
	MixishTheme::Apply(dialog);
}

void MixishAddInputController::LoadRecentLists()
{
	recentTypes = RecentConfigString("MixishRecentInputTypes").split('\n', Qt::SkipEmptyParts);
	recentMedia = RecentConfigString("MixishRecentMediaFiles").split('\n', Qt::SkipEmptyParts);
}

void MixishAddInputController::SaveRecentTypes()
{
	SaveRecentConfigString("MixishRecentInputTypes", recentTypes.join('\n'));
}

void MixishAddInputController::SaveRecentMedia()
{
	SaveRecentConfigString("MixishRecentMediaFiles", recentMedia.join('\n'));
}

void MixishAddInputController::AddRecentType(const QString &type)
{
	recentTypes.removeAll(type);
	recentTypes.prepend(type);
	while (recentTypes.size() > kRecentTypeLimit)
		recentTypes.removeLast();
	SaveRecentTypes();
	BuildRecentTypesRow();
}

void MixishAddInputController::AddRecentMedia(const QString &path)
{
	recentMedia.removeAll(path);
	recentMedia.prepend(path);
	while (recentMedia.size() > kRecentMediaLimit)
		recentMedia.removeLast();
	SaveRecentMedia();
	RefreshMediaList();
}

QString MixishAddInputController::SelectedTypeId() const
{
	if (!typeList || !typeList->currentItem())
		return QString();
	return typeList->currentItem()->data(kUnversionedIdRole).toString();
}

bool MixishAddInputController::IsMediaType(const QString &type) const
{
	return type == QStringLiteral("ffmpeg_source") || type == QStringLiteral("vlc_source");
}

QString MixishAddInputController::TypeDescription(const QString &type) const
{
	if (type == QStringLiteral("monitor_capture"))
		return tr("Capture an entire display. Choose the display, capture method, and cursor options in Properties.");
	if (type == QStringLiteral("window_capture"))
		return tr("Capture a single window. Pick the window and capture method in Properties.");
	if (type == QStringLiteral("game_capture"))
		return tr("Capture a game or fullscreen application using Mixish's game capture hooks.");
	if (type == QStringLiteral("dshow_input"))
		return tr("Capture video from a webcam, capture card, or another DirectShow device.");
	if (type == QStringLiteral("wasapi_input_capture"))
		return tr("Capture audio from a microphone or another input device.");
	if (type == QStringLiteral("wasapi_output_capture"))
		return tr("Capture desktop audio from a playback device.");
	if (type == QStringLiteral("wasapi_process_output_capture"))
		return tr("Capture audio from a single application.");
	if (type == QStringLiteral("browser_source"))
		return tr("Display a web page, HTML overlay, or browser-based widget.");
	if (type == QStringLiteral("color_source"))
		return tr("A solid color input. Choose the color in Properties.");
	if (type == QStringLiteral("image_source"))
		return tr("Display a still image file.");
	if (type == QStringLiteral("slideshow"))
		return tr("Cycle through a folder of images.");
	if (type == QStringLiteral("text_gdiplus") || type == QStringLiteral("text_ft2_source"))
		return tr("Render text, titles, or lower-thirds with fonts and effects.");
	if (type == QStringLiteral("ffmpeg_source"))
		return tr("Play a video or audio file. Recent files and Browse are available on the right.");
	if (type == QStringLiteral("vlc_source"))
		return tr("Play a local playlist or network stream using VLC.");
	if (type.contains(QStringLiteral("asio")))
		return tr("Capture low-latency audio from an ASIO device.");
	if (type.contains(QStringLiteral("ndi")))
		return tr("Receive a video source over the network with NDI.");
	if (type.contains(QStringLiteral("srt")))
		return tr("Receive a video stream over SRT.");

	return tr("Create this input in its own hidden scene. Detailed settings are available in Properties after creation.");
}

QListWidgetItem *MixishAddInputController::FindTypeItem(const QString &type) const
{
	if (!typeList)
		return nullptr;

	for (int i = 0; i < typeList->count(); ++i) {
		QListWidgetItem *item = typeList->item(i);
		if (item && item->data(kUnversionedIdRole).toString() == type)
			return item;
	}

	return nullptr;
}

void MixishAddInputController::OnTypeChanged(QListWidgetItem *, QListWidgetItem *)
{
	HideObsOnlyControls();

	if (auto *title = dialog->findChild<QLabel *>(QStringLiteral("sourceSelectTitle")))
		title->setText(tr("Add Input"));
	if (auto *description = dialog->findChild<QLabel *>(QStringLiteral("sourceSelectDescription")))
		description->setText(
			tr("Create a new input in its own hidden scene. The current Preview and Program scenes are not changed."));

	const QString type = SelectedTypeId();
	if (!IsMediaType(type))
		selectedMediaFile.clear();

	UpdateMediaPanel();

	if (createButton) {
		createButton->setText(tr("Create Input"));
		createButton->hide();
	}
}

void MixishAddInputController::CreateInput()
{
	const QString type = SelectedTypeId();
	if (type.isEmpty() || type == QStringLiteral("scene"))
		return;

	QListWidgetItem *typeItem = FindTypeItem(type);
	if (!typeItem)
		return;

	QString baseName = typeItem->data(Qt::DisplayRole).toString();
	if (baseName.isEmpty())
		baseName = tr("Input");

	std::string name = baseName.toStdString();
	for (int suffix = 2;; ++suffix) {
		OBSSourceAutoRelease existing = obs_get_source_by_name(name.c_str());
		if (!existing)
			break;
		name = baseName.toStdString() + " " + std::to_string(suffix);
	}

	OBSDataAutoRelease settings = obs_data_create();
	if (!selectedMediaFile.isEmpty()) {
		if (type == QStringLiteral("ffmpeg_source")) {
			obs_data_set_bool(settings, "is_local_file", true);
			obs_data_set_string(settings, "local_file", selectedMediaFile.toUtf8().constData());
		} else if (type == QStringLiteral("vlc_source")) {
			OBSDataArrayAutoRelease playlist = obs_data_array_create();
			OBSDataAutoRelease entry = obs_data_create();
			obs_data_set_string(entry, "value", selectedMediaFile.toUtf8().constData());
			obs_data_set_bool(entry, "hidden", false);
			obs_data_array_push_back(playlist, entry);
			obs_data_set_array(settings, "playlist", playlist);
		}
	}

	const char *inputId = obs_get_latest_input_type_id(type.toUtf8().constData());
	OBSSourceAutoRelease source = obs_source_create(inputId, name.c_str(), settings, nullptr);
	if (!source) {
		QMessageBox::warning(dialog, tr("Add Input"), tr("Unable to create the input."));
		return;
	}

	const int number = numberSpin ? numberSpin->value() : 0;
	OBSScene scene = switcher->CreateInputScene(OBSSource(source.Get()), number);
	if (!scene) {
		QMessageBox::warning(dialog, tr("Add Input"), tr("Unable to create the input scene."));
		return;
	}

	AddRecentType(type);
	if (IsMediaType(type) && !selectedMediaFile.isEmpty())
		AddRecentMedia(selectedMediaFile);

	switcher->OpenInputProperties(scene, number);
	dialog->close();
}

void MixishAddInputController::BrowseMedia()
{
	const QString path = QFileDialog::getOpenFileName(
		dialog, tr("Select Media File"), QString(),
		tr("Media Files (*.mp4 *.mkv *.mov *.avi *.webm *.flv *.ts *.m4v *.mp3 *.wav *.flac *.ogg);;All Files (*)"));
	if (path.isEmpty())
		return;

	selectedMediaFile = path;
	AddRecentMedia(path);
	RefreshMediaList();
}

void MixishAddInputController::OnRecentMediaClicked(QListWidgetItem *item)
{
	if (!item)
		return;

	const QString path = item->data(Qt::UserRole).toString();
	if (!path.isEmpty())
		selectedMediaFile = path;
}
