-- Physical FDN search; held # shows the number without entering record actions.
_G.nsb1_fdn_contacts={
    {1,4},{2,4},{3,2},{2,4},{1,4},{1,4},{0,8},{1,4},{1,4},{1,4},{4,16}
}
_G.nsb1_fdn_frames={39,43,45,47,49,51}
_G.nsb1_fdn_hold_last=4
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb1_fdn_input.lua')
