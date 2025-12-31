#pragma once

#include <ftxui/dom/elements.hpp>

#include <functional>
#include <optional>
#include <vector>

#include "primitives.hpp"
#include "style.hpp"

namespace repo::tui::render {

using namespace ftxui;

// ListView - A reusable scrollable list component
template <typename Item> class ListView {
  public:
    // Configuration for the list view
    struct Config {
        // Item rendering function
        std::function<Element(const Item&, bool is_selected)> render_item;

        // Optional: Group items into sections
        std::function<std::optional<std::string>(const Item&)> get_section;

        // Optional: Filter predicate
        std::function<bool(const Item&)> filter = [](const Item&) { return true; };

        // Optional: Custom empty state message
        std::string empty_message = "No items";

        // Visual options
        bool show_cursor = true;
        bool show_border = false;
        bool show_scrollbar = true;
        size_t visible_items = 20;

        // Selection color
        Color selection_bg = Color::Blue;
        Color selection_fg = Color::White;
    };

    explicit ListView(Config config);

    // Set items to display
    auto set_items(std::vector<Item> items) -> void;

    // Get current items (after filtering)
    auto items() const -> const std::vector<Item>&;

    // Selection management
    auto selected_index() const -> size_t { return selected_index_; }
    auto set_selected_index(size_t index) -> void;
    auto selected_item() const -> std::optional<Item>;

    // Navigation
    auto move_up(size_t count = 1) -> void;
    auto move_down(size_t count = 1) -> void;
    auto move_to_top() -> void;
    auto move_to_bottom() -> void;
    auto page_up() -> void;
    auto page_down() -> void;

    // Multi-selection
    auto toggle_selection() -> void;
    auto selected_items() const -> std::vector<Item>;
    auto clear_selection() -> void;

    // Scrolling
    auto scroll_offset() const -> size_t { return scroll_offset_; }
    auto set_scroll_offset(size_t offset) -> void;

    // Render the list
    auto render() -> Element;

  private:
    auto apply_filter() -> void;
    auto ensure_selection_visible() -> void;
    auto render_scrollbar() -> Element;

    Config config_;
    std::vector<Item> all_items_;
    std::vector<Item> filtered_items_;
    std::vector<bool> item_selections_; // For multi-select
    size_t selected_index_ = 0;
    size_t scroll_offset_ = 0;
};

// ListView specialization for simple string lists
using StringListView = ListView<std::string>;

// Factory functions for common list view patterns

// Create a file list view (for status view)
template <typename FileItem>
auto make_file_list_view(std::function<Element(const FileItem&, bool)> render_item)
    -> ListView<FileItem> {
    return ListView<FileItem>({
        .render_item = render_item,
        .empty_message = "No files to display",
        .show_cursor = true,
        .show_border = true,
        .show_scrollbar = true,
    });
}

// Create a commit list view (for log view)
template <typename CommitItem>
auto make_commit_list_view(std::function<Element(const CommitItem&, bool)> render_item)
    -> ListView<CommitItem> {
    return ListView<CommitItem>({
        .render_item = render_item,
        .empty_message = "No commits to display",
        .show_cursor = true,
        .show_border = true,
        .show_scrollbar = true,
        .visible_items = 25,
    });
}

// Create a branch list view (for branch view)
template <typename BranchItem>
auto make_branch_list_view(std::function<Element(const BranchItem&, bool)> render_item)
    -> ListView<BranchItem> {
    return ListView<BranchItem>({
        .render_item = render_item,
        .empty_message = "No branches to display",
        .show_cursor = true,
        .show_border = true,
        .show_scrollbar = true,
    });
}

// Template implementation

template <typename Item> ListView<Item>::ListView(Config config) : config_(std::move(config)) {}

template <typename Item> auto ListView<Item>::set_items(std::vector<Item> items) -> void {
    all_items_ = std::move(items);
    apply_filter();
    item_selections_.resize(filtered_items_.size(), false);

    // Reset selection if out of bounds
    if (selected_index_ >= filtered_items_.size() && !filtered_items_.empty()) {
        selected_index_ = 0;
    }
    ensure_selection_visible();
}

template <typename Item> auto ListView<Item>::items() const -> const std::vector<Item>& {
    return filtered_items_;
}

template <typename Item> auto ListView<Item>::set_selected_index(size_t index) -> void {
    if (index < filtered_items_.size()) {
        selected_index_ = index;
        ensure_selection_visible();
    }
}

template <typename Item> auto ListView<Item>::selected_item() const -> std::optional<Item> {
    if (selected_index_ < filtered_items_.size()) {
        return filtered_items_[selected_index_];
    }
    return std::nullopt;
}

template <typename Item> auto ListView<Item>::move_up(size_t count) -> void {
    if (selected_index_ >= count) {
        selected_index_ -= count;
    } else {
        selected_index_ = 0;
    }
    ensure_selection_visible();
}

template <typename Item> auto ListView<Item>::move_down(size_t count) -> void {
    selected_index_ =
        std::min(selected_index_ + count, filtered_items_.empty() ? 0 : filtered_items_.size() - 1);
    ensure_selection_visible();
}

template <typename Item> auto ListView<Item>::move_to_top() -> void {
    selected_index_ = 0;
    scroll_offset_ = 0;
}

template <typename Item> auto ListView<Item>::move_to_bottom() -> void {
    if (!filtered_items_.empty()) {
        selected_index_ = filtered_items_.size() - 1;
        ensure_selection_visible();
    }
}

template <typename Item> auto ListView<Item>::page_up() -> void {
    move_up(config_.visible_items);
}

template <typename Item> auto ListView<Item>::page_down() -> void {
    move_down(config_.visible_items);
}

template <typename Item> auto ListView<Item>::toggle_selection() -> void {
    if (selected_index_ < item_selections_.size()) {
        item_selections_[selected_index_] = !item_selections_[selected_index_];
    }
}

template <typename Item> auto ListView<Item>::selected_items() const -> std::vector<Item> {
    std::vector<Item> result;
    for (size_t i = 0; i < filtered_items_.size() && i < item_selections_.size(); ++i) {
        if (item_selections_[i]) {
            result.push_back(filtered_items_[i]);
        }
    }
    return result;
}

template <typename Item> auto ListView<Item>::clear_selection() -> void {
    std::fill(item_selections_.begin(), item_selections_.end(), false);
}

template <typename Item> auto ListView<Item>::set_scroll_offset(size_t offset) -> void {
    scroll_offset_ = offset;
}

template <typename Item> auto ListView<Item>::render() -> Element {
    if (filtered_items_.empty()) {
        return empty_state(config_.empty_message);
    }

    std::vector<Element> elements;

    // Determine visible range
    size_t start = scroll_offset_;
    size_t end = std::min(start + config_.visible_items, filtered_items_.size());

    // Track current section (if grouping is enabled)
    std::optional<std::string> current_section;

    for (size_t i = start; i < end; ++i) {
        const auto& item = filtered_items_[i];
        bool is_selected = (i == selected_index_);

        // Render section header if needed
        if (config_.get_section) {
            auto section = config_.get_section(item);
            if (section && section != current_section) {
                current_section = section;
                elements.push_back(text(*section) | bold | color(Style::color_scheme().dimmed));
            }
        }

        // Render item
        auto item_elem = config_.render_item(item, is_selected);

        // Apply selection styling
        if (is_selected && config_.show_cursor) {
            item_elem = item_elem | bgcolor(config_.selection_bg) | color(config_.selection_fg);
        }

        // Show multi-selection indicator
        if (i < item_selections_.size() && item_selections_[i]) {
            item_elem = hbox({text("[x] ") | color(Style::color_scheme().success), item_elem});
        }

        elements.push_back(item_elem);
    }

    auto content = vbox(std::move(elements));

    // Add scrollbar if needed
    if (config_.show_scrollbar && filtered_items_.size() > config_.visible_items) {
        content = hbox({content | flex, render_scrollbar()});
    }

    // Add border if requested
    if (config_.show_border) {
        content = content | border;
    }

    return content;
}

template <typename Item> auto ListView<Item>::apply_filter() -> void {
    filtered_items_.clear();
    for (const auto& item : all_items_) {
        if (config_.filter(item)) {
            filtered_items_.push_back(item);
        }
    }
}

template <typename Item> auto ListView<Item>::ensure_selection_visible() -> void {
    if (filtered_items_.empty()) {
        scroll_offset_ = 0;
        return;
    }

    // Scroll down if selection is below visible area
    if (selected_index_ >= scroll_offset_ + config_.visible_items) {
        scroll_offset_ = selected_index_ - config_.visible_items + 1;
    }

    // Scroll up if selection is above visible area
    if (selected_index_ < scroll_offset_) {
        scroll_offset_ = selected_index_;
    }
}

template <typename Item> auto ListView<Item>::render_scrollbar() -> Element {
    if (filtered_items_.empty()) {
        return text("");
    }

    size_t bar_height = config_.visible_items;
    size_t thumb_size =
        std::max(size_t(1), (bar_height * config_.visible_items) / filtered_items_.size());
    size_t thumb_pos = (scroll_offset_ * bar_height) / filtered_items_.size();

    std::vector<Element> bar_elements;
    for (size_t i = 0; i < bar_height; ++i) {
        if (i >= thumb_pos && i < thumb_pos + thumb_size) {
            bar_elements.push_back(text("█") | color(Style::color_scheme().cursor));
        } else {
            bar_elements.push_back(text("│") | color(Style::color_scheme().dimmed));
        }
    }

    return vbox(std::move(bar_elements));
}

} // namespace repo::tui::render
