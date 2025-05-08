-- psycle pluginselector (c) 2017 by psycledelics
-- File: commandhandler.lua
-- copyright 2016 members of the psycle project http://psycle.sourceforge.net
-- This source is free software ; you can redistribute it and/or modify it under
-- the terms of the GNU General Public License as published by the Free Software
-- Foundation ; either version 2, or (at your option) any later version.  
-- commandhandler.lua

local signal = require("psycle.signal")
local alignstyle = require("psycle.ui.alignstyle")
local point = require("psycle.ui.point")
local dimension = require("psycle.ui.dimension")
local rect = require("psycle.ui.rect")
local boxspace = require("psycle.ui.boxspace")
local fontinfo = require("psycle.ui.fontinfo")
local group = require("psycle.ui.group")
local ornamentfactory = require("psycle.ui.ornamentfactory"):new()
local text = require("psycle.ui.text")
local scintilla = require("psycle.ui.scintilla")
local serpent = require("psycle.serpent")
local keycodes = require("psycle.ui.keycodes")

local commandhandler = group:new(parent)

commandhandler.timeout = 5

function commandhandler:new(parent)
  local m = group:new(parent)
  setmetatable(m, self)
  self.__index = self    
  m:init()
  return m
end

function commandhandler:init()
  self.index = "keymapms.lua"
  self.rootdir = psycle.setting():dir("lua") .. "\\plugineditor\\keymaps\\"
  psycle.setting():addlistener(self)
  self.commands = {}
  self.dojump = signal:new()
  self.activated = 0
  self.helplevel = 1
  self.title = text:new(self)
                   :setalign(alignstyle.TOP)
                   :setautosize(false, true)
                   :setjustify(text.CENTERJUSTIFY)                   
  self.memo = scintilla:new(self)
                       :setalign(alignstyle.CLIENT)
                       :hidelinenumbers()  
                       :hidebreakpoints()
                       :hidehorscrollbar()
  self:onapplysetting()
  if self.helplevel < 3 then                  
    self:hide()
  end 
end

function commandhandler:addcommands(...)
  local args = {...}      
  for _, commands in pairs(args) do
    self.commands[#self.commands + 1] = commands
  end
  return self
end

function commandhandler:setstatusbar(statusbar)
  self.statusbar_ = statusbar
end

function commandhandler:setpagecontainer(container)
  self.pagecontainer_ = container
  return self
end

function commandhandler:typename()
  return "commandhandler"
end

function commandhandler:loadrootlink(displayerr)
  local path = self.rootdir .. self.index   
  local f, err = loadfile(path)
  if err then
    if displayerr then
      psycle.alert("Failed to load keymap. \n" .. err)
    end
    f = nil
  end  
  if f then
    self.link = f()    
  else
    self.link = commandhandler.fallbacklink
  end
  if self.helplevel == 3 then    
    self:builddisplay()
  end  
end

function commandhandler:setproperties(properties)  
  if properties.color then    
    self.memo:setforegroundcolor(properties.color:value())	    
  end
  if properties.backgroundcolor then    
    self.memo:setbackgroundcolor(properties.backgroundcolor:value())
  end
  local fontinfo = fontinfo:new()
  if properties.font_family then  
    fontinfo:setfamily(properties.font_family:value())   
  end
  if properties.font_size then  
    fontinfo:setsize(properties.font_size:value())   
  end 
  if properties.color then    
    self.memo:setcaretcolor(properties.color:value())
  end
  self.memo:setfontinfo(fontinfo)
  self.memo:setselalpha(75)
  self.memo:styleclearall()    
end

function commandhandler:jumpmain()  
  self:loadrootlink(false)
  self:builddisplay()
  self.activated = 0
end

function commandhandler:setactivated()
  local showdisplay = function()
    self:show();
    self:parent():updatealign();   
  end
  if self.helplevel > 0 and not self:visible() then
    self.timer = psycle.proxy:settimeout(showdisplay, 800)
  end
  self.activated = 1 
end

function commandhandler:deactivate()  
  self.activated = 0
  self:jumpmain()
  if self.statusbar_ then
    self.statusbar_:clearcontrol()
  end
  if self.helplevel < 3 then  
    self:hide():parent():updatealign()
  end
  if self.timer then
    psycle.proxy:cleartimeout(self.timer)
  end
end

function commandhandler:haskeycode(keycode)
  return self.link.keymap[keycode] ~= nil
end

function commandhandler:preventedkeys()
  local prevented = {}
  if self.link.keymap ~= nil then
    for key, _ in pairs(self.link.keymap) do
      prevented[#prevented +1] = key
    end
  end 
  return prevented;  
end

function commandhandler:jump(keycode)
  if self.link.keymap then
    local src = self.link.keymap[keycode]
    if src then
      if src:sub(1, 1) == "@" then
        self:jumpmain()
        for i = 1, #self.commands do
          local f = self.commands[i][src:sub(2, -1)];          
          if f then
            f:execute()
            break
          end
        end
        self:deactivate()        
      else
        path = self.rootdir .. src .. ".lua"        
        local f, err = loadfile(path)
        if f then
          self.link = f()
          self:builddisplay()
          self.dojump:emit(self.link)        
        else          
          self.memo:addtext("Error\nJump" .. path .. "\n" .. err)  
        end        
        self:setactivated()        
      end
    end
  end
  return self
end

lpad = function(str, len, char)
  if char == nil then 
    char = ' '
  end
  return str .. string.rep(char, len - #str)
end

function commandhandler:builddisplay()
  if self.link then
    self.memo:clearall()      
    local titlewithwhitespaces = self.link.title:gsub(".", "%1 "):sub(1,-2)
    self.title:settext("< < <      " .. titlewithwhitespaces .. "      > > >")        
    if self.link.display then
      local lines = {""}
      local linecounter = 1
      local sectioncounter = 0
      for _, value  in pairs(self.link.display) do        
        if value.section then          
          lines[1] = lines[1] ..  value.section;          
          linecounter = 2
          sectioncounter = sectioncounter + 1
          lines[1] = lpad(lines[1], (sectioncounter)*30)
        else          
          if linecounter > #lines then
            lines[#lines + 1] = ""            
            lines[#lines] = lpad(lines[#lines], (sectioncounter - 1)*30)            
          end
          local space = ""
          if value.shortdesc  ~= nil and value.shortdesc ~= "" then
            space = lpad(space, 4)
          end
          lines[linecounter] = lines[linecounter] .. value.shortdesc .. space .. value.desc;          
          lines[linecounter] = lpad(lines[linecounter], sectioncounter*30)
          linecounter = linecounter + 1          
        end
      end    
      for i=1, #lines do
        self.memo:addtext(lines[i] .. "\n")
      end
    end
  end
end

function filenamewithoutextension(filename)
  local result = ""
  local found, len, remainder = string.find(filename, "^(.*)%.[^%.]*$")
  if found then
    result = remainder
  else
    result = path
  end
  return result		
end

function commandhandler:onapplysetting()
  local setting = psycle.setting():find("help")
  if setting and setting.properties then
    local properties = setting.properties
    if properties.helplevel ~= nil and  properties.helplevel:value() then    
      self.helplevel = properties.helplevel:value()    
    end    
  end  
  local setting = psycle.setting():find("keycontrol")
  if setting and setting.properties then
    local properties = setting.properties
    if properties.keymap ~= nil and  properties.keymap:value() then
      local filepath, err = commandhandler.replaceline(properties.keymap:value())        
      if not err then         
        filepath = string.gsub(filepath, [[\]], [[/]])                  
        local sep = "/"  
        self.rootdir = filepath:match("(.*" .. sep ..")") 
        self.index = filepath:match("([^/]+)$")        
      else
        psycle.alert("Could not set keymap path correctly.")
      end      
    end    
  end
  self:loadrootlink(true)  
end

function commandhandler.replaceline(line)
  local output = ""      
  local last = 1
  local err = false
  for text, expr, index in string.gmatch(line, "(.-)$(%b())()") do 
    last = index
    if text ~= "" then
      output = output .. text
    end             
    local trimmedexp = expr:sub(2, -2)      
    local realvalue = psycle.setting():dir(trimmedexp)
    if not realvalue then
       err = true
       break
    else        
      output = output .. realvalue
    end
  end    
  output = output .. string.sub(line, last)    
  return output, err
end

commandhandler.fallbacklink = {
  title = "MAIN MENU -- FALLBACK",
  keymap = {        
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
    {shortdesc = "^O", desc = "Open File"},
    {shortdesc = "^S", desc = "Save File"},
    {shortdesc = "^W", desc = "Close File"},
    {shortdesc = "^F", desc = "Find/Replace Selection"},
    {section = "--Edit keys--"},
    {shortdesc = "^X", desc = "Cut"},
    {shortdesc = "^C", desc = "Copy"},
    {shortdesc = "^V", desc = "Paste"},   
  }
}
      
return commandhandler
