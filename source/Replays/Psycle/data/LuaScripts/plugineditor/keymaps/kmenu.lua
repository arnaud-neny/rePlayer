--[[ 
psycle plugineditor (c) 2017 by psycledelics
File:  kmenu.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local keycodes = require("psycle.ui.keycodes")

return {
  title = "BLOCK MENU",
  keymap = {
    [keycodes.KEYS] = "@saveresume",
    [keycodes.KEYD] = "@savedone",
    [keycodes.KEYQ] = "@abandon",
    [keycodes.KEYB] = "@setblockbegin",
    [keycodes.KEYK] = "@setblockend",
    [keycodes.KEYY] = "@deleteblock",
    [keycodes.KEYC] = "@copyblock",
    [keycodes.KEYV] = "@moveblock"
    --W = "@writeblock"
  },  
  display = {
    {section = "-Saving Files-"},
    {shortdesc = "S", desc = "Save & resume"},
    {shortdesc = "D", desc = "Save--done"},
    {shortdesc = "Q", desc = "Abandon file"},
    {section = "-Block Operations-"},    
    {shortdesc = "B", desc = "Begin"},
    {shortdesc = "K", desc = "End"},
    {shortdesc = "C", desc = "Copy"},
    {shortdesc = "Y", desc = "Delete"},
    {shortdesc = "V", desc = "Move"},    
    --{shortdesc = "W", desc = "Write"}
    {section = "--Other Menus--"},
    {shortdesc = "^J", desc = "Help"},
    {shortdesc = "^K", desc = "Block"}
  }
}
