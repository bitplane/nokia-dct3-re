-- Own FDN Search/Options/Erase with a separate physical cancellation control.
_G.nsb1_fdn_contacts={
    {1,4},{2,4},{3,2},{2,4},{1,4},{1,4},{0,8},{1,4},{1,4},{1,4},{1,4},
    {0,8},{1,4}, -- Erase
    {3,4},{4,4},{2,8},{3,8},{1,4}, -- PIN25678
    _G.nsb1_fdn_erase_cancel and {1,2} or {1,4}
}
_G.nsb1_fdn_frames={47,49,51,59,61,63,65,67}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb1_fdn_input.lua')
