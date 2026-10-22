-- Restore handset state, never replay an external SIP dialog.
local source=debug.getinfo(1,'S').source:sub(2)
local directory=assert(source:match('^(.*[/\\])'))
dofile(directory..'nsb7_pin_host_outgoing_call.lua')
local machine=manager.machine
local capture=dofile(directory..'arm_architecture_snapshot.lua')
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local saved,complete
local retained={}
local function snapshot(event)
    local value=capture(machine)
    machine:logerror(string.format('nsb7_sip_state: event=%s pc=%08x sp=%08x ram=%08x cpu=%s t=%.9f\n',
        event,value.pc,value.sp,value.ram,value.cpu,value.time))
    return value
end
retained[#retained+1]=emu.add_machine_pre_save_notifier(function()
    saved=snapshot('saved')
    machine.screens[':screen']:snapshot('nsb7_sip_state_saved.png')
    machine:logerror('sip_state: saved\n')
end)
retained[#retained+1]=emu.add_machine_post_load_notifier(function()
    local restored=snapshot('restored')
    assert(saved,'save observation absent')
    for _,name in ipairs({'time','pc','sp','ram','cpu'}) do
        assert(saved[name]==restored[name],'architectural state mismatch: '..name)
    end
    machine.screens[':screen']:snapshot('nsb7_sip_state_restored.png')
    machine:logerror('sip_state: restored\n')
    machine:logerror('state_roundtrip: result=pass scenario=nsb7_sip_outgoing\n')
    -- Loading cancels old Lua waits; explicitly resume physical cleanup.
    local cleanup=coroutine.create(function()
        for _,step in ipairs({{66,'End'},{70,'Left Softkey / Menu'},{73,'End'}}) do
            assert(emu.wait(step[1]-machine.time:as_double()))
            local prefix=step[1]==66 and 'nsb7_outgoing_call_input' or 'nsb7_sip_cleanup_input'
            machine:logerror(string.format('%s: key=%s state=1 t=%.9f\n',prefix,step[2],machine.time:as_double()))
            fields[step[2]]:set_value(1)
            assert(emu.wait(0.2))
            fields[step[2]]:set_value(0)
            machine:logerror(string.format('%s: key=%s state=0 t=%.9f\n',prefix,step[2],machine.time:as_double()))
            assert(emu.wait(0.8))
            machine.screens[':screen']:snapshot('nsb7_sip_cleanup_'..step[1]..'.png')
        end
        assert(emu.wait(76-machine.time:as_double()))
        machine.screens[':screen']:snapshot('nsb7_outgoing_call_final.png')
        machine:logerror('nsb7_outgoing_call_done: t=76.000000000\n')
        complete=true
        machine:exit()
    end)
    retained[#retained+1]=cleanup
    assert(coroutine.resume(cleanup))
end)
local runner=coroutine.create(function()
    assert(emu.wait(60))
    machine:save('nsb7_sip_outgoing')
    assert(emu.wait(0.5))
    assert(saved,'save did not execute')
    machine:load('nsb7_sip_outgoing')
end)
retained[#retained+1]=runner
retained[#retained+1]=emu.add_machine_stop_notifier(function()
    if not complete then machine:logerror('nsb7_sip_state: FAIL incomplete\n') end
end)
_G.nsb7_sip_outgoing_restore=retained
assert(coroutine.resume(runner))
