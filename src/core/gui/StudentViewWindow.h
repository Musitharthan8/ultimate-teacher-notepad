/*
 * Ultimate Teacher Notepad
 *
 * Student-facing projector window
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <cstddef>
#include <string>

#include <gtk/gtk.h>

#include "view/DocumentView.h"

class Control;

class StudentViewWindow {
public:
    explicit StudentViewWindow(Control* control);
    ~StudentViewWindow();

    void show();
    void hide();
    void setFullscreen(bool enabled);
    void refresh();

    bool isVisible() const;

private:
    void draw(cairo_t* cr, int width, int height);
    static bool isTeacherOnlyLayerName(const std::string& name);

private:
    Control* control;
    GtkWindow* window = nullptr;
    GtkWidget* drawingArea = nullptr;
    guint refreshTimer = 0;
    DocumentView documentView;
};
