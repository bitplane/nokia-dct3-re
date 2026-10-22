-- Save an unanswered editor; only physical keys supply the restored reply.
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
local machine=manager.machine
local capture=dofile(directory..'arm_architecture_snapshot.lua')
local retained={}
local saved
_G.npe3_host_ussd_capture='6210_host_ussd_request.png'
_G.npe3_host_ussd_actions={{26,'Left Softkey / Menu'}}
retained[#retained+1]=emu.add_machine_pre_save_notifier(function()
    saved=capture(machine)
    machine:logerror(string.format('6210_ussd_restore: saved t=%.9f pc=%08x ram=%08x\n',saved.time,saved.pc,saved.ram))
end)
retained[#retained+1]=emu.add_machine_post_load_notifier(function()
    local restored=capture(machine)
    assert(saved,'missing saved architecture')
    for _,key in ipairs({'time','pc','sp','ram','cpu'}) do
        assert(saved[key]==restored[key],'restored architecture mismatch '..key)
    end
    machine:logerror('6210_ussd_restore: architecture=pass\n')
    local replay=coroutine.create(function()
        local fields={}
        for _,port in pairs(machine.ioport.ports) do
            for _,field in pairs(port.fields) do fields[field.name]=field end
        end
        for _,action in ipairs({{30,'Keypad 2'},{32,'Left Softkey / Menu'},{36,'End'},{38,'Left Softkey / Menu'}}) do
            assert(emu.wait(action[1]-machine.time:as_double()))
            local field=assert(fields[action[2]])
            machine:logerror(string.format('6210_host_ussd_input: key=%s state=1 t=%.9f\n',action[2],machine.time:as_double()))
            field:set_value(1)
            assert(emu.wait(0.15))
            field:set_value(0)
            machine:logerror(string.format('6210_host_ussd_input: key=%s state=0 t=%.9f\n',action[2],machine.time:as_double()))
            assert(emu.wait(0.85))
            machine.screens[':screen']:snapshot('6210_host_ussd_key_'..action[1]..'.png')
        end
        assert(emu.wait(41-machine.time:as_double()))
        machine:logerror('6210_host_ussd: complete=1 firmware_state_forcing=0\n')
    end)
    retained[#retained+1]=replay
    assert(coroutine.resume(replay))
end)
_G.npe3_host_ussd_after_actions=function()
    assert(emu.wait(28-machine.time:as_double()))
    machine:save('npe3_ussd_pending')
    assert(emu.wait(1))
    assert(saved,'save did not execute')
    machine:load('npe3_ussd_pending')
end
_G.npe3_host_ussd_restore_retained=retained
dofile(directory..'noki6210_host_ussd_notify.lua')
