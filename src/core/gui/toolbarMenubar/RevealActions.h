/*
 * Ultimate Teacher Notepad
 *
 * Revealing and hiding answers from the Hide & Reveal menu and the Answers context bar, with the safeguards that
 * belong to a classroom: no modal dialogs, but a way back after every reveal and a second press before everything
 * is shown to a class that may be watching.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <cstdint>

class Control;

namespace utn {

/// How long after the first press of "Reveal all answers" the second press counts
constexpr int64_t REVEAL_ALL_CONFIRM_MICROSECONDS = 5 * 1000 * 1000;

enum class RevealOutcome {
    NothingToReveal,  ///< no hidden answers in scope
    Revealed,
    NeedsConfirmation  ///< "Reveal all" while the class may be watching: press again to go ahead
};

/**
 * Reveal the hidden answers of the current page, or of the whole lesson. Revealing everything while students may be
 * watching only arms the action; repeating it within REVEAL_ALL_CONFIRM_MICROSECONDS carries it out. A toast offers
 * "Hide again" after every reveal.
 * @param nowMicroseconds monotonic time; a parameter so tests do not have to wait
 */
RevealOutcome revealAnswersFromUi(Control* control, bool allPages, int64_t nowMicroseconds);
RevealOutcome revealAnswersFromUi(Control* control, bool allPages);

/// Hide every answers layer in the lesson from students
void hideAnswersFromUi(Control* control);

/// Forget a pending "Reveal all" confirmation (tests, and whenever the situation changes)
void cancelRevealAllConfirmation();

}  // namespace utn
