/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2013 Joel Holdsworth <joel@airwebreathe.org.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <algorithm>
#include <cassert>

#include <QDebug>
#include <QToolTip>

#include "cursorpair.hpp"

#include "pv/util.hpp"
#include "ruler.hpp"
#include "view.hpp"

using std::max;
using std::make_pair;
using std::min;
using std::shared_ptr;
using std::pair;

namespace pv {
namespace views {
namespace trace {

const int CursorPair::DeltaPadding = 8;
const QColor CursorPair::ViewportFillColor(220, 231, 243);

CursorPair::CursorPair(View &view) :
	TimeItem(view),
	first_(new Cursor(view, 0.0)),
	second_(new Cursor(view, 1.0))
{
	connect(&view_, SIGNAL(hover_point_changed(const QWidget*, QPoint)),
		this, SLOT(on_hover_point_changed(const QWidget*, QPoint)));
}

bool CursorPair::enabled() const
{
	return view_.cursors_shown();
}

shared_ptr<Cursor> CursorPair::first() const
{
	return first_;
}

shared_ptr<Cursor> CursorPair::second() const
{
	return second_;
}

void CursorPair::set_time(const pv::util::Timestamp& time)
{
	const pv::util::Timestamp delta = second_->time() - first_->time();
	first_->set_time(time);
	second_->set_time(time + delta);
}

float CursorPair::get_x() const
{
	return (first_->get_x() + second_->get_x()) / 2.0f;
}

QPoint CursorPair::drag_point(const QRect &rect) const
{
	return first_->drag_point(rect);
}

pv::widgets::Popup* CursorPair::create_popup(QWidget *parent)
{
	(void)parent;
	return nullptr;
}

QRectF CursorPair::label_rect(const QRectF &rect) const
{
	const QSizeF label_size(text_size_ + LabelPadding * 2);
	const pair<float, float> offsets(get_cursor_offsets());
	const pair<float, float> normal_offsets(
		(offsets.first < offsets.second) ? offsets :
		make_pair(offsets.second, offsets.first));

	const float height = label_size.height();
	const float left = max(normal_offsets.first + DeltaPadding, -height);
	const float right = min(normal_offsets.second - DeltaPadding,
		(float)rect.width() + height);

	return QRectF(left, rect.height() - label_size.height() -
		TimeMarker::ArrowSize - 0.5f,
		right - left, height);
}

void CursorPair::paint_label(QPainter &p, const QRect &rect, bool hover)
{
	assert(first_);
	assert(second_);

	if (!enabled())
		return;

	const QColor text_color = ViewItem::select_text_color(Cursor::FillColor);
	p.setPen(text_color);

	QString text = format_string();
	text_size_ = p.boundingRect(QRectF(), 0, text).size();

	QRectF delta_rect(label_rect(rect));
	const int radius = delta_rect.height() / 2;
	QRectF text_rect(delta_rect.intersected(rect).adjusted(radius, 0, -radius, 0));

	if (text_rect.width() < text_size_.width()) {
		text = "...";
		text_size_ = p.boundingRect(QRectF(), 0, text).size();
		label_incomplete_ = true;
	} else
		label_incomplete_ = false;

	if (selected()) {
		p.setBrush(Qt::transparent);
		p.setPen(highlight_pen());
		p.drawRoundedRect(delta_rect, radius, radius);
	}

	p.setBrush(hover ? Cursor::FillColor.lighter() : Cursor::FillColor);
	p.setPen(Cursor::FillColor.darker());
	p.drawRoundedRect(delta_rect, radius, radius);

	delta_rect.adjust(1, 1, -1, -1);
	p.setPen(Cursor::FillColor.lighter());
	const int highlight_radius = delta_rect.height() / 2 - 2;
	p.drawRoundedRect(delta_rect, highlight_radius, highlight_radius);
	label_area_ = delta_rect;

	p.setPen(text_color);
	p.drawText(text_rect, Qt::AlignCenter | Qt::AlignVCenter, text);
}

void CursorPair::paint_back(QPainter &p, ViewItemPaintParams &pp)
{
	if (!enabled())
		return;

	p.setPen(Qt::NoPen);
	p.setBrush(QBrush(ViewportFillColor));

	const pair<float, float> offsets(get_cursor_offsets());
	const int l = (int)max(min(offsets.first, offsets.second), 0.0f);
	const int r = (int)min(max(offsets.first, offsets.second), (float)pp.width());

	p.drawRect(l, pp.top(), r - l, pp.height());
}

QString CursorPair::format_string()
{
	const pv::util::SIPrefix prefix = view_.tick_prefix();
	const pv::util::Timestamp diff = abs(second_->time() - first_->time());

	const QString s1 = Ruler::format_time_with_distance(
		diff, diff, prefix, view_.time_unit(), 12, false);  /* Always use 12 precision digits */
	const QString s2 = util::format_time_si(
		1 / diff, pv::util::SIPrefix::unspecified, 4, "Hz", false);

	return QString("%1 / %2").arg(s1, s2);
}

pair<float, float> CursorPair::get_cursor_offsets() const
{
	assert(first_);
	assert(second_);

	return pair<float, float>(first_->get_x(), second_->get_x());
}

void CursorPair::on_hover_point_changed(const QWidget* widget, const QPoint& hp)
{
	if (widget != view_.ruler())
		return;

	if (!label_incomplete_)
		return;

	if (label_area_.contains(hp))
		QToolTip::showText(view_.mapToGlobal(hp), format_string());
	else
		QToolTip::hideText();  // TODO Will break other tooltips when there can be others
}

} // namespace trace
} // namespace views
} // namespace pv
