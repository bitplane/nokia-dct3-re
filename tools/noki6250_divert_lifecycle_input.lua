-- Own NHM-3 passive observer; no NPE-3 firmware addresses or provisioning.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
dofile(directory .. 'noki6250_runtime_observe.lua')
_G.dct3_divert_product = '6250'
_G.dct3_divert_start = 20
_G.dct3_divert_save_window = _G.noki6250_state_divert and 3 or nil
dofile(directory .. 'dct3_divert_lifecycle_input.lua')
