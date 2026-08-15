-- Screen type 6: the launcher starts the app, and Help > About opens.
-- One scenario per AIF, because Multi-Vue fixes the screen type at launch, so
-- the only way to exercise a mode is to start it from that mode's launcher.
local g = require "gxtest"
local d = require "mvdesk"

d.launch("r06")
g.snapshot("board")

d.menu_pick(d.HELP_X.narrow, d.MENUBAR_Y, d.ITEM1_Y)
g.wait(8)
g.snapshot("about")
