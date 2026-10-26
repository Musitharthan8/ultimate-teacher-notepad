#include "RevealActions.h"

#include <functional>
#include <string>
#include <utility>

#include <glib.h>

#include "control/Control.h"
#include "gui/MainWindow.h"
#include "gui/UtnToast.h"
#include "util/i18n.h"

namespace utn {
namespace {
/// 0 = not armed. A monotonic time in microseconds until which a second press confirms "Reveal all".
int64_t revealAllArmedUntil = 0;

void toast(Control* control, const std::string& message, const std::string& actionLabel = {},
           std::function<void()> action = {}) {
    if (auto* window = control->getWindow()) {
        if (auto* t = window->getToast()) {
            t->show(message, actionLabel, std::move(action));
        }
    }
}
}  // namespace

void cancelRevealAllConfirmation() { revealAllArmedUntil = 0; }

void hideAnswersFromUi(Control* control) {
    cancelRevealAllConfirmation();
    const size_t changed = control->setAnswersRevealed(false, true);
    toast(control, changed ? _("Answers are hidden from students") : _("No answers are showing"));
}

RevealOutcome revealAnswersFromUi(Control* control, bool allPages, int64_t now) {
    if (allPages && control->getStudentsMayWatch()) {
        if (revealAllArmedUntil == 0 || now > revealAllArmedUntil) {
            revealAllArmedUntil = now + REVEAL_ALL_CONFIRM_MICROSECONDS;
            toast(control, _("The class may be watching. Choose \"Reveal all answers\" again within 5 seconds to "
                             "show every answer."));
            return RevealOutcome::NeedsConfirmation;
        }
    }
    cancelRevealAllConfirmation();

    const size_t changed = control->setAnswersRevealed(true, allPages);
    if (changed == 0) {
        toast(control, allPages ? _("No hidden answers in this lesson") : _("No hidden answers on this page"));
        return RevealOutcome::NothingToReveal;
    }
    // Hiding is always lesson-wide, so "Hide again" also undoes a reveal of one page
    toast(control, allPages ? _("All answers are now showing") : _("Answers on this page are now showing"),
          _("Hide again"), [control]() { hideAnswersFromUi(control); });
    return RevealOutcome::Revealed;
}

RevealOutcome revealAnswersFromUi(Control* control, bool allPages) {
    return revealAnswersFromUi(control, allPages, g_get_monotonic_time());
}

}  // namespace utn
