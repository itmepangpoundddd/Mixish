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

#include "MixishInputProperties.hpp"
#include "MixishTheme.hpp"

#include <utility/display-helpers.hpp>
#include <widgets/OBSBasic.hpp>
#include <widgets/OBSQTDisplay.hpp>

#include <properties-view.hpp>

#include <QBoxLayout>
#include <QCloseEvent>
#include <QComboBox>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cstring>
#include <string>

#include "moc_MixishInputProperties.cpp"

namespace {


static QString SourceTypeName(obs_source_t *source)
{
	if (!source)
		return QString();

	const char *id = obs_source_get_id(source);
	const char *name = id ? obs_source_get_display_name(id) : nullptr;
	return name ? QString::fromUtf8(name) : QString();
}

} // namespace

MixishInputProperties::MixishInputProperties(QWidget *parent, OBSBasic *main_, OBSScene scene, OBSSource source_,
					     int number_)
	: QDialog(parent),
	  main(main_),
	  sceneSource(obs_scene_get_source(scene)),
	  source(source_ ? source_ : sceneSource),
	  number(number_),
	  oldSettings(obs_data_create())
{
	/* The OBSData constructor takes one reference. */
	obs_data_release(oldSettings);

	if (source) {
		OBSDataAutoRelease settings = obs_source_get_settings(source);
		obs_data_apply(oldSettings, settings);
	}

	setObjectName(QStringLiteral("MixishInputProperties"));
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setSizeGripEnabled(true);
	resize(980, 720);
	setMinimumSize(760, 540);

	BuildUi();

	if (sceneSource) {
		sceneRemovedSignal.Connect(obs_source_get_signal_handler(sceneSource), "remove",
					   MixishInputProperties::SourceRemoved, this);
		sceneRenamedSignal.Connect(obs_source_get_signal_handler(sceneSource), "rename",
					   MixishInputProperties::SourceRenamed, this);
	}

	if (source) {
		sourceRemovedSignal.Connect(obs_source_get_signal_handler(source), "remove",
					    MixishInputProperties::SourceRemoved, this);
		sourceRenamedSignal.Connect(obs_source_get_signal_handler(source), "rename",
					    MixishInputProperties::SourceRenamed, this);
		updatePropertiesSignal.Connect(obs_source_get_signal_handler(source), "update_properties",
					       MixishInputProperties::UpdateProperties, this);
		obs_source_inc_showing(source);
	}

	UpdateTitle();
	PopulateSections();
}

MixishInputProperties::~MixishInputProperties()
{
	if (preview && preview->GetDisplay())
		obs_display_remove_draw_callback(preview->GetDisplay(), MixishInputProperties::DrawPreview, this);

	if (source)
		obs_source_dec_showing(source);

	if (main)
		main->SaveProject();
}

void MixishInputProperties::BuildUi()
{
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(0, 0, 0, 0);
	root->setSpacing(0);

	auto *header = new QFrame(this);
	header->setObjectName(QStringLiteral("mixishPropsHeader"));
	header->setFixedHeight(58);

	auto *headerLayout = new QHBoxLayout(header);
	headerLayout->setContentsMargins(12, 6, 12, 6);
	headerLayout->setSpacing(12);

	auto *tab = new QLabel(tr("GENERAL"), header);
	tab->setObjectName(QStringLiteral("mixishPropsTab"));
	tab->setAlignment(Qt::AlignCenter);
	tabLabel = tab;

	auto *titleColumn = new QVBoxLayout();
	titleColumn->setContentsMargins(0, 0, 0, 0);
	titleColumn->setSpacing(1);

	titleLabel = new QLabel(header);
	titleLabel->setObjectName(QStringLiteral("mixishPropsTitle"));
	titleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

	subtitleLabel = new QLabel(header);
	subtitleLabel->setObjectName(QStringLiteral("mixishPropsSubtitle"));
	subtitleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

	titleColumn->addWidget(titleLabel);
	titleColumn->addWidget(subtitleLabel);

	headerLayout->addWidget(tab);
	headerLayout->addLayout(titleColumn, 1);

	auto *body = new QHBoxLayout();
	body->setContentsMargins(0, 0, 0, 0);
	body->setSpacing(0);

	sectionList = new QListWidget(this);
	sectionList->setObjectName(QStringLiteral("mixishPropsSidebar"));
	sectionList->setFixedWidth(190);
	sectionList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	sectionList->setFocusPolicy(Qt::NoFocus);
	body->addWidget(sectionList);

	auto *content = new QWidget(this);
	auto *contentLayout = new QVBoxLayout(content);
	contentLayout->setContentsMargins(0, 0, 0, 0);
	contentLayout->setSpacing(0);

	auto *host = new QFrame(content);
	host->setObjectName(QStringLiteral("mixishPropsHost"));
	auto *hostLayout = new QVBoxLayout(host);
	hostLayout->setContentsMargins(14, 14, 14, 14);
	hostLayout->setSpacing(6);

	emptyLabel = new QLabel(tr("This input has no configurable properties."), host);
	emptyLabel->setAlignment(Qt::AlignCenter);
	emptyLabel->setWordWrap(true);
	MixishTheme::SetRole(emptyLabel, "muted");
	emptyLabel->hide();
	hostLayout->addWidget(emptyLabel);

	if (source) {
		OBSDataAutoRelease settings = obs_source_get_settings(source);
		view = new OBSPropertiesView(settings.Get(), source, (PropertiesReloadCallback)obs_source_properties,
					     (PropertiesUpdateCallback) nullptr,
					     (PropertiesVisualUpdateCb)obs_source_update);
		view->setObjectName(QStringLiteral("mixishPropertiesView"));
		hostLayout->addWidget(view, 1);

		connect(view, &OBSPropertiesView::PropertiesRefreshed, this, [this]() {
			QTimer::singleShot(0, this, &MixishInputProperties::PopulateSections);
		});
	}

	contentLayout->addWidget(host, 1);
	body->addWidget(content, 1);

	/* vMix keeps the live input preview under the settings body, spanning the
	 * full width. */
	auto *previewFrame = new QFrame(this);
	previewFrame->setObjectName(QStringLiteral("mixishPropsPreview"));
	previewFrame->setMinimumHeight(220);

	auto *previewLayout = new QVBoxLayout(previewFrame);
	previewLayout->setContentsMargins(1, 1, 1, 1);
	previewLayout->setSpacing(0);

	preview = new OBSQTDisplay(previewFrame);
	preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	preview->setMinimumHeight(200);
	previewLayout->addWidget(preview);

	connect(preview, &OBSQTDisplay::DisplayCreated, this, [this]() {
		if (preview && preview->GetDisplay())
			obs_display_add_draw_callback(preview->GetDisplay(), MixishInputProperties::DrawPreview, this);
	});

	if (!source || !(obs_source_get_output_flags(source) & OBS_SOURCE_VIDEO))
		previewFrame->hide();

	contentLayout->addWidget(previewFrame);

	auto *footer = new QFrame(this);
	footer->setObjectName(QStringLiteral("mixishPropsFooter"));
	footer->setFixedHeight(54);

	auto *footerLayout = new QHBoxLayout(footer);
	footerLayout->setContentsMargins(12, 8, 12, 8);
	footerLayout->setSpacing(8);

	defaultsButton = new QPushButton(tr("Restore Defaults"), footer);
	MixishTheme::SetVariant(defaultsButton, "secondary");
	defaultsButton->setFixedHeight(28);
	okButton = new QPushButton(tr("OK"), footer);
	okButton->setObjectName(QStringLiteral("mixishPropsOk"));
	MixishTheme::SetVariant(okButton, "primary");
	okButton->setFixedHeight(28);
	cancelButton = new QPushButton(tr("Cancel"), footer);
	MixishTheme::SetVariant(cancelButton, "secondary");
	cancelButton->setFixedHeight(28);

	footerLayout->addWidget(defaultsButton);
	footerLayout->addStretch(1);
	footerLayout->addWidget(cancelButton);
	footerLayout->addWidget(okButton);

	root->addWidget(header);
	root->addLayout(body, 1);
	root->addWidget(footer);

	connect(sectionList, &QListWidget::currentItemChanged, this,
		[this](QListWidgetItem *current, QListWidgetItem *) { ScrollToSection(current); });
	connect(defaultsButton, &QPushButton::clicked, this, &MixishInputProperties::RestoreDefaults);
	connect(okButton, &QPushButton::clicked, this, &QWidget::close);
	connect(cancelButton, &QPushButton::clicked, this, &MixishInputProperties::RevertChanges);

	MixishTheme::Apply(this);
}

void MixishInputProperties::UpdateTitle()
{
	if (!source)
		return;

	const char *sceneName = sceneSource ? obs_source_get_name(sceneSource) : obs_source_get_name(source);
	const QString name = sceneName ? QString::fromUtf8(sceneName) : QString();

	QString title = number > 0 ? tr("Input %1 %2").arg(number).arg(name) : name;
	if (titleLabel)
		titleLabel->setText(title);
	setWindowTitle(title);

	QString subtitle = SourceTypeName(source);
	const char *sourceName = obs_source_get_name(source);
	if (sourceName && *sourceName && (!sceneName || std::strcmp(sourceName, sceneName) != 0)) {
		if (!subtitle.isEmpty())
			subtitle += QStringLiteral("  \u00b7  ");
		subtitle += QString::fromUtf8(sourceName);
	}

	if (subtitleLabel)
		subtitleLabel->setText(subtitle);
}

void MixishInputProperties::PopulateSections()
{
	if (!sectionList)
		return;

	const QString current = sectionList->currentItem() ? sectionList->currentItem()->text() : QString();
	QSignalBlocker block(sectionList);
	sectionList->clear();

	auto *general = new QListWidgetItem(tr("General"), sectionList);
	general->setData(Qt::UserRole, QString());

	bool hasProperties = false;

	if (source) {
		obs_properties_t *properties = obs_source_properties(source);
		if (properties) {
			for (obs_property_t *property = obs_properties_first(properties); property;
			     obs_property_next(&property)) {
				if (!obs_property_visible(property))
					continue;

				hasProperties = true;

				if (obs_property_get_type(property) != OBS_PROPERTY_GROUP)
					continue;

				const char *description = obs_property_description(property);
				const char *name = obs_property_name(property);
				QString label = description && *description ? QString::fromUtf8(description)
									    : QString::fromUtf8(name ? name : "");
				if (label.isEmpty())
					continue;

				bool duplicate = false;
				for (int i = 0; i < sectionList->count(); ++i) {
					if (sectionList->item(i)->text() == label) {
						duplicate = true;
						break;
					}
				}
				if (duplicate)
					continue;

				auto *item = new QListWidgetItem(label, sectionList);
				item->setData(Qt::UserRole, label);
			}
			obs_properties_destroy(properties);
		}
	}

	int restoreRow = 0;
	for (int i = 0; i < sectionList->count(); ++i) {
		if (sectionList->item(i)->text() == current) {
			restoreRow = i;
			break;
		}
	}
	sectionList->setCurrentRow(restoreRow);

	if (tabLabel) {
		QListWidgetItem *item = sectionList->currentItem();
		tabLabel->setText((item ? item->text() : tr("General")).toUpper());
	}

	if (view)
		view->setVisible(hasProperties);
	if (emptyLabel)
		emptyLabel->setVisible(!hasProperties);
}

void MixishInputProperties::ScrollToSection(QListWidgetItem *item)
{
	if (!view || !item)
		return;

	if (tabLabel)
		tabLabel->setText(item->text().toUpper());

	const QString title = item->data(Qt::UserRole).toString();
	if (title.isEmpty()) {
		view->verticalScrollBar()->setValue(0);
		return;
	}

	const auto groups = view->findChildren<QGroupBox *>();
	for (QGroupBox *group : groups) {
		if (group->title() == title) {
			view->ensureWidgetVisible(group, 0, 16);
			return;
		}
	}

	view->verticalScrollBar()->setValue(0);
}

void MixishInputProperties::RestoreDefaults()
{
	if (!source)
		return;

	const auto answer = QMessageBox::question(this, tr("Restore Defaults"),
						  tr("Reset all settings for this input to their defaults?"),
						  QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
	if (answer != QMessageBox::Yes)
		return;

	OBSDataAutoRelease settings = obs_source_get_settings(source);
	obs_data_clear(settings);

	if (view && !view->DeferUpdate())
		obs_source_update(source, nullptr);

	if (view)
		view->ReloadProperties();
}

void MixishInputProperties::RevertChanges()
{
	discardChanges = true;

	if (source && !obs_source_removed(source))
		obs_source_reset_settings(source, oldSettings);

	close();
}

void MixishInputProperties::CommitSettings()
{
	if (!source || !main || obs_source_removed(source))
		return;

	if (view && view->DeferUpdate())
		view->UpdateSettings();

	OBSDataAutoRelease newSettings = obs_data_create();
	OBSDataAutoRelease currentSettings = obs_source_get_settings(source);
	obs_data_apply(newSettings, currentSettings);

	const char *uuid = obs_source_get_uuid(source);
	obs_data_set_string(newSettings, "undo_uuid", uuid);
	obs_data_set_string(oldSettings, "undo_uuid", uuid);

	std::string undoData(obs_data_get_json(oldSettings));
	std::string redoData(obs_data_get_json(newSettings));
	if (undoData.compare(redoData) == 0)
		return;

	OBSSource currentScene = main->GetCurrentSceneSource();
	const char *sceneUuidPtr = currentScene ? obs_source_get_uuid(currentScene) : nullptr;
	const std::string sceneUuid = sceneUuidPtr ? sceneUuidPtr : "";
	auto undoRedo = [sceneUuid](const std::string &data) {
		OBSDataAutoRelease settings = obs_data_create_from_json(data.c_str());
		OBSSourceAutoRelease source = obs_get_source_by_uuid(obs_data_get_string(settings, "undo_uuid"));
		if (!source)
			return;

		obs_source_reset_settings(source, settings);
		obs_source_update_properties(source);

		OBSBasic *main = OBSBasic::Get();
		OBSSourceAutoRelease sceneSource = obs_get_source_by_uuid(sceneUuid.c_str());
		if (main && sceneSource)
			main->SetCurrentScene(sceneSource.Get(), true);
	};

	const char *name = obs_source_get_name(source);
	main->undo_s.add_action(tr("Edit %1 Properties").arg(QString::fromUtf8(name ? name : "")), undoRedo,
				undoRedo, undoData, redoData);
}

void MixishInputProperties::closeEvent(QCloseEvent *event)
{
	if (!discardChanges)
		CommitSettings();

	QDialog::closeEvent(event);
}

void MixishInputProperties::SourceRemoved(void *data, calldata_t *)
{
	auto *window = static_cast<MixishInputProperties *>(data);
	window->discardChanges = true;
	QMetaObject::invokeMethod(window, [window]() { window->close(); }, Qt::QueuedConnection);
}

void MixishInputProperties::SourceRenamed(void *data, calldata_t *)
{
	auto *window = static_cast<MixishInputProperties *>(data);
	QMetaObject::invokeMethod(window, &MixishInputProperties::UpdateTitle, Qt::QueuedConnection);
}

void MixishInputProperties::UpdateProperties(void *data, calldata_t *)
{
	auto *window = static_cast<MixishInputProperties *>(data);
	if (window->view)
		QMetaObject::invokeMethod(window->view, &OBSPropertiesView::ReloadProperties, Qt::QueuedConnection);
}

void MixishInputProperties::DrawPreview(void *data, uint32_t cx, uint32_t cy)
{
	auto *window = static_cast<MixishInputProperties *>(data);
	if (!window->source)
		return;

	const uint32_t sourceCX = std::max(obs_source_get_width(window->source), 1u);
	const uint32_t sourceCY = std::max(obs_source_get_height(window->source), 1u);

	int x, y;
	int newCX, newCY;
	float scale;

	GetScaleAndCenterPos(sourceCX, sourceCY, cx, cy, x, y, scale);

	newCX = int(scale * float(sourceCX));
	newCY = int(scale * float(sourceCY));

	gs_viewport_push();
	gs_projection_push();
	const bool previous = gs_set_linear_srgb(true);

	gs_ortho(0.0f, float(sourceCX), 0.0f, float(sourceCY), -100.0f, 100.0f);
	gs_set_viewport(x, y, newCX, newCY);
	obs_source_video_render(window->source);

	gs_set_linear_srgb(previous);
	gs_projection_pop();
	gs_viewport_pop();
}
