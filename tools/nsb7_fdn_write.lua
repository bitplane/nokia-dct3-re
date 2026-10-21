-- Physical Fixed dialing/Numbers/Add; PIN2 and updates remain firmware-owned.
_G.nsb7_security_with_pin=true
_G.nsb7_menu_steps={{26,'Left Softkey / Menu'},{28,'Keypad 4'},
    {30,'Keypad 6'},{32,'Keypad 4'},{35,'Left Softkey / Menu'},
    {38,'Left Softkey / Menu'},{41,'Scroll Down'},{44,'Scroll Down'},
    {47,'Left Softkey / Menu'},{50,'Scroll Down'},{53,'Left Softkey / Menu'},
    {56,'Keypad 5'},{57,'Keypad 6'},{58,'Keypad 7'},{59,'Keypad 8'},
    {60,'Left Softkey / Menu'},{63,'Keypad 2'},{66,'Left Softkey / Menu'},
    {70,'Keypad 1'},{71,'Keypad 2'},{72,'Keypad 3'},{73,'Keypad 4'},
    {74,'Keypad 5'},{75,'Left Softkey / Menu'}}
if _G.nsb7_fdn_wrong_pin2 then
    _G.nsb7_menu_steps[15]={59,'Keypad 5'}
    while #_G.nsb7_menu_steps>16 do table.remove(_G.nsb7_menu_steps) end
end
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_menu_observe.lua')
