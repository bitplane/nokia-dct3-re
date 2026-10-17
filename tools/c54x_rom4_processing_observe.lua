-- Combine passive control and arithmetic observations for the no-cell gate.
local directory = assert(debug.getinfo(1, "S").source:match("^@(.*/)"))
dofile(directory .. "c54x_rom4_codec_observe.lua")
dofile(directory .. "c54x_rom4_mode_observe.lua")
dofile(directory .. "c54x_rom4_comparison_observe.lua")
dofile(directory .. "c54x_rom4_transfer_observe.lua")
dofile(directory .. "c54x_rom4_rf_consumers_observe.lua")
