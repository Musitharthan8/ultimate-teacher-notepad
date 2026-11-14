#include "PdfToolsDialogs.h"

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <cairo.h>
#include <poppler.h>

#include "pdf/tools/PageOrganiser.h"
#include "pdf/tools/PdfTools.h"
#include "util/PathUtil.h"
#include "util/i18n.h"

namespace tools = utn::pdftools;

namespace utn::pdftoolsui {
namespace {

constexpr int RESPONSE_PRIMARY = GTK_RESPONSE_ACCEPT;

std::string utf8(const fs::path& p) {
    const auto s = p.u8string();
    return std::string(s.begin(), s.end());
}

fs::path fromUtf8(const char* s) {
    const std::string str = s ? s : "";
    return fs::path(std::u8string(str.begin(), str.end()));
}

void tell(GtkWindow* parent, GtkMessageType type, const std::string& text) {
    GtkWidget* message = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL, type, GTK_BUTTONS_CLOSE, "%s", text.c_str());
    gtk_dialog_run(GTK_DIALOG(message));
    gtk_widget_destroy(message);
}

void addPdfFilter(GtkFileChooser* chooser) {
    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, _("PDF files"));
    gtk_file_filter_add_pattern(filter, "*.pdf");
    gtk_file_filter_add_pattern(filter, "*.PDF");
    gtk_file_chooser_add_filter(chooser, filter);
}

std::vector<fs::path> chooseOpenPdfs(GtkWindow* parent, const fs::path& folder, bool multiple) {
    std::vector<fs::path> result;
    GtkFileChooserNative* chooser = gtk_file_chooser_native_new(multiple ? _("Choose PDFs") : _("Choose a PDF"), parent,
                                                                GTK_FILE_CHOOSER_ACTION_OPEN, _("Choose"), _("Cancel"));
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(chooser), multiple);
    addPdfFilter(GTK_FILE_CHOOSER(chooser));
    std::error_code ec;
    if (!folder.empty() && fs::is_directory(folder, ec)) {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), utf8(folder).c_str());
    }
    if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
        GSList* names = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(chooser));
        for (GSList* it = names; it; it = it->next) {
            result.push_back(fromUtf8(static_cast<const char*>(it->data)));
            g_free(it->data);
        }
        g_slist_free(names);
    }
    g_object_unref(chooser);
    return result;
}

std::optional<fs::path> chooseSavePdf(GtkWindow* parent, const fs::path& folder, const std::string& suggestedName) {
    std::optional<fs::path> result;
    GtkFileChooserNative* chooser = gtk_file_chooser_native_new(_("Save the new PDF"), parent,
                                                                GTK_FILE_CHOOSER_ACTION_SAVE, _("Save"), _("Cancel"));
    addPdfFilter(GTK_FILE_CHOOSER(chooser));
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(chooser), TRUE);
    std::error_code ec;
    if (!folder.empty() && fs::is_directory(folder, ec)) {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), utf8(folder).c_str());
    }
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(chooser), suggestedName.c_str());
    if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
        gchar* name = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
        if (name) {
            fs::path p = fromUtf8(name);
            if (p.extension() != ".pdf" && p.extension() != ".PDF") {
                p += ".pdf";
            }
            result = p;
            g_free(name);
        }
    }
    g_object_unref(chooser);
    return result;
}

GtkWidget* newDialog(GtkWindow* parent, const char* title, int width, int height) {
    GtkWidget* dialog = gtk_dialog_new_with_buttons(title, parent, GTK_DIALOG_MODAL, _("Cancel"), GTK_RESPONSE_CANCEL,
                                                    nullptr);
    gtk_window_set_default_size(GTK_WINDOW(dialog), width, height);
    return dialog;
}

GtkWidget* contentBox(GtkWidget* dialog) {
    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 12);
    gtk_box_set_spacing(GTK_BOX(content), 8);
    return content;
}

GtkWidget* wrappedLabel(const std::string& text) {
    GtkWidget* label = gtk_label_new(text.c_str());
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    return label;
}

void setAccessibleName(GtkWidget* widget, const char* name) {
    if (AtkObject* a = gtk_widget_get_accessible(widget)) {
        atk_object_set_name(a, name);
    }
}

GtkWidget* iconButton(const char* glyph, const char* tooltip) {
    GtkWidget* b = gtk_button_new_with_label(glyph);
    gtk_widget_set_tooltip_text(b, tooltip);
    setAccessibleName(b, tooltip);
    gtk_button_set_relief(GTK_BUTTON(b), GTK_RELIEF_NONE);
    return b;
}

void clearChildren(GtkWidget* container) {
    GList* children = gtk_container_get_children(GTK_CONTAINER(container));
    for (GList* c = children; c; c = c->next) {
        gtk_widget_destroy(GTK_WIDGET(c->data));
    }
    g_list_free(children);
}

std::string stemOf(const fs::path& p) { return utf8(p.stem()); }

// ---------------------------------------------------------------------------------------------------------------
// Merge
// ---------------------------------------------------------------------------------------------------------------
struct MergeState {
    GtkWindow* parent = nullptr;
    GtkWidget* dialog = nullptr;
    GtkWidget* list = nullptr;
    GtkWidget* hint = nullptr;
    GtkWidget* remove = nullptr;
    GtkWidget* up = nullptr;
    GtkWidget* down = nullptr;
    fs::path folder;
    std::vector<fs::path> files;
    std::map<std::string, long> pages;

    long pagesOf(const fs::path& f) {
        const std::string key = utf8(f);
        auto it = pages.find(key);
        if (it == pages.end()) {
            it = pages.emplace(key, tools::pageCount(f)).first;
        }
        return it->second;
    }

    int selected() const {
        GtkListBoxRow* row = gtk_list_box_get_selected_row(GTK_LIST_BOX(list));
        return row ? gtk_list_box_row_get_index(row) : -1;
    }

    void refresh(int select = -1) {
        clearChildren(list);
        long total = 0;
        for (const auto& f: files) {
            const long n = pagesOf(f);
            total += std::max(0L, n);
            const std::string text = utf8(f.filename()) + (n >= 0 ? "   (" + std::to_string(n) + (n == 1 ? " page)" : " pages)")
                                                                  : std::string("   (cannot be read)"));
            GtkWidget* label = gtk_label_new(text.c_str());
            gtk_label_set_xalign(GTK_LABEL(label), 0);
            gtk_widget_set_tooltip_text(label, utf8(f).c_str());
            gtk_widget_set_margin_top(label, 4);
            gtk_widget_set_margin_bottom(label, 4);
            gtk_list_box_insert(GTK_LIST_BOX(list), label, -1);
        }
        gtk_widget_show_all(list);
        if (select >= 0 && select < static_cast<int>(files.size())) {
            gtk_list_box_select_row(GTK_LIST_BOX(list), gtk_list_box_get_row_at_index(GTK_LIST_BOX(list), select));
        }
        updateButtons(total);
    }

    void updateButtons(long total) {
        const int sel = selected();
        gtk_widget_set_sensitive(remove, sel >= 0);
        gtk_widget_set_sensitive(up, sel > 0);
        gtk_widget_set_sensitive(down, sel >= 0 && sel + 1 < static_cast<int>(files.size()));
        const bool ready = files.size() >= 2 && std::all_of(files.begin(), files.end(), [&](const fs::path& f) {
                               return pagesOf(f) >= 0;
                           });
        gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), RESPONSE_PRIMARY, ready);
        std::string text = files.size() < 2 ? std::string(_("Add at least two PDFs. They are joined in the order shown."))
                                            : std::to_string(files.size()) + _(" PDFs, ") + std::to_string(total) +
                                                      _(" pages in all. They are joined in the order shown.");
        gtk_label_set_text(GTK_LABEL(hint), text.c_str());
    }
};

}  // namespace

void showMergeDialog(GtkWindow* parent, const fs::path& startFolder) {
    MergeState s;
    s.parent = parent;
    s.folder = startFolder;
    s.dialog = newDialog(parent, _("Merge PDFs"), 520, 420);
    GtkWidget* merge = gtk_dialog_add_button(GTK_DIALOG(s.dialog), _("Merge…"), RESPONSE_PRIMARY);
    gtk_widget_set_tooltip_text(merge, _("Choose a name and save the merged PDF"));
    GtkWidget* content = contentBox(s.dialog);

    s.hint = wrappedLabel("");
    gtk_box_pack_start(GTK_BOX(content), s.hint, FALSE, FALSE, 0);

    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll), GTK_SHADOW_IN);
    s.list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(s.list), GTK_SELECTION_SINGLE);
    setAccessibleName(s.list, _("PDFs to merge"));
    gtk_container_add(GTK_CONTAINER(scroll), s.list);
    gtk_box_pack_start(GTK_BOX(row), scroll, TRUE, TRUE, 0);

    GtkWidget* buttons = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget* add = gtk_button_new_with_label(_("Add PDFs…"));
    s.remove = gtk_button_new_with_label(_("Remove"));
    s.up = gtk_button_new_with_label(_("Move up"));
    s.down = gtk_button_new_with_label(_("Move down"));
    for (GtkWidget* b: {add, s.remove, s.up, s.down}) {
        gtk_box_pack_start(GTK_BOX(buttons), b, FALSE, FALSE, 0);
    }
    gtk_box_pack_start(GTK_BOX(row), buttons, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), row, TRUE, TRUE, 0);

    g_signal_connect(add, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         auto* st = static_cast<MergeState*>(data);
                         for (auto& f: chooseOpenPdfs(st->parent, st->folder, true)) {
                             if (st->folder.empty()) {
                                 st->folder = f.parent_path();
                             }
                             st->files.push_back(std::move(f));
                         }
                         st->refresh(static_cast<int>(st->files.size()) - 1);
                     }), &s);
    g_signal_connect(s.remove, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         auto* st = static_cast<MergeState*>(data);
                         const int i = st->selected();
                         if (i >= 0) {
                             st->files.erase(st->files.begin() + i);
                             st->refresh(std::min(i, static_cast<int>(st->files.size()) - 1));
                         }
                     }), &s);
    g_signal_connect(s.up, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         auto* st = static_cast<MergeState*>(data);
                         const int i = st->selected();
                         if (i > 0) {
                             std::swap(st->files[i], st->files[i - 1]);
                             st->refresh(i - 1);
                         }
                     }), &s);
    g_signal_connect(s.down, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         auto* st = static_cast<MergeState*>(data);
                         const int i = st->selected();
                         if (i >= 0 && i + 1 < static_cast<int>(st->files.size())) {
                             std::swap(st->files[i], st->files[i + 1]);
                             st->refresh(i + 1);
                         }
                     }), &s);
    g_signal_connect(s.list, "row-selected", G_CALLBACK(+[](GtkListBox*, GtkListBoxRow*, gpointer data) {
                         auto* st = static_cast<MergeState*>(data);
                         long total = 0;
                         for (const auto& f: st->files) {
                             total += std::max(0L, st->pagesOf(f));
                         }
                         st->updateButtons(total);
                     }), &s);

    s.refresh();
    gtk_widget_show_all(s.dialog);
    while (gtk_dialog_run(GTK_DIALOG(s.dialog)) == RESPONSE_PRIMARY) {
        const std::string name = s.files.empty() ? "Merged.pdf" : stemOf(s.files.front()) + " (merged).pdf";
        auto out = chooseSavePdf(GTK_WINDOW(s.dialog), s.folder, name);
        if (!out) {
            continue;  // they changed their mind about the name; stay in the dialog
        }
        const auto result = tools::mergeFiles(s.files, *out);
        if (result.ok) {
            tell(GTK_WINDOW(s.dialog), GTK_MESSAGE_INFO,
                 std::string(_("Saved ")) + utf8(out->filename()) + " (" + std::to_string(result.pages) + _(" pages)."));
            break;
        }
        tell(GTK_WINDOW(s.dialog), GTK_MESSAGE_ERROR, result.error);
    }
    gtk_widget_destroy(s.dialog);
}

namespace {

// ---------------------------------------------------------------------------------------------------------------
// A source PDF chosen for extract / split / organise
// ---------------------------------------------------------------------------------------------------------------
struct Source {
    fs::path file;
    long pages = -1;
    GtkWidget* label = nullptr;

    bool set(GtkWindow* parent, const fs::path& f) {
        std::string error;
        const long n = tools::pageCount(f, &error);
        if (n < 0) {
            tell(parent, GTK_MESSAGE_ERROR, "\"" + utf8(f.filename()) + "\": " + error);
            return false;
        }
        file = f;
        pages = n;
        if (label) {
            const std::string text = utf8(f.filename()) + "   (" + std::to_string(n) + (n == 1 ? " page)" : " pages)");
            gtk_label_set_text(GTK_LABEL(label), text.c_str());
            gtk_widget_set_tooltip_text(label, utf8(f).c_str());
        }
        return true;
    }
};

/// The "Source: file (N pages) [Choose another PDF…]" row. `onChanged` runs when a different PDF is chosen.
GtkWidget* sourceRow(Source& src, GtkWindow* parent, std::function<void()>* onChanged) {
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* caption = gtk_label_new(_("PDF:"));
    src.label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(src.label), 0);
    gtk_label_set_ellipsize(GTK_LABEL(src.label), PANGO_ELLIPSIZE_MIDDLE);
    GtkWidget* choose = gtk_button_new_with_label(_("Choose another PDF…"));
    gtk_box_pack_start(GTK_BOX(row), caption, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), src.label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), choose, FALSE, FALSE, 0);
    struct Ctx {
        Source* src;
        GtkWindow* parent;
        std::function<void()>* changed;
    };
    auto* ctx = new Ctx{&src, parent, onChanged};
    g_signal_connect_data(choose, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                              auto* c = static_cast<Ctx*>(data);
                              auto picked = chooseOpenPdfs(c->parent, c->src->file.parent_path(), false);
                              if (!picked.empty() && c->src->set(c->parent, picked.front()) && c->changed && *c->changed) {
                                  (*c->changed)();
                              }
                          }),
                          ctx, +[](gpointer data, GClosure*) { delete static_cast<Ctx*>(data); }, GConnectFlags(0));
    return row;
}

/// Makes sure there is a source: the suggested one, or one the teacher picks. False when they cancel.
bool pickInitialSource(Source& src, GtkWindow* parent, const fs::path& suggested) {
    std::error_code ec;
    if (!suggested.empty() && fs::exists(suggested, ec) && src.set(parent, suggested)) {
        return true;
    }
    auto picked = chooseOpenPdfs(parent, fs::path(), false);
    return !picked.empty() && src.set(parent, picked.front());
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// Extract
// ---------------------------------------------------------------------------------------------------------------
void showExtractDialog(GtkWindow* parent, const fs::path& currentPdf) {
    Source src;
    if (!pickInitialSource(src, parent, currentPdf)) {
        return;
    }
    GtkWidget* dialog = newDialog(parent, _("Extract Pages"), 520, 220);
    gtk_dialog_add_button(GTK_DIALOG(dialog), _("Extract…"), RESPONSE_PRIMARY);
    GtkWidget* content = contentBox(dialog);

    std::function<void()> update;
    gtk_box_pack_start(GTK_BOX(content), sourceRow(src, parent, &update), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), wrappedLabel(_("Pages to take (page 1 is the first). Examples: 3, 1-4, 2-, 5,1")),
                       FALSE, FALSE, 0);
    GtkWidget* entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), _("1-3, 5, 8-"));
    setAccessibleName(entry, _("Pages to extract"));
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_box_pack_start(GTK_BOX(content), entry, FALSE, FALSE, 0);
    GtkWidget* summary = wrappedLabel("");
    gtk_box_pack_start(GTK_BOX(content), summary, FALSE, FALSE, 0);

    update = [&]() {
        bool ok = false;
        const std::string text = tools::describeRanges(gtk_entry_get_text(GTK_ENTRY(entry)),
                                                       static_cast<size_t>(std::max(0L, src.pages)), &ok);
        gtk_label_set_text(GTK_LABEL(summary), text.c_str());
        gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), RESPONSE_PRIMARY, ok);
    };
    g_signal_connect(entry, "changed", G_CALLBACK(+[](GtkEditable*, gpointer data) {
                         (*static_cast<std::function<void()>*>(data))();
                     }), &update);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), RESPONSE_PRIMARY);
    update();
    gtk_widget_show_all(dialog);
    while (gtk_dialog_run(GTK_DIALOG(dialog)) == RESPONSE_PRIMARY) {
        auto out = chooseSavePdf(GTK_WINDOW(dialog), src.file.parent_path(), stemOf(src.file) + " (pages).pdf");
        if (!out) {
            continue;
        }
        const auto result = tools::extractPages(src.file, gtk_entry_get_text(GTK_ENTRY(entry)), *out);
        if (result.ok) {
            tell(GTK_WINDOW(dialog), GTK_MESSAGE_INFO,
                 std::string(_("Saved ")) + utf8(out->filename()) + " (" + std::to_string(result.pages) + _(" pages)."));
            break;
        }
        tell(GTK_WINDOW(dialog), GTK_MESSAGE_ERROR, result.error);
    }
    gtk_widget_destroy(dialog);
}

// ---------------------------------------------------------------------------------------------------------------
// Split
// ---------------------------------------------------------------------------------------------------------------
void showSplitDialog(GtkWindow* parent, const fs::path& currentPdf) {
    Source src;
    if (!pickInitialSource(src, parent, currentPdf)) {
        return;
    }
    GtkWidget* dialog = newDialog(parent, _("Split a PDF"), 520, 260);
    gtk_dialog_add_button(GTK_DIALOG(dialog), _("Split"), RESPONSE_PRIMARY);
    GtkWidget* content = contentBox(dialog);

    std::function<void()> update;
    gtk_box_pack_start(GTK_BOX(content), sourceRow(src, parent, &update), FALSE, FALSE, 0);

    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(row), gtk_label_new(_("Pages per file:")), FALSE, FALSE, 0);
    GtkWidget* spin = gtk_spin_button_new_with_range(1, 1000, 1);
    setAccessibleName(spin, _("Pages per file"));
    gtk_box_pack_start(GTK_BOX(row), spin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), row, FALSE, FALSE, 0);
    GtkWidget* summary = wrappedLabel("");
    gtk_box_pack_start(GTK_BOX(content), summary, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), wrappedLabel(_("The new files are saved next to the PDF. Existing files are never replaced.")),
                       FALSE, FALSE, 0);

    update = [&]() {
        const size_t per = static_cast<size_t>(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin)));
        const auto names = tools::splitNames(stemOf(src.file), static_cast<size_t>(std::max(0L, src.pages)), per);
        std::string text;
        if (names.empty()) {
            text = _("The PDF has no more pages than this, so there is nothing to split.");
        } else {
            text = std::to_string(names.size()) + _(" files: ") + names.front() + (names.size() > 2 ? ", ..., " : ", ") +
                   names.back();
        }
        gtk_label_set_text(GTK_LABEL(summary), text.c_str());
        gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), RESPONSE_PRIMARY, !names.empty());
    };
    g_signal_connect(spin, "value-changed", G_CALLBACK(+[](GtkSpinButton*, gpointer data) {
                         (*static_cast<std::function<void()>*>(data))();
                     }), &update);
    update();
    gtk_widget_show_all(dialog);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == RESPONSE_PRIMARY) {
        std::vector<fs::path> written;
        const auto result = tools::splitByPageCount(src.file,
                                                    static_cast<size_t>(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin))),
                                                    src.file.parent_path(), stemOf(src.file), &written);
        if (result.ok) {
            tell(GTK_WINDOW(dialog), GTK_MESSAGE_INFO,
                 std::to_string(written.size()) + _(" files were saved next to ") + utf8(src.file.filename()) + ".");
        } else {
            tell(GTK_WINDOW(dialog), GTK_MESSAGE_ERROR, result.error);
        }
    }
    gtk_widget_destroy(dialog);
}

// ---------------------------------------------------------------------------------------------------------------
// Organise pages
// ---------------------------------------------------------------------------------------------------------------
namespace {

constexpr int THUMB_HEIGHT = 120;

/// A small picture of the page, turned by `degrees`
cairo_surface_t* makeThumbnail(PopplerPage* page, int degrees) {
    double w = 0;
    double h = 0;
    poppler_page_get_size(page, &w, &h);
    const double scale = THUMB_HEIGHT / std::max(1.0, (degrees % 180 == 0) ? h : w);
    const int outW = std::max(1, static_cast<int>(((degrees % 180 == 0) ? w : h) * scale));
    const int outH = std::max(1, static_cast<int>(((degrees % 180 == 0) ? h : w) * scale));
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, outW, outH);
    cairo_t* cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    switch (degrees) {
        case 90:
            cairo_translate(cr, outW, 0);
            cairo_rotate(cr, G_PI / 2);
            break;
        case 180:
            cairo_translate(cr, outW, outH);
            cairo_rotate(cr, G_PI);
            break;
        case 270:
            cairo_translate(cr, 0, outH);
            cairo_rotate(cr, -G_PI / 2);
            break;
        default:
            break;
    }
    cairo_scale(cr, scale, scale);
    poppler_page_render(page, cr);
    cairo_destroy(cr);
    return surface;
}

struct OrganiseState {
    GtkWindow* parent = nullptr;
    GtkWidget* dialog = nullptr;
    GtkWidget* list = nullptr;
    GtkWidget* scroll = nullptr;
    GtkWidget* undo = nullptr;
    GtkWidget* reset = nullptr;
    GtkWidget* summary = nullptr;
    Source src;
    std::unique_ptr<tools::PageOrganiser> organiser;
    PopplerDocument* doc = nullptr;

    ~OrganiseState() {
        if (doc) {
            g_object_unref(doc);
        }
    }

    void load(const fs::path& file, long pages) {
        organiser = std::make_unique<tools::PageOrganiser>(file, static_cast<size_t>(pages));
        if (doc) {
            g_object_unref(doc);
            doc = nullptr;
        }
        const auto uri = Util::toUri(file);
        if (uri) {
            doc = poppler_document_new_from_file(uri->c_str(), nullptr, nullptr);
        }
    }

    void rebuild() {
        GtkAdjustment* adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroll));
        const double keep = gtk_adjustment_get_value(adj);
        clearChildren(list);
        for (size_t i = 0; i < organiser->size(); ++i) {
            const tools::Piece& piece = organiser->at(i);
            GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
            gtk_widget_set_margin_top(row, 4);
            gtk_widget_set_margin_bottom(row, 4);
            gtk_widget_set_margin_start(row, 4);
            if (doc) {
                if (PopplerPage* page = poppler_document_get_page(doc, static_cast<int>(piece.pageIndex))) {
                    cairo_surface_t* thumb = makeThumbnail(page, piece.rotateDegrees);
                    GtkWidget* image = gtk_image_new_from_surface(thumb);
                    cairo_surface_destroy(thumb);
                    gtk_box_pack_start(GTK_BOX(row), image, FALSE, FALSE, 0);
                    g_object_unref(page);
                }
            }
            std::string text = std::string(_("Page ")) + std::to_string(i + 1);
            if (piece.pageIndex != i) {
                text += std::string("  (") + _("was page ") + std::to_string(piece.pageIndex + 1) + ")";
            }
            if (piece.rotateDegrees != 0) {
                text += std::string("  ") + _("turned ") + std::to_string(piece.rotateDegrees) + "°";
            }
            GtkWidget* label = gtk_label_new(text.c_str());
            gtk_label_set_xalign(GTK_LABEL(label), 0);
            gtk_box_pack_start(GTK_BOX(row), label, TRUE, TRUE, 0);

            struct Btn {
                const char* glyph;
                const char* tip;
                int op;  // 0 up, 1 down, 2 anticlockwise, 3 clockwise, 4 remove
            };
            const Btn buttons[] = {{"▲", _("Move this page up"), 0},    {"▼", _("Move this page down"), 1},
                                   {"↺", _("Turn this page anticlockwise"), 2}, {"↻", _("Turn this page clockwise"), 3},
                                   {"✕", _("Remove this page"), 4}};
            for (const Btn& b: buttons) {
                GtkWidget* button = iconButton(b.glyph, b.tip);
                g_object_set_data(G_OBJECT(button), "op", GINT_TO_POINTER(b.op));
                g_object_set_data(G_OBJECT(button), "index", GINT_TO_POINTER(static_cast<int>(i)));
                const bool enabled = !(b.op == 0 && i == 0) && !(b.op == 1 && i + 1 == organiser->size()) &&
                                     !(b.op == 4 && organiser->size() == 1);
                gtk_widget_set_sensitive(button, enabled);
                g_signal_connect(button, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer data) {
                                     auto* st = static_cast<OrganiseState*>(data);
                                     const int op = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "op"));
                                     const size_t idx = static_cast<size_t>(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "index")));
                                     switch (op) {
                                         case 0: st->organiser->moveUp(idx); break;
                                         case 1: st->organiser->moveDown(idx); break;
                                         case 2: st->organiser->rotate(idx, -1); break;
                                         case 3: st->organiser->rotate(idx, 1); break;
                                         default: st->organiser->remove(idx); break;
                                     }
                                     st->rebuild();
                                 }), this);
                gtk_box_pack_start(GTK_BOX(row), button, FALSE, FALSE, 0);
            }
            gtk_list_box_insert(GTK_LIST_BOX(list), row, -1);
        }
        gtk_widget_show_all(list);
        gtk_adjustment_set_value(adj, keep);
        updateButtons();
    }

    void updateButtons() {
        gtk_widget_set_sensitive(undo, organiser->canUndo());
        gtk_widget_set_sensitive(reset, organiser->changed());
        gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), RESPONSE_PRIMARY, organiser->changed());
        const size_t total = organiser->size();
        const size_t originalCount = static_cast<size_t>(std::max(0L, src.pages));
        std::string text = std::to_string(total) + (total == 1 ? _(" page") : _(" pages"));
        if (total != originalCount) {
            text += std::string(" (") + std::to_string(originalCount) + _(" in the original)");
        }
        text += organiser->changed() ? std::string(". ") + _("Save as new PDF… writes a new file; the original is not changed.")
                                     : std::string(". ") + _("Change something to save a new PDF.");
        gtk_label_set_text(GTK_LABEL(summary), text.c_str());
    }
};

}  // namespace

void showOrganiseDialog(GtkWindow* parent, const fs::path& currentPdf) {
    OrganiseState s;
    s.parent = parent;
    if (!pickInitialSource(s.src, parent, currentPdf)) {
        return;
    }
    s.dialog = newDialog(parent, _("Organise Pages"), 640, 560);
    gtk_dialog_add_button(GTK_DIALOG(s.dialog), _("Save as new PDF…"), RESPONSE_PRIMARY);
    GtkWidget* content = contentBox(s.dialog);

    std::function<void()> onSourceChanged = [&]() {
        s.load(s.src.file, s.src.pages);
        s.rebuild();
    };
    gtk_box_pack_start(GTK_BOX(content), sourceRow(s.src, parent, &onSourceChanged), FALSE, FALSE, 0);

    GtkWidget* tools_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    s.undo = gtk_button_new_with_label(_("Undo"));
    s.reset = gtk_button_new_with_label(_("Start over"));
    gtk_box_pack_start(GTK_BOX(tools_), s.undo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(tools_), s.reset, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), tools_, FALSE, FALSE, 0);

    s.scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(s.scroll), GTK_SHADOW_IN);
    s.list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(s.list), GTK_SELECTION_NONE);
    setAccessibleName(s.list, _("Pages"));
    gtk_container_add(GTK_CONTAINER(s.scroll), s.list);
    gtk_box_pack_start(GTK_BOX(content), s.scroll, TRUE, TRUE, 0);
    s.summary = wrappedLabel("");
    gtk_box_pack_start(GTK_BOX(content), s.summary, FALSE, FALSE, 0);

    g_signal_connect(s.undo, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         auto* st = static_cast<OrganiseState*>(data);
                         st->organiser->undo();
                         st->rebuild();
                     }), &s);
    g_signal_connect(s.reset, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         auto* st = static_cast<OrganiseState*>(data);
                         st->organiser->reset();
                         st->rebuild();
                     }), &s);

    s.load(s.src.file, s.src.pages);
    gtk_widget_show_all(s.dialog);
    s.rebuild();
    while (gtk_dialog_run(GTK_DIALOG(s.dialog)) == RESPONSE_PRIMARY) {
        auto out = chooseSavePdf(GTK_WINDOW(s.dialog), s.src.file.parent_path(), stemOf(s.src.file) + " (organised).pdf");
        if (!out) {
            continue;
        }
        const auto result = tools::assemble(s.organiser->pieces(), *out);
        if (result.ok) {
            tell(GTK_WINDOW(s.dialog), GTK_MESSAGE_INFO,
                 std::string(_("Saved ")) + utf8(out->filename()) + " (" + std::to_string(result.pages) + _(" pages)."));
            break;
        }
        tell(GTK_WINDOW(s.dialog), GTK_MESSAGE_ERROR, result.error);
    }
    gtk_widget_destroy(s.dialog);
}

}  // namespace utn::pdftoolsui
