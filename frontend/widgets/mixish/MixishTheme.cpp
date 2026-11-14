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

#include "MixishTheme.hpp"

#include <QFile>
#include <QFont>
#include <QLabel>
#include <QStyle>
#include <QWidget>

namespace MixishTheme {

QString StyleSheet()
{
	static QString sheet;
	if (!sheet.isEmpty())
		return sheet;

	QFile file(QStringLiteral(":/res/mixish/mixish.qss"));
	if (file.open(QIODevice::ReadOnly | QIODevice::Text))
		sheet = QString::fromUtf8(file.readAll());

	return sheet;
}

void Apply(QWidget *root)
{
	if (!root)
		return;

	const QString sheet = StyleSheet();
	if (sheet.isEmpty() || root->styleSheet() == sheet)
		return;

	root->setStyleSheet(sheet);
}

void Repolish(QWidget *widget)
{
	if (!widget)
		return;

	widget->style()->unpolish(widget);
	widget->style()->polish(widget);
	widget->update();
}

void SetVariant(QWidget *widget, const char *variant)
{
	if (!widget)
		return;
	if (widget->property("mixishVariant").toString() == QLatin1String(variant))
		return;

	widget->setProperty("mixishVariant", variant);
	Repolish(widget);
}

void SetRole(QWidget *widget, const char *role)
{
	if (!widget)
		return;
	if (widget->property("mixishRole").toString() == QLatin1String(role))
		return;

	widget->setProperty("mixishRole", role);
	Repolish(widget);
}

void SetState(QWidget *widget, const char *state)
{
	if (!widget)
		return;
	if (widget->property("mixishState").toString() == QLatin1String(state))
		return;

	widget->setProperty("mixishState", state);
	Repolish(widget);
}

void SetControl(QWidget *widget)
{
	if (!widget)
		return;
	if (widget->property("mixishControl").toBool())
		return;

	widget->setProperty("mixishControl", true);
	Repolish(widget);
}

void MakeSectionLabel(QLabel *label)
{
	if (!label)
		return;

	SetRole(label, "section");

	QFont font = label->font();
	font.setPixelSize(10);
	font.setBold(true);
	font.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
	label->setFont(font);
}

void MakeTitleLabel(QLabel *label)
{
	if (!label)
		return;

	SetRole(label, "title");

	QFont font = label->font();
	font.setPixelSize(14);
	font.setBold(true);
	font.setLetterSpacing(QFont::AbsoluteSpacing, 0.0);
	label->setFont(font);
}

} // namespace MixishTheme
