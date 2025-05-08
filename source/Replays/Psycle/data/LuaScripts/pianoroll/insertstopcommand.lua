local player = require("psycle.player"):new()
local machinebar = require("psycle.machinebar"):new()
local patternevent = require("patternevent")
local rawpattern = require("psycle.pattern")
local cmddef = require("psycle.ui.cmddef")

local insertstopcommand = {}

function insertstopcommand:new(...)
  local m = {}
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  return m
end

function insertstopcommand:init(inputhandler, sequence, cursor, scroller)
  self.inputhandler_ = inputhandler
  self.sequence_ = sequence
  self.cursor_ = cursor
  self.scroller_ = scroller
end

function insertstopcommand:execute()
  local pattern = self.sequence_:at(self.cursor_:seqpos())
  local event = self:event(pattern)
  if event then
    pattern:changelength(event, self.cursor_:position() - event:position())
    self.cursor_:notify()
  end
  self.cursor_:steprow()
end

function insertstopcommand:event(pattern)
  local result = nil
  local event = pattern:firstnote()
  while event do
    if event:isnote() then
     if event:over(self.cursor_:position()) and event:track() == self.cursor_:track() then                 
       result = event
       break
     end  
    end
    event = event.next     
  end
  return result
end

return insertstopcommand
