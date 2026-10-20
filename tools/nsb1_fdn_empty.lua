-- Stop after submitting the empty Search name, before dismissing its result.
_G.nsb1_fdn_contacts={
    {1,4},{2,4},{3,2},{2,4},{1,4},{1,4},{0,8},{1,4},{1,4},{1,4}
}
_G.nsb1_fdn_frames={43,45,47,49}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb1_fdn_input.lua')
