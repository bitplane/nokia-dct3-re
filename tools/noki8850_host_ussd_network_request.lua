-- Read reveals Answer; the second softkey opens the reply editor.
_G.nsm2_host_ussd_actions = {
    {35, 1, 'Menu'}, {37, 1, 'Menu'},
    {39, 3, 'Keypad 2'}, {41, 1, 'Menu'},
    {45, 0, 'End'}, {47, 1, 'Menu'},
}
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/])') .. 'noki8850_host_ussd_notify.lua')
