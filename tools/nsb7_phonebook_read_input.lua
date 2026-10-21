-- Physical Names/Find with empty search, then Details; no save keys.
_G.nsb7_menu_steps={{26,'Right Softkey / C'},{29,'Left Softkey / Menu'},
    {32,'Left Softkey / Menu'},{35,'Left Softkey / Menu'}}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb7_menu_observe.lua')
