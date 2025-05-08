--[[ 
psycle plugineditor (c) 2017 by psycledelics
File:  statusline.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local keycodes = require("psycle.ui.keycodes")

return {
  title = "STATUS LINE",
  display = {
    {shortdesc = "LINE n", desc = "is line of cursor position"},
    {shortdesc = "COL n", desc = "is column on line of cursor position"},
    {shortdesc = "INSERT ON", desc = "shows if character insertion is on"},
    {shortdesc = "MODIFIED", desc = "shows if file is modified"}
  }
}
