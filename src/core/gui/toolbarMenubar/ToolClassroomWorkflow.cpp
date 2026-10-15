/*
 * Ultimate Teacher Notepad
 * Classroom material and marking workflows, based on Xournal++ GPLv2+.
 */
#include "ToolClassroomWorkflow.h"

#include <string>
#include <utility>

#include <gio/gio.h>

#include "AnswerBoxStyles.h"  // for MARKING_COMMENT_STYLE
#include "SignaturePad.h"
#include "control/Control.h"
#include "control/ToolHandler.h"
#include "enums/Action.enum.h"
#include "gui/MainWindow.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"

namespace {
using utn::SignaturePad;
using utn::paintSignature;

void openSignaturePad(Control* control) {
    auto* dialog = gtk_dialog_new_with_buttons(_("Draw signature"), control->getGtkWindow(),
            GTK_DIALOG_MODAL, _("Cancel"), GTK_RESPONSE_CANCEL, _("Clear"), 1, _("Save PNG…"), 2,
            _("Insert"), GTK_RESPONSE_ACCEPT, nullptr);
    SignaturePad pad;
    auto* area = gtk_drawing_area_new();
    atk_object_set_name(gtk_widget_get_accessible(area), _("Signature drawing area"));
    gtk_widget_set_size_request(area, 480, 180);
    gtk_widget_add_events(area, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
    g_signal_connect(area, "draw", G_CALLBACK(+[](GtkWidget*, cairo_t* cr, gpointer data) -> gboolean {
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_paint(cr);
        paintSignature(cr, *static_cast<SignaturePad*>(data));
        return TRUE;
    }), &pad);
    g_signal_connect(area, "button-press-event", G_CALLBACK(+[](GtkWidget* widget, GdkEventButton* e, gpointer data) -> gboolean {
        if (e->button != 1) return FALSE;
        auto& pad = *static_cast<SignaturePad*>(data);
        pad.drawing = true;
        pad.strokes.push_back({{e->x, e->y}});
        gtk_widget_queue_draw(widget);
        return TRUE;
    }), &pad);
    g_signal_connect(area, "motion-notify-event", G_CALLBACK(+[](GtkWidget* widget, GdkEventMotion* e, gpointer data) -> gboolean {
        auto& pad = *static_cast<SignaturePad*>(data);
        if (!pad.drawing) return FALSE;
        pad.strokes.back().push_back({e->x, e->y});
        gtk_widget_queue_draw(widget);
        return TRUE;
    }), &pad);
    g_signal_connect(area, "button-release-event", G_CALLBACK(+[](GtkWidget*, GdkEventButton* e, gpointer data) -> gboolean {
        if (e->button != 1) return FALSE;
        static_cast<SignaturePad*>(data)->drawing = false;
        return TRUE;
    }), &pad);
    auto* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_pack_start(GTK_BOX(content), gtk_label_new(_("Draw with your pen or mouse. Insert adds a movable image.")), FALSE, FALSE, 8);
    gtk_box_pack_start(GTK_BOX(content), area, TRUE, TRUE, 8);
    gtk_widget_show_all(dialog);
    for (;;) {
        const auto response = gtk_dialog_run(GTK_DIALOG(dialog));
        if (response == 1) {
            pad.strokes.clear();
            pad.drawing = false;
            gtk_widget_queue_draw(area);
            continue;
        }
        if ((response == GTK_RESPONSE_ACCEPT || response == 2) && !pad.strokes.empty()) {
            const int width = gtk_widget_get_allocated_width(area);
            const int height = gtk_widget_get_allocated_height(area);
            auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
            auto* cr = cairo_create(surface);
            paintSignature(cr, pad);
            auto* image = gdk_pixbuf_get_from_surface(surface, 0, 0, width, height);
            if (image) {
                if (response == GTK_RESPONSE_ACCEPT) {
                    control->clearSelectionEndText();
                    control->clipboardPasteImage(image);
                } else {
                    auto* chooser = gtk_file_chooser_native_new(_("Save signature"), GTK_WINDOW(dialog),
                            GTK_FILE_CHOOSER_ACTION_SAVE, _("Save"), _("Cancel"));
                    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(chooser), "signature.png");
                    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(chooser), TRUE);
                    if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
                        gchar* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
                        GError* error = nullptr;
                        if (filename && !gdk_pixbuf_save(image, filename, "png", &error, nullptr)) {
                            auto* message = gtk_message_dialog_new(GTK_WINDOW(dialog), GTK_DIALOG_MODAL,
                                    GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", error ? error->message : _("Could not save signature."));
                            gtk_dialog_run(GTK_DIALOG(message));
                            gtk_widget_destroy(message);
                        }
                        g_clear_error(&error);
                        g_free(filename);
                    }
                    g_object_unref(chooser);
                }
                g_object_unref(image);
            }
            cairo_destroy(cr);
            cairo_surface_destroy(surface);
        }
        if (response == 2) continue;
        break;
    }
    gtk_widget_destroy(dialog);
}

enum class Command { SCAN, PASTE_IMAGE, IMAGE, COMMENT, MARKING, TEACHING };

GtkBox* makePanel(GtkNotebook* tabs, const char* label) {
    auto* box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 5));
    gtk_widget_set_margin_start(GTK_WIDGET(box), 10);
    gtk_widget_set_margin_end(GTK_WIDGET(box), 10);
    gtk_widget_set_margin_top(GTK_WIDGET(box), 10);
    gtk_widget_set_margin_bottom(GTK_WIDGET(box), 10);
    auto* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scroll, 400, 420);
    gtk_container_add(GTK_CONTAINER(scroll), GTK_WIDGET(box));
    gtk_notebook_append_page(tabs, scroll, gtk_label_new(label));
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
    gtk_widget_set_focus_on_click(button, false);  // keyboard users can still Tab to it
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
    gtk_widget_set_focus_on_click(button, false);  // keyboard users can still Tab to it
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

void readAloud(GtkWidget* textView, GtkWidget* label, int rate, int volume, int pitch, const char* voice) {
#ifdef _WIN32
    if (g_object_get_data(G_OBJECT(label), "utn-speech-process")) {
        setResult(label, _("Stop the current reading before starting another."));
        return;
    }
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
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Speech
$speaker=New-Object System.Speech.Synthesis.SpeechSynthesizer
try {
  if ($env:UTN_SPEAK_VOICE) { $speaker.SelectVoice($env:UTN_SPEAK_VOICE) }
  $speaker.Rate=[int]$env:UTN_SPEAK_RATE
  $speaker.Volume=[int]$env:UTN_SPEAK_VOLUME
  $text=[Security.SecurityElement]::Escape($env:UTN_SPEAK_TEXT)
  $pitch=([int]$env:UTN_SPEAK_PITCH).ToString('+#;-#;0')+'st'
  $language=$speaker.Voice.Culture.Name
  $speaker.SpeakSsml('<speak version="1.0" xmlns="http://www.w3.org/2001/10/synthesis" xml:lang="'+$language+'"><prosody pitch="'+$pitch+'">'+$text+'</prosody></speak>')
} finally { $speaker.Dispose() }
)UTN";
    GSubprocessLauncher* launcher = g_subprocess_launcher_new(
            GSubprocessFlags(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE));
    g_subprocess_launcher_setenv(launcher, "UTN_SPEAK_TEXT", passage, TRUE);
    g_subprocess_launcher_setenv(launcher, "UTN_SPEAK_RATE", std::to_string(rate).c_str(), TRUE);
    g_subprocess_launcher_setenv(launcher, "UTN_SPEAK_VOLUME", std::to_string(volume).c_str(), TRUE);
    g_subprocess_launcher_setenv(launcher, "UTN_SPEAK_PITCH", std::to_string(pitch).c_str(), TRUE);
    g_subprocess_launcher_setenv(launcher, "UTN_SPEAK_VOICE", voice ? voice : "", TRUE);
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
    g_object_set_data_full(G_OBJECT(label), "utn-speech-process", g_object_ref(process), g_object_unref);
    g_object_set_data(G_OBJECT(label), "utn-speech-stopped", nullptr);
    g_subprocess_communicate_utf8_async(process, nullptr, nullptr, +[](GObject* source, GAsyncResult* result,
                                                                      gpointer userData) {
        GtkWidget* status = GTK_WIDGET(userData);
        gchar* output = nullptr;
        gchar* errors = nullptr;
        GError* error = nullptr;
        const gboolean ok = g_subprocess_communicate_utf8_finish(
                G_SUBPROCESS(source), result, &output, &errors, &error);
        const bool stopped = g_object_get_data(G_OBJECT(status), "utn-speech-stopped") != nullptr;
        setResult(status, stopped ? _("Reading stopped.") : ok && !error && g_subprocess_get_successful(G_SUBPROCESS(source)) ?
                                  _("Finished reading.") :
                                  ((errors && *errors) ? errors : _("Windows speech could not complete.")));
        if (error) g_error_free(error);
        g_free(output);
        g_free(errors);
        g_object_set_data(G_OBJECT(status), "utn-speech-process", nullptr);
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

    auto* signature = newMenuEntry(_("Draw and insert signature…"));
    g_object_set_data(G_OBJECT(signature), "utn-control", control);
    g_signal_connect_object(signature, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer data) {
        auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(button), "utn-control"));
        gtk_popover_popdown(GTK_POPOVER(data));
        openSignaturePad(ctrl);
    }), popover, GConnectFlags(0));
    gtk_box_append(materials, signature);
    addHint(materials, _("Signatures are embedded as images. Save PNG to reuse via Insert picture from file; select an inserted signature to resize, rotate or copy it."));
    addAction(materials, popover, _("Record audio"), _("Start or stop recording with the existing notebook audio tool"), Action::AUDIO_RECORD);
    addAction(materials, popover, _("Play / pause audio"), _("Play or pause notebook audio"), Action::AUDIO_PAUSE_PLAYBACK);
    addAction(materials, popover, _("Stop audio"), _("Stop notebook audio playback"), Action::AUDIO_STOP_PLAYBACK);

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
    gtk_box_append(reading, gtk_label_new(_("Installed voice")));
    GtkWidget* voice = gtk_combo_box_text_new();
    atk_object_set_name(gtk_widget_get_accessible(voice), _("Installed voice"));
    g_signal_connect(voice, "destroy", G_CALLBACK(+[](GtkWidget* widget, gpointer) {
        g_object_set_data(G_OBJECT(widget), "utn-destroyed", GINT_TO_POINTER(1));
    }), nullptr);
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(voice), "", _("System default"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(voice), 0);
    gtk_box_append(reading, voice);
#ifdef _WIN32
    GSubprocess* voices = g_subprocess_new(
            GSubprocessFlags(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE), nullptr,
            "powershell.exe", "-NoProfile", "-NonInteractive", "-Command",
            "[Console]::OutputEncoding=[Text.Encoding]::UTF8; Add-Type -AssemblyName System.Speech; "
            "$s=New-Object System.Speech.Synthesis.SpeechSynthesizer; "
            "try {$s.GetInstalledVoices() | Where-Object Enabled | ForEach-Object {$_.VoiceInfo.Name}} finally {$s.Dispose()}", nullptr);
    if (voices) {
        g_subprocess_communicate_utf8_async(voices, nullptr, nullptr, +[](GObject* source, GAsyncResult* result, gpointer data) {
            gchar* output = nullptr;
            gchar* errors = nullptr;
            GError* error = nullptr;
            if (g_subprocess_communicate_utf8_finish(G_SUBPROCESS(source), result, &output, &errors, &error) &&
                !error && g_subprocess_get_successful(G_SUBPROCESS(source)) && output &&
                !g_object_get_data(G_OBJECT(data), "utn-destroyed")) {
                gchar** names = g_strsplit(output, "\n", -1);
                for (gchar** name = names; *name; ++name) {
                    g_strstrip(*name);
                    if (**name) gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(data), *name, *name);
                }
                g_strfreev(names);
            }
            g_clear_error(&error);
            g_free(output);
            g_free(errors);
            g_object_unref(data);
        }, g_object_ref(voice));
        g_object_unref(voices);
    }
#endif
    gtk_box_append(reading, gtk_label_new(_("Reading speed (−10 slow to 10 fast)")));
    GtkWidget* rate = gtk_spin_button_new_with_range(-10, 10, 1);
    atk_object_set_name(gtk_widget_get_accessible(rate), _("Reading speed"));
    gtk_box_append(reading, rate);
    gtk_box_append(reading, gtk_label_new(_("Volume (%)")));
    GtkWidget* volume = gtk_spin_button_new_with_range(0, 100, 5);
    atk_object_set_name(gtk_widget_get_accessible(volume), _("Reading volume"));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(volume), 100);
    gtk_box_append(reading, volume);
    gtk_box_append(reading, gtk_label_new(_("Pitch (semitones; voice support varies)")));
    GtkWidget* pitch = gtk_spin_button_new_with_range(-10, 10, 1);
    atk_object_set_name(gtk_widget_get_accessible(pitch), _("Reading pitch"));
    gtk_box_append(reading, pitch);
    g_object_set_data(G_OBJECT(speak), "utn-speech-pitch", pitch);
    g_object_set_data(G_OBJECT(speak), "utn-speech-voice", voice);
    g_object_set_data(G_OBJECT(speak), "utn-speech-rate", rate);
    g_object_set_data(G_OBJECT(speak), "utn-speech-volume", volume);
    g_object_set_data(G_OBJECT(speak), "utn-speech-input", passage);
    g_object_set_data(G_OBJECT(speak), "utn-speech-status", speechStatus);
    g_signal_connect(speak, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer) {
        readAloud(GTK_WIDGET(g_object_get_data(G_OBJECT(button), "utn-speech-input")),
                  GTK_WIDGET(g_object_get_data(G_OBJECT(button), "utn-speech-status")),
                  gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(button), "utn-speech-rate"))),
                  gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(button), "utn-speech-volume"))),
                  gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(button), "utn-speech-pitch"))),
                  gtk_combo_box_get_active_id(GTK_COMBO_BOX(g_object_get_data(G_OBJECT(button), "utn-speech-voice"))));
    }), nullptr);
    gtk_box_append(reading, speak);
    GtkWidget* stop = gtk_button_new_with_label(_("Stop reading"));
    g_signal_connect_object(stop, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
        auto* status = GTK_WIDGET(data);
        auto* process = static_cast<GSubprocess*>(g_object_get_data(G_OBJECT(status), "utn-speech-process"));
        if (process) {
            g_object_set_data(G_OBJECT(status), "utn-speech-stopped", GINT_TO_POINTER(1));
            g_subprocess_force_exit(process);
        }
    }), speechStatus, GConnectFlags(0));
    gtk_box_append(reading, stop);
    g_signal_connect_object(passage, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer data) {
        auto* process = static_cast<GSubprocess*>(g_object_get_data(G_OBJECT(data), "utn-speech-process"));
        if (process) {
            g_object_set_data(G_OBJECT(data), "utn-speech-stopped", GINT_TO_POINTER(1));
            g_subprocess_force_exit(process);
        }
    }), speechStatus, GConnectFlags(0));
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
    gtk_widget_set_focus_on_click(GTK_WIDGET(menu), false);  // keyboard users can still Tab to it
    gtk_widget_set_tooltip_text(GTK_WIDGET(menu), getToolDisplayName().c_str());
    auto* heading = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5));
    gtk_box_append(heading, getNewToolIcon());
    if (horizontal) gtk_box_append(heading, gtk_label_new(_("Classroom")));
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
