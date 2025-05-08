--[[ 
psycle plugineditor (c) 2017 by psycledelics
File:  keymapms.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local keycodes = require("psycle.ui.keycodes")

return {
  title = "MAIN MENU",
  keymap = {
    [keycodes.KEYJ] = "helpmain",
    [keycodes.KEYK] = "kmenu",
    [keycodes.KEYQ] = "quickmenu",
    [keycodes.KEYS] = "@charleft",
    [keycodes.KEYD] = "@charright",
    [keycodes.KEYE] = "@lineup",
    [keycodes.KEYX] = "@linedown",
    [keycodes.KEYA] = "@wordleft",
    [keycodes.KEYF] = "@wordright",
    [keycodes.KEYO] = "@loadpage"
  },
  display = {
    {section = "--Cursor Movement--"},
    {shortdesc = "", desc = "^S char left ^D char right"},
    {shortdesc = "", desc = "^A word left ^F word right"},
    {shortdesc = "", desc = "^E line up   ^X line down"},
    {section = "--Common--"},
    {shortdesc = "^O", desc = "Open File"},
    {section = "--Other Menus--"},
    {shortdesc = "^J", desc = "Help"},
    {shortdesc = "^K", desc = "Block"},
    {shortdesc = "^Q", desc = "Quick"}
  }
}
