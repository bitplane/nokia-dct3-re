-- Physical supplementary-service input after the own-product security fixture.
local source = debug.getinfo(1, "S").source:sub(2)
local directory = assert(source:match("^(.*[/])"))
_G.noki8850_security_only = true
dofile(directory .. "noki8850_security_input.lua")
_G.dct3_ussd_product = "8850"
_G.dct3_ussd_start = 25
dofile(directory .. "dct3_ussd_input.lua")
