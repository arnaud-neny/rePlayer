--[[ 
psycle plugineditor (c) 2017 by psycledelics
File:  helplevel.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local keycodes = require("psycle.ui.keycodes")

local link = {
  title = "HELP LEVELS",  
  prompt = "ENTER Space OR NEW HELP LEVEL (0, 1, 2, OR 3)",  
  keymap = {
    [keycodes.DIGIT0] = "@sethelplevel0",
    [keycodes.DIGIT1] = "@sethelplevel1",
    [keycodes.DIGIT2] = "@sethelplevel2",
    [keycodes.DIGIT3] = "@sethelplevel3"
  },  
  display = {
    {section = ""},
    {shortdesc = "3", desc = "Help always displayed"},
    {shortdesc = "2", desc = "Help at Control key displayed"},
    {shortdesc = "1", desc = "same as 2"},
    {shortdesc = "0", desc = "Help surpressed"},    
  }
}

return link
