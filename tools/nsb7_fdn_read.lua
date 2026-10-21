-- Physical FDN Numbers/Search, empty search and Details after a cold boot.
_G.nsb7_security_with_pin=true
_G.nsb7_menu_steps={{26,'Left Softkey / Menu'},{28,'Keypad 4'},
    {30,'Keypad 6'},{32,'Keypad 4'},{35,'Left Softkey / Menu'},
    {38,'Left Softkey / Menu'},{41,'Scroll Down'},{44,'Scroll Down'},
    {47,'Left Softkey / Menu'},{50,'Left Softkey / Menu'},
    {53,'Left Softkey / Menu'},{56,'Left Softkey / Menu'}}
if _G.nsb7_fdn_empty then table.remove(_G.nsb7_menu_steps) end
for _,step in ipairs(_G.nsb7_fdn_record_steps or {}) do
    _G.nsb7_menu_steps[#_G.nsb7_menu_steps+1]=step
end
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_menu_observe.lua')
