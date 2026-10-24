-- Physical editor dismissal is separate from the network application's release.
_G.npe3_host_ussd_actions={{26,'Left Softkey / Menu'},{30,'End'}}
_G.npe3_host_ussd_after_actions=function()
    local machine=manager.machine
    assert(emu.wait(40-machine.time:as_double()))
    print('6210_host_ussd: terminate ready')
    io.stdout:flush()
    assert(emu.wait(56-machine.time:as_double()))
    local menu
    for _,port in pairs(machine.ioport.ports) do
        for _,field in pairs(port.fields) do
            if field.name=='Left Softkey / Menu' then menu=field end
        end
    end
    assert(menu)
    menu:set_value(1)
    assert(emu.wait(0.15))
    menu:set_value(0)
    assert(emu.wait(1))
    machine.screens[':screen']:snapshot('6210_host_ussd_terminated_menu.png')
    machine:logerror('6210_host_ussd_termination: complete=1 firmware_state_forcing=0\n')
end
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
if _G.npe3_ussd_termination_restore then
    local machine=manager.machine
    local capture=dofile(directory..'arm_architecture_snapshot.lua')
    local finish=_G.npe3_host_ussd_after_actions
    local retained={}
    local saved
    retained[#retained+1]=emu.add_machine_pre_save_notifier(function()
        saved=capture(machine)
    end)
    retained[#retained+1]=emu.add_machine_post_load_notifier(function()
        local restored=capture(machine)
        assert(saved,'missing termination checkpoint')
        for _,key in ipairs({'time','pc','sp','ram','cpu'}) do
            assert(saved[key]==restored[key],'termination restore differs: '..key)
        end
        machine:logerror('6210_ussd_termination_restore: architecture=pass\n')
        local replay=coroutine.create(finish)
        retained[#retained+1]=replay
        assert(coroutine.resume(replay))
    end)
    _G.npe3_host_ussd_after_actions=function()
        assert(emu.wait(34-machine.time:as_double()))
        machine:save('npe3_ussd_termination')
        assert(emu.wait(1))
        assert(saved,'termination save did not execute')
        machine:load('npe3_ussd_termination')
    end
    _G.npe3_ussd_termination_retained=retained
end
dofile(directory..'noki6210_host_ussd_notify.lua')
