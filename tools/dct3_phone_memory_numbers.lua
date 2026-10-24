-- Menu positions are fixture policy, not emulated phone logic.
return function(product,action)
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local function press(name,label)
    local field=assert(fields[name])
    machine:logerror(product..'_phone_memory_numbers: action='..label..'\n')
    field:set_value(1)
    assert(emu.wait(0.15))
    field:set_value(0)
    assert(emu.wait(0.85))
    machine.screens[':screen']:snapshot(product..'_phone_memory_numbers_'..label..'.png')
end
local input=coroutine.create(function()
    assert(emu.wait(20))
    press('Right Softkey / C','names')
    for _,label in ipairs({'search','result','first'}) do
        press('Left Softkey / Menu',label)
    end
    if action=='add' then
        press('Left Softkey / Menu','options')
        for i=1,5 do press('Scroll Down','add_option_'..i) end
        press('Left Softkey / Menu','type')
        press('Left Softkey / Menu','editor')
        for _,digit in ipairs({'7','8','9'}) do press('Keypad '..digit,'number_'..digit) end
        press('Left Softkey / Menu','saved')
    else
        assert(action=='read' or action=='erase')
        press('Scroll Down','second')
        if action=='erase' then
            press('Left Softkey / Menu','options')
            for i=1,2 do press('Scroll Down','erase_option_'..i) end
            press('Left Softkey / Menu','erase_confirm')
            press('Left Softkey / Menu','erased')
        else
            press('Scroll Down','third')
        end
    end
    machine:logerror(product..'_phone_memory_numbers: complete=1 firmware_state_forcing=0\n')
end)
_G.dct3_phone_memory_numbers=input
assert(coroutine.resume(input))
end
