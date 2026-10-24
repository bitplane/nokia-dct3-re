-- Menu positions are fixture policy; all mutations are firmware-owned.
return function(product,action)
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local function press(name,action)
    local field=assert(fields[name])
    machine:logerror(product..'_phone_memory_lifecycle: action='..action..'\n')
    field:set_value(1)
    assert(emu.wait(0.15))
    field:set_value(0)
    assert(emu.wait(0.85))
    machine.screens[':screen']:snapshot(product..'_phone_memory_lifecycle_'..action..'.png')
end
local input=coroutine.create(function()
    assert(emu.wait(20))
    press('Right Softkey / C','names')
    if action=='edit' or action=='rename' then
        press('Left Softkey / Menu','search')
        press('Left Softkey / Menu','result')
        press('Left Softkey / Menu','contact')
        press('Left Softkey / Menu','options')
        if action=='rename' then
            for i=1,12 do press('Scroll Down','rename_option_'..i) end
            press('Left Softkey / Menu','name_editor')
            press('Right Softkey / C','name_empty')
            press('Keypad 3','name_D')
            press('Left Softkey / Menu','renamed')
        else
            press('Scroll Down','edit_number')
            press('Left Softkey / Menu','number_editor')
            press('Right Softkey / C','number_12')
            press('Keypad 4','number_124')
            press('Left Softkey / Menu','edited')
        end
    else
        assert(action=='erase' or action=='cancel')
        for i=1,3 do press('Scroll Down','erase_'..i) end
        press('Left Softkey / Menu','erase_menu')
        press('Left Softkey / Menu','erase_one')
        press('Left Softkey / Menu','erase_confirm')
        press(action=='cancel' and 'Right Softkey / C' or 'Left Softkey / Menu',action)
    end
    machine:logerror(product..'_phone_memory_lifecycle: complete=1 firmware_state_forcing=0\n')
end)
_G.dct3_phone_memory_lifecycle=input
assert(coroutine.resume(input))
end
