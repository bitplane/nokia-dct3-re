-- Read and open Answer, then dismiss the empty editor with the physical End key.
_G.nsm2_host_ussd_actions = {
    {35, 1, 'Menu'}, {37, 1, 'Menu'}, {39, 0, 'End'}, {45, 1, 'Menu'},
}
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki8850_host_ussd_notify.lua')
