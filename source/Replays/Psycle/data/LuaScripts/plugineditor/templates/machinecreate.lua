local orderedtable = require("psycle.orderedtable")
local fontinfo = require("psycle.ui.fontinfo")
local stock = require("psycle.stock")
local property = require("property")

local machinecreate = orderedtable.new()  

machinecreate.meta = {
  name = "machinecreate",
  version = 0.1
}

machinecreate.fx = {}
machinecreate.fx.label = "fx"
machinecreate.fx.isplugin = true
machinecreate.fx.general = orderedtable.new()
machinecreate.fx.general.machinename = property:new("", "machinename")
machinecreate.fx.general.vendor = property:new("", "vendor")
machinecreate.fx.general.machmode = property:new("machinemodes.FX", "machmode"):preventedit()
machinecreate.fx.general.link = property:new("", "link"):preventedit()
machinecreate.fx.outputs = {
  {template = "machine.lu$", path = "machine.lua"}
}

machinecreate.generator = {}
machinecreate.generator.label = "generator"
machinecreate.generator.isplugin = true
machinecreate.generator.general = orderedtable.new()
machinecreate.generator.general.machinename = property:new("", "machinename")
machinecreate.generator.general.vendor = property:new("", "vendor")
machinecreate.generator.general.machmode = property:new("machinemodes.GENERATOR", "machmode"):preventedit()
machinecreate.generator.general.link = property:new("", "link"):preventedit()
machinecreate.generator.outputs = {
  {template = "machine.lu$", path = "machine.lua"}
}

machinecreate.hostextension = {}
machinecreate.hostextension.label = "hostextension"
machinecreate.hostextension.isplugin = true
machinecreate.hostextension.general = orderedtable.new()
machinecreate.hostextension.general.machinename = property:new("", "machinename")
machinecreate.hostextension.general.vendor = property:new("", "vendor")
machinecreate.hostextension.general.machmode = property:new("machinemodes.HOST", "machmode"):preventedit()
machinecreate.hostextension.general.link = property:new(
"function psycle.install()\n" ..
"  return [[\n" ..
"  link = {\n" ..
'    label = "$(machinename)",\n' ..
'    plugin = "$(machinename).lua",\n' ..
"    viewport = psycle.CHILDVIEWPORT,\n" ..
"    userinterface = psycle.SDI\n" ..
"  }\n" ..
'  psycle.addmenu("view", link)\n' ..
"]]\n" ..
"end", "link"
):preventedit()
machinecreate.hostextension.mainviewport = {}
machinecreate.hostextension.mainviewport.properties = orderedtable.new()
machinecreate.hostextension.mainviewport.properties.classname = property:new("mainviewport", "classname")
machinecreate.hostextension.mainviewport.properties.requirecode = property:new([[
local titlebaricons = require("psycle.ui.titlebaricons")
local toolicon = require("psycle.ui.toolicon")
local text = require("psycle.ui.text")
]]):preventedit()
machinecreate.hostextension.mainviewport.properties.initcode = property:new([[
  self:link(require("psycle.ui.standarduisheet"))
  self:invalidatedirect()
  self.toolbar = group:new(self)
                      :setautosize(false, true)
                      :setalign(alignstyle.TOP)
  self.titlebaricons = titlebaricons:new(self.toolbar)
                                    :setalign(alignstyle.RIGHT)
  text:new(self):settext("Psycle Extension on Host Vers 11.2")
]]):preventedit()
machinecreate.hostextension.outputs = {
  {template = "hostextension.lu$", path = "machine.lua"},  
  {template = "viewport.lu$", path = "mainviewport.lua", properties = machinecreate.hostextension.mainviewport.properties},
  {template = "defaultsetting.lu$", path = "defaultsetting.lua"},
  {template = "global.lu$", path = "global.lua"}
}

machinecreate.class = {}
machinecreate.class.label = "class"
machinecreate.class.isplugin = false
machinecreate.class.properties = orderedtable.new()
machinecreate.class.properties.classname = property:new("", "classname")
machinecreate.class.outputs = {
  {label = "general", template = "class.lu$", path = "$(classname).lua", properties = machinecreate.class.properties}
}

machinecreate.window = {}
machinecreate.window.label = "window"
machinecreate.window.isplugin = false
machinecreate.window.properties = orderedtable.new()
machinecreate.window.properties.classname = property:new("", "classname")
machinecreate.window.outputs = {
  {label = "general", template = "window.lu$", path = "$(classname).lua",  properties = machinecreate.window.properties}
}

machinecreate.group = {}
machinecreate.group.label = "group"
machinecreate.group.isplugin = false
machinecreate.group.properties = orderedtable.new()
machinecreate.group.properties.classname = property:new("", "classname")
machinecreate.group.outputs = {
  {label = "general", template = "group.lu$", path = "$(classname).lua", properties = machinecreate.group.properties}
}

machinecreate.viewport = {}
machinecreate.viewport.label = "viewport"
machinecreate.viewport.isplugin = false
machinecreate.viewport.properties = orderedtable.new()
machinecreate.viewport.properties.classname = property:new("", "classname")
machinecreate.viewport.properties.requirecode = property:new("", "Requires"):preventedit()
machinecreate.viewport.properties.initcode = property:new("", "Init"):preventedit()
machinecreate.viewport.outputs = {
  {label = "general", template = "viewport.lu$", path = "$(classname).lua", properties = machinecreate.viewport.properties}
}

return machinecreate
 
