/*
 * Ultimate Teacher Notepad
 * Classroom material and marking workflows, based on Xournal++ GPLv2+.
 */
#include "ToolClassroomWorkflow.h"

#include <string>
#include <utility>

#include <gio/gio.h>

#include "AnswerBoxStyles.h"  // for MARKING_COMMENT_STYLE
#include "control/Control.h"
#include "control/ToolHandler.h"
#include "enums/Action.enum.h"
#include "gui/MainWindow.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"

namespace {
enum class Command { SCAN, PASTE_IMAGE, IMAGE, COMMENT, MARKING, TEACHING };

GtkBox* makePanel(GtkNotebook* tabs, const char* label) {
    auto* box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 5));
    gtk_widget_set_margin_start(GTK_WIDGET(box), 10);
    gtk_widget_set_margin_end(GTK_WIDGET(box), 10);
    gtk_widget_set_margin_top(GTK_WIDGET(box), 10);
    gtk_widget_set_margin_bottom(GTK_WIDGET(box), 10);
    gtk_notebook_append_page(tabs, GTK_WIDGET(box), gtk_label_new(label));
    return box;
}

void addHint(GtkBox* panel, const char* text) {
    auto* label = gtk_label_new(text);
    gtk_label_set_line_wrap(GTK_LABEL(label), true);
    gtk_label_set_max_width_chars(GTK_LABEL(label), 42);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_add_css_class(label, "utn-context-hint");
    gtk_box_append(panel, label);
}

// Menu entries look like the other UTN menus: flat, left-aligned text
GtkWidget* newMenuEntry(const char* label) {
    auto* button = gtk_button_new_with_label(label);
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    gtk_label_set_xalign(GTK_LABEL(gtk_bin_get_child(GTK_BIN(button))), 0.0F);
    return button;
}

void addAction(GtkBox* panel, GtkPopover* popover, const char* label, const char* tooltip, Action action) {
    auto* button = newMenuEntry(label);
    gtk_widget_set_tooltip_text(button, tooltip);
    gtk_widget_set_can_focus(button, false);
    const std::string name = std::string("win.") + Action_toString(action);
    gtk_actionable_set_action_name(GTK_ACTIONABLE(button), name.c_str());
    g_signal_connect_object(button, "clicked", G_CALLBACK(+[](GtkButton*, gpointer p) {
                                gtk_popover_popdown(GTK_POPOVER(p));
                            }),
                            popover, GConnectFlags(0));
    gtk_box_append(panel, button);
}

void addCommand(GtkBox* panel, GtkPopover* popover, Control* control, const char* label, const char* tooltip,
                Command command) {
    auto* button = newMenuEntry(label);
    gtk_widget_set_tooltip_text(button, tooltip);
    gtk_widget_set_can_focus(button, false);
    g_object_set_data(G_OBJECT(button), "utn-control", control);
    g_object_set_data(G_OBJECT(button), "utn-command", GINT_TO_POINTER(static_cast<int>(command)));
    g_signal_connect_object(
            button, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer p) {
                auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(button), "utn-control"));
                auto command = static_cast<Command>(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "utn-command")));
                // Changing workspace may destroy this popover.
                gtk_popover_popdown(GTK_POPOVER(p));
                switch (command) {
                    case Command::SCAN:
                        ctrl->askInsertScanPage();
                        break;
                    case Command::PASTE_IMAGE:
                        ctrl->pasteImage();
                        break;
                    case Command::IMAGE:
                        ctrl->clearSelectionEndText();
                        ctrl->selectTool(TOOL_IMAGE);
                        break;
                    case Command::COMMENT: {
                        ctrl->clearSelectionEndText();
                        utn::applyAnswerBoxStyle(*ctrl->getToolHandler(), utn::MARKING_COMMENT_STYLE);
                        ctrl->selectTextMode(TextMode::AnswerBox);
                        break;
                    }
                    case Command::MARKING:
                    case Command::TEACHING:
                        ctrl->clearSelectionEndText();
                        if (auto* window = ctrl->getWindow()) {
                            window->toolbarSelected(command == Command::MARKING ? "UTN Marking" : "UTN Teacher");
                        }
                        break;
                }
            }),
            popover, GConnectFlags(0));
    gtk_box_append(panel, button);
}

// UTN teaching reference tools. Processes are launched asynchronously, so the UI remains responsive.
// Values are passed through environment variables or argv, never concatenated into shell expressions.
void setResult(GtkWidget* label, const char* message) {
    gtk_label_set_text(GTK_LABEL(label), message);
}

void readProcessOutput(GObject* source, GAsyncResult* result, gpointer userData) {
    GtkWidget* label = GTK_WIDGET(userData);
    gchar* output = nullptr;
    gchar* errors = nullptr;
    GError* error = nullptr;
    const gboolean finished = g_subprocess_communicate_utf8_finish(
            G_SUBPROCESS(source), result, &output, &errors, &error);
    if (!finished || error || !g_subprocess_get_successful(G_SUBPROCESS(source))) {
        setResult(label, (errors && *errors) ? errors :
                                (error ? error->message : _("The requested tool could not finish.")));
    } else {
        setResult(label, (output && *output) ? output : _("No text was returned."));
    }
    if (error) g_error_free(error);
    g_free(output);
    g_free(errors);
    g_object_unref(label);
}

// Windows PowerShell is present in supported Windows installations. The dictionary does not
// require browser navigation; PowerShell performs the HTTPS request and formats the source data.
void runDictionary(GtkWidget* entry, GtkWidget* label) {
#ifdef _WIN32
    const char* word = gtk_entry_get_text(GTK_ENTRY(entry));
    if (!word || !*word || g_utf8_strlen(word, -1) > 80) {
        setResult(label, _("Enter a word (up to 80 characters)."));
        return;
    }
    static constexpr char command[] = R"UTN(
[Console]::OutputEncoding=[Text.Encoding]::UTF8
try {
  $word=[Uri]::EscapeDataString($env:UTN_LOOKUP_WORD)
  $data=Invoke-RestMethod -Uri ('https://api.dictionaryapi.dev/api/v2/entries/en/'+$word) -TimeoutSec 12
  $entry=$data[0]
  $lines=@($entry.word)
  foreach($meaning in @($entry.meanings) | Select-Object -First 4) {
    $lines+=('['+$meaning.partOfSpeech+']')
    foreach($definition in @($meaning.definitions) | Select-Object -First 3) {
      $lines+=('• '+$definition.definition)
      if($definition.example) {$lines+=('  Example: '+$definition.example)}
    }
  }
  $lines -join [Environment]::NewLine
} catch {
  'No dictionary result. Check the spelling and Internet connection.'
}
)UTN";
    GSubprocessLauncher* launcher = g_subprocess_launcher_new(
            GSubprocessFlags(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE));
    g_subprocess_launcher_setenv(launcher, "UTN_LOOKUP_WORD", word, TRUE);
    GError* error = nullptr;
    GSubprocess* process = g_subprocess_launcher_spawn(launcher, &error, "powershell.exe",
                                                      "-NoProfile", "-NonInteractive", "-Command", command, nullptr);
    g_object_unref(launcher);
    if (!process) {
        setResult(label, error ? error->message : _("Could not start the online dictionary."));
        if (error) g_error_free(error);
        return;
    }
    setResult(label, _("Looking up the word online…"));
    g_subprocess_communicate_utf8_async(process, nullptr, nullptr, readProcessOutput, g_object_ref(label));
    g_object_unref(process);
#else
    setResult(label, _("Online dictionary is currently available in the Windows build."));
#endif
}

void readAloud(GtkWidget* textView, GtkWidget* label) {
#ifdef _WIN32
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textView));
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    gchar* passage = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    if (!passage || !*passage) {
        setResult(label, _("Enter or paste a passage first."));
        g_free(passage);
        return;
    }
    if (g_utf8_strlen(passage, -1) > 5000) {
        setResult(label, _("Please read a shorter passage (up to 5,000 characters)."));
        g_free(passage);
        return;
    }
    static constexpr char command[] = R"UTN(
Add-Type -AssemblyName System.Speech
$speaker=New-Object System.Speech.Synthesis.SpeechSynthesizer
$speaker.Speak($env:UTN_SPEAK_TEXT)
)UTN";
    GSubprocessLauncher* launcher = g_subprocess_launcher_new(
            GSubprocessFlags(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE));
    g_subprocess_launcher_setenv(launcher, "UTN_SPEAK_TEXT", passage, TRUE);
    g_free(passage);
    GError* error = nullptr;
    GSubprocess* process = g_subprocess_launcher_spawn(launcher, &error, "powershell.exe",
                                                      "-NoProfile", "-NonInteractive", "-Command", command, nullptr);
    g_object_unref(launcher);
    if (!process) {
        setResult(label, error ? error->message : _("Windows speech could not start."));
        if (error) g_error_free(error);
        return;
    }
    setResult(label, _("Reading aloud using the installed Windows voice…"));
    g_subprocess_communicate_utf8_async(process, nullptr, nullptr, +[](GObject* source, GAsyncResult* result,
                                                                      gpointer userData) {
        GtkWidget* status = GTK_WIDGET(userData);
        gchar* output = nullptr;
        gchar* errors = nullptr;
        GError* error = nullptr;
        const gboolean ok = g_subprocess_communicate_utf8_finish(
                G_SUBPROCESS(source), result, &output, &errors, &error);
        setResult(status, ok && !error && g_subprocess_get_successful(G_SUBPROCESS(source)) ?
                                  _("Finished reading.") :
                                  ((errors && *errors) ? errors : _("Windows speech could not complete.")));
        if (error) g_error_free(error);
        g_free(output);
        g_free(errors);
        g_object_unref(status);
    }, g_object_ref(label));
    g_object_unref(process);
#else
    setResult(label, _("Read Aloud uses Windows speech and is not available on this system."));
#endif
}

void selectImageForOcr(GtkWidget* label, GtkWindow* parent) {
    GtkFileChooserNative* chooser = gtk_file_chooser_native_new(
            _("Choose a scanned worksheet or image"), parent, GTK_FILE_CHOOSER_ACTION_OPEN,
            _("Recognise text"), _("Cancel"));
    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, _("Images (PNG, JPEG, TIFF, BMP)"));
    for (const char* type : {"image/png", "image/jpeg", "image/tiff", "image/bmp"}) {
        gtk_file_filter_add_mime_type(filter, type);
    }
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);
    g_signal_connect(chooser, "response", G_CALLBACK(+[](GtkNativeDialog* dialog, gint response, gpointer data) {
        GtkWidget* label = GTK_WIDGET(data);
        if (response == GTK_RESPONSE_ACCEPT) {
            gchar* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
            if (filename) {
                // Optional OCR engine, kept external until its Windows runtime and models
                // have been measured and made part of UTN's installer.
                GError* error = nullptr;
                GSubprocess* process = g_subprocess_new(
                        GSubprocessFlags(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE),
                        &error, "tesseract.exe", filename, "stdout", "-l", "eng", nullptr);
                if (!process) {
                    setResult(label, _("OCR needs Tesseract installed on PATH. The installer does not bundle it yet."));
                    if (error) g_error_free(error);
                } else {
                    setResult(label, _("Recognising text from the selected image…"));
                    g_subprocess_communicate_utf8_async(
                            process, nullptr, nullptr, readProcessOutput, g_object_ref(label));
                    g_object_unref(process);
                }
                g_free(filename);
            }
        }
        g_object_unref(label);
        g_object_unref(dialog);
    }), g_object_ref(label));
    gtk_native_dialog_show(GTK_NATIVE_DIALOG(chooser));
}

}  // namespace

ToolClassroomWorkflow::ToolClassroomWorkflow(std::string id, Control* control, IconNameHelper icons):
        AbstractToolItem(std::move(id), Category::MISC), control(control), iconName(icons.iconName("utn-workflow")) {}

auto ToolClassroomWorkflow::createItem(bool horizontal) -> xoj::util::WidgetSPtr {
    auto* popover = GTK_POPOVER(gtk_popover_new());
    gtk_widget_add_css_class(GTK_WIDGET(popover), "toolbar");
    auto* tabs = GTK_NOTEBOOK(gtk_notebook_new());
    gtk_popover_set_child(popover, GTK_WIDGET(tabs));

    auto* materials = makePanel(tabs, _("Materials"));
    addCommand(materials, popover, control, _("Add scan or photo as a page"),
               _("Insert a photo of a worksheet after this page. Keep the image file with your notebook."), Command::SCAN);
    addCommand(materials, popover, control, _("Paste picture"), _("Paste the copied picture onto this page"),
               Command::PASTE_IMAGE);
    addCommand(materials, popover, control, _("Insert picture from file"),
               _("Choose a picture, then click the page to place it"), Command::IMAGE);
    addAction(materials, popover, _("Open a PDF to annotate"),
              _("Open a worksheet PDF. You will be asked to save changes to the current notebook first."),
              Action::ANNOTATE_PDF);
    addHint(materials, _("Keep notebooks and their picture files in the same folder. Text in scans cannot be selected."));

    auto* pages = makePanel(tabs, _("Pages"));
    addAction(pages, popover, _("Insert blank page"), _("Add an empty page after this one"),
              Action::NEW_PAGE_AFTER);
    addAction(pages, popover, _("Duplicate page"), _("Copy this page with its annotations. Undo removes the copy."),
              Action::DUPLICATE_PAGE);
    addAction(pages, popover, _("Move page up"), _("Move this page one place earlier"),
              Action::MOVE_PAGE_TOWARDS_BEGINNING);
    addAction(pages, popover, _("Move page down"), _("Move this page one place later"),
              Action::MOVE_PAGE_TOWARDS_END);
    addAction(pages, popover, _("Delete page"), _("Delete this page. Undo restores it."), Action::DELETE_PAGE);
    addAction(pages, popover, _("Save pages as PDF…"),
              _("Choose which pages to save as a separate PDF"), Action::EXPORT_AS);
    addHint(pages,
              _("Use Save pages as PDF to split a worksheet into parts."));

    auto* marking = makePanel(tabs, _("Marking"));
    addCommand(marking, popover, control, _("Switch to marking layout"),
              _("Feedback first, for marking student work. Your document is not changed."), Command::MARKING);
    addCommand(marking, popover, control, _("Switch to teaching layout"), _("The everyday layout for lessons"),
              Command::TEACHING);
    addCommand(marking, popover, control, _("Write a marking comment"),
              _("Click the page to add a red comment box you can edit"), Command::COMMENT);
    addAction(marking, popover, _("Save a marked copy…"),
              _("Save your marking as a new notebook, keeping the original"), Action::SAVE_AS);
    addAction(marking, popover, _("Export marked pages as PDF…"), _("Save the pages with your marking as a PDF for students"),
              Action::EXPORT_AS);
    addHint(marking,
              _("Use Feedback for ticks and ready-made comments."));

    // First classroom-assistance slice: contextual tools live together instead of crowding
    // the fixed drawing rail. These are deliberately separate from the editor's active tool.
    auto* reading = makePanel(tabs, _("Reading & Dictionary"));
    addHint(reading, _("Paste text to read aloud using Windows speech. Voice quality depends on installed voices."));
    GtkWidget* passage = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(passage), GTK_WRAP_WORD_CHAR);
    gtk_widget_set_size_request(passage, 330, 100);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(passage), 8);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(passage), 8);
    gtk_box_append(reading, passage);
    GtkWidget* speechStatus = gtk_label_new(_("Ready to read."));
    gtk_label_set_line_wrap(GTK_LABEL(speechStatus), TRUE);
    GtkWidget* speak = gtk_button_new_with_label(_("Read Aloud"));
    g_object_set_data(G_OBJECT(speak), "utn-speech-input", passage);
    g_object_set_data(G_OBJECT(speak), "utn-speech-status", speechStatus);
    g_signal_connect(speak, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer) {
        readAloud(GTK_WIDGET(g_object_get_data(G_OBJECT(button), "utn-speech-input")),
                  GTK_WIDGET(g_object_get_data(G_OBJECT(button), "utn-speech-status")));
    }), nullptr);
    gtk_box_append(reading, speak);
    gtk_box_append(reading, speechStatus);

    gtk_box_append(reading, gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    addHint(reading, _("Online English dictionary. The queried word is sent to dictionaryapi.dev."));
    GtkWidget* word = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(word), _("Type a word to define"));
    gtk_box_append(reading, word);
    GtkWidget* definition = gtk_label_new(_("Definitions appear here."));
    gtk_label_set_xalign(GTK_LABEL(definition), 0);
    gtk_label_set_line_wrap(GTK_LABEL(definition), TRUE);
    gtk_label_set_selectable(GTK_LABEL(definition), TRUE);
    gtk_widget_set_size_request(definition, 310, -1);
    GtkWidget* lookup = gtk_button_new_with_label(_("Find definition"));
    g_object_set_data(G_OBJECT(lookup), "utn-dictionary-entry", word);
    g_object_set_data(G_OBJECT(lookup), "utn-dictionary-result", definition);
    g_signal_connect(lookup, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer) {
        runDictionary(GTK_WIDGET(g_object_get_data(G_OBJECT(button), "utn-dictionary-entry")),
                      GTK_WIDGET(g_object_get_data(G_OBJECT(button), "utn-dictionary-result")));
    }), nullptr);
    gtk_box_append(reading, lookup);
    GtkWidget* definitionScroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_widget_set_size_request(definitionScroll, 350, 170);
    gtk_container_add(GTK_CONTAINER(definitionScroll), definition);
    gtk_box_append(reading, definitionScroll);

    auto* ocr = makePanel(tabs, _("OCR"));
    addHint(ocr, _("Extract text from a scanned PNG or JPEG. Requires Tesseract OCR on PATH for this developer preview."));
    GtkWidget* ocrResult = gtk_label_new(_("Choose an image to recognise its text."));
    gtk_label_set_xalign(GTK_LABEL(ocrResult), 0);
    gtk_label_set_line_wrap(GTK_LABEL(ocrResult), TRUE);
    gtk_label_set_selectable(GTK_LABEL(ocrResult), TRUE);
    gtk_widget_set_size_request(ocrResult, 310, -1);
    GtkWidget* scan = gtk_button_new_with_label(_("Recognise text in image…"));
    g_object_set_data(G_OBJECT(scan), "utn-ocr-result", ocrResult);
    g_signal_connect(scan, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer data) {
        selectImageForOcr(GTK_WIDGET(g_object_get_data(G_OBJECT(button), "utn-ocr-result")),
                          GTK_WINDOW(data));
    }), control->getGtkWindow());
    gtk_box_append(ocr, scan);
    GtkWidget* ocrScroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_widget_set_size_request(ocrScroll, 350, 230);
    gtk_container_add(GTK_CONTAINER(ocrScroll), ocrResult);
    gtk_box_append(ocr, ocrScroll);


    auto* menu = GTK_MENU_BUTTON(gtk_menu_button_new());
    gtk_widget_set_can_focus(GTK_WIDGET(menu), false);
    gtk_widget_set_tooltip_text(GTK_WIDGET(menu), getToolDisplayName().c_str());
    auto* heading = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5));
    gtk_box_append(heading, getNewToolIcon());
    gtk_box_append(heading, gtk_label_new(_("Classroom")));
    gtk_widget_show_all(GTK_WIDGET(heading));
    gtk_button_set_child(GTK_BUTTON(menu), GTK_WIDGET(heading));
    gtk_menu_button_set_popover(menu, GTK_WIDGET(popover));
    gtk_menu_button_set_direction(menu, horizontal ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT);
    gtk_widget_show_all(GTK_WIDGET(tabs));
    return xoj::util::WidgetSPtr(GTK_WIDGET(menu), xoj::util::adopt);
}

auto ToolClassroomWorkflow::getToolDisplayName() const -> std::string { return _("Classroom"); }
auto ToolClassroomWorkflow::getNewToolIcon() const -> GtkWidget* {
    return gtk_image_new_from_icon_name(iconName.c_str(), GTK_ICON_SIZE_LARGE_TOOLBAR);
}
