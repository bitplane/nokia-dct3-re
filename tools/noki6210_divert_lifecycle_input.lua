-- Own NPE-3 observation and physical forwarding lifecycle.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
dofile(directory .. 'noki6210_staged_observe.lua')
manager.machine.devices[':maincpu'].debug:bpset(0x4fad30, nil,
    'logerror "6210_keypad_decoded: key=%02x\\n",r0;g')
_G.dct3_divert_product = '6210'
_G.dct3_divert_start = 20
_G.dct3_divert_save_window = _G.noki6210_state_scenario == 'divert' and 3 or nil
dofile(directory .. 'dct3_divert_lifecycle_input.lua')
_G.noki6210_resume_divert = _G.dct3_resume_divert
