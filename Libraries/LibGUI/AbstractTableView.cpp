/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <AK/StringBuilder.h>
#include <AK/Vector.h>
#include <LibGUI/AbstractTableView.h>
#include <LibGUI/Action.h>
#include <LibGUI/HeaderView.h>
#include <LibGUI/Menu.h>
#include <LibGUI/Model.h>
#include <LibGUI/Painter.h>
#include <LibGUI/ScrollBar.h>
#include <LibGUI/Window.h>
#include <LibGfx/Palette.h>

namespace GUI {

AbstractTableView::AbstractTableView()
{
    m_column_header = add<HeaderView>(*this, Gfx::Orientation::Horizontal);
    m_column_header->move_to_back();
    set_should_hide_unnecessary_scrollbars(true);
}

AbstractTableView::~AbstractTableView()
{
}

void AbstractTableView::select_all()
{
    selection().clear();
    for (int item_index = 0; item_index < item_count(); ++item_index) {
        auto index = model()->index(item_index);
        selection().add(index);
    }
}

void AbstractTableView::update_column_sizes()
{
    if (!model())
        return;

    auto& model = *this->model();
    int column_count = model.column_count();
    int row_count = model.row_count();

    for (int column = 0; column < column_count; ++column) {
        if (!column_header().is_section_visible(column))
            continue;
        int header_width = m_column_header->font().width(model.column_name(column));
        if (column == m_key_column && model.is_column_sortable(column))
            header_width += font().width(" \xE2\xAC\x86"); // UPWARDS BLACK ARROW
        int column_width = header_width;
        for (int row = 0; row < row_count; ++row) {
            auto cell_data = model.index(row, column).data();
            int cell_width = 0;
            if (cell_data.is_icon()) {
                cell_width = item_height();
            } else if (cell_data.is_bitmap()) {
                cell_width = cell_data.as_bitmap().width();
            } else if (cell_data.is_valid()) {
                cell_width = font().width(cell_data.to_string());
            }
            column_width = max(column_width, cell_width);
        }
        column_header().set_section_size(column, max(m_column_header->section_size(column), column_width));
    }
}

void AbstractTableView::update_content_size()
{
    if (!model())
        return set_content_size({});

    int content_width = 0;
    int column_count = model()->column_count();

    for (int i = 0; i < column_count; ++i) {
        if (column_header().is_section_visible(i))
            content_width += column_width(i) + horizontal_padding() * 2;
    }
    int content_height = item_count() * item_height();

    set_content_size({ content_width, content_height });
    column_header().set_width(content_width);
    set_size_occupied_by_fixed_elements({ 0, m_column_header->height() });
}

TableCellPaintingDelegate* AbstractTableView::column_painting_delegate(int column) const
{
    // FIXME: This should return a const pointer I think..
    return const_cast<TableCellPaintingDelegate*>(m_column_painting_delegate.get(column).value_or(nullptr));
}

void AbstractTableView::set_column_painting_delegate(int column, OwnPtr<TableCellPaintingDelegate> delegate)
{
    if (!delegate)
        m_column_painting_delegate.remove(column);
    else
        m_column_painting_delegate.set(column, move(delegate));
}

int AbstractTableView::column_width(int column_index) const
{
    if (!model())
        return 0;
    return m_column_header->section_size(column_index);
}

void AbstractTableView::set_column_width(int column, int width)
{
    column_header().set_section_size(column, width);
}

Gfx::TextAlignment AbstractTableView::column_header_alignment(int column_index) const
{
    if (!model())
        return Gfx::TextAlignment::CenterLeft;
    return m_column_header->section_alignment(column_index);
}

void AbstractTableView::set_column_header_alignment(int column, Gfx::TextAlignment alignment)
{
    column_header().set_section_alignment(column, alignment);
}

void AbstractTableView::mousedown_event(MouseEvent& event)
{
    if (!model())
        return AbstractView::mousedown_event(event);

    if (event.button() != MouseButton::Left)
        return AbstractView::mousedown_event(event);

    auto adjusted_position = this->adjusted_position(event.position());
    Gfx::IntPoint horizontally_adjusted_position(adjusted_position.x(), event.position().y());

    bool is_toggle;
    auto index = index_at_event_position(event.position(), is_toggle);

    if (index.is_valid() && is_toggle && model()->row_count(index)) {
        toggle_index(index);
        return;
    }

    AbstractView::mousedown_event(event);
}

ModelIndex AbstractTableView::index_at_event_position(const Gfx::IntPoint& position, bool& is_toggle) const
{
    is_toggle = false;
    if (!model())
        return {};

    auto adjusted_position = this->adjusted_position(position);
    for (int row = 0, row_count = model()->row_count(); row < row_count; ++row) {
        if (!row_rect(row).contains(adjusted_position))
            continue;
        for (int column = 0, column_count = model()->column_count(); column < column_count; ++column) {
            if (!content_rect(row, column).contains(adjusted_position))
                continue;
            return model()->index(row, column);
        }
        return model()->index(row, 0);
    }
    return {};
}

ModelIndex AbstractTableView::index_at_event_position(const Gfx::IntPoint& position) const
{
    bool is_toggle;
    auto index = index_at_event_position(position, is_toggle);
    return is_toggle ? ModelIndex() : index;
}

int AbstractTableView::item_count() const
{
    if (!model())
        return 0;
    return model()->row_count();
}

void AbstractTableView::move_selection(int vertical_steps, int horizontal_steps)
{
    if (!model())
        return;
    auto& model = *this->model();
    ModelIndex new_index;
    if (!selection().is_empty()) {
        auto old_index = selection().first();
        new_index = model.index(old_index.row() + vertical_steps, old_index.column() + horizontal_steps);
    } else {
        new_index = model.index(0, 0);
    }
    if (model.is_valid(new_index)) {
        selection().set(new_index);
        scroll_into_view(new_index, Orientation::Vertical);
        update();
    }
}

void AbstractTableView::scroll_into_view(const ModelIndex& index, Orientation orientation)
{
    auto rect = row_rect(index.row()).translated(0, -m_column_header->height());
    ScrollableWidget::scroll_into_view(rect, orientation);
}

void AbstractTableView::scroll_into_view(const ModelIndex& index, bool scroll_horizontally, bool scroll_vertically)
{
    auto rect = row_rect(index.row()).translated(0, -m_column_header->height());
    ScrollableWidget::scroll_into_view(rect, scroll_horizontally, scroll_vertically);
}

void AbstractTableView::doubleclick_event(MouseEvent& event)
{
    if (!model())
        return;
    if (event.button() == MouseButton::Left) {
        if (!selection().is_empty())
            activate_or_edit_selected();
    }
}

void AbstractTableView::context_menu_event(ContextMenuEvent& event)
{
    if (!model())
        return;

    bool is_toggle;
    auto index = index_at_event_position(event.position(), is_toggle);
    if (index.is_valid()) {
        if (!selection().contains(index))
            selection().set(index);
    } else {
        selection().clear();
    }
    if (on_context_menu_request)
        on_context_menu_request(index, event);
}

Gfx::IntRect AbstractTableView::content_rect(int row, int column) const
{
    auto row_rect = this->row_rect(row);
    int x = 0;
    for (int i = 0; i < column; ++i)
        x += column_width(i) + horizontal_padding() * 2;

    return { row_rect.x() + x, row_rect.y(), column_width(column) + horizontal_padding() * 2, item_height() };
}

Gfx::IntRect AbstractTableView::content_rect(const ModelIndex& index) const
{
    return content_rect(index.row(), index.column());
}

Gfx::IntRect AbstractTableView::row_rect(int item_index) const
{
    return { 0, m_column_header->height() + (item_index * item_height()), max(content_size().width(), width()), item_height() };
}

Gfx::IntPoint AbstractTableView::adjusted_position(const Gfx::IntPoint& position) const
{
    return position.translated(horizontal_scrollbar().value() - frame_thickness(), vertical_scrollbar().value() - frame_thickness());
}

void AbstractTableView::did_update_model(unsigned flags)
{
    AbstractView::did_update_model(flags);
    update_column_sizes();
    update_content_size();
    update();
}

void AbstractTableView::resize_event(ResizeEvent& event)
{
    AbstractView::resize_event(event);

    if (column_header().is_visible())
        column_header().set_relative_rect(frame_thickness(), frame_thickness(), content_width(), column_header().preferred_size().height());
}

void AbstractTableView::header_did_change_section_size(Badge<HeaderView>, Gfx::Orientation, int, int)
{
    update_content_size();
    update();
}

void AbstractTableView::header_did_change_section_visibility(Badge<HeaderView>, Gfx::Orientation, int, bool)
{
    update_content_size();
    update();
}

void AbstractTableView::set_column_hidden(int column, bool hidden)
{
    column_header().set_section_visible(column, !hidden);
}

void AbstractTableView::set_column_headers_visible(bool visible)
{
    column_header().set_visible(visible);
}

void AbstractTableView::did_scroll()
{
    AbstractView::did_scroll();
    column_header().set_x(frame_thickness() + -horizontal_scrollbar().value());
}

}
