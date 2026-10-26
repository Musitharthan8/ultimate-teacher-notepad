/*
 * Ultimate Teacher Notepad
 *
 * A saved partially erased stroke whose expensive raster fallback is produced after the Document lock was released.
 *
 * SaveHandler::prepareSave runs under the shared Document lock, which blocks the exclusive lock that pen-up needs. It
 * therefore only snapshots the stroke (a copy of its points and erasures); the raster is drawn later by
 * SaveHandler::resolveDeferredStrokes(), before any output file is touched.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <memory>

#include "model/Stroke.h"

#include "XmlNode.h"

class OutputStream;

class XmlDeferredStrokeNode final: public XmlNode {
public:
    XmlDeferredStrokeNode(StringUtils::StaticStringView tag, std::unique_ptr<Stroke> snapshot):
            XmlNode(tag), snapshot(std::move(snapshot)) {}

    const Stroke& getSnapshot() const { return *snapshot; }

    /// The node that is written in this one's place; nullptr until resolved. A resolved empty node writes nothing.
    void setResolved(std::unique_ptr<XmlNode> node, bool resolvedAsNothing = false) {
        resolved = std::move(node);
        isResolved = resolved != nullptr || resolvedAsNothing;
    }
    bool hasBeenResolved() const { return isResolved; }

    void writeOut(OutputStream* out) override {
        if (resolved) {
            resolved->writeOut(out);
        }
    }

private:
    std::unique_ptr<Stroke> snapshot;
    std::unique_ptr<XmlNode> resolved;
    bool isResolved = false;
};
