-- flipper, stage 1: launch the app and capture its Multi-Vue window.
--
-- By the time this runs, the shim has booted NitrOS-9 and typed "flipper\r" at
-- the interactive OS-9 shell prompt. All that is left is to let the app fork,
-- open its window and draw, then capture one frame.
local g = require "gxtest"

-- Forking the program is disk-bound; wait for the screen to stop changing
-- rather than guessing a fixed delay.
g.wait(5)
g.wait_idle(2)

g.snapshot("startup")
