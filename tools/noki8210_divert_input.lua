_G.noki8210_supplementary_service = 'divert'
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki8210_supplementary_input.lua')
