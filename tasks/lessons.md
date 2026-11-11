# Lessons

Carried over from Lucky Seven where they apply.

## Async-loading test fixtures
- **Rule:** when waiting on async state, wait for the exact expected value (e.g. a buffer length), not "non-zero".

## Don't disturb a live DAW session
- **Rule:** while the DAW is in use for testing, build with `nice -n 19` and limited `-j`, build only test/tool targets, and install (full build) only after the host has quit.
