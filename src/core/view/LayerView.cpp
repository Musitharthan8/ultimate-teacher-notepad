#include "LayerView.h"

#include <algorithm>      // for max
#include <cstdint>        // for uint64_t
#include <memory>         // for unique_ptr
#include <unordered_set>  // for unordered_set
#include <vector>         // for vector

#include <cairo.h>  // for cairo_clip_extents, cairo_rectangle
#include <glib.h>   // for g_message

#include "model/Element.h"  // for Element
#include "model/Layer.h"    // for Layer
#include "model/Stroke.h"   // for Stroke
#include "util/Color.h"     // for cairo_set_source_rgbi

#include "DebugShowRepaintBounds.h"
#include "StrokeView.h"  // for IF_DEBUG_REPAINT
#include "View.h"                    // for Context, ElementView

using namespace xoj::view;

LayerView::LayerView(const Layer* layer): layer(layer) {}

const Layer* LayerView::getLayer() const { return layer; }

void LayerView::draw(const Context& ctx) const {
    IF_DEBUG_REPAINT(int drawn = 0; int notDrawn = 0;);

    // Get the bounds of the mask, in page coordinates
    double minX;
    double maxX;
    double minY;
    double maxY;
    cairo_clip_extents(ctx.cr, &minX, &minY, &maxX, &maxY);

    std::unordered_set<uint64_t> drawnHighlighterGroups;

    for (auto const& e: layer->getElementsView()) {

        IF_DEBUG_REPAINT({
            auto cr = ctx.cr;
            cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
            cairo_set_source_rgb(cr, 0, 1, 0);
            cairo_set_line_width(cr, 1);
            cairo_rectangle(cr, e->getX(), e->getY(), e->getElementWidth(), e->getElementHeight());
            cairo_stroke(cr);
        });

        const auto* stroke = e->getType() == ELEMENT_STROKE ? dynamic_cast<const Stroke*>(e) : nullptr;
        const uint64_t groupId = stroke ? stroke->getHighlighterGroupId() : 0;

        if (!ctx.noColor && stroke && stroke->getToolType() == StrokeTool::HIGHLIGHTER && groupId != 0 &&
            stroke->getErasable() == nullptr) {
            if (drawnHighlighterGroups.contains(groupId)) {
                continue;
            }
            drawnHighlighterGroups.insert(groupId);

            std::vector<const Stroke*> group;
            bool groupIntersectsClip = false;
            bool styleMatches = true;

            for (const auto* candidateElement: layer->getElementsView()) {
                if (candidateElement->getType() != ELEMENT_STROKE) {
                    continue;
                }

                const auto* candidate = dynamic_cast<const Stroke*>(candidateElement);
                if (!candidate || candidate->getHighlighterGroupId() != groupId ||
                    candidate->getToolType() != StrokeTool::HIGHLIGHTER || candidate->getErasable() != nullptr) {
                    continue;
                }

                styleMatches &= candidate->getColor() == stroke->getColor() &&
                                candidate->getWidth() == stroke->getWidth() &&
                                candidate->getStrokeCapStyle() == stroke->getStrokeCapStyle();
                group.emplace_back(candidate);
                groupIntersectsClip |= candidate->intersectsArea(minX, minY, maxX - minX, maxY - minY);
            }

            if (styleMatches && group.size() > 1 && groupIntersectsClip) {
                cairo_save(ctx.cr);
                cairo_push_group_with_content(ctx.cr, CAIRO_CONTENT_ALPHA);

                Context maskContext{ctx.cr, NORMAL_NON_AUDIO, ctx.showCurrentEdition, COLORBLIND};
                for (const auto* fragment: group) {
                    StrokeView(fragment).draw(maskContext);
                }

                cairo_pattern_t* mask = cairo_pop_group(ctx.cr);

                double alpha = StrokeView::OPACITY_HIGHLIGHTER;
                if (ctx.fadeOutNonAudio && stroke->getAudioFilename().empty()) {
                    alpha *= OPACITY_NO_AUDIO;
                    alpha = std::max(StrokeView::MINIMAL_ALPHA, alpha);
                }

                cairo_set_operator(ctx.cr, CAIRO_OPERATOR_MULTIPLY);
                Util::cairo_set_source_rgbi(ctx.cr, stroke->getColor(), alpha);
                cairo_mask(ctx.cr, mask);

                cairo_pattern_destroy(mask);
                cairo_restore(ctx.cr);

                IF_DEBUG_REPAINT(drawn += static_cast<int>(group.size()););
                continue;
            }
        }

        if (e->intersectsArea(minX, minY, maxX - minX, maxY - minY)) {
            ElementView::createFromElement(e)->draw(ctx);
            IF_DEBUG_REPAINT(drawn++;);
        }
        IF_DEBUG_REPAINT(else { notDrawn++; });
    }
    IF_DEBUG_REPAINT(g_message("DBG:LayerView::draw: draw %i / not draw %i", drawn, notDrawn););
}
