---
id: utn-eraser-save-tests
trigger: "when fixing eraser or save code"
confidence: 0.7
domain: testing
source: local-repo-analysis
---

# Pair eraser/save fixes with unit tests

## Action
Change `EraseHandler.cpp` / `SaveHandler.cpp` together with `test/unit_tests/control/EraseHandlerTest.cpp` or a soak test.

## Evidence
- EraseHandlerTest.cpp changed 20 times, EraseHandler.cpp 19, SaveHandler.cpp 7 in 200 commits.
