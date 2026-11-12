-- Laboratory incoming call on nse1ap5: external page plus physical Answer/End.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'c54x_rom4_staged_menu.lua')
dofile(directory .. 'npe3_bts_page_input.lua')
