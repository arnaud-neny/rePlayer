local player = require("psycle.player"):new()
local machinebar = require("psycle.machinebar"):new()
local patternevent = require("patternevent")
local rawpattern = require("psycle.pattern")
local cmddef = require("psycle.ui.cmddef")

local insertnotecommand = {}

function insertnotecommand:new(...)
  local m = {}
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  return m
end

function insertnotecommand:init(inputhandler, sequence, cursor, note, scroller)
  self.inputhandler_ = inputhandler
  self.sequence_ = sequence
  self.cursor_ = cursor
  self.note_ = note
  self.scroller_ = scroller
end

function insertnotecommand:execute()
    local pattern = self.sequence_:at(self.cursor_:seqpos()) 
    local basenote = self.note_ + 12*cmddef.currentoctave()
    local line = math.floor(self.cursor_:position() * player:tpb())    
    rawpattern.addundo(pattern.ps_, self.cursor_:track(), line,
          1, 1, self.cursor_:track(), line, 0, self.cursor_:seqpos() - 1);    
    local events = pattern:copy(self.inputhandler_:chord():pattern(),
                                basenote - self.inputhandler_:chord():offset(),
                                self.cursor_:position(),
                                self.cursor_:track())
    for i=1, #events do
      events[i]:setmach(machinebar:currmachine())
               :setinst(machinebar:curraux())               
    end
    pattern:deselectall():setselection(events)
    local visiblekeys = self.scroller_:visiblekeys()
    if basenote > visiblekeys.top or basenote < visiblekeys.bottom then
      self.scroller_:centertokey(basenote)
    end
    self.cursor_:steprow()
end

return insertnotecommand
