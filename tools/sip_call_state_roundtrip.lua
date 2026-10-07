-- Physical input plus emulator save/load; no firmware-state mutation.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
dofile(directory .. '../mame_nokia_dct3_input_exerciser.lua')
local machine = manager.machine
local saved, loaded = false, false
local before = emu.add_machine_pre_save_notifier(function()
    saved = true
    machine:logerror('sip_state: saved\n')
end)
local after = emu.add_machine_post_load_notifier(function()
    assert(saved, 'SIP call snapshot was never saved')
    loaded = true
    machine:logerror('sip_state: restored\n')
end)
local runner = coroutine.create(function()
    assert(emu.wait(19))
    machine:save('sip_connected')
    assert(emu.wait(1))
    assert(saved, 'SIP call save failed')
    machine:load('sip_connected')
end)
_G.sip_call_state_fixture = {before, after, runner,
    emu.add_machine_stop_notifier(function()
        assert(loaded, 'SIP call restore never completed')
    end)}
assert(coroutine.resume(runner))
