local node = require("psycle.node") 
local orderedtable = require("psycle.orderedtable")
local stock = require("psycle.stock")
local property = require("psycle.property")

local setting = node:new():setname("setting")

setting.meta = {
  name = "plugineditor",
  version = 1.0
}

local help = node:new(setting):settext("Help"):setname("help")
help.properties = orderedtable.new()
help.properties.helplevel = property:new(2, "Help Level")

local tab = node:new(setting):settext("Tab"):setname("tab")
tab.properties = orderedtable.new()
tab.properties.tabwidth = property:new(4, "Tabwidth")

local textpage = node:new(setting):settext("Textpage"):setname("textpage")
textpage.properties = orderedtable.new()
textpage.properties.mapcapslocktoctrl = property:new(true, "Map Capslock to Control")
textpage.properties.haslinenumbers = property:new(false, "Show Linenumbers")

local keycontrol = node:new(setting):settext("Key Control"):setname("keycontrol")
keycontrol.properties = orderedtable.new()
keycontrol.properties.keymap = property:new("$(lua)/plugineditor/keymaps/keymapms.lua", "Keymap File", "filepath")

local skin = node:new(setting):settext("Skin"):setname("skin")
skin.properties = orderedtable.new()
skin.properties.stylesheet = property:new("$(lua)/plugineditor/skin.lua", "Stylesheet", "filepath")

return setting
