--[[ 
psycle plugineditor (c) 2017 by psycledelics
File:  keymapws.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local keycodes = require("psycle.ui.keycodes")

return {
  title = "MAIN MENU",
  keymap = {
    [keycodes.KEYJ] = "helpmain",
    [keycodes.KEYK] = "kmenu",
    [keycodes.KEYF] = "@displaysearch",
    [keycodes.KEYS] = "@saveresume",
    [keycodes.KEYW] = "@savedone",
    [keycodes.KEYO] = "@loadpage",
    [keycodes.KEYN] = "@newpage",
    [keycodes.KEYX] = "@cut",
    [keycodes.KEYC] = "@copy",
    [keycodes.KEYV] = "@paste",    
    [keycodes.KEYA] = "@selall"
  },
  display = {
    {section = "--Controlkeys--"},
    {shortdesc = "^N", desc = "New File"},
    {shortdesc = "^O", desc = "Open File"},
    {shortdesc = "^N", desc = "Open File"},
    {shortdesc = "^S", desc = "Save File"},
    {shortdesc = "^W", desc = "Close File"},   
    {section = "--Edit keys--"},
    {shortdesc = "^X", desc = "Cut"},
    {shortdesc = "^C", desc = "Copy"},
    {shortdesc = "^V", desc = "Paste"},
    {shortdesc = "^A", desc = "Paste"},
    {shortdesc = "^F", desc = "Find/Replace Selection"},
    {section = "--Other Menus--"},
    {shortdesc = "^J", desc = "Help"},
    {shortdesc = "^K", desc = "Block"}
  }
}
