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

#include "MixishProgramTransport.hpp"
#include "MixishMediaTransport.hpp"
#include "MixishTheme.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>

#include "moc_MixishProgramTransport.cpp"

MixishProgramTransport::MixishProgramTransport(QWidget *parent) : QFrame(parent)
{
	setObjectName(QStringLiteral("mixishProgramTransport"));
	setFixedHeight(30);

	auto *layout = new QHBoxLayout(this);
	layout->setContentsMargins(MixishTheme::Space2, 0, MixishTheme::Space2, 0);
	layout->setSpacing(MixishTheme::Space2);

	queueInfo = new QLabel(this);
	queueInfo->setObjectName(QStringLiteral("mixishProgramQueueInfo"));
	MixishTheme::SetRole(queueInfo, "muted");
	queueInfo->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	queueInfo->setMinimumWidth(120);
	queueInfo->setMaximumWidth(340);
	queueInfo->hide();
	layout->addWidget(queueInfo);

	transport = new MixishMediaTransport(this);
	transport->SetPlaceholder(tr("No Playlist on Program"));
	transport->SetQueueNavigationVisible(true);
	layout->addWidget(transport, 1);

	connect(transport, &MixishMediaTransport::previousRequested, this,
		&MixishProgramTransport::previousRequested);
	connect(transport, &MixishMediaTransport::nextRequested, this,
		&MixishProgramTransport::nextRequested);

	transport->SetSource(nullptr, false);

	MixishTheme::Apply(this);
}

void MixishProgramTransport::SetSource(obs_source_t *source)
{
	if (!transport)
		return;

	if (source) {
		transport->SetPlaceholder(tr("No Playlist on Program"));
		transport->SetSource(source, true);
	} else {
		transport->SetPlaceholder(tr("No Playlist on Program"));
		transport->SetSource(nullptr, false);
		SetQueueInfo(QString());
	}
}

void MixishProgramTransport::SetPlaceholder(const QString &text)
{
	if (transport)
		transport->SetPlaceholder(text);
}

void MixishProgramTransport::SetQueueInfo(const QString &text)
{
	if (!queueInfo || queueInfo->text() == text)
		return;

	queueInfo->setText(text);
	queueInfo->setToolTip(text);
	queueInfo->setVisible(!text.isEmpty());
}

bool MixishProgramTransport::HasSource() const
{
	return transport && transport->HasSource();
}

MixishMediaTransport *MixishProgramTransport::Transport() const
{
	return transport;
}
