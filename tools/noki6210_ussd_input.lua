-- Own NPE-3 decoded input observer and physical *123#; no firmware writes.
local source = debug.getinfo(1, "S").source:sub(2)
local directory = assert(source:match("^(.*[/])"))
dofile(directory .. "noki6210_staged_observe.lua")
manager.machine.devices[":maincpu"].debug:bpset(0x4fad30, nil,
    'logerror "6210_keypad_decoded: key=%02x\\n",r0;g')
_G.dct3_supplementary_product = "6210"
_G.dct3_supplementary_service = "ussd"
_G.dct3_supplementary_start = 20
_G.dct3_supplementary_send = {0, "Send"}
_G.dct3_supplementary_back = "Right Softkey / C"
dofile(directory .. "dct3_supplementary_input.lua")
