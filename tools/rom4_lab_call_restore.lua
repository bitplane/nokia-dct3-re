-- Laboratory incoming call with a save/replay/load round trip mid-call.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'c54x_rom4_staged_observe.lua')
dofile(directory .. 'npe3_bts_page_input.lua')
dofile(directory .. 'c54x_rom4_codec_restore.lua')
