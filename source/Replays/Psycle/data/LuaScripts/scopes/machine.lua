-- scopes.lua

machine = require("psycle.machine"):new()

local mainviewport = require("mainviewport")
local settingsmanager = require("psycle.settingsmanager")

function machine:info()
  return {
    vendor = "psycle",
    name = "scopes",
    mode = machine.HOST,
    version = 0,
    api=0
  }
end

function machine:init()  
  self.settingsmanager = settingsmanager:new("scopes")
  self.mainviewport = mainviewport:new()
  self:setviewport(self.mainviewport)
end

return machine
