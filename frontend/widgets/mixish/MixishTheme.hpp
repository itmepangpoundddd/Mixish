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

#pragma once

#include <QString>

class QLabel;
class QWidget;

/* Mixish design tokens and the single stylesheet loader.
 *
 * All Mixish visual styling lives in frontend/forms/mixish/mixish.qss.
 * Widgets select a variant/role/state with a dynamic property; this helper
 * applies the sheet and repolishes the widget when the property changes.
 * OBS's own widgets keep their theme because every selector in the sheet is
 * property based or scoped to a Mixish object name. */
namespace MixishTheme {

/* Layout tokens (px). Heights are applied in code because Qt style sheets
 * cannot express fixed widget heights reliably. */
constexpr int TopBarHeight = 36;
constexpr int InputsBarHeight = 40;
constexpr int BottomBarHeight = 40;
constexpr int MonitorHeaderHeight = 32;
constexpr int ControlHeight = 32;
constexpr int CompactHeight = 24;
constexpr int IconButtonSize = 28;
constexpr int OverlayToggleWidth = 40;
constexpr int OverlayToggleHeight = 32;
constexpr int Space1 = 4;
constexpr int Space2 = 8;
constexpr int Space3 = 12;
constexpr int Space4 = 16;

/* The complete Mixish style sheet. */
QString StyleSheet();

/* Applies the style sheet to a Mixish root widget (and its children). */
void Apply(QWidget *root);

/* Dynamic-property helpers. Each one repolishes the widget. */
void SetVariant(QWidget *widget, const char *variant); /* primary/secondary/icon/danger */
void SetRole(QWidget *widget, const char *role);       /* section/title/body/muted/... */
void SetState(QWidget *widget, const char *state);     /* selected/onAir/assigned/empty/... */
void SetControl(QWidget *widget);                      /* combo/spin/line edit */
void Repolish(QWidget *widget);

/* Section labels are uppercase, bold, 10 px with 1 px tracking. Qt style
 * sheets do not support letter spacing, so the font is built here. */
void MakeSectionLabel(QLabel *label);
void MakeTitleLabel(QLabel *label);

} // namespace MixishTheme
