-- Own FDN Search/Options/Edit; preserve name, replace the last number digit.
_G.nsb1_fdn_contacts={
    {1,4},{2,4},{3,2},{2,4},{1,4},{1,4},{0,8},{1,4},{1,4},{1,4},{1,4},
    {1,4},{3,4},{4,4},{2,8},{3,8},{1,4}, -- Edit/PIN25678
    {1,4},{1,2},{4,4},{1,4} -- Accept name, Clear,6,Save
}
_G.nsb1_fdn_frames={47,49,59,61,63,65,67,69,71}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb1_fdn_input.lua')
