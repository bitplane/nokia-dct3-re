-- Shared physical menu sequence, qualified separately by each product's runner.
return function(product,empty)
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local function press(name,action)
    local field=assert(fields[name])
    machine:logerror(product..'_phone_memory_physical: action='..action..'\n')
    field:set_value(1)
    assert(emu.wait(0.15))
    field:set_value(0)
    assert(emu.wait(0.85))
    machine.screens[':screen']:snapshot(product..'_phone_memory_'..action..'.png')
end
local input=coroutine.create(function()
    assert(emu.wait(20))
    press('Right Softkey / C','names')
    for i=1,5 do press('Scroll Down','option_'..i) end
    press('Left Softkey / Menu','options')
    press('Left Softkey / Menu','memory')
    press('Scroll Down','phone')
    press('Left Softkey / Menu','selected')
    press('End','idle')
    press('Right Softkey / C','names_again')
    if empty then
        press('Left Softkey / Menu','search')
        press('Left Softkey / Menu','empty')
    else
        press('Scroll Down','add')
        press('Left Softkey / Menu','name_editor')
        press('Keypad 2','name_a')
        press('Left Softkey / Menu','number_editor')
        press('Keypad 1','number_1')
        press('Keypad 2','number_12')
        press('Keypad 3','number_123')
        press('Left Softkey / Menu','saved')
    end
    machine:logerror(product..'_phone_memory: complete=1 firmware_state_forcing=0\n')
end)
_G.dct3_phone_memory_input=input
assert(coroutine.resume(input))
end
