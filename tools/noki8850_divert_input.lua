-- Own security setup followed by physical *#21# interrogation.
local source = debug.getinfo(1, "S").source:sub(2)
local directory = assert(source:match("^(.*[/])"))
_G.noki8850_security_only = true
dofile(directory .. "noki8850_security_input.lua")
_G.dct3_supplementary_product = "8850"
_G.dct3_supplementary_start = 25
_G.dct3_supplementary_service = "divert"
dofile(directory .. "dct3_supplementary_input.lua")
