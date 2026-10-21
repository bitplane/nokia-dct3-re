-- Exact architecture/protocol replay; all continuation input is physical.
_G.nsb7_security_continue=true
local source=debug.getinfo(1,'S').source:sub(2)
local directory=assert(source:match('^(.*[/\\])'))
local scenario=_G.nsb7_state_scenario or 'idle'
assert(scenario=='idle' or scenario=='call' or scenario=='sms')
dofile(directory..(scenario=='call' and 'nsb7_outgoing_call_observe.lua' or 'nsb7_page_observe.lua'))
local machine=manager.machine
-- With the own test00 setting, CP-ACK is at19.3474 and RP-ACK at19.3632.
local save_time=({idle=27,call=33,sms=19.35})[scenario]
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
end)
retained[#retained+1]=emu.add_machine_post_load_notifier(function()
    local restored=snapshot('restored')
    assert(saved,'save observation absent')
    for _,name in ipairs({'time','pc','sp','ram','cpu'}) do
        assert(saved[name]==restored[name],'architectural state mismatch: '..name)
    end
    machine:logerror(string.format('state_roundtrip: result=pass scenario=nsb7_%s requested_at=%.9f t=%.9f\n',scenario,saved.time,restored.time))
    machine:logerror(string.format('state_replay: phase=restored event=begin t=%.9f\n',restored.time))
    local replay=coroutine.create(function()
        assert(emu.wait(3))
        machine:logerror(string.format('state_replay: phase=restored event=end t=%.9f\n',machine.time:as_double()))
        machine.screens[':screen']:snapshot('nsb7_state_'..scenario..'_restored.png')
        if scenario=='sms' then
            local key
            for _,port in pairs(machine.ioport.ports) do key=key or port.fields['Left Softkey / Menu'] end
            assert(key,'Menu contact absent')
            for _,target in ipairs({23,25}) do
                assert(emu.wait(target-machine.time:as_double()))
                machine:logerror(string.format('nsb7_sms_input: key=Menu state=1 t=%.9f\n',machine.time:as_double()))
                key:set_value(1)
                assert(emu.wait(0.2))
                key:set_value(0)
                machine:logerror(string.format('nsb7_sms_input: key=Menu state=0 t=%.9f\n',machine.time:as_double()))
                assert(emu.wait(0.8))
                machine.screens[':screen']:snapshot('nsb7_sms_read_'..target..'.png')
            end
            assert(emu.wait(30-machine.time:as_double()))
            complete=true
            machine:logerror('nsb7_state_sms: complete\n')
            machine:exit()
            return
        end
        if scenario=='call' then
            assert(emu.wait(40-machine.time:as_double()))
            local key
            for _,port in pairs(machine.ioport.ports) do key=key or port.fields['End'] end
            assert(key,'End contact absent')
            machine:logerror(string.format('nsb7_outgoing_call_input: key=End state=1 t=%.9f\n',machine.time:as_double()))
            key:set_value(1)
            assert(emu.wait(0.2))
            key:set_value(0)
            machine:logerror(string.format('nsb7_outgoing_call_input: key=End state=0 t=%.9f\n',machine.time:as_double()))
            assert(emu.wait(50-machine.time:as_double()))
            machine.screens[':screen']:snapshot('nsb7_state_call_final.png')
            machine:logerror(string.format('nsb7_outgoing_call_done: t=%.9f\n',machine.time:as_double()))
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
    assert(emu.wait(save_time))
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
