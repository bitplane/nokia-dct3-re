-- Firmware-owned time setting; all layout knowledge belongs to this harness.
_G.nsb7_menu_steps={
    {26,'Left Softkey / Menu'},
    {29,'Scroll Down'}, {32,'Scroll Down'}, {35,'Scroll Down'},
    {38,'Left Softkey / Menu'}, {41,'Scroll Down'},
    {44,'Left Softkey / Menu'}, {47,'Scroll Down'},
    {50,'Left Softkey / Menu'},
    {53,'Keypad 1'}, {56,'Keypad 3'}, {59,'Keypad 4'}, {62,'Keypad 7'},
    {65,'Left Softkey / Menu'},
    {68,'Right Softkey / C'}, {71,'Right Softkey / C'}, {74,'Right Softkey / C'},
}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb7_menu_observe.lua')
