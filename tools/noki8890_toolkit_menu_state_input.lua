local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
_G.dct3_toolkit_menu_state_product = '8890'
dofile(directory .. 'dct3_toolkit_menu_state.lua')
