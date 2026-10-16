/*
 * Ultimate Teacher Notepad
 *
 * Audit 16 October 2026: choosing Rectangle left Freehand active and duplicated the Pen properties. Building the
 * radio group "clicked" the previously active Freehand radio, whose handler switched the Pen back to Freehand and
 * rebuilt the panel mid-construction. These tests drive the real GTK radio group.
 *
 * Based on Xournal++ GPLv2+
 */

#include <algorithm>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <gtk/gtk.h>

#include "gui/toolbarMenubar/ShapePicker.h"

namespace {

struct Recorder {
    std::vector<DrawingType> chosen;
};

void record(DrawingType type, gpointer data) { static_cast<Recorder*>(data)->chosen.push_back(type); }

std::vector<GtkWidget*> buttonsOf(GtkWidget* picker) {
    std::vector<GtkWidget*> buttons;
    for (const auto& choice: utn::SHAPE_CHOICES) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(picker));
        for (GList* it = children; it; it = it->next) {
            if (utn::shapePickerButtonType(GTK_WIDGET(it->data)) == choice.type) {
                buttons.push_back(GTK_WIDGET(it->data));
            }
        }
        g_list_free(children);
    }
    return buttons;
}

std::vector<DrawingType> activeTypes(GtkWidget* picker) {
    std::vector<DrawingType> active;
    for (GtkWidget* button: buttonsOf(picker)) {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button))) {
            active.push_back(utn::shapePickerButtonType(button));
        }
    }
    return active;
}

class UtnShapePicker: public ::testing::TestWithParam<utn::ShapeChoice> {
protected:
    void SetUp() override {
        if (!gtk_init_check(nullptr, nullptr)) {
            GTEST_SKIP() << "No display available";
        }
    }
};

}  // namespace

TEST_P(UtnShapePicker, buildingShowsCurrentShapeWithoutReportingAChoice) {
    Recorder recorder;
    GtkWidget* picker = GTK_WIDGET(g_object_ref_sink(utn::createShapePicker(GetParam().type, record, &recorder)));

    EXPECT_TRUE(recorder.chosen.empty()) << "building the picker must not change the tool";
    EXPECT_EQ(activeTypes(picker), std::vector<DrawingType>{GetParam().type});
    g_object_unref(picker);
}

TEST_P(UtnShapePicker, choosingEachOtherShapeReportsOnlyThatShape) {
    for (const auto& target: utn::SHAPE_CHOICES) {
        if (target.type == GetParam().type) {
            continue;
        }
        Recorder recorder;
        GtkWidget* picker = GTK_WIDGET(g_object_ref_sink(utn::createShapePicker(GetParam().type, record, &recorder)));
        for (GtkWidget* button: buttonsOf(picker)) {
            if (utn::shapePickerButtonType(button) == target.type) {
                gtk_button_clicked(GTK_BUTTON(button));  // what a mouse or touch release does
            }
        }
        // The old handler reported Freehand (the radio being switched off) and never the requested shape
        EXPECT_EQ(recorder.chosen, std::vector<DrawingType>{target.type}) << target.label;
        EXPECT_EQ(activeTypes(picker), std::vector<DrawingType>{target.type}) << target.label;
        g_object_unref(picker);
    }
}

TEST_P(UtnShapePicker, clickingTheActiveShapeAgainChangesNothing) {
    Recorder recorder;
    GtkWidget* picker = GTK_WIDGET(g_object_ref_sink(utn::createShapePicker(GetParam().type, record, &recorder)));
    for (GtkWidget* button: buttonsOf(picker)) {
        if (utn::shapePickerButtonType(button) == GetParam().type) {
            gtk_button_clicked(GTK_BUTTON(button));
        }
    }
    EXPECT_TRUE(recorder.chosen.empty());
    EXPECT_EQ(activeTypes(picker), std::vector<DrawingType>{GetParam().type});
    g_object_unref(picker);
}

INSTANTIATE_TEST_SUITE_P(EveryShape, UtnShapePicker, ::testing::ValuesIn(utn::SHAPE_CHOICES),
                         [](const ::testing::TestParamInfo<utn::ShapeChoice>& info) {
                             std::string name = info.param.label;
                             name.erase(std::remove(name.begin(), name.end(), ' '), name.end());
                             return name;
                         });

// The GTK behaviour behind the defect: switching a radio on emits "clicked" on the radio it replaces, so any handler
// connected to an earlier radio runs while a later one is being set up.
TEST(UtnShapePickerFallback, gtkClicksThePreviousRadioWhileAGroupIsBuilt) {
    if (!gtk_init_check(nullptr, nullptr)) {
        GTEST_SKIP() << "No display available";
    }
    GtkWidget* freehand = GTK_WIDGET(g_object_ref_sink(gtk_radio_button_new_with_label(nullptr, "Freehand")));
    int freehandClicks = 0;
    g_signal_connect(freehand, "clicked", G_CALLBACK(+[](GtkButton*, gpointer n) { ++*static_cast<int*>(n); }),
                     &freehandClicks);
    GtkWidget* rectangle = GTK_WIDGET(
            g_object_ref_sink(gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(freehand), "Rectangle")));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rectangle), true);
    EXPECT_EQ(freehandClicks, 1);
    EXPECT_FALSE(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(freehand)));
    g_object_unref(rectangle);
    g_object_unref(freehand);
}

TEST(UtnShapePickerFallback, unknownCurrentShapeShowsFreehand) {
    if (!gtk_init_check(nullptr, nullptr)) {
        GTEST_SKIP() << "No display available";
    }
    Recorder recorder;
    GtkWidget* picker = GTK_WIDGET(g_object_ref_sink(utn::createShapePicker(DRAWING_TYPE_SPLINE, record, &recorder)));
    EXPECT_TRUE(recorder.chosen.empty());
    EXPECT_EQ(activeTypes(picker), std::vector<DrawingType>{DRAWING_TYPE_DEFAULT});
    g_object_unref(picker);
}
