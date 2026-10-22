-- Physical PUK/new-PIN/confirmation and registered Menu recovery.
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
    machine:logerror(string.format('6250_puk_input: key=%s state=1 t=%.9f\n',key,machine.time:as_double()))
    field:set_value(1)
    assert(emu.wait(0.15))
    field:set_value(0)
    machine:logerror(string.format('6250_puk_input: key=%s state=0 t=%.9f\n',key,machine.time:as_double()))
end
local runner=coroutine.create(function()
    assert(emu.wait(7))
    machine.screens[':screen']:snapshot('6250_puk_prompt.png')
    local delay=_G.nhm3_permanent_puk and 2 or 0
    for index=1,8 do press('Keypad '..(_G.nhm3_wrong_puk and 9 or index),7+index+delay) end
    press('Left Softkey / Menu',16+delay)
    assert(emu.wait(2))
    machine.screens[':screen']:snapshot('6250_puk_new_pin.png')
    for group=0,1 do
        for index,digit in ipairs({2,4,6,8}) do press('Keypad '..digit,19+group*8+index+delay) end
        press('Left Softkey / Menu',24+group*8+delay)
        assert(emu.wait(2))
        machine.screens[':screen']:snapshot(group==0 and '6250_puk_confirm.png' or '6250_puk_result.png')
    end
    if _G.nhm3_wrong_puk then
        if _G.nhm3_permanent_puk then
            press('End',36+delay)
            assert(emu.wait(2))
            machine.screens[':screen']:snapshot('6250_puk_permanent_after.png')
            machine:logerror('6250_puk_input: permanent_complete=1 firmware_state_forcing=0\n')
            return
        end
        machine:logerror('6250_puk_input: wrong_complete=1 firmware_state_forcing=0\n')
        return
    end
    assert(emu.wait(50-machine.time:as_double()))
    machine.screens[':screen']:snapshot('6250_puk_idle.png')
    press('Left Softkey / Menu',52)
    assert(emu.wait(2))
    machine.screens[':screen']:snapshot('6250_puk_menu.png')
    machine:logerror('6250_puk_input: complete=1 firmware_state_forcing=0\n')
end)
_G.noki6250_puk_recovery=runner
assert(coroutine.resume(runner))
