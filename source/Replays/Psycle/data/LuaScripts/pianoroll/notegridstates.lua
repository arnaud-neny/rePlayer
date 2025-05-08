--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  notegridstates.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

return {
  idling = require("idlestate"):new(),  
  dragging = require("dragstate"):new(),  
  resizingleft = require("resizeleftstate"):new(),
  resizingright = require("resizerightstate"):new(),
  selectidling = require("selectidlestate"):new(),
  selectingevents = require("selecteventsstate"):new(),
  slicing = require("sliceidlestate"):new()
}
