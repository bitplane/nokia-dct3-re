-- Own-product startup stays interactive while rejecting a busy DISPLAY TEXT.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8890_clock_settlement_only = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_clock_input.lua')
