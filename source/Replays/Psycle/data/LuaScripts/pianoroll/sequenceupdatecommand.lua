local player = require("psycle.player"):new()
local machinebar = require("psycle.machinebar"):new()
local patternevent = require("patternevent")
local cmddef = require("psycle.ui.cmddef")

local sequenceupdatecommand = {}

function sequenceupdatecommand:new(...)
  local m = {}
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  return m
end

function sequenceupdatecommand:init(sequence, cursor, key, scroller)
  self.sequence_ = sequence
  self.cursor_ = cursor
  self.note_ = key
  self.scroller_ = scroller
end

function sequenceupdatecommand:execute()
    self.sequence_:update()
    self.cursor_:notify()
end

return sequenceupdatecommand
