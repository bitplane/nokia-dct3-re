-- Architectural restore while ringing; external SIP dialog is cleared, not saved.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
_G.noki6210_state_scenario = 'incoming_alerting'
dofile(directory .. 'noki6210_state_roundtrip.lua')
local machine = manager.machine
local ready = coroutine.create(function()
    assert(emu.wait(32))
    machine.screens[':screen']:snapshot('6210_sip_registered_idle.png')
end)
_G.noki6210_sip_alerting_restore = {
    ready,
    emu.add_machine_pre_save_notifier(function() machine:logerror('sip_state: saved\n') end),
    emu.add_machine_post_load_notifier(function() machine:logerror('sip_state: restored\n') end)
}
assert(coroutine.resume(ready))
