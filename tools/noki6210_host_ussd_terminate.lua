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
dofile(directory..'noki6210_host_ussd_notify.lua')
