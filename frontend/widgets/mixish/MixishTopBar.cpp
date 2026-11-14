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

#include "MixishTopBar.hpp"
#include "MixishTheme.hpp"
#include "MixishSwitcher.hpp"

#include <widgets/OBSBasic.hpp>

#include <QAction>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QSizePolicy>
#include <QToolButton>
#include <QWidget>

#include "moc_MixishTopBar.cpp"

MixishTopBar::MixishTopBar(MixishSwitcher *switcher_, OBSBasic *main_, QWidget *parent)
	: QToolBar(parent),
	  switcher(switcher_),
	  main(main_)
{
	setObjectName(QStringLiteral("mixishTopBar"));
	setMovable(false);
	setFloatable(false);
	setIconSize(QSize(18, 18));
	setToolButtonStyle(Qt::ToolButtonTextOnly);
	setFixedHeight(MixishTheme::TopBarHeight);

	auto *logoWidget = new QWidget(this);
	auto *logoLayout = new QHBoxLayout(logoWidget);
	logoLayout->setContentsMargins(4, 0, 12, 0);
	logoLayout->setSpacing(MixishTheme::Space2);

	auto *logoIcon = new QLabel(logoWidget);
	logoIcon->setPixmap(QIcon(QStringLiteral(":/res/images/mixish-icon.svg")).pixmap(24, 24));
	logoIcon->setFixedSize(24, 24);

	auto *logoText = new QLabel(QStringLiteral("MIXISH"), logoWidget);
	logoText->setObjectName(QStringLiteral("mixishLogo"));
	QFont logoFont = logoText->font();
	logoFont.setPixelSize(14);
	logoFont.setBold(true);
	logoFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
	logoText->setFont(logoFont);

	logoLayout->addWidget(logoIcon);
	logoLayout->addWidget(logoText);
	addWidget(logoWidget);

	BuildPresetMenu();

	auto *presetButton = MakeMenuButton(tr("Preset"), presetMenu);
	presetButton->setToolTip(tr("Preset / scene collection and Mixish menus"));

	auto *newAction = addAction(tr("New"));
	newAction->setToolTip(tr("New scene collection"));
	connect(newAction, &QAction::triggered, main->ui->actionNewSceneCollection, &QAction::trigger);

	auto *openMenu = new QMenu(this);
	openMenu->addAction(tr("Import Scene Collection..."), main->ui->actionImportSceneCollection,
			    &QAction::trigger);
	openMenu->addAction(tr("Import Profile..."), main->ui->actionImportProfile, &QAction::trigger);
	openMenu->addSeparator();
	openMenu->addAction(tr("Show Settings Folder"), main->ui->actionShowSettingsFolder, &QAction::trigger);
	MakeMenuButton(tr("Open"), openMenu)->setToolTip(tr("Import a scene collection or profile"));

	auto *saveAction = addAction(tr("Save"));
	saveAction->setToolTip(tr("Save scene collection"));
	connect(saveAction, &QAction::triggered, main, [this]() { main->SaveProject(); });

	auto *saveAsMenu = new QMenu(this);
	saveAsMenu->addAction(tr("Export Scene Collection..."), main->ui->actionExportSceneCollection,
			      &QAction::trigger);
	saveAsMenu->addAction(tr("Export Profile..."), main->ui->actionExportProfile, &QAction::trigger);
	MakeMenuButton(tr("Save As"), saveAsMenu)->setToolTip(tr("Export scene collection or profile"));

	auto *lastAction = addAction(tr("Last"));
	lastAction->setEnabled(false);
	lastAction->setToolTip(tr("Last preset (not available yet)"));

	auto *fullscreenMenu = new QMenu(this);
	fullscreenMenu->addAction(tr("Fullscreen Interface"), main->ui->actionFullscreenInterface,
				  &QAction::trigger);
	fullscreenMenu->addAction(tr("Always on Top"), main->ui->actionAlwaysOnTop, &QAction::trigger);
	MakeMenuButton(tr("Fullscreen"), fullscreenMenu);

	auto *spacer = new QWidget(this);
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	addWidget(spacer);

	auto *settingsMenu = new QMenu(this);
	settingsMenu->addAction(tr("Settings..."), main->ui->action_Settings, &QAction::trigger);
	settingsMenu->addAction(tr("Auto-Configuration Wizard"), main->ui->autoConfigure, &QAction::trigger);
	settingsMenu->addSeparator();
	settingsMenu->addAction(tr("Advanced Audio Properties"), main->ui->actionAdvAudioProperties,
				&QAction::trigger);
	MakeMenuButton(tr("Settings"), settingsMenu);

	auto *helpMenu = new QMenu(this);
	helpMenu->addAction(tr("Help Portal"), main->ui->actionHelpPortal, &QAction::trigger);
	helpMenu->addAction(tr("Show Log Files"), main->ui->actionShowLogs, &QAction::trigger);
	helpMenu->addSeparator();
	helpMenu->addAction(tr("About Mixish"), main->ui->actionShowAbout, &QAction::trigger);
	MakeMenuButton(tr("?"), helpMenu)->setToolTip(tr("Help"));

	MixishTheme::Apply(this);
}

QToolButton *MixishTopBar::MakeMenuButton(const QString &text, QMenu *menu)
{
	auto *button = new QToolButton(this);
	button->setText(text);
	button->setMenu(menu);
	button->setPopupMode(QToolButton::InstantPopup);
	button->setToolButtonStyle(Qt::ToolButtonTextOnly);
	button->setAutoRaise(true);
	addWidget(button);
	return button;
}

void MixishTopBar::BuildPresetMenu()
{
	presetMenu = new QMenu(tr("Preset"), this);

	presetMenu->addAction(tr("New Preset (Scene Collection)"), main->ui->actionNewSceneCollection,
			      &QAction::trigger);
	presetMenu->addAction(tr("Open Preset (Import Scene Collection)..."),
			      main->ui->actionImportSceneCollection, &QAction::trigger);
	presetMenu->addAction(tr("Save Preset"), main, [this]() { main->SaveProject(); });
	presetMenu->addAction(tr("Save Preset As (Export Scene Collection)..."),
			      main->ui->actionExportSceneCollection, &QAction::trigger);
	presetMenu->addSeparator();

	auto addSourceMenu = [this](const QString &title, QMenu *source) {
		QMenu *sub = presetMenu->addMenu(title);
		if (source)
			sub->addActions(source->actions());
	};

	addSourceMenu(tr("Edit"), main->ui->menuBasic_MainMenu_Edit);
	addSourceMenu(tr("View"), main->ui->viewMenu);
	addSourceMenu(tr("Docks"), main->ui->menuDocks);
	addSourceMenu(tr("Profile"), main->ui->profileMenu);
	addSourceMenu(tr("Scene Collection"), main->ui->sceneCollectionMenu);
	addSourceMenu(tr("Tools"), main->ui->menuTools);
	addSourceMenu(tr("Help"), main->ui->menuBasic_MainMenu_Help);

	presetMenu->addSeparator();
	presetMenu->addAction(tr("About Mixish"), main->ui->actionShowAbout, &QAction::trigger);
	presetMenu->addAction(tr("Exit"), main->ui->actionE_xit, &QAction::trigger);
}
