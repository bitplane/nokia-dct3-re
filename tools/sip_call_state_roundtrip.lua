-- Physical input plus emulator save/load; no firmware-state mutation.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
dofile(directory .. '../mame_nokia_dct3_input_exerciser.lua')
local machine = manager.machine
local idle = _G.sip_state_scenario == 'idle'
local outgoing = _G.sip_state_scenario == 'outgoing'
local saved, loaded = false, false
local before = emu.add_machine_pre_save_notifier(function()
    saved = true
    machine:logerror('sip_state: saved\n')
end)
local after = emu.add_machine_post_load_notifier(function()
    assert(saved, 'SIP call snapshot was never saved')
    loaded = true
    machine:logerror('sip_state: restored\n')
    if idle then
        local answer = coroutine.create(function()
            assert(emu.wait(5))
            local key = assert(machine.ioport.ports[':COL.1'].fields['Navi / Left Softkey'])
            machine:logerror('sip_state: physical Answer after idle restoration\n')
            key:set_value(1)
            assert(emu.wait(0.22))
            key:set_value(0)
        end)
        _G.sip_idle_answer = answer
        assert(coroutine.resume(answer))
    end
end)
local runner = coroutine.create(function()
    assert(emu.wait(outgoing and 25 or 19))
    local name = idle and 'sip_idle' or outgoing and 'sip_outgoing' or 'sip_connected'
    machine:save(name)
    assert(emu.wait(1))
    assert(saved, 'SIP call save failed')
    machine:load(name)
end)
_G.sip_call_state_fixture = {before, after, runner,
    emu.add_machine_stop_notifier(function()
        assert(loaded, 'SIP call restore never completed')
    end)}
assert(coroutine.resume(runner))
