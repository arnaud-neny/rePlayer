--[[ 
psycle plugineditor (c) 2017 by psycledelics
File:  helpmain.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local keycodes = require("psycle.ui.keycodes")

return {
  title = "HELP MENU",
  keymap = {    
    [keycodes.KEYH] = "helplevel",
    [keycodes.KEYS] = "statusline"
  },
  display = {
    {section = ""},
    {shortdesc = "H", desc = "Display & set help level"},    
    {shortdesc = "S", desc = "Status line"},
    {section = "--Other Menus--"},
    {shortdesc = "^J", desc = "Help"},
    {shortdesc = "^K", desc = "Block"},
    {shortdesc = "^Q", desc = "Quick"}
  }
}
