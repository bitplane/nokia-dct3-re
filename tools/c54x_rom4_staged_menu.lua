-- Physical input/LCD oracle plus passive staged CPU and upload observations.
local directory = assert(debug.getinfo(1, "S").source:match("^@(.*/)"))
dofile(directory .. "../mame_nokia_dct3_input_exerciser.lua")
dofile(directory .. "c54x_rom4_staged_observe.lua")
