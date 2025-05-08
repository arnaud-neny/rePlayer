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
  title = "QUICK MENU",
  keymap = {
    [keycodes.KEYF] = "@displaysearch"
  },  
  display = {
    {section = "--Miscellaneous--"},
    {shortdesc = "F", desc = "Find text in file"}    
  }
}
