/*
 * Ultimate Teacher Notepad
 *
 * UndoRedoHandler listeners: the Undo History panel adds and removes itself while the handler tells its listeners, and
 * reads the step counts the panel's jump checks after every step. Free of GTK widgets.
 *
 * Based on Xournal++ GPLv2+
 */

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "undo/UndoAction.h"
#include "undo/UndoRedoHandler.h"

namespace {
class FakeStep final: public UndoAction {
public:
    explicit FakeStep(std::string text): UndoAction("FakeStep"), text(std::move(text)) {}
    bool undo(Control*) override { return true; }
    bool redo(Control*) override { return true; }
    std::string getText() override { return text; }

private:
    std::string text;
};

struct CountingListener: UndoRedoListener {
    int changed = 0;
    void undoRedoChanged() override { ++changed; }
    void undoRedoPageChanged(PageRef) override {}
};

/// Runs @p whenTold the first time it is told
struct ActingListener: CountingListener {
    std::function<void()> whenTold;
    void undoRedoChanged() override {
        ++changed;
        if (auto act = std::move(whenTold)) {
            whenTold = nullptr;
            act();
        }
    }
};
}  // namespace

TEST(UndoRedoHandlerListeners, aListenerRemovedWhileTheyAreToldIsNotToldAgainAndTheOthersStillAre) {
    UndoRedoHandler handler(nullptr);
    ActingListener first;
    CountingListener second;
    first.whenTold = [&] { handler.removeUndoRedoListener(&first); };
    handler.addUndoRedoListener(&first);
    handler.addUndoRedoListener(&second);

    handler.fireUpdateUndoRedoButtons({});
    EXPECT_EQ(first.changed, 1);
    EXPECT_EQ(second.changed, 1) << "the listener after the removed one is still told";

    handler.fireUpdateUndoRedoButtons({});
    EXPECT_EQ(first.changed, 1) << "removed";
    EXPECT_EQ(second.changed, 2);
}

TEST(UndoRedoHandlerListeners, aListenerRemovingALaterOneBeforeItsTurnSparesItTheCall) {
    UndoRedoHandler handler(nullptr);
    ActingListener first;
    CountingListener second;
    CountingListener third;
    first.whenTold = [&] { handler.removeUndoRedoListener(&second); };
    handler.addUndoRedoListener(&first);
    handler.addUndoRedoListener(&second);
    handler.addUndoRedoListener(&third);

    handler.fireUpdateUndoRedoButtons({});
    EXPECT_EQ(second.changed, 0) << "removed before its turn: never called, so it may be destroyed right away";
    EXPECT_EQ(third.changed, 1);
}

TEST(UndoRedoHandlerListeners, aListenerAddedWhileTheyAreToldHearsOfTheNextChangeAndTheOthersAreNotDisturbed) {
    UndoRedoHandler handler(nullptr);
    ActingListener first;
    CountingListener added;
    CountingListener last;
    first.whenTold = [&] { handler.addUndoRedoListener(&added); };  // may move the listeners in memory
    handler.addUndoRedoListener(&first);
    handler.addUndoRedoListener(&last);

    handler.fireUpdateUndoRedoButtons({});
    EXPECT_EQ(first.changed, 1);
    EXPECT_EQ(last.changed, 1);

    handler.fireUpdateUndoRedoButtons({});
    EXPECT_EQ(added.changed, 1) << "not told in the round that added it, told in the next";
    EXPECT_EQ(first.changed, 2);
    EXPECT_EQ(last.changed, 2);
}

TEST(UndoRedoHandlerListeners, removingOutsideATellingDropsTheListenerAndUnknownOnesAreIgnored) {
    UndoRedoHandler handler(nullptr);
    CountingListener gone;
    CountingListener kept;
    CountingListener never;
    handler.addUndoRedoListener(&gone);
    handler.addUndoRedoListener(&kept);
    handler.removeUndoRedoListener(&gone);
    handler.removeUndoRedoListener(&never);
    handler.fireUpdateUndoRedoButtons({});
    EXPECT_EQ(gone.changed, 0);
    EXPECT_EQ(kept.changed, 1);
}

TEST(UndoRedoHandlerListeners, aNestedTellingKeepsTheBlankedSlotsUntilTheOutermostEnds) {
    UndoRedoHandler handler(nullptr);
    ActingListener first;
    CountingListener second;
    first.whenTold = [&] {
        handler.removeUndoRedoListener(&second);
        handler.fireUpdateUndoRedoButtons({});  // as a listener that closes a document would
    };
    handler.addUndoRedoListener(&first);
    handler.addUndoRedoListener(&second);
    handler.fireUpdateUndoRedoButtons({});
    EXPECT_EQ(second.changed, 0);
    handler.fireUpdateUndoRedoButtons({});
    EXPECT_EQ(first.changed, 3);
    EXPECT_EQ(second.changed, 0);
}

TEST(UndoRedoHandlerListeners, theCountsAndTextsFollowUndoAndRedoOneStepAtATime) {
    UndoRedoHandler handler(nullptr);
    handler.addUndoAction(std::make_unique<FakeStep>("A"));
    handler.addUndoAction(std::make_unique<FakeStep>("B"));
    handler.addUndoAction(std::make_unique<FakeStep>("C"));
    EXPECT_EQ(handler.undoCount(), 3U);
    EXPECT_EQ(handler.redoCount(), 0U);

    handler.undo();
    EXPECT_EQ(handler.undoCount(), 2U);
    EXPECT_EQ(handler.redoCount(), 1U);
    EXPECT_TRUE(handler.lastStepSucceeded());
    EXPECT_EQ(handler.undoTexts(10), (std::vector<std::string>{"B", "A"}));
    EXPECT_EQ(handler.redoTexts(10), (std::vector<std::string>{"C"}));
    EXPECT_EQ(handler.undoTexts(1), (std::vector<std::string>{"B"}));

    handler.redo();
    EXPECT_EQ(handler.undoCount(), 3U);
    EXPECT_EQ(handler.redoCount(), 0U);

    handler.undo();
    handler.addUndoAction(std::make_unique<FakeStep>("D"));
    EXPECT_EQ(handler.redoCount(), 0U) << "a new step drops what Redo could bring back";
}

TEST(UndoRedoHandlerListeners, aListenerThatAddsAnotherEveryTimeCannotKeepTheTellingGoing) {
    UndoRedoHandler handler(nullptr);
    std::vector<std::unique_ptr<CountingListener>> made;
    struct Breeder: CountingListener {
        UndoRedoHandler* handler = nullptr;
        std::vector<std::unique_ptr<CountingListener>>* made = nullptr;
        void undoRedoChanged() override {
            ++changed;
            made->push_back(std::make_unique<CountingListener>());
            handler->addUndoRedoListener(made->back().get());
        }
    } breeder;
    breeder.handler = &handler;
    breeder.made = &made;
    handler.addUndoRedoListener(&breeder);
    handler.fireUpdateUndoRedoButtons({});
    EXPECT_EQ(breeder.changed, 1);
    EXPECT_EQ(made.size(), 1U) << "the listener it made was not told in the same round, so no more were made";
}

TEST(UndoRedoHandlerListeners, registeringTwiceStillTellsOnceAndOneRemovalDropsIt) {
    UndoRedoHandler handler(nullptr);
    CountingListener once;
    handler.addUndoRedoListener(&once);
    handler.addUndoRedoListener(&once);
    handler.addUndoRedoListener(nullptr);
    handler.fireUpdateUndoRedoButtons({});
    EXPECT_EQ(once.changed, 1);
    handler.removeUndoRedoListener(&once);
    handler.fireUpdateUndoRedoButtons({});
    EXPECT_EQ(once.changed, 1);
}

TEST(UndoRedoHandlerListeners, aListenerRemovedAndAddedAgainWhileToldIsToldAgainOnlyFromTheNextChange) {
    UndoRedoHandler handler(nullptr);
    ActingListener first;
    CountingListener second;
    first.whenTold = [&] {
        handler.removeUndoRedoListener(&second);
        handler.addUndoRedoListener(&second);
    };
    handler.addUndoRedoListener(&first);
    handler.addUndoRedoListener(&second);
    handler.fireUpdateUndoRedoButtons({});
    EXPECT_EQ(second.changed, 0) << "its old slot was blanked before its turn, the new one is past the snapshot";
    handler.fireUpdateUndoRedoButtons({});
    EXPECT_EQ(second.changed, 1);
}

TEST(UndoRedoHandlerListeners, theRevisionCountsEveryChangeSoAReplacedStepIsNotTheSameHistory) {
    UndoRedoHandler handler(nullptr);
    const auto start = handler.revision();
    handler.addUndoAction(std::make_unique<FakeStep>("A"));
    const auto afterAdd = handler.revision();
    EXPECT_NE(afterAdd, start);
    handler.undo();
    const auto afterUndo = handler.revision();
    EXPECT_NE(afterUndo, afterAdd);
    handler.redo();
    const auto afterRedo = handler.revision();
    EXPECT_NE(afterRedo, afterUndo);
    handler.undo();
    EXPECT_NE(handler.revision(), afterRedo);

    // Same counts, different history: one step undone, then a new one in its place
    handler.clearContents();
    handler.addUndoAction(std::make_unique<FakeStep>("A"));
    handler.addUndoAction(std::make_unique<FakeStep>("B"));
    const auto undoCount = handler.undoCount();
    const auto redoCount = handler.redoCount();
    const auto before = handler.revision();
    handler.undo();
    handler.addUndoAction(std::make_unique<FakeStep>("C"));
    EXPECT_EQ(handler.undoCount(), undoCount);
    EXPECT_EQ(handler.redoCount(), redoCount);
    EXPECT_NE(handler.revision(), before) << "equal counts, another history";

    const auto beforeClear = handler.revision();
    handler.clearContents();
    EXPECT_NE(handler.revision(), beforeClear);
    handler.addUndoAction(nullptr);
    EXPECT_EQ(handler.revision(), beforeClear + 1) << "adding nothing changes nothing";
    handler.undo();
    EXPECT_EQ(handler.revision(), beforeClear + 1) << "undo with nothing to undo changes nothing";
}

TEST(UndoRedoHandlerListeners, theAutosavePointFollowsTheNewestStepAndSurvivesUndoAndRedoOnly) {
    UndoRedoHandler handler(nullptr);
    EXPECT_FALSE(handler.isChangedAutosave());
    handler.addUndoAction(std::make_unique<FakeStep>("A"));
    EXPECT_TRUE(handler.isChangedAutosave());
    handler.documentAutosaved();
    EXPECT_FALSE(handler.isChangedAutosave());
    EXPECT_TRUE(handler.isChanged()) << "autosaving is not saving";
    handler.addUndoAction(std::make_unique<FakeStep>("B"));
    EXPECT_TRUE(handler.isChangedAutosave());
    handler.undo();
    EXPECT_FALSE(handler.isChangedAutosave()) << "back at the autosaved step";
    handler.documentSaved();
    EXPECT_FALSE(handler.isChanged());
    handler.undo();
    EXPECT_TRUE(handler.isChanged());
    EXPECT_TRUE(handler.isChangedAutosave());
    handler.clearContents();
    EXPECT_FALSE(handler.isChangedAutosave());
    EXPECT_FALSE(handler.isChanged());
}

TEST(UndoRedoHandlerListeners, busyChangesAreToldOnlyToListenersThatAskAndNeverAsAHistoryChange) {
    UndoRedoHandler handler(nullptr);
    struct BusyListener: CountingListener {
        int busy = 0;
        void undoRedoBusyChanged() override { ++busy; }
    } busyAware;
    CountingListener plain;
    handler.addUndoRedoListener(&busyAware);
    handler.addUndoRedoListener(&plain);
    handler.fireBusyChanged();
    EXPECT_EQ(busyAware.busy, 1);
    EXPECT_EQ(busyAware.changed, 0);
    EXPECT_EQ(plain.changed, 0);
}
