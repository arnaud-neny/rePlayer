--[[ psycle pianoroll (c) 2017 by psycledelics
File: inputhandler.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.  
]]

local cmddef = require("psycle.ui.cmddef")
local keycodes = require("psycle.ui.keycodes")
local rawpattern = require("psycle.pattern"):new()
local multicommand = require("multicommand")
local clearrowcommand = require("clearrowcommand")
local deleterowcommand = require("deleterowcommand")
local insertrowcommand = require("insertrowcommand")
local insertnotecommand = require("insertnotecommand")
local insertstopcommand = require("insertstopcommand")
local sequenceupdatecommand = require("sequenceupdatecommand")
local editquantizecommand = require("editquantizecommand")
local selectioncommands = require("selectioncommands")
local redrawgridcommand = require("redrawgridcommand")
local cursorcommands = require("cursorcommands")

local inputhandler = {}

function inputhandler:new(...)
  local m = {}
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  return m
end

function inputhandler:init(gridgroup, sequence, cursor, scroller)
  self:initredrawcommands(gridgroup)
  self.notedownmap = {}
  self.noteupmap = {} 
  self:createkeydowncommands(sequence, cursor, scroller)
  self:createkeyupcommands(sequence, cursor, scroller)
end

function inputhandler:initredrawcommands(gridgroup)
  self.redrawnotescommand = redrawgridcommand:new(gridgroup, redrawgridcommand.NOTES)
  self.redrawnotesandcontrolscommand = redrawgridcommand:new(gridgroup, redrawgridcommand.NOTES + redrawgridcommand.CONTROLS)  
end

function inputhandler.packkey(keycode, shiftkey, ctrlkey, extendedkey)
  local result = keycode
  result = shiftkey and result + 0x100 or result
  result = ctrlkey and result + 0x200 or result
  result = extendedkey and result + 0x300 or result
  return result
end

function inputhandler:createkeydowncommands(sequence, cursor, scroller)
  self:createnotecommands(sequence, cursor, scroller)
  self:addkeydowncommand(cmddef.EDITQUANTIZEDEC, editquantizecommand:new(-1))
    :addkeydowncommand(cmddef.EDITQUANTIZEINC, editquantizecommand:new(1))
    :addkeydowncommand(cmddef.ROWDELETE, deleterowcommand:new(sequence, cursor))
    :addkeydowncommand(cmddef.ROWINSERT, insertrowcommand:new(sequence, cursor))
    :addkeydowncommand(cmddef.SELECTALL,
                       selectioncommands.build(selectioncommands.SELECTALL, sequence, cursor))
    :addkeydowncommand(cmddef.ROWCLEAR, clearrowcommand:new(sequence, cursor))
    :addkeydowncommand(cmddef.TRANSPOSEBLOCKINC,
      multicommand:new(selectioncommands.build(selectioncommands.TRANSPOSE, sequence, 1),
                       self.redrawnotescommand))
    :addkeydowncommand(cmddef.TRANSPOSEBLOCKDEC,
      multicommand:new(selectioncommands.build(selectioncommands.TRANSPOSE, sequence, -1),
                       self.redrawnotescommand))
    :addkeydowncommand(cmddef.TRANSPOSEBLOCKINC12,
      multicommand:new(selectioncommands.build(selectioncommands.TRANSPOSE, sequence, 12),
                       self.redrawnotescommand))
    :addkeydowncommand(cmddef.TRANSPOSEBLOCKDEC12,
      multicommand:new(selectioncommands.build(selectioncommands.TRANSPOSE, sequence, -12),
                       self.redrawnotescommand))
    :addkeydowncommand(cmddef.BLOCKINTERPOLATE,
      multicommand:new(selectioncommands.build(selectioncommands.INTERPOLATELINEAR,
                                               sequence,
                                               cursor)))
    :addkeydowncommand(cmddef.BLOCKSTART,
      selectioncommands.build(selectioncommands.BLOCKSTART,
                                               sequence,
                                               cursor))
    :addkeydowncommand(cmddef.BLOCKEND,
      selectioncommands.build(selectioncommands.BLOCKEND,
                                               sequence,
                                               cursor))                                               
    :addkeydowncommand(cmddef.NAVLEFT,
                       cursorcommands.build(cursorcommands.LEFT, cursor))
    :addkeydowncommand(cmddef.NAVRIGHT,
                       cursorcommands.build(cursorcommands.RIGHT, cursor))
    :addkeydowncommand(cmddef.NAVUP,
                       cursorcommands.build(cursorcommands.UP, cursor))
    :addkeydowncommand(cmddef.NAVDN,
                       cursorcommands.build(cursorcommands.DOWN, cursor))
    :addkeydowncommand(cmddef.NAVPAGEUP,
                       cursorcommands.build(cursorcommands.PAGEUP, cursor))
    :addkeydowncommand(cmddef.NAVPAGEDN,
                       cursorcommands.build(cursorcommands.PAGEDOWN, cursor))
    :addkeydowncommand(cmddef.COLUMNPREV,
                       cursorcommands.build(cursorcommands.TRACKPREV, cursor))
    :addkeydowncommand(cmddef.COLUMNNEXT,
                       cursorcommands.build(cursorcommands.TRACKNEXT, cursor))                      
end

function inputhandler:createnotecommands(sequence, cursor, scroller)
  local notes = {
		"KEYC_0", "KEYCS0", "KEYD_0", "KEYDS0", "KEYE_0", "KEYF_0", "KEYFS0",
    "KEYG_0", "KEYGS0", "KEYA_0", "KEYAS0", "KEYB_0", "KEYC_1", "KEYCS1",
    "KEYD_1", "KEYDS1", "KEYE_1", "KEYF_1", "KEYFS1", "KEYG_1", "KEYGS1",
    "KEYA_1", "KEYAS1", "KEYB_1", "KEYC_2", "KEYCS2", "KEYD_2", "KEYDS2",
    "KEYE_2", "KEYF_2", "KEYFS2", "KEYG_2", "KEYGS2", "KEYA_2", "KEYAS2",
    "KEYB_2", "KEYC_3", "KEYCS3", "KEYD_3", "KEYDS3", "KEYE_3", "KEYF_3",
		"KEYFS3", "KEYG_3", "KEYGS3", "KEYA_3", "KEYAS3", "KEYB_3"
	}
  for note=1, #notes do
    self:addkeydowncommand(cmddef[notes[note]], insertnotecommand:new(self, sequence, cursor, (note-1), scroller))
  end
  self:addkeydowncommand(cmddef.KEYSTOP,
                         insertstopcommand:new(self, sequence, cursor, scroller))     
end

function inputhandler:addkeydowncommand(cmd, command)
  self.notedownmap[self.packkey(cmddef.cmdtokey(cmd))] = command
  return self
end

function inputhandler:createkeyupcommands(sequence, cursor, scroller)
end

function inputhandler:addkeyupcommand(cmd, command)
  self.noteupmap[self.packkey(cmddef.cmdtokey(cmd))] = command
  return self
end

function inputhandler:handlekeyinput(ev)
  local command = self.notedownmap[self.packkey(ev:keycode(), ev:shiftkey(), ev:ctrlkey(), ev:extendedkey())]
  if not command then
    command = self.notedownmap[self.packkey(ev:keycode(), not ev:shiftkey(), ev:ctrlkey(), ev:extendedkey())]   
  end
  if command then
    command:execute()
  end
  return command
end

function inputhandler:handlekeyup(ev)  

  local command = self.noteupmap[self.packkey(ev:keycode(), ev:shiftkey(), ev:ctrlkey(), ev:extendedkey())]
  if not command then
    command = self.noteupmap[self.packkey(ev:keycode(), not ev:shiftkey(), ev:ctrlkey(), ev:extendedkey())]
  end
  if command then
    command:execute()
  end
  return command
end

function inputhandler:oninsertchordchanged(chordselector)
  self.chord_ = chordselector:chord()
end

function inputhandler:setchord(chord)
  self.chord_ = chord
  return self
end

function inputhandler:chord()
  return self.chord_
end

return inputhandler
