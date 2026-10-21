-- Physical Names/Add entry: A / 12345. No menu knowledge enters the driver.
_G.nsb7_menu_steps={
    {26,'Right Softkey / C'},{29,'Scroll Down'},{32,'Left Softkey / Menu'},
    {35,'Keypad 2'},{38,'Left Softkey / Menu'},
    {41,'Keypad 1'},{44,'Keypad 2'},{47,'Keypad 3'},{50,'Keypad 4'},{53,'Keypad 5'},
    {56,'Left Softkey / Menu'},
}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb7_menu_observe.lua')
