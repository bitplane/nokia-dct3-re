_G.nsb1_toolkit_menu_item=2
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nsb1_toolkit_menu.lua')
