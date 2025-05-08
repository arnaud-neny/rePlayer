--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  controlgridstates.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local controlidlestate = require("controlidlestate")
local controlselectidlestate = require("controlselectidlestate")
local controltweakstate = require("controltweakstate")
local controlselecteventsstate = require("controlselecteventsstate")

return {
  idling = controlidlestate:new(),
  selectidling = controlselectidlestate:new(),
  tweaking = controltweakstate:new(),
  selectingevents = controlselecteventsstate:new()
}
