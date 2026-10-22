-- Recover the retained blocked card through real PUK/new-PIN UI contacts.
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_staged_observe.lua')
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local function press(name,when)
    assert(emu.wait(when-machine.time:as_double()))
    local field=assert(fields[name])
    machine:logerror(string.format('6210_puk_input: key=%s state=1 t=%.9f\n',name,machine.time:as_double()))
    field:set_value(1)
    assert(emu.wait(0.15))
    field:set_value(0)
    machine:logerror(string.format('6210_puk_input: key=%s state=0 t=%.9f\n',name,machine.time:as_double()))
end
local runner=coroutine.create(function()
    assert(emu.wait(7))
    machine.screens[':screen']:snapshot('6210_puk_prompt.png')
    if _G.npe3_puk_verify_rotated then
        for i,digit in ipairs({2,4,6,8}) do press('Keypad '..digit,7+i) end
        press('Left Softkey / Menu',12)
        assert(emu.wait(33-machine.time:as_double()))
        machine.screens[':screen']:snapshot('6210_puk_idle.png')
        press('Left Softkey / Menu',34)
        assert(emu.wait(2))
        machine.screens[':screen']:snapshot('6210_security_then_menu.png')
        machine:logerror('6210_puk_input: reauthenticated=1 firmware_state_forcing=0\n')
        return
    end
    for i=1,8 do press('Keypad '..(_G.npe3_puk_wrong_only and 9 or i),7+i) end
    press('Left Softkey / Menu',16)
    assert(emu.wait(2))
    machine.screens[':screen']:snapshot('6210_puk_new_pin.png')
    for group=0,1 do
        for i,digit in ipairs({2,4,6,8}) do press('Keypad '..digit,19+group*8+i) end
        press('Left Softkey / Menu',24+group*8)
        assert(emu.wait(2))
        machine.screens[':screen']:snapshot(group==0 and '6210_puk_confirm.png' or '6210_puk_result.png')
    end
    if _G.npe3_puk_wrong_only or _G.npe3_puk_permanent_retry then
        if _G.npe3_puk_permanent_retry then
            press('End',36)
            assert(emu.wait(2))
            machine.screens[':screen']:snapshot('6210_puk_permanent_after.png')
        end
        machine:logerror('6210_puk_input: '..(_G.npe3_puk_permanent_retry and 'permanent_complete' or 'wrong_complete')..'=1 firmware_state_forcing=0\n')
        return
    end
    assert(emu.wait(50-machine.time:as_double()))
    machine.screens[':screen']:snapshot('6210_puk_idle.png')
    press('Left Softkey / Menu',52)
    assert(emu.wait(2))
    machine.screens[':screen']:snapshot('6210_security_then_menu.png')
    machine:logerror('6210_puk_input: complete=1 firmware_state_forcing=0\n')
end)
_G.npe3_puk_runner=runner
assert(coroutine.resume(runner))
