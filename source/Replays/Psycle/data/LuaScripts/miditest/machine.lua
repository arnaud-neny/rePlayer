-- miditest.lua

machine = require("psycle.machine"):new()

local mainviewport = require("mainviewport")
local midiinput = require("psycle.midiinput")
local run = require("psycle.run")

function machine:info()
  return {
    vendor = "psycle",
    name = "miditest",
    mode = machine.GENERATOR,
    version = 0,
    api=0
  }
end

function machine:init()
  self:initmidi()
  self.mainviewport = mainviewport:new()
  self:setviewport(self.mainviewport)  
end

function machine:initmidi()
  self.midiinput = midiinput:new()
  local that = self
  self.runmidioutput = run:new()
  function self.runmidioutput:setmidi(note, channel, velocity)
    self.note, self.channel, self.velocity = note, channel, velocity
    return self
  end
  function self.runmidioutput:run()
    that.mainviewport:outputmidi(self.note, self.channel, self.velocity)  
  end
  self.runupdatestatus = run:new()
  function self.runupdatestatus:setstate(state)
    self.state = state
    return self
  end
  function self.runupdatestatus:run()
    that.mainviewport:updatestatus(self.state)  
  end
  function self.midiinput:onopen()
    that.runupdatestatus:setstate(true)
    psycle.invokelater(that.runupdatestatus)
  end
  function self.midiinput:onclose()
    that.runupdatestatus:setstate(false)
    psycle.invokelater(that.runupdatestatus)
  end
  function self.midiinput:onmididata(note, channel, velocity)    
    psycle.invokelater(that.runmidioutput:setmidi(note, channel, velocity))
  end
end

return machine
