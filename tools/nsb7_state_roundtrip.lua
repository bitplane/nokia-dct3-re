-- Exact architecture/protocol replay; all continuation input is physical.
_G.nsb7_security_continue=true
local source=debug.getinfo(1,'S').source:sub(2)
local directory=assert(source:match('^(.*[/\\])'))
local scenario=_G.nsb7_state_scenario or 'idle'
assert(scenario=='idle' or scenario=='call' or scenario=='sms' or scenario=='incoming_call' or scenario=='toolkit_menu' or scenario=='ussd')
local observer=_G.nsb7_state_observer or (scenario=='call' and 'nsb7_outgoing_call_observe.lua' or
    scenario=='incoming_call' and 'nsb7_call_observe.lua' or 'nsb7_page_observe.lua')
dofile(directory..observer)
local machine=manager.machine
-- Default PIN-disabled SMS: CP-ACK19.3474 precedes RP-ACK19.3632.
local save_time=_G.nsb7_state_save_time or ({idle=27,call=33,sms=19.35,incoming_call=23})[scenario]
local capture=dofile(directory..'arm_architecture_snapshot.lua')
local saved,complete
local retained={}
local function snapshot(event)
    local state=capture(machine)
    machine:logerror(string.format('nsb7_state: event=%s pc=%08x sp=%08x ram=%08x cpu=%s t=%.9f\n',
        event,state.pc,state.sp,state.ram,state.cpu,state.time))
    return state
end
retained[#retained+1]=emu.add_machine_pre_save_notifier(function()
    saved=snapshot('saved')
    machine:logerror(string.format('state_replay: phase=reference event=begin t=%.9f\n',saved.time))
    if _G.nsb7_state_replay_action then _G.nsb7_state_replay_action('reference') end
end)
retained[#retained+1]=emu.add_machine_post_load_notifier(function()
    local restored=snapshot('restored')
    assert(saved,'save observation absent')
    for _,name in ipairs({'time','pc','sp','ram','cpu'}) do
        assert(saved[name]==restored[name],'architectural state mismatch: '..name)
    end
    machine:logerror(string.format('state_roundtrip: result=pass scenario=nsb7_%s requested_at=%.9f t=%.9f\n',scenario,saved.time,restored.time))
    machine:logerror(string.format('state_replay: phase=restored event=begin t=%.9f\n',restored.time))
    if _G.nsb7_state_replay_action then _G.nsb7_state_replay_action('restored') end
    local replay=coroutine.create(function()
        assert(emu.wait(3))
        machine:logerror(string.format('state_replay: phase=restored event=end t=%.9f\n',machine.time:as_double()))
        machine.screens[':screen']:snapshot('nsb7_state_'..scenario..'_restored.png')
        if scenario=='sms' then
            local key
            for _,port in pairs(machine.ioport.ports) do key=key or port.fields['Left Softkey / Menu'] end
            assert(key,'Menu contact absent')
            for _,target in ipairs(_G.nsb7_sms_read_times or {23,25}) do
                assert(emu.wait(target-machine.time:as_double()))
                machine:logerror(string.format('nsb7_sms_input: key=Menu state=1 t=%.9f\n',machine.time:as_double()))
                key:set_value(1)
                assert(emu.wait(0.2))
                key:set_value(0)
                machine:logerror(string.format('nsb7_sms_input: key=Menu state=0 t=%.9f\n',machine.time:as_double()))
                assert(emu.wait(0.8))
                machine.screens[':screen']:snapshot('nsb7_sms_read_'..target..'.png')
            end
            assert(emu.wait((_G.nsb7_state_finish_time or 30)-machine.time:as_double()))
            complete=true
            machine:logerror('nsb7_state_sms: complete\n')
            machine:exit()
            return
        end
        if scenario=='call' or scenario=='incoming_call' or scenario=='toolkit_menu' or scenario=='ussd' then
            local incoming=scenario=='incoming_call'
            local end_time=_G.nsb7_state_end_time or (incoming and 27 or 40)
            local finish_time=_G.nsb7_state_finish_time or (incoming and 35 or 50)
            local prefix=scenario=='ussd' and 'nsb7_ussd_state_input' or scenario=='toolkit_menu' and 'nsb7_toolkit_state_input' or
                incoming and 'nsb7_call_input' or 'nsb7_outgoing_call_input'
            assert(emu.wait(end_time-machine.time:as_double()))
            local key
            for _,port in pairs(machine.ioport.ports) do key=key or port.fields['End'] end
            assert(key,'End contact absent')
            machine:logerror(string.format('%s: key=End state=1 t=%.9f\n',prefix,machine.time:as_double()))
            key:set_value(1)
            assert(emu.wait(0.2))
            key:set_value(0)
            machine:logerror(string.format('%s: key=End state=0 t=%.9f\n',prefix,machine.time:as_double()))
            assert(emu.wait(finish_time-machine.time:as_double()))
            machine.screens[':screen']:snapshot('nsb7_state_'..scenario..'_final.png')
            if scenario=='call' then
                machine:logerror(string.format('nsb7_outgoing_call_done: t=%.9f\n',machine.time:as_double()))
            end
            complete=true
            machine:exit()
            return
        end
        local key
        for _,port in pairs(machine.ioport.ports) do
            key=key or port.fields['Left Softkey / Menu']
        end
        assert(key,'Menu contact absent')
        machine:logerror('nsb7_state_physical: key=Menu state=1\n')
        key:set_value(1)
        assert(emu.wait(0.2))
        key:set_value(0)
        machine:logerror('nsb7_state_physical: key=Menu state=0\n')
        assert(emu.wait(0.8))
        machine.screens[':screen']:snapshot('nsb7_state_idle_menu.png')
        complete=true
        machine:exit()
    end)
    retained[#retained+1]=replay
    assert(coroutine.resume(replay))
end)
local runner=coroutine.create(function()
    if _G.nsb7_state_wait_for_save then
        _G.nsb7_state_wait_for_save()
    else
        assert(emu.wait(save_time))
    end
    machine:save('nsb7_'..scenario)
    assert(emu.wait(3))
    assert(saved,'save did not execute')
    machine:logerror(string.format('state_replay: phase=reference event=end t=%.9f\n',machine.time:as_double()))
    machine.screens[':screen']:snapshot('nsb7_state_'..scenario..'_reference.png')
    machine:load('nsb7_'..scenario)
end)
retained[#retained+1]=runner
retained[#retained+1]=emu.add_machine_stop_notifier(function()
    if not complete then machine:logerror('nsb7_state: FAIL incomplete\n') end
end)
_G.nsb7_state_retained=retained
assert(coroutine.resume(runner))
