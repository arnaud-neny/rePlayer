--[[ psycle plugineditor (c) 2015, 2016, 2017 by psycledelics
File: mainviewport.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.  
]]

-- require('mobdebug').start()
-- local serpent = require("psycle.serpent")

local machine = require("psycle.machine")
local machinemodes = require("psycle.machinemodes")
local catcher = require("psycle.plugincatcher")
local keycodes = require("psycle.ui.keycodes")
local alignstyle = require("psycle.ui.alignstyle")
local orderedtable = require("psycle.orderedtable")
local run = require("psycle.run")
local point = require("psycle.ui.point")
local dimension = require("psycle.ui.dimension")
local rect = require("psycle.ui.rect")
local boxspace = require("psycle.ui.boxspace")
local ornamentfactory = require("psycle.ui.ornamentfactory"):new()
local image = require("psycle.ui.image")
local group = require("psycle.ui.group")
local viewport = require("psycle.ui.viewport")
local splitter = require("psycle.ui.splitter")
local combobox = require("psycle.ui.combobox")
local region = require("psycle.ui.region")

local filehelper = require("psycle.file")
local fileexplorer = require("fileexplorer")
local settingspage = require("psycle.ui.settingspage")
local textpage = require("textpage")
local advancededit = require("advancededit")
local commandhandler = require("commandhandler")

local toolicon = require("psycle.ui.toolicon")
local toolbar = require("psycle.ui.toolbar")
local tabgroup = require("psycle.ui.tabgroup")
local titlebaricons = require("psycle.ui.titlebaricons")
local separator = require("psycle.ui.separator")

local project = require("project")
local search = require("search")
local output = require("output")
local callstack = require("callstack")
local pluginselector = require("pluginselector")
local statusbar = require("statusbar")
local run = require("psycle.run")
local serpent = require("psycle.serpent")
local filecommands = require("filecommands")
local editcommands = require("editcommands")
local runcommands = require("runcommands")
local sci = require("scintilladef")


local mainviewport = viewport:new()

function mainviewport:new(machine)
  local c = viewport:new()  
  setmetatable(c, self)
  self.__index = self  
  c.machine_ = machine  
  c:init()
  return c
end

function mainviewport:init()
  self.icondir = psycle.setting():dir("icons")
  psycle.setting():addlistener(self)  
  self:link(require("psycle.ui.standarduisheet"))
  self:onapplysetting()  
  self.machines = machine:new()  
  self:invalidatedirect()
  self:createpluginselector()  
  self:createpagegroup()
  self.fileexplorer = self:createfileexplorer()
  self.commandhandler = commandhandler:new()
                                      :setalign(alignstyle.TOP)
                                      :setautosize(false, false)
                                      :setposition(rect:new(point:new(),dimension:new(0, 140)))
  self:createsearch()
  self:createcommands();  
  self:inittoolbar()  
  self:initcommandhandler()  
  self:createoutputs()
  splitter:new(self, splitter.HORZ)    
  self:add(self.fileexplorer)  
  self.pluginselector.fileexplorer = self.fileexplorer
  splitter:new(self, splitter.VERT)
  self:add(self.pages)
end

function mainviewport:onapplysetting()
  local skinpath = psycle.setting():dir("lua") .. "\\plugineditor\\skin.lua"
  local setting = psycle.setting():find("skin")
  if setting and setting.properties then
    local properties = setting.properties
    if properties.stylesheet ~= nil and 
      properties.stylesheet:value() ~= "" then    
      skinpath = psycle.setting():replaceline(properties.stylesheet:value())   
    end     
  end
  local f, err = loadfile(skinpath)
  if err then
    psycle.alert("Unable to load skin. \n" .. err)
  end
  if f then
    self:link(f()) 
  end
  if self.filecommands_ and self.pages and self.commandhandler then 
    local preventedkeys = self.commandhandler:preventedkeys() 
    self.filecommands_.newpage.preventedkeys = preventedkeys
    self.filecommands_.loadpage.preventedkeys = preventedkeys
    local pages = self.pages.children:windows()
    for i=1, #self.pages do
      if self.pages[i].setpreventedkeys then
         self.pages[i]:setpreventedkeys(preventedkeys)
      end
    end
  end
end  

function mainviewport:initcommandhandler()
  self.commandhandler:addcommands(self.filecommands_, self.editcommands_)  
  self.statusbar:setcommandhandler(self.commandhandler)
  self:add(self.commandhandler)  
end

function mainviewport:createcommands()
  local that = self
  local preventedkeys = that.commandhandler:preventedkeys()
  self.filecommands_ = {    
    newpage = filecommands.build(filecommands.NEWPAGE, that.pages, nil, preventedkeys),
    loadpage = filecommands.build(filecommands.LOADPAGE, that.pages, that.fileexplorer, preventedkeys),  
    saveresume = filecommands.build(filecommands.SAVERESUMEPAGE, that.pages, that.fileexplorer),
    savedone = filecommands.build(filecommands.SAVEDONEPAGE, that.pages, that.fileexplorer),
    saveexit = filecommands.build(filecommands.SAVEEXITPAGE, that.pages, that.fileexplorer),
    abandon = filecommands.build(filecommands.ABANDONPAGE, that.pages),   
    displaysearch = filecommands.build(filecommands.DISPLAYSEARCH, that.search),
    sethelplevel0 = filecommands.build(filecommands.HELPLEVEL, that.commandhandler, 0),  
    sethelplevel1 = filecommands.build(filecommands.HELPLEVEL, that.commandhandler, 1),  
    sethelplevel2 = filecommands.build(filecommands.HELPLEVEL, that.commandhandler, 2),  
    sethelplevel3 = filecommands.build(filecommands.HELPLEVEL, that.commandhandler, 3)  
  }
  self.editcommands_ = {
    undopage = editcommands.build(editcommands.UNDOPAGE, that.pages),
    redopage = editcommands.build(editcommands.REDOPAGE, that.pages),
    deleteblock = editcommands.build(editcommands.DELETEBLOCK, that.pages),
    copyblock = editcommands.build(editcommands.COPYBLOCK, that.pages),
    moveblock = editcommands.build(editcommands.MOVEBLOCK, that.pages),
    setblockbegin = editcommands.build(editcommands.SETBLOCKBEGIN, that.pages),  
    setblockend = editcommands.build(editcommands.SETBLOCKEND, that.pages),
    charleft = editcommands.build(editcommands.CHARLEFT, that.pages),
    charright = editcommands.build(editcommands.CHARRIGHT, that.pages),
    lineup = editcommands.build(editcommands.LINEUP, that.pages),
    linedown = editcommands.build(editcommands.LINEDOWN, that.pages),
    wordleft = editcommands.build(editcommands.WORDLEFT, that.pages),
    wordright = editcommands.build(editcommands.WORDRIGHT, that.pages),
    cut = editcommands.build(editcommands.CUT, that.pages),
    copy = editcommands.build(editcommands.COPY, that.pages),
    paste = editcommands.build(editcommands.PASTE, that.pages),
    delete = editcommands.build(editcommands.DELETE, that.pages),
    selall = editcommands.build(editcommands.SELALL, that.pages),
  }
  self.runcommands_ = {
    run = runcommands.build(runcommands.RUN, that)
  }
end

function mainviewport:filecommands()
  return self.filecommands_;
end

function mainviewport:editcommands()
  return self.editcommands_;
end

function mainviewport:runcommands()
  return self.runcommands_;
end

function mainviewport:createpluginselector()  
  self.pluginselector 
      = pluginselector:new(self)
                      :hide()
                      :setalign(alignstyle.TOP)
                      :setautosize(false, false)
                      :setposition(rect:new(point:new(), dimension:new(0, 200)))
  self.pluginselector.doopen:connect(mainviewport.onopenplugin, self)
  self.pluginselector.docreatemodule:connect(mainviewport.oncreatemodule, self)
end

function mainviewport:onopenplugin(machinemeta) 
  self:openplugin(machinemeta)
  psycle.proxy.project = project:new():setplugininfo(machinemeta)
  self:fillinstancecombobox()  
  psycle.proxy:settitle(machinemeta:name())
end

function mainviewport:oncreatemodule(outputs)    
  for _, output in pairs(outputs) do    
    self:openfromfile(output.realpath)    
  end  
  self.pluginselector:hide()
  self.fileexplorer:update()
  self:setfocus()  
  self:updatealign()
end

function mainviewport:createsearch() 
  self.search
      = search:new(self)
              :setposition(rect:new(point:new(), dimension:new(200, 200)))
              :hide()
              :setalign(alignstyle.BOTTOM)    
  self.search.dosearch:connect(mainviewport.onsearch, self)
  self.search.doreplace:connect(mainviewport.onreplace, self)
  self.search.dohide:connect(mainviewport.onsearchhide, self)
  self.searchbeginpos = -1
  self.searchrestarted = false
end

function mainviewport:createoutputs()
  self.outputs 
       = tabgroup:new(self)
                 :setposition(rect:new(point:new(), dimension:new(0, 180)))
                 :setalign(alignstyle.BOTTOM)
  self.output = output:new()  
  self.outputs:addpage(self.output, "Output", true)    
  self.callstack = self:createcallstack()  
  self.outputs:addpage(self.callstack, "Call stack", true)  
end

function mainviewport:setindicator(page)
   page:f(sci.SCI_INDICSETSTYLE, 8, sci.INDIC_PLAIN)
   page:f(sci.SCI_INDICSETSTYLE, 9, sci.INDIC_SQUIGGLE)
   page:f(sci.SCI_INDICSETSTYLE, 10, sci.INDIC_TT)
   page:f(sci.SCI_INDICSETSTYLE, 11, sci.INDIC_DIAGONAL)
   page:f(sci.SCI_INDICSETSTYLE, 12, sci.INDIC_STRIKE)
   page:f(sci.SCI_INDICSETSTYLE, 13, sci.INDIC_BOX)
   page:f(sci.SCI_INDICSETSTYLE, 14, sci.INDIC_ROUNDBOX)
   page:f(sci.SCI_SETINDICATORCURRENT, 14, 0)
   page:f(sci.SCI_POSITIONFROMLINE, 14, 0)
   page:f(sci.SCI_INDICATORFILLRANGE, 2, 5)
end

function mainviewport:createcallstack()
  local callstack = callstack:new()
  callstack.change:connect(mainviewport.openinfo, self)  
  return callstack
end

function mainviewport:createpagegroup()
  self.pages = tabgroup:new()
                       :setalign(alignstyle.CLIENT)             
  self.pages.dopageclose:connect(mainviewport.onclosepage, self)  
end

function mainviewport:createfileexplorer()
  local fileexplorer = fileexplorer:new(nil, self.machine_.settingsmanager:setting().fileexplorer)
                                   :setposition(rect:new(point:new(), dimension:new(200, 0)))
                                   :setalign(alignstyle.LEFT)
  fileexplorer:setpath(psycle.setting():dir("lua"))  
  fileexplorer.click:connect(mainviewport.onpluginexplorerclick, self)  
  --pluginexplorer.onremove:connect(mainviewport.onpluginexplorernoderemove, self)
  return fileexplorer
end

function mainviewport:setoutputtext(text)
  self.output:clearall()
  self.output:addtext(text)  
end

function mainviewport:setplugindir(plugininfo)  
  local path = plugininfo:dllname()
  path = path:sub(1, -5).."\\"
  self.fileexplorer:setpath(path)
  self.machineselector:settext(plugininfo:name())  
end

function mainviewport:dopageexist(fname)
  local found = nil
  local windows = self.pages.children:windows()    
  for i=1, #windows do
    local page = windows[i]
    if page:filename() == fname then
      found = page
      break
    end
  end
  return found
end

function mainviewport:onkeydown(ev)  
  if ev:ctrlkey() then
      if self.commandhandler:haskeycode(ev:keycode()) then
        self.statusbar:setcontrolkey(ev:keycode())        
        self.commandhandler:jump(ev:keycode())
        if self.commandhandler.activated == 1 then
          self.statusbar:setfocus()                  
        end
        ev:stoppropagation()
      end    
  end
  self:onidle()
  ev:stoppropagation()  
end

function mainviewport:openfromfile(fname, line)
  if not line  then line = 0 end
  local page = self:dopageexist(fname)  
  if page ~= nil then
    page:reload()
    if self.pages:activepage() ~= page then   
      self.pages:setactivepage(page)
    end
  else    
    page = textpage:new()
    page:loadfile(fname)    
    local sep = "\\"  
    local dir = fname:match("(.*"..sep..")")    
    --self:addfiletowatcher(dir)  
    local name = fname:match("([^\\]+)$")           
    self.pages:addpage(page, name)         
  end  
  page:gotoline(line - 1)    
end

function mainviewport:setcallstack(trace)
  for i=1, #trace do    
    self.callstack:addline(trace[i])
  end  
end

function mainviewport:playplugin()  
  local meta = psycle.proxy.project:plugininfo()
  if meta and meta:mode() == machinemodes.HOST then
    psycle.alert("Use the View menu to start the host extension.")
    self.playicon:resethover()
  else
    local pluginindex = psycle.proxy.project:pluginindex()  
    if pluginindex ~= -1 then
       machine = machine:new(pluginindex)
       if machine then
         self.filecommands_.saveresume:execute()
         machine:reload()        
         self.playicon:seton(true)     
       end
    else
      self.filecommands_.saveresume:execute() 
      if meta then     
        local fname = meta:dllname():match("([^\\]+)$"):sub(1, -5)    
        machine = machine:new(fname)      
        local pluginindex = self.machines:insert(machine)    
        psycle.proxy.project:setpluginindex(pluginindex)    
        self:fillinstancecombobox()
        self:setpluginindex(pluginindex)
        self.playicon:seton(true)   
      end
    end
  end
end

function mainviewport:inittoolbar()  
  self.tg = group:new(self)
                 :setautosize(false, true)
                 :setalign(alignstyle.TOP)
  self.windowtoolbar = titlebaricons:new(self.tg)
  local that = self
  function self.windowtoolbar.settingsicon:onclick()
    local settingspage = settingspage:new()                                        
    if settingspage then        
      that.pages:addpage(settingspage, "Settings")
    end    
  end
  self:initmachineselector(self.tg)
  self:initfiletoolbar():setalign(alignstyle.LEFT)
  separator:new(self.tg):setalign(alignstyle.LEFT):setmargin(boxspace:new(10))
  self:initplaytoolbar():setalign(alignstyle.LEFT)
  self:initstatusbar()  
end

function mainviewport:createreloadicon()
  local reloadicon = toolicon:new(self.tg, self.icondir .. "reload.png", 0xFFFFFF)
  self:formattoolicon(reloadicon)
  reloadicon:setalign(alignstyle.RIGHT)                
  local that = self
  function reloadicon:onclick()
    that.machine_:reload()
  end  
end

function mainviewport:initmachineselector(parent)
  self.machineselector = 
      advancededit:new(parent)
                  :setalign(alignstyle.LEFT)
                  :setposition(rect:new(point:new(), dimension:new(200, 40)))
  local that = self  
  function self.machineselector:onmousedown(ev)    
    that.pluginselector:show()
    that:updatealign() 
    advancededit.onmousedown(self, ev)  
  end  
  function self.machineselector:onkillfocus(ev)    
    that:updatealign()  
    advancededit.onkillfocus(self, ev)  
  end  
  function self.machineselector:onkeydown(ev)    
    advancededit.onkeydown(self, ev)
    if ev:keycode() == keycodes.ENTER then
      that:onmachineselectorreturnkey(self:text())
      that.pluginselector:hide()
    end 
    if ev:keycode() == keycodes.ESCAPE then      
      that.pluginselector:hide()
      that:updatealign()
    end
  end
  function self.machineselector:onkeyup(ev)
    if self.selectorhasfocus then
      that.pluginselector:searchandfocusmachine(self:text())
      that:updatealign()    
    end
    advancededit.onkeyup(self, ev)  
  end  
end

function mainviewport:onmachineselectorreturnkey(text)  
   self.pluginselector:openselectednode()
end

function mainviewport:initstatusbar()
  self.statusbar = statusbar:new(self)
                            :setautosize(false, true)
                            :setalign(alignstyle.TOP) 
                            :setmargin(boxspace:new(3, 0, 2, 0))
  self.statusbar.gotopage:connect(mainviewport.ongotopage, self)
  self.statusbar.escape:connect(mainviewport.onstatuslineescape, self)
end

function mainviewport:ongotopage(edit)
  if self.pages:activepage() then   
     local number = tonumber(edit:text())
     if number then
       self.pages:activepage():gotoline(number - 1)
     end
     self.pages:activepage():setfocus()
  end
end

function mainviewport:onstatuslineescape(edit)
  if self.pages:activepage() then   
     self.pages:activepage():setfocus()
  end
end

function mainviewport:fillinstancecombobox()
  local items = {"new instance"}
  self.cbxtopluginindex = {-1}
  if (psycle.proxy.project:plugininfo()) then
    for machineindex= 0, 255 do
      local machine = machine:new(machineindex);       
      if machine and machine:type() == machine.MACH_LUA and machine:pluginname() == psycle.proxy.project:plugininfo():name() then
        items[#items + 1] = machine:pluginname() .. "[" .. machineindex .. "]"
        self.cbxtopluginindex[#self.cbxtopluginindex + 1] = machineindex
      end
    end     
  end
  self.cbx:setitems(items)
  self.cbx:setitemindex(1)
  return self
end  

function mainviewport:setpluginindex(pluginindex)
  local cbxindex = 1  
  for i = 1, #self.cbxtopluginindex do    
    if self.cbxtopluginindex[i] == pluginindex then       
       cbxindex = i
       break    
    end
  end  
  self.cbx:setitemindex(cbxindex)  
end

function mainviewport:createinstanceselect(parent)  
  self.cbx = combobox:new(parent)
                     :setautosize(false, false)
                     :setposition(rect:new(point:new(0, 0), dimension:new(150, 0)))
                     :setalign(alignstyle.LEFT)
                     :setmargin(boxspace:new(10))
  local that = self
  function self.cbx:onselect()    
    local pluginindex = that.cbxtopluginindex[self:itemindex()]
    if pluginindex then
      psycle.proxy.project:setpluginindex(pluginindex)       
    else
      psycle.proxy.project:setpluginindex(-1)       
    end
  end
  self.cbx:setitems({"new instance"})
  self.cbx:setitemindex(1)
end

function mainviewport:formattoolicon(icon)    
  icon:setautosize(false, false)
  icon:setverticalalignment(toolicon.CENTER)
  icon:setjustify(toolicon.CENTERJUSTIFY)
  icon:setposition(rect:new(point:new(), dimension:new(40, 0)))
  return self
end

function mainviewport:initfiletoolbar()  
  local t = toolbar:new(self.tg)
                   :setautosize(true, false)
  local that = self                   
  local transparency = 0xFFFFFF
  local icons = {   
    inew = toolicon:new(t, that.icondir .. "new.png", transparency),                      
    iopen = toolicon:new(t, that.icondir .. "open.png", transparency),                        
    isave = toolicon:new(t, that.icondir .. "save.png", transparency),
    iundo = toolicon:new(t, that.icondir .. "undo.png", transparency),
    iredo = toolicon:new(t, that.icondir .. "redo.png", transparency)
  }
  for _, icon in pairs(icons) do    
    self:formattoolicon(icon)
  end  
  icons.inew:setcommand(self.filecommands_.newpage)  
  icons.isave:setcommand(self.filecommands_.saveresume)  
  icons.iundo:setcommand(self.editcommands_.undopage) 
  icons.iredo:setcommand(self.editcommands_.redopage)
  icons.iopen:setcommand(self.filecommands_.loadpage);  
  return t
end

function mainviewport:initplaytoolbar()
  local t = toolbar:new(self.tg)
                   :setautosize(true, false)  
  self.playicon = toolicon:new(t, self.icondir .. "poweroff.png", 0xFFFFFF)                          
  self:formattoolicon(self.playicon)
  self:formattoolicon(self.playicon)
  self.playicon.is_toggle = true
  local on = image:new():load(self.icondir .. "poweron.png")
  on:settransparent(0xFFFFFF)
  self.playicon:settoggleimage(on)
  self.playicon:setcommand(self.runcommands_.run)
  self:createinstanceselect(t)
  return t
end

function mainviewport:openinfo(info)
  local isfile = false
  if info.source:len() > 1 then
     local firstchar = info.source:sub(1, 1)     
     if firstchar == "@" then
       local fname = info.source:sub(2) 
       self:openfromfile(fname, info.line)
       isfile = true
     end
  end
  return isfile
end

function mainviewport:onsearch(searchtext, dir, case, wholeword, regexp)
  local page = self.pages:activepage()
  if page then 
    page:onsearch(searchtext, dir, case, wholeword, regexp)
  end
end

function mainviewport:onreplace()
  local page = self.pages:activepage()
  if page then 
    if page:hasselection() then     
      page:replacesel(self.search.replacefield:text())      
    end                 
    self:onsearch(self.search.edit:text(), 
                  self.search.DOWN,
                  self.search.casesensitive:checked(),
                  self.search.wholeword:checked(),
                  self.search.useregexp:checked())
  end
end

function mainviewport:openplugin(info)
  self:preventdraw()
  self.pages:removeall()      
  self.fileexplorer:sethomepath(psycle.setting():dir("lua") .. "\\" .. info.machinepath)  
  self:openfromfile(psycle.setting():dir("lua") .. "\\" .. info.machinepath .. "\\machine.lua")  
  self.machineselector:settext(info:name())
  self.machineselector:resetcache()
  self:updatealign():enabledraw():invalidate()  
end

function mainviewport:onpluginexplorerclick(ev)
   if ev.filename ~= "" and ev.path then       
     self:openfromfile(ev.path .. "\\" .. ev.filename, 0)
   end
end

function mainviewport:onpluginexplorernoderemove(node)  
  node:parent():remove(node)
  self.pluginexplorer:updatetree()
  local windows = self.pages.children:windows()
  for i = 1, #windows do        
    if node.path..node.filename == windows[i]:filename() then
      self.pages:removepage(windows[i])
      filehelper.remove(node.path..node.filename)
      break;
    end
  end  
end

function mainviewport:onidle()
  local activepage = self.pages:activepage()
  if activepage and activepage.status then  
    self.statusbar:updatestatus(activepage:status())    
  end  
  self:updatepowericon()
end

function mainviewport:updatepowericon()
  if self.cbxtopluginindex ~= nil then    
    local isselectedmachinemute = self.machines:muted(self.cbxtopluginindex[self.cbx:itemindex()])  
    if self.playicon:on() == isselectedmachinemute then      
      self.playicon:toggle()
    end
  end  
end

function mainviewport:onsearchhide()
  self.search:hide():updatealign()
  if self.pages:activepage() then
    self.pages:activepage():setfocus()
  end
end

function mainviewport:onclosepage(ev)  
  if ev.page:modified() and psycle.confirm("Do you want to save changes to " .. ev.page:filename() .. "?") then
    self:savepage()
  end
end

function mainviewport:onmousedown(ev)
  self.commandhandler:deactivate()
end

return mainviewport
