--[[ psycle pianoroll (c) 2017 by psycledelics
File: selectioncommands.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.  
]]

local command = require("psycle.command")
local player = require("psycle.player"):new()
local rawpattern = require("psycle.pattern"):new()
local machinebar = require("psycle.machinebar"):new()

local transposecommand = command:new()

function transposecommand:new(...)
  local m = command:new()  
  setmetatable(m, self)
  self.__index = self
  m:init(...)
  return m
end

function transposecommand:init(sequence, offset)
  self.sequence_ = sequence
  self.offset_ = offset
end

function transposecommand:execute() 
  local selection = self.sequence_:selection()
  for i=1, #selection do
    selection[i]:setnote(selection[i]:note() + self.offset_)
                :sync()
  end
end

local interpolatelinearcommand = command:new()

function interpolatelinearcommand:new(...)
  local m = command:new()  
  setmetatable(m, self)
  self.__index = self
  m:init(...)
  return m
end

function interpolatelinearcommand:init(sequence, cursor)
  self.sequence_ = sequence
  self.cursor_ = cursor
end

function interpolatelinearcommand:execute()
  local pattern = self.sequence_:at(self.cursor_:seqpos())
  local selection = pattern:selection()
  local minpos, maxpos = pattern:numbeats(), 0
  for i=1, #selection do
    local event = selection[i]
    minpos = math.min(event:position(), minpos)
    maxpos = math.max(event:position(), maxpos)
  end
  rawpattern:interpolate(self.cursor_:track(),
                         math.floor(minpos * player:tpb()),
                         math.floor(maxpos * player:tpb()))
  pattern:import()
end

local interpolatecurvecommand = command:new()

function interpolatecurvecommand:new(...)
  local m = command:new()  
  setmetatable(m, self)
  self.__index = self  
  m:init(...)
  return m
end

function interpolatecurvecommand:init(sequence, cursor)
  self.sequence_ = sequence
  self.cursor_ = cursor
end

function interpolatecurvecommand:execute()
  local pattern = self.sequence_:at(self.cursor_:seqpos())
  local selection = pattern:selection()
  local minpos, maxpos = pattern:numbeats(), 0
  for i=1, #selection do
    local event = selection[i]
    minpos = math.min(event:position(), minpos)
    maxpos = math.max(event:position(), maxpos)
  end
  rawpattern:interpolatecurve(self.cursor_:track(),
                              math.floor(minpos * player:tpb()),
                              math.floor(maxpos * player:tpb()))
  pattern:import()  
end

local pastecommand = command:new()

function pastecommand:new(...)
  local m = command:new()  
  setmetatable(m, self)
  self.__index = self  
  m:init(...)
  return m
end

function pastecommand:init(sequence, cursor)
  self.sequence_ = sequence
  self.cursor_ = cursor
end

function pastecommand:execute()
  local source = self.sequence_:at(self.cursor_:seqpos())
  local events = source:selection()
  local maxnote, minpos = 0, source:numbeats()
  for i=1, #events do    
    maxnote = math.max(maxnote, events[i]:note())
    minpos = math.min(minpos, events[i]:position())    
  end  
  local maxpos = self.cursor_:position()
  local target = source
  for i=1, #events do
    local event = events[i]:clone()
    target:insert(event, event:position() - minpos + self.cursor_:position())
    maxpos = math.max(maxpos, event:position())
  end
  if rawpattern.movecursorpaste() then
    self.cursor_:setposition(math.min(target:numbeats(), maxpos + player:bpt()))
  end
end

local movecommand = command:new()

function movecommand:new(...)
  local m = command:new()  
  setmetatable(m, self)
  self.__index = self  
  m:init(...)
  return m
end

function movecommand:init(sequence, cursor)
  self.sequence_ = sequence
  self.cursor_ = cursor
end

function movecommand:execute()
  local source = self.sequence_:at(self.cursor_:seqpos())
  local events = source:selection()
  local maxnote, minpos = 0, source:numbeats()
  for i=1, #events do
    local event = events[i]  
    maxnote = math.max(maxnote, event:note())
    minpos = math.min(minpos, event:position())
    source:erase(event)
  end  
  local target = source
  for i=1, #events do
    local event = events[i]  
    target:insert(event, event:position() - minpos + self.cursor_:position())
  end
end

local erasecommand = command:new()

function erasecommand:new(...)
  local m = command:new()  
  setmetatable(m, self)
  self.__index = self  
  m:init(...)
  return m
end

function erasecommand:init(sequence)
  self.sequence_ = sequence
end

function erasecommand:execute() 
  self.sequence_:eraseselection()  
end

local changetrackcommand = command:new()

function changetrackcommand:new(...)
  local m = command:new()  
  setmetatable(m, self)
  self.__index = self  
  m:init(...)
  return m
end

function changetrackcommand:init(sequence, cursor)
  self.sequence_ = sequence
  self.cursor_ = cursor
end

function changetrackcommand:mintrack(selection)
  local mintrack = 64
  for i=1, #selection do
    mintrack = math.min(mintrack, selection[i]:track())
  end
  return mintrack
end

function changetrackcommand:execute()
  local selection = self.sequence_:selection()
  local mintrack = self:mintrack(selection)
  for i=1, #selection do
    local event = selection[i]
    local target = event:selectedpattern()
    target:erase(event)
    event:settrack(math.max(0, math.min(event:track() - mintrack + self.cursor_:track(), 64)))    
    target:insert(event, event:position())
  end
end

local changegeneratorcommand = command:new()

function changegeneratorcommand:new(...)
  local m = command:new()  
  setmetatable(m, self)
  self.__index = self  
  m:init(...)
  return m
end

function changegeneratorcommand:init(sequence)
  self.sequence_ = sequence
end

function changegeneratorcommand:execute() 
  local selection = self.sequence_:selection()
  for i=1, #selection do
    selection[i]:setmach(machinebar:currmachine())
                :sync()
  end  
end

local changeinstrumentcommand = command:new()

function changeinstrumentcommand:new(...)
  local m = command:new()  
  setmetatable(m, self)
  self.__index = self  
  m:init(...)
  return m
end

function changeinstrumentcommand:init(sequence)
  self.sequence_ = sequence
end

function changeinstrumentcommand:execute() 
  local selection = self.sequence_:selection()
  for i=1, #selection do
    selection[i]:setinst(machinebar:curraux())
                :sync()
  end 
end

local blockstartcommand = command:new()

function blockstartcommand:new(...)
  local m = command:new()  
  setmetatable(m, self)
  self.__index = self  
  m:init(...)
  return m
end

function blockstartcommand:init(sequence, cursor)
  self.sequence_ = sequence
  self.cursor_ = cursor
end

function blockstartcommand:execute()
  self.sequence_:setblockstart(self.cursor_:position(), self.cursor_:seqpos())
  self.cursor_:notify()
end

local blockendcommand = command:new()

function blockendcommand:new(...)
  local m = command:new()  
  setmetatable(m, self)
  self.__index = self  
  m:init(...)
  return m
end

function blockendcommand:init(sequence, cursor)
  self.sequence_ = sequence
  self.cursor_ = cursor
end

function blockendcommand:execute()
  self.sequence_:setblockend(self.cursor_:position(), self.cursor_:seqpos())
  self.cursor_:notify()
end

local selectallcommand = command:new()

function selectallcommand:new(...)
  local m = command:new()
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  return m
end

function selectallcommand:init(sequence, cursor)
  self.sequence_ = sequence
  self.cursor_ = cursor
end

function selectallcommand:execute()
  local pattern = self.sequence_:at(self.cursor_:seqpos())
  pattern:deselectall()
  local function select(event)
    if self.sequence_.trackviewmode == self.sequence_.VIEWALLTRACKS or 
       self.cursor_:track() == event:track() then
      event:select(pattern)
    end
  end
  pattern:work(select, pattern:firstnote())
  pattern:work(select, pattern:firstcmd())
  self.cursor_:notify()
end

local commandfactory = {}

commandfactory.TRANSPOSE = 1
commandfactory.INTERPOLATELINEAR = 2
commandfactory.INTERPOLATECURVE = 3
commandfactory.PASTE = 4
commandfactory.ERASE = 5
commandfactory.MOVE = 6
commandfactory.CHANGETRACK = 7
commandfactory.CHANGEGENERATOR = 8
commandfactory.CHANGEINSTRUMENT = 9
commandfactory.BLOCKSTART = 10
commandfactory.BLOCKEND = 11
commandfactory.SELECTALL = 12

function commandfactory.build(cmd, ...)
  local result = nil
  if cmd == commandfactory.TRANSPOSE then
    result = transposecommand:new(...)
  elseif cmd == commandfactory.INTERPOLATELINEAR then
    result = interpolatelinearcommand:new(...)
  elseif cmd == commandfactory.INTERPOLATECURVE then
    result = interpolatecurvecommand:new(...)
  elseif cmd == commandfactory.PASTE then
    result = pastecommand:new(...)    
  elseif cmd == commandfactory.ERASE then
    result = erasecommand:new(...)
  elseif cmd == commandfactory.MOVE then
    result = movecommand:new(...)    
  elseif cmd == commandfactory.CHANGETRACK then
    result = changetrackcommand:new(...)
  elseif cmd == commandfactory.CHANGEGENERATOR then
    result = changegeneratorcommand:new(...)    
  elseif cmd == commandfactory.CHANGEINSTRUMENT then
    result = changeinstrumentcommand:new(...)    
  elseif cmd == commandfactory.BLOCKSTART then
    result = blockstartcommand:new(...)    
  elseif cmd == commandfactory.BLOCKEND then
    result = blockendcommand:new(...)    
  elseif cmd == commandfactory.SELECTALL then
    result = selectallcommand:new(...)
  end
  return result
end

return commandfactory
