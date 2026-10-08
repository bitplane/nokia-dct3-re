-- Own NHM-3 observer and physical forwarding interrogation.
local source = debug.getinfo(1, "S").source:sub(2)
local directory = assert(source:match("^(.*[/])"))
dofile(directory .. "noki6250_runtime_observe.lua")
_G.dct3_supplementary_product = "6250"
_G.dct3_supplementary_service = "divert"
_G.dct3_supplementary_start = 20
_G.dct3_supplementary_send = {0, "Send"}
_G.dct3_supplementary_back = "Right Softkey / C"
dofile(directory .. "dct3_supplementary_input.lua")
