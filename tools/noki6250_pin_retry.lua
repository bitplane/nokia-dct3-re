-- Physical wrong-PIN rejection followed by ordinary correct-PIN recovery.
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6250_runtime_observe.lua')
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local function press(key,when)
    assert(emu.wait(when-machine.time:as_double()))
    local field=assert(fields[key])
    machine:logerror(string.format('6250_pin_retry: key=%s state=1 t=%.9f\n',key,machine.time:as_double()))
    field:set_value(1)
    assert(emu.wait(0.15))
    field:set_value(0)
    machine:logerror(string.format('6250_pin_retry: key=%s state=0 t=%.9f\n',key,machine.time:as_double()))
    assert(emu.wait(0.85))
end
local runner=coroutine.create(function()
    local mode=_G.nhm3_pin_retry_mode
    if mode~='cold-recovery' and mode~='cold-rotated' then
        for attempt=1,(mode=='exhaust' and 3 or 1) do
            local start=8+(attempt-1)*8
            for second=start,start+3 do press('Keypad 9',second) end
            press('Left Softkey / Menu',start+4)
            machine.screens[':screen']:snapshot('6250_pin_retry_error.png')
            if mode=='exhaust' then
                machine.screens[':screen']:snapshot('6250_pin_exhaust_'..attempt..'.png')
            end
        end
        if mode=='wrong-only' or mode=='exhaust' then
            machine:logerror('6250_pin_retry: wrong_only_complete=1 firmware_state_forcing=0\n')
            return
        end
    end
    local start=(mode=='cold-recovery' or mode=='cold-rotated') and 7 or 17
    local digits=mode=='cold-rotated' and {2,4,6,8} or {1,2,3,4}
    for index,digit in ipairs(digits) do press('Keypad '..digit,start+index) end
    press('Left Softkey / Menu',start+5)
    assert(emu.wait(32-machine.time:as_double()))
    machine.screens[':screen']:snapshot('6250_pin_retry_idle.png')
    press('Left Softkey / Menu',34)
    machine.screens[':screen']:snapshot('6250_pin_retry_menu.png')
    machine:logerror('6250_pin_retry: complete=1 firmware_state_forcing=0\n')
end)
_G.noki6250_pin_retry=runner
assert(coroutine.resume(runner))
