/*
 * Xournal++
 *
 * Saves a document
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <memory>  // for unique_ptr
#include <optional>
#include <string>  // for string
#include <vector>  // for vector

#include "control/xml/XmlNode.h"    // for XmlNode
#include "model/BackgroundImage.h"  // for BackgroundImage
#include "model/ExportAudience.h"   // for ExportAudience
#include "model/PageRef.h"          // for PageRef
#include "util/Color.h"             // for Color

#include "filesystem.h"  // for path

class XmlPointNode;
class XmlDeferredStrokeNode;
class ProgressListener;
class AudioContent;
class Document;
class Layer;
class OutputStream;
class Stroke;

class SaveHandler {
public:
    SaveHandler();
    virtual ~SaveHandler() = default;

public:
    /// Prepare an XML tree corresponding to the document - Needs read-only access to the Document
    void prepareSave(const Document* doc, const fs::path& target);
    /// Writes the XML to the given path. Does not access the Document instance
    void saveTo(const fs::path& filepath, ProgressListener* listener = nullptr);
    /**
     * Writes the XML to the given stream. Does not access the Document instance with the following exception:
     * attached background images are written to disk. This is safe as long as we do not modify the background images
     * anywhere
     */
    void saveTo(OutputStream* out, const fs::path& filepath, ProgressListener* listener = nullptr);
    /**
     * Writes the whole file to a uniquely named temporary file next to `filepath` (created exclusively, so concurrent
     * saves never share one) and then replaces `filepath` with it using Util::replaceFileKeepingOld, which never removes
     * the destination first. A failure (including an annotation that cannot be rendered for a student copy, a locked
     * destination, a missing folder or any exception) leaves an existing `filepath` untouched, removes only this save's
     * temporary file and sets the error message. Does not access the Document instance.
     * If a save to the same destination that started later has already replaced it, this one does not overwrite that
     * newer file: it succeeds without replacing and wasSupersededByNewerSave() is true.
     */
    void saveAtomically(const fs::path& filepath, ProgressListener* listener = nullptr);
    bool wasSupersededByNewerSave() const { return superseded; }

    using ReplaceStep = bool (*)(const fs::path& from, const fs::path& to, std::string& error);
    /// Replaces the final replacement step of saveAtomically (nullptr restores the default). For tests.
    static void setReplaceStepForTesting(ReplaceStep step);
    /// Update document information. Requires write access to the Document.
    void updateDocumentInfo(Document* doc);

    /**
     * Draws the raster fallbacks of partially erased strokes. prepareSave only snapshots those strokes so that the
     * shared Document lock is not held for the drawing. saveTo calls this before it touches any output file, so it
     * never needs the Document. Safe to call more than once.
     */
    void resolveDeferredStrokes();

    const std::string& getErrorMessage();

    /// Problems that did not stop the save but changed what was written (see resolveDeferredStrokes)
    const std::vector<std::string>& getWarnings() const { return warnings; }

    /**
     * UTN: who the written file is for. Teacher (the default) writes the complete private lesson, which is what the
     * native save needs. Student writes only what students may see (see utn::visibleToStudents) and omits the page
     * thumbnail, which is rendered from every visible layer, private names and editable erased-ink metadata.
     * Student compatibility export rejects PDF/image backgrounds and LaTeX before writing any file or asset: the
     * original payloads can contain undisplayed pages, source comments and metadata. Use rendered student PNG.
     */
    void setExportAudience(ExportAudience audience) { this->exportAudience = audience; }

protected:
    static std::string getColorStr(Color c, unsigned char alpha = 0xff);

    /// Whether a layer is written for the current export audience
    bool includeLayer(const Layer& layer) const;

    virtual void visitPage(XmlNode* root, ConstPageRef p, const Document* doc, int id, const fs::path& target);
    virtual void visitLayer(XmlNode* page, const Layer* l);
    virtual void visitStroke(XmlPointNode* stroke, const Stroke* s);

    /**
     * Export the fill attributes
     */
    virtual void visitStrokeExtended(XmlPointNode* stroke, const Stroke* s);

    virtual void writeHeader();
    virtual void writeSolidBackground(XmlNode* background, ConstPageRef p);
    virtual void writeAudio(XmlNode* node, const AudioContent& audio);
    virtual void writeBackgroundName(XmlNode* background, ConstPageRef p);

protected:
    std::unique_ptr<XmlNode> root{};
    ExportAudience exportAudience = ExportAudience::Teacher;
    bool firstPdfPageVisited;
    int attachBgId;

    std::string errorMessage;
    bool superseded = false;
    std::vector<std::string> warnings;

    /// Partially erased strokes still waiting for their raster fallback. Owned by the tree below `root`.
    std::vector<XmlDeferredStrokeNode*> deferredStrokes;

    struct ImageInfo {
        BackgroundImage image;  ///< This is a wrapped shared pointer
        std::optional<fs::path> newPath;
        int newId;
    };
    std::vector<ImageInfo> backgroundImages{};
};
