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

/* Mixish Timer: a clock, countdown and stopwatch input source. The text is
 * rendered with GDI+ into an OBS texture so it can be placed, scaled and
 * recorded like any other source. */

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <combaseapi.h>
#include <gdiplus.h>

#include <obs-module.h>
#include <graphics/graphics.h>
#include <util/platform.h>
#include <util/threading.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace Gdiplus;

static ULONG_PTR gdiplus_token = 0;

enum TimerMode {
	ModeClock = 0,
	ModeCountdown = 1,
	ModeStopwatch = 2,
};

struct RenderStyle {
	std::string face;
	int fontSize = 72;
	int fontFlags = 0;
	uint32_t textColor = 0xFFFFFF;
	int textOpacity = 100;
	uint32_t outlineColor = 0x000000;
	int outlineOpacity = 80;
	int outlineSize = 2;
	uint32_t backgroundColor = 0x000000;
	int backgroundOpacity = 0;
	uint32_t width = 800;
	uint32_t height = 160;
};

struct TimerSource {
	obs_source_t *source = nullptr;
	pthread_mutex_t mutex;

	TimerMode mode = ModeClock;
	bool clock24 = true;
	bool showSeconds = true;
	bool showDate = false;
	bool autoStart = false;
	int durationHours = 0;
	int durationMinutes = 5;
	int durationSeconds = 0;

	std::string face = "Segoe UI";
	int fontSize = 72;
	int fontFlags = 0;
	uint32_t textColor = 0xFFFFFF;
	int textOpacity = 100;
	uint32_t outlineColor = 0x000000;
	int outlineOpacity = 80;
	int outlineSize = 2;
	uint32_t backgroundColor = 0x000000;
	int backgroundOpacity = 0;
	std::atomic<uint32_t> width{800};
	std::atomic<uint32_t> height{160};

	bool running = false;
	int64_t baseNs = 0;
	int64_t startNs = 0;
	int64_t durationNs = 5LL * 60 * 1000000000LL;

	std::atomic<bool> dirty{true};
	gs_texture_t *texture = nullptr;
	uint32_t textureWidth = 0;
	uint32_t textureHeight = 0;
	std::string lastText;
};

static inline BYTE color_red(uint32_t color)
{
	return BYTE((color >> 16) & 0xFF);
}

static inline BYTE color_green(uint32_t color)
{
	return BYTE((color >> 8) & 0xFF);
}

static inline BYTE color_blue(uint32_t color)
{
	return BYTE(color & 0xFF);
}

static inline BYTE color_alpha(int opacity)
{
	opacity = std::clamp(opacity, 0, 100);
	return BYTE((opacity * 255 + 50) / 100);
}

static std::wstring to_wide(const std::string &text)
{
	if (text.empty())
		return std::wstring();

	const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), int(text.size()), nullptr, 0);
	if (length <= 0)
		return std::wstring();

	std::wstring result(size_t(length), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, text.c_str(), int(text.size()), result.data(), length);
	return result;
}

static int64_t timer_total_ns(const TimerSource *s, int64_t now)
{
	int64_t total = s->baseNs;

	if (s->running) {
		const int64_t elapsed = now - s->startNs;
		if (s->mode == ModeCountdown)
			total -= elapsed;
		else
			total += elapsed;
	}

	if (s->mode == ModeCountdown && total < 0)
		total = 0;

	return total;
}

static std::string format_hms(int64_t ns, bool roundUp)
{
	if (ns < 0)
		ns = 0;

	const int64_t seconds = roundUp ? (ns + 999999999LL) / 1000000000LL
					: ns / 1000000000LL;
	const int64_t hours = seconds / 3600;
	const int64_t minutes = (seconds / 60) % 60;
	const int64_t secs = seconds % 60;

	char buffer[64];
	snprintf(buffer, sizeof(buffer), "%02lld:%02lld:%02lld", (long long)hours,
		 (long long)minutes, (long long)secs);
	return std::string(buffer);
}

static std::string format_clock(const TimerSource *s)
{
	const time_t now = time(nullptr);
	struct tm local = {};
	localtime_s(&local, &now);

	char timeBuffer[64];
	if (s->clock24) {
		if (s->showSeconds)
			snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d:%02d", local.tm_hour,
				 local.tm_min, local.tm_sec);
		else
			snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", local.tm_hour,
				 local.tm_min);
	} else {
		int hour = local.tm_hour % 12;
		if (hour == 0)
			hour = 12;
		const char *suffix = local.tm_hour < 12 ? "AM" : "PM";
		if (s->showSeconds)
			snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d:%02d %s", hour,
				 local.tm_min, local.tm_sec, suffix);
		else
			snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d %s", hour,
				 local.tm_min, suffix);
	}

	std::string text;
	if (s->showDate) {
		char dateBuffer[32];
		snprintf(dateBuffer, sizeof(dateBuffer), "%04d-%02d-%02d  ",
			 local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
		text = dateBuffer;
	}
	text += timeBuffer;
	return text;
}

static RenderStyle snapshot_style(const TimerSource *s)
{
	RenderStyle style;
	style.face = s->face;
	style.fontSize = s->fontSize;
	style.fontFlags = s->fontFlags;
	style.textColor = s->textColor;
	style.textOpacity = s->textOpacity;
	style.outlineColor = s->outlineColor;
	style.outlineOpacity = s->outlineOpacity;
	style.outlineSize = s->outlineSize;
	style.backgroundColor = s->backgroundColor;
	style.backgroundOpacity = s->backgroundOpacity;
	style.width = s->width.load();
	style.height = s->height.load();
	return style;
}

static bool render_texture(TimerSource *s, const std::string &text, const RenderStyle &style)
{
	const uint32_t width = std::max<uint32_t>(64, style.width);
	const uint32_t height = std::max<uint32_t>(32, style.height);
	std::vector<uint8_t> bits(size_t(width) * height * 4, 0);

	{
		Bitmap bitmap(width, height, width * 4, PixelFormat32bppARGB, bits.data());
		Graphics graphics(&bitmap);
		graphics.SetSmoothingMode(SmoothingModeAntiAlias);
		graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);
		graphics.Clear(Color(color_alpha(style.backgroundOpacity),
				     color_red(style.backgroundColor),
				     color_green(style.backgroundColor),
				     color_blue(style.backgroundColor)));

		if (!text.empty()) {
			const std::wstring wideText = to_wide(text);
			const std::wstring wideFace = to_wide(style.face);

			auto family = std::make_unique<FontFamily>(wideFace.c_str());
			if (family->GetLastStatus() != Ok)
				family = std::make_unique<FontFamily>(L"Segoe UI");
			if (family->GetLastStatus() != Ok)
				family = std::make_unique<FontFamily>(L"Arial");

			INT fontStyle = FontStyleRegular;
			if (style.fontFlags & OBS_FONT_BOLD)
				fontStyle |= FontStyleBold;
			if (style.fontFlags & OBS_FONT_ITALIC)
				fontStyle |= FontStyleItalic;
			if (style.fontFlags & OBS_FONT_UNDERLINE)
				fontStyle |= FontStyleUnderline;
			if (style.fontFlags & OBS_FONT_STRIKEOUT)
				fontStyle |= FontStyleStrikeout;

			const REAL fontSize = REAL(std::clamp(style.fontSize, 8, 512));
			Font font(family.get(), fontSize, fontStyle, UnitPixel);

			StringFormat format;
			format.SetAlignment(StringAlignmentCenter);
			format.SetLineAlignment(StringAlignmentCenter);
			format.SetFormatFlags(StringFormatFlagsNoWrap | StringFormatFlagsNoClip);

			const RectF box(0.0f, 0.0f, REAL(width), REAL(height));
			SolidBrush brush(Color(color_alpha(style.textOpacity),
					      color_red(style.textColor),
					      color_green(style.textColor),
					      color_blue(style.textColor)));

			if (style.outlineSize > 0 && style.outlineOpacity > 0) {
				GraphicsPath path;
				path.AddString(wideText.c_str(), INT(wideText.size()), family.get(),
					       fontStyle, fontSize, box, &format);

				Pen pen(Color(color_alpha(style.outlineOpacity),
					      color_red(style.outlineColor),
					      color_green(style.outlineColor),
					      color_blue(style.outlineColor)),
					REAL(std::max(1, style.outlineSize)));
				pen.SetLineJoin(LineJoinRound);
				graphics.DrawPath(&pen, &path);
				graphics.FillPath(&brush, &path);
			} else {
				graphics.DrawString(wideText.c_str(), -1, &font, box, &format,
						    &brush);
			}
		}
	}

	obs_enter_graphics();

	const uint8_t *data = bits.data();
	if (!s->texture || s->textureWidth != width || s->textureHeight != height) {
		if (s->texture)
			gs_texture_destroy(s->texture);
		s->texture = gs_texture_create(width, height, GS_BGRA, 1, &data, GS_DYNAMIC);
		s->textureWidth = width;
		s->textureHeight = height;
	} else {
		gs_texture_set_image(s->texture, bits.data(), width * 4, false);
	}

	obs_leave_graphics();

	return s->texture != nullptr;
}

static void timer_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);
	TimerSource *s = static_cast<TimerSource *>(data);
	const int64_t now = os_gettime_ns();

	pthread_mutex_lock(&s->mutex);

	if (s->mode == ModeCountdown && s->running) {
		const int64_t total = timer_total_ns(s, now);
		if (total <= 0) {
			s->baseNs = 0;
			s->running = false;
			s->dirty = true;
		}
	}

	std::string text;
	if (s->mode == ModeClock) {
		text = format_clock(s);
	} else if (s->mode == ModeCountdown) {
		text = format_hms(timer_total_ns(s, now), true);
	} else {
		text = format_hms(timer_total_ns(s, now), false);
	}

	const RenderStyle style = snapshot_style(s);
	const bool dirty = s->dirty.exchange(false);
	const bool changed = text != s->lastText;

	pthread_mutex_unlock(&s->mutex);

	if (dirty || changed || !s->texture) {
		if (render_texture(s, text, style))
			s->lastText = text;
		else
			s->dirty = true;
	}
}

static void timer_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	TimerSource *s = static_cast<TimerSource *>(data);
	if (s->texture)
		obs_source_draw(s->texture, 0, 0, 0, 0, false);
}

static const char *timer_get_name(void *)
{
	return "Timer";
}

static uint32_t timer_get_width(void *data)
{
	return static_cast<TimerSource *>(data)->width.load();
}

static uint32_t timer_get_height(void *data)
{
	return static_cast<TimerSource *>(data)->height.load();
}

static void timer_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "mode", ModeClock);
	obs_data_set_default_bool(settings, "clock24", true);
	obs_data_set_default_bool(settings, "show_seconds", true);
	obs_data_set_default_bool(settings, "show_date", false);
	obs_data_set_default_bool(settings, "auto_start", false);
	obs_data_set_default_int(settings, "duration_hours", 0);
	obs_data_set_default_int(settings, "duration_minutes", 5);
	obs_data_set_default_int(settings, "duration_seconds", 0);
	obs_data_set_default_int(settings, "text_color", 0xFFFFFF);
	obs_data_set_default_int(settings, "text_opacity", 100);
	obs_data_set_default_int(settings, "outline_color", 0x000000);
	obs_data_set_default_int(settings, "outline_opacity", 80);
	obs_data_set_default_int(settings, "outline_size", 2);
	obs_data_set_default_int(settings, "background_color", 0x000000);
	obs_data_set_default_int(settings, "background_opacity", 0);
	obs_data_set_default_int(settings, "width", 800);
	obs_data_set_default_int(settings, "height", 160);

	obs_data_t *font = obs_data_create();
	obs_data_set_string(font, "face", "Segoe UI");
	obs_data_set_int(font, "size", 72);
	obs_data_set_int(font, "flags", 0);
	obs_data_set_default_obj(settings, "font", font);
	obs_data_release(font);
}

static void timer_update(void *data, obs_data_t *settings)
{
	TimerSource *s = static_cast<TimerSource *>(data);

	std::string face = "Segoe UI";
	int fontSize = 72;
	int fontFlags = 0;
	obs_data_t *font = obs_data_get_obj(settings, "font");
	if (font) {
		const char *value = obs_data_get_string(font, "face");
		if (value && *value)
			face = value;
		fontSize = int(obs_data_get_int(font, "size"));
		fontFlags = int(obs_data_get_int(font, "flags"));
		obs_data_release(font);
	}
	if (face.empty())
		face = "Segoe UI";
	fontSize = std::clamp(fontSize, 8, 512);

	const TimerMode mode = TimerMode(std::clamp(int(obs_data_get_int(settings, "mode")),
						    int(ModeClock), int(ModeStopwatch)));
	const int hours = std::clamp(int(obs_data_get_int(settings, "duration_hours")), 0, 99);
	const int minutes =
		std::clamp(int(obs_data_get_int(settings, "duration_minutes")), 0, 59);
	const int seconds =
		std::clamp(int(obs_data_get_int(settings, "duration_seconds")), 0, 59);
	const int64_t durationNs =
		(int64_t(hours) * 3600 + int64_t(minutes) * 60 + seconds) * 1000000000LL;

	const uint32_t width =
		uint32_t(std::clamp(int(obs_data_get_int(settings, "width")), 64, 3840));
	const uint32_t height =
		uint32_t(std::clamp(int(obs_data_get_int(settings, "height")), 32, 2160));

	pthread_mutex_lock(&s->mutex);

	const bool reset = mode != s->mode || durationNs != s->durationNs;

	s->mode = mode;
	s->clock24 = obs_data_get_bool(settings, "clock24");
	s->showSeconds = obs_data_get_bool(settings, "show_seconds");
	s->showDate = obs_data_get_bool(settings, "show_date");
	s->autoStart = obs_data_get_bool(settings, "auto_start");
	s->durationHours = hours;
	s->durationMinutes = minutes;
	s->durationSeconds = seconds;
	s->durationNs = durationNs;
	s->face = face;
	s->fontSize = fontSize;
	s->fontFlags = fontFlags;
	s->textColor = uint32_t(obs_data_get_int(settings, "text_color"));
	s->textOpacity = int(obs_data_get_int(settings, "text_opacity"));
	s->outlineColor = uint32_t(obs_data_get_int(settings, "outline_color"));
	s->outlineOpacity = int(obs_data_get_int(settings, "outline_opacity"));
	s->outlineSize = int(obs_data_get_int(settings, "outline_size"));
	s->backgroundColor = uint32_t(obs_data_get_int(settings, "background_color"));
	s->backgroundOpacity = int(obs_data_get_int(settings, "background_opacity"));
	s->width = width;
	s->height = height;

	if (reset) {
		s->running = false;
		s->startNs = 0;
		s->baseNs = mode == ModeCountdown ? durationNs : 0;
	}

	s->dirty = true;

	pthread_mutex_unlock(&s->mutex);
}

static void timer_start(TimerSource *s)
{
	pthread_mutex_lock(&s->mutex);

	if (!s->running && s->mode != ModeClock) {
		const int64_t now = os_gettime_ns();
		if (s->mode == ModeCountdown && timer_total_ns(s, now) <= 0)
			s->baseNs = s->durationNs;

		s->startNs = now;
		s->running = true;
		s->dirty = true;
	}

	pthread_mutex_unlock(&s->mutex);
}

static void timer_pause(TimerSource *s)
{
	pthread_mutex_lock(&s->mutex);

	if (s->running) {
		s->baseNs = timer_total_ns(s, os_gettime_ns());
		s->running = false;
		s->dirty = true;
	}

	pthread_mutex_unlock(&s->mutex);
}

static void timer_reset(TimerSource *s)
{
	pthread_mutex_lock(&s->mutex);

	s->running = false;
	s->startNs = 0;
	s->baseNs = s->mode == ModeCountdown ? s->durationNs : 0;
	s->dirty = true;

	pthread_mutex_unlock(&s->mutex);
}

static bool start_clicked(obs_properties_t *props, obs_property_t *property, void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	timer_start(static_cast<TimerSource *>(data));
	return false;
}

static bool pause_clicked(obs_properties_t *props, obs_property_t *property, void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	timer_pause(static_cast<TimerSource *>(data));
	return false;
}

static bool reset_clicked(obs_properties_t *props, obs_property_t *property, void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	timer_reset(static_cast<TimerSource *>(data));
	return false;
}

static obs_properties_t *timer_properties(void *data)
{
	UNUSED_PARAMETER(data);

	obs_properties_t *props = obs_properties_create();

	obs_properties_t *timer = obs_properties_create();
	obs_property_t *p = obs_properties_add_list(timer, "mode", "Mode",
						    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, "Clock (current time)", ModeClock);
	obs_property_list_add_int(p, "Countdown", ModeCountdown);
	obs_property_list_add_int(p, "Stopwatch", ModeStopwatch);

	obs_properties_add_bool(timer, "clock24", "24-hour clock");
	obs_properties_add_bool(timer, "show_seconds", "Show seconds");
	obs_properties_add_bool(timer, "show_date", "Show date");
	obs_properties_add_bool(timer, "auto_start", "Start automatically when loaded");
	obs_properties_add_int(timer, "duration_hours", "Countdown hours", 0, 99, 1);
	obs_properties_add_int(timer, "duration_minutes", "Countdown minutes", 0, 59, 1);
	obs_properties_add_int(timer, "duration_seconds", "Countdown seconds", 0, 59, 1);
	obs_properties_add_button2(timer, "start", "Start", start_clicked, data);
	obs_properties_add_button2(timer, "pause", "Pause", pause_clicked, data);
	obs_properties_add_button2(timer, "reset", "Reset", reset_clicked, data);
	obs_properties_add_group(props, "timer", "Timer", OBS_GROUP_NORMAL, timer);

	obs_properties_t *style = obs_properties_create();
	obs_properties_add_font(style, "font", "Font");
	obs_properties_add_color(style, "text_color", "Text color");
	p = obs_properties_add_int_slider(style, "text_opacity", "Text opacity", 0, 100, 1);
	obs_property_int_set_suffix(p, "%");
	obs_properties_add_color(style, "outline_color", "Outline color");
	p = obs_properties_add_int_slider(style, "outline_opacity", "Outline opacity", 0,
					  100, 1);
	obs_property_int_set_suffix(p, "%");
	obs_properties_add_int(style, "outline_size", "Outline size (0 = off)", 0, 20, 1);
	obs_properties_add_color(style, "background_color", "Background color");
	p = obs_properties_add_int_slider(style, "background_opacity", "Background opacity",
					  0, 100, 1);
	obs_property_int_set_suffix(p, "%");
	obs_properties_add_int(style, "width", "Canvas width", 64, 3840, 1);
	obs_properties_add_int(style, "height", "Canvas height", 32, 2160, 1);
	obs_properties_add_group(props, "style", "Style", OBS_GROUP_NORMAL, style);

	return props;
}

static void *timer_create(obs_data_t *settings, obs_source_t *source)
{
	TimerSource *s = new TimerSource();
	s->source = source;
	pthread_mutex_init(&s->mutex, nullptr);
	timer_update(s, settings);
	if (s->autoStart && s->mode != ModeClock)
		timer_start(s);
	return s;
}

static void timer_destroy(void *data)
{
	TimerSource *s = static_cast<TimerSource *>(data);

	obs_enter_graphics();
	if (s->texture)
		gs_texture_destroy(s->texture);
	obs_leave_graphics();

	pthread_mutex_destroy(&s->mutex);
	delete s;
}

static const char *timer_icon(void *)
{
	return obs_module_file("timer.svg");
}

static struct obs_source_info timer_info = {};

OBS_DECLARE_MODULE()

bool obs_module_load(void)
{
	GdiplusStartupInput input;
	if (GdiplusStartup(&gdiplus_token, &input, nullptr) != Ok) {
		blog(LOG_ERROR, "[Mixish Timer] Failed to initialize GDI+");
		return false;
	}

	timer_info.id = "mixish_timer";
	timer_info.type = OBS_SOURCE_TYPE_INPUT;
	timer_info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB;
	timer_info.get_name = timer_get_name;
	timer_info.create = timer_create;
	timer_info.destroy = timer_destroy;
	timer_info.get_width = timer_get_width;
	timer_info.get_height = timer_get_height;
	timer_info.get_defaults = timer_defaults;
	timer_info.get_properties = timer_properties;
	timer_info.update = timer_update;
	timer_info.video_tick = timer_tick;
	timer_info.video_render = timer_render;
	timer_info.icon_type = OBS_ICON_TYPE_CUSTOM;
	timer_info.get_dark_icon = timer_icon;
	timer_info.get_light_icon = timer_icon;

	obs_register_source(&timer_info);
	blog(LOG_INFO, "[Mixish Timer] Loaded");
	return true;
}

void obs_module_unload(void)
{
	GdiplusShutdown(gdiplus_token);
	gdiplus_token = 0;
	blog(LOG_INFO, "[Mixish Timer] Unloaded");
}
