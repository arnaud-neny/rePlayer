-- psycle closebutton (c) 2015, 2016 by psycledelics
-- File: closebutton.lua
-- copyright 2015 members of the psycle project http://psycle.sourceforge.net
-- This source is free software ; you can redistribute it and/or modify it under
-- the terms of the GNU General Public License as published by the Free Software
-- Foundation ; either version 2, or (at your option) any later version.

local point = require("psycle.ui.point")
local dimension = require("psycle.ui.dimension")
local rect = require("psycle.ui.rect")
local boxspace = require("psycle.ui.boxspace")
local text = require("psycle.ui.text")
local ornamentfactory = require("psycle.ui.ornamentfactory"):new()
local toolicon = require("psycle.ui.toolicon")
local alignstyle = require("psycle.ui.alignstyle")

local closebutton = {}

function closebutton.new(parent)    
  local t = toolicon:new(parent)
                     :setautosize(false, false)           
                     :setposition(rect:new(point:new(), dimension:new(20, 15)))
                     :settext("X")
                     :setverticalalignment(toolicon.CENTER)
                     :setjustify(toolicon.CENTERJUSTIFY)
                     :setalign(alignstyle.RIGHT)    
  return t
end

return closebutton