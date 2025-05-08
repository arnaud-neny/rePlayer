-- mainviewport.lua

local point = require("psycle.ui.point")
local dimension = require("psycle.ui.dimension")
local rect = require("psycle.ui.rect")
local ornamentfactory = require("psycle.ui.ornamentfactory"):new()
local boxspace = require("psycle.ui.boxspace")
local alignstyle = require("psycle.ui.alignstyle")
local keycodes = require("psycle.ui.keycodes")
local group = require("psycle.ui.group")
local viewport = require("psycle.ui.viewport")
local text = require("psycle.ui.text")
local combobox = require("psycle.ui.combobox")
local button = require("psycle.ui.button")


local mainviewport = viewport:new()

function mainviewport:new()
  local m = viewport:new()
  setmetatable(m, self)
  self.__index = self    
  m:init()
  return m
end

function mainviewport:init()  
  self:link(require("psycle.ui.standarduisheet"))
  self:invalidatedirect()
  self.toolbar = group:new(self)
                      :setautosize(false, true)
                      :setalign(alignstyle.TOP)
  text:new(self):settext("Midi Input Test")

  self.driverbox = combobox:new(self)
                           :setposition(rect:new(point:new(10, 40), dimension:new(200, 20)))
  self:filldriverbox()  
  self:initmidiopenbutton()
  self:initmidiclosebutton()
  self:initstatus()
  self:initmidioutput()
end


function mainviewport:filldriverbox()
  local machine = psycle.proxy
  local numdevices = machine.midiinput:numdevices()
  for i=1, numdevices do
    self.driverbox:additem(machine.midiinput:devicedescription(i))
  end  
  if numdevices > 0 then
    self.driverbox:setitemindex(1)
  end
end

function mainviewport:initmidiopenbutton()  
  local button = button:new(self)
                       :settext("Open Midi Input")
                       :setposition(rect:new(point:new(220, 40), dimension:new(200, 20)))
  local that = self
  function button:onclick()
    local machine = psycle.proxy
    machine.midiinput:setdevice(machine.midiinput.DRIVER_MIDI, that.driverbox:itemindex())
    machine.midiinput:open()
  end 
end

function mainviewport:initmidiclosebutton()  
  local button = button:new(self)
                       :settext("Close Midi Input")
                       :setposition(rect:new(point:new(220, 70), dimension:new(200, 20)))
  function button:onclick()
    local machine = psycle.proxy
    machine.midiinput:close()
  end 
end

function mainviewport:initstatus()
  self.statustext = text:new(self)
                        :setposition(rect:new(point:new(10, 70), dimension:new(200, 20)))
  self:updatestatus(false) 
end

function mainviewport:initmidioutput()
  self.miditext = text:new(self)
                      :settext("Midi Input received")
                      :setautosize(false, false)
                      :setposition(rect:new(point:new(10, 100), dimension:new(200, 20)))  
end

function mainviewport:updatestatus(open) 
  local msg = ""
  if open then
    msg = "active"
  else
    msg = "not active"
  end  
  self.statustext:settext(msg)
end

function mainviewport:outputmidi(note, channel, velocity)
  self.miditext:settext("note " .. note .. " channel " .. channel .. ", velocity " .. velocity)
end

return mainviewport
