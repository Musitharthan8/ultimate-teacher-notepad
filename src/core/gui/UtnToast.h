/*
 * Ultimate Teacher Notepad
 *
 * A short message at the bottom of the canvas, e.g. "Page cleared" with an Undo button.
 * It never takes keyboard focus and disappears by itself.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <functional>
#include <string>

#include <gtk/gtk.h>

class UtnToast {
public:
    explicit UtnToast(GtkOverlay* overlay);
    ~UtnToast();

    UtnToast(const UtnToast&) = delete;
    UtnToast& operator=(const UtnToast&) = delete;

    /**
     * Show @p message for a few seconds. With @p actionLabel, a button runs @p action and closes the toast.
     * A new message replaces the current one.
     */
    void show(const std::string& message, const std::string& actionLabel = {}, std::function<void()> action = {});
    void hide();

    /// The visible message, for tests; empty when hidden
    std::string currentMessage() const;

private:
    static gboolean onTimeout(gpointer self);

    GtkWidget* revealer = nullptr;
    GtkWidget* label = nullptr;
    GtkWidget* button = nullptr;
    std::function<void()> action;
    guint timeout = 0;
};
