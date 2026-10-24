-- Wrong credential input only; no card or firmware state is written.
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki8850_startup_observe.lua')
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local input=coroutine.create(function()
    for attempt=1,(_G.nsm2_pin_exhaust and 3 or 1) do
    assert(emu.wait(8+(attempt-1)*8-machine.time:as_double()))
    for _,name in ipairs({'Keypad 9','Keypad 9','Keypad 9','Keypad 9','Menu'}) do
        local field=assert(fields[name])
        for _,state in ipairs({1,0}) do
            machine:logerror(string.format('8850_pin_wrong: key=%s state=%d t=%.9f\n',
                name,state,machine.time:as_double()))
            field:set_value(state)
            assert(emu.wait(state==1 and 0.15 or 0.85))
        end
    end
    machine.screens[':screen']:snapshot('8850_pin_wrong_attempt_'..attempt..'.png')
    end
    machine.screens[':screen']:snapshot('8850_pin_wrong.png')
    machine:logerror('8850_pin_wrong: complete=1 firmware_state_forcing=0\n')
end)
_G.noki8850_pin_wrong=input
assert(coroutine.resume(input))
