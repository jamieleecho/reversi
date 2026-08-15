-- mvdesk: drive the Multi-Vue desktop from a graphics-test scenario.
--
-- A Multi-Vue app cannot be launched by typing at a shell -- this disk boots
-- straight into the desktop and has no interactive shell at all -- so a test
-- has to do what a person does: point at the disk, open it, and double-click a
-- launcher. That needs the mouse.
--
-- MAME exposes the CoCo joystick as a pair of absolute AD-stick axes, 0..1023,
-- which Multi-Vue maps onto the screen. Coordinates below are captured-image
-- pixels.
--
-- The mapping and the coordinates in this file are a matched pair, both arrived
-- at by trying them against the real desktop. Do not "improve" one without
-- re-deriving the other: a more accurate pixel mapping simply moves every
-- landmark, and the landmarks are what is actually verified to work.

local M = {}

local g = require "gxtest"

local p = manager.machine.ioport.ports
local rx = p[":joystick_rx"].fields["AD Stick X"]
local ry = p[":joystick_ry"].fields["AD Stick Y"]
local btn = p[":joystick_buttons"].fields["Right Button 1"]

local function clamp(v)
    if v < 0 then return 0 end
    if v > 1023 then return 1023 end
    return math.floor(v)
end

--- Move the pointer to a screen pixel and let Multi-Vue notice.
function M.moveto(sx, sy)
    rx:set_value(clamp((sx - 10) * 1023 / 625))
    ry:set_value(clamp((sy - 25) * 1023 / 193))
    g.wait(0.6)
end

--- n complete button presses at the current position.
function M.click(n)
    for _ = 1, (n or 1) do
        btn:set_value(1); g.wait(0.15)
        btn:set_value(0); g.wait(0.15)
    end
end

function M.dblclick() M.click(2) end

--- Choose a menu item: click the title to open the menu, then click the item.
-- Multi-Vue menus are NOT press-drag-release. A press opens the menu, but
-- dragging onto an item and releasing selects nothing -- the menu just closes.
-- @param title_x  x of the menu title in the menu bar
-- @param title_y  y of the menu bar
-- @param item_y   y of the wanted item in the dropdown
function M.menu_pick(title_x, title_y, item_y)
    M.moveto(title_x, title_y)
    M.click(1)
    g.wait(1.5)
    M.moveto(title_x, item_y)
    M.click(1)
end

-- Where things sit on this disk's desktop. The file window's layout depends on
-- how many entries the disk holds, so these move if the disk contents change.
M.DISK_ICON   = {57, 42}
M.LAUNCHER = {
    r05 = {168, 133},
    r06 = {293, 133},
    r07 = {397, 133},
    r08 = {548, 100},
}

-- The menu bar sits at the top of the app's window. The 320-wide screen types
-- double each pixel horizontally, so the same character column is twice as far
-- across as it is in the 640-wide ones.
M.MENUBAR_Y = 27
-- Measured by sweeping the menu bar with the button held: the Help title sits
-- at x=190 in a 320-wide screen. The 640-wide types put the same character
-- column at half the pixel offset.
M.HELP_X    = { narrow = 190, wide = 95 }
M.ITEM1_Y   = 41

--- Boot-to-board: open the disk, then double-click one launcher.
-- @param which  key into M.LAUNCHER, e.g. "r07"
function M.launch(which)
    -- Wait for the screen to stop changing rather than for a fixed time. The
    -- desktop takes a variable while to come up and a click delivered while it
    -- is still drawing is simply lost, which looks exactly like a wrong
    -- coordinate -- the failure mode is silent either way.
    -- The scenario starts as soon as DOS is typed (~10s in), and Multi-Vue's
    -- desktop is not up until roughly 60s, so this wait is the scenario's own
    -- business -- there is no shell-typing step padding it out any more.
    g.wait(50)
    g.wait_idle(3)
    M.moveto(M.DISK_ICON[1], M.DISK_ICON[2])
    M.dblclick()
    -- Enumerating the disk takes roughly 35s with this many files, and
    -- wait_idle caps itself at secs*4+5, so the fixed wait carries most of it.
    g.wait(35)
    g.wait_idle(4)
    local at = M.LAUNCHER[which]
    M.moveto(at[1], at[2])
    M.dblclick()
    g.wait(25)
    g.wait_idle(5)                   -- screen switch, then the app drawing
end

return M
