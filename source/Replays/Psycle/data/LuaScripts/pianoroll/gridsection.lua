local eventfilter = require("eventfilter")

local gridsection = {}

function gridsection:new(...)
  local m = {}
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  return m
end

function gridsection:init(view, label)
  self.view = view
  self.label_ = label and label or ""
  self.eventfilter = eventfilter:new()
  self.track_ = -1
end

function gridsection:label()
  return self.label_
end

function gridsection:setlabel(text)
  self.label_ = text
  return self
end

function gridsection:display()
    local result = ""
    local pattern = self.view.sequence:at(self.view.cursor:seqpos())
      if pattern then
      local event = pattern:firstcmd()
      local found = nil
      while event do
        if event.pos == self.view.cursor:position() then
          if self.eventfilter:accept(event) then
            found = event
            break
          end
        end
        event = event.next
      end
      if found and found:mach() ~= 255 then
        local macname = self.view.grid.machines.paramname(found.mach_, found.inst_)
        if macname then
          result = macname
        end
      end
    end
    return result
  end

function gridsection:height()
  return 50
end

function gridsection:settrack(track)
  self.track_ = track
end

function gridsection:track()
  return self.track_ == -1 and self.view.cursor:track() or self.track_
end

return gridsection
