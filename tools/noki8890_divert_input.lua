-- Own security and clock/date setup followed by physical *#21# interrogation.
local source = debug.getinfo(1, "S").source:sub(2)
local directory = assert(source:match("^(.*[/])"))
_G.noki8890_clock_settlement_only = true
dofile(directory .. "noki8890_clock_input.lua")
_G.dct3_supplementary_product = "8890"
_G.dct3_supplementary_start = 45
_G.dct3_supplementary_service = "divert"
dofile(directory .. "dct3_supplementary_input.lua")
