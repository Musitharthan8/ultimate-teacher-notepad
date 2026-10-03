/*
 * Ultimate Teacher Notepad
 *
 * Clean student-facing preview window
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <string>

#include <gtk/gtk.h>

#include "model/PageRef.h"

#include "AbstractToolItem.h"

class Control;

class ToolStudentView: public AbstractToolItem {
public:
    ToolStudentView(std::string id, Control* control);
    ~ToolStudentView() override;

    xoj::util::WidgetSPtr createItem(bool horizontal) override;

    std::string getToolDisplayName() const override;
    GtkWidget* getNewToolIcon() const override;

private:
    void showStudentView();
    void closeStudentView();
    void setFrozen(bool frozen);
    void setBlanked(bool blanked);
    void setStudentFullscreen(bool enabled);

    static gboolean drawStudentView(GtkWidget* widget, cairo_t* cr, gpointer data);
    static gboolean refreshStudentView(gpointer data);
    static gboolean studentViewKeyPressed(GtkWidget* widget, GdkEventKey* event, gpointer data);
    static void studentViewDestroyed(GtkWidget* widget, gpointer data);

private:
    Control* control;
    GtkWidget* window = nullptr;
    GtkWidget* drawingArea = nullptr;
    guint refreshTimer = 0;
    bool fullscreen = false;
    bool frozen = false;
    bool blanked = false;
    PageRef frozenPage;
};
