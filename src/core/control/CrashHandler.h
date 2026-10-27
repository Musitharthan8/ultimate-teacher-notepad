/*
 * Xournal++
 *
 * Error handler, prints a stacktrace if Xournal++ crashes
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <functional>  // for function

#include "filesystem.h"  // for path

class Document;
void setEmergencyDocument(Document* doc);

/// Installs the handlers. Call from the main thread before anything else runs.
void installCrashHandlers();

/// Writes the document to the recovery file. Normal program flow only: never call it from a signal handler.
void emergencySave();

/// SIGINT/SIGTERM only leave a note for the program. Returns the pending signal number and clears it, or 0.
int takeShutdownRequest();

/// Calls `callback` from the GLib main loop (never from a signal handler) after a SIGINT/SIGTERM
void watchForShutdownRequests(std::function<void(int)> callback);
void unwatchForShutdownRequests();

void setEmergencyFileForTesting(fs::path file);
