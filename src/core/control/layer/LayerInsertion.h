/*
 * Ultimate Teacher Notepad
 *
 * Where a new layer goes. Kept free of GTK and document state so it can be unit tested.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include "model/Layer.h"  // for Layer::Index

namespace xoj::layer {

/**
 * Vector index at which a new layer is inserted on a page.
 *
 * Layer IDs: 0 is the background, and ID n is the layer at vector index n-1.
 * A new layer sits directly above the selected layer, or directly below it when belowSelected is set.
 * When the background is selected, or the page has no layers, the new layer goes on top of the page.
 * Asking for "below" the background places the layer at the bottom, which is the closest valid position.
 */
inline Layer::Index insertIndexForNewLayer(Layer::Index selectedId, Layer::Index layerCount, bool belowSelected) {
    if (belowSelected) {
        return selectedId > 0 ? selectedId - 1 : 0;
    }
    if (selectedId == 0) {
        return layerCount;
    }
    return selectedId;
}

}  // namespace xoj::layer
