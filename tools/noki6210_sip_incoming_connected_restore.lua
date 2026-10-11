-- Physical Answer and architectural restore; external SIP is cleared, not saved.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
_G.noki6210_state_scenario = 'incoming_connected'
dofile(directory .. 'noki6210_state_roundtrip.lua')
local machine = manager.machine
_G.noki6210_sip_connected_restore = {
    emu.add_machine_pre_save_notifier(function() machine:logerror('sip_state: saved\n') end),
    emu.add_machine_post_load_notifier(function() machine:logerror('sip_state: restored\n') end)
}
