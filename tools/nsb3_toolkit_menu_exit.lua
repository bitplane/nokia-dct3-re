_G.nsb3_toolkit_menu_exit = true
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nsb3_toolkit_menu.lua')
