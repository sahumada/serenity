/*
 * Copyright (c) 2020, the SerenityOS developers.
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

#pragma once

#include <AK/HashMap.h>
#include <AK/HashTable.h>
#include <AK/String.h>
#include <AK/StringBuilder.h>
#include <AK/Traits.h>
#include <AK/Types.h>
#include <AK/WeakPtr.h>
#include <AK/Weakable.h>
#include <LibCore/Object.h>
#include <LibJS/Interpreter.h>

namespace Spreadsheet {

struct Position {
    String column;
    size_t row { 0 };

    bool operator==(const Position& other) const
    {
        return row == other.row && column == other.column;
    }

    bool operator!=(const Position& other) const
    {
        return !(other == *this);
    }
};

class Sheet;

struct Cell : public Weakable<Cell> {
    Cell(String data, WeakPtr<Sheet> sheet)
        : dirty(false)
        , data(move(data))
        , kind(LiteralString)
        , sheet(sheet)
    {
    }

    Cell(String source, JS::Value&& cell_value, WeakPtr<Sheet> sheet)
        : dirty(false)
        , data(move(source))
        , evaluated_data(move(cell_value))
        , kind(Formula)
        , sheet(sheet)
    {
    }

    bool dirty { false };
    bool evaluated_externally { false };
    String data;
    JS::Value evaluated_data;

    enum Kind {
        LiteralString,
        Formula,
    } kind { LiteralString };

    WeakPtr<Sheet> sheet;
    Vector<WeakPtr<Cell>> referencing_cells;

    void reference_from(Cell*);

    void set_data(String new_data)
    {
        if (data == new_data)
            return;

        if (new_data.starts_with("=")) {
            new_data = new_data.substring(1, new_data.length() - 1);
            kind = Formula;
        } else {
            kind = LiteralString;
        }

        data = move(new_data);
        dirty = true;
        evaluated_externally = false;
    }

    void set_data(JS::Value new_data)
    {
        dirty = true;
        evaluated_externally = true;

        StringBuilder builder;

        builder.append("=");
        builder.append(new_data.to_string_without_side_effects());
        data = builder.build();

        evaluated_data = move(new_data);
    }

    String source() const;

    JS::Value js_data();

    void update(Badge<Sheet>) { update_data(); }
    void update();

private:
    void update_data();
};

class Sheet : public Core::Object {
    C_OBJECT(Sheet);

public:
    ~Sheet();

    static Optional<Position> parse_cell_name(const StringView&);

    JsonObject to_json() const;
    static RefPtr<Sheet> from_json(const JsonObject&);

    const String& name() const { return m_name; }
    void set_name(const StringView& name) { m_name = name; }

    JsonObject gather_documentation() const;

    Optional<Position> selected_cell() const { return m_selected_cell; }
    const HashMap<Position, NonnullOwnPtr<Cell>>& cells() const { return m_cells; }
    HashMap<Position, NonnullOwnPtr<Cell>>& cells() { return m_cells; }

    Cell* at(const Position& position);
    const Cell* at(const Position& position) const { return const_cast<Sheet*>(this)->at(position); }

    const Cell* at(const StringView& name) const { return const_cast<Sheet*>(this)->at(name); }
    Cell* at(const StringView&);

    const Cell& ensure(const Position& position) const { return const_cast<Sheet*>(this)->ensure(position); }
    Cell& ensure(const Position& position)
    {
        if (auto cell = at(position))
            return *cell;

        m_cells.set(position, make<Cell>(String::empty(), make_weak_ptr()));
        return *at(position);
    }

    size_t add_row();
    String add_column();

    size_t row_count() const { return m_rows; }
    size_t column_count() const { return m_columns.size(); }
    const Vector<String>& columns() const { return m_columns; }
    const String& column(size_t index) const
    {
        ASSERT(column_count() > index);
        return m_columns[index];
    }

    void update();
    void update(Cell&);

    JS::Value evaluate(const StringView&, Cell* = nullptr);
    JS::Interpreter& interpreter() { return *m_interpreter; }

    Cell*& current_evaluated_cell() { return m_current_cell_being_evaluated; }
    bool has_been_visited(Cell* cell) const { return m_visited_cells_in_update.contains(cell); }

private:
    enum class EmptyConstruct { EmptyConstructTag };

    explicit Sheet(EmptyConstruct);
    explicit Sheet(const StringView& name);

    String m_name;
    Vector<String> m_columns;
    size_t m_rows { 0 };
    HashMap<Position, NonnullOwnPtr<Cell>> m_cells;
    Optional<Position> m_selected_cell; // FIXME: Make this a collection.

    Cell* m_current_cell_being_evaluated { nullptr };

    size_t m_current_column_name_length { 0 };

    mutable NonnullOwnPtr<JS::Interpreter> m_interpreter;
    HashTable<Cell*> m_visited_cells_in_update;
};

}

namespace AK {

template<>
struct Traits<Spreadsheet::Position> : public GenericTraits<Spreadsheet::Position> {
    static constexpr bool is_trivial() { return false; }
    static unsigned hash(const Spreadsheet::Position& p)
    {
        return pair_int_hash(
            string_hash(p.column.characters(), p.column.length()),
            u64_hash(p.row));
    }
};

}
