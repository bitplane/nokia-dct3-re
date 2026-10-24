-- Menu positions are fixture policy; all mutations are firmware-owned.
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_staged_observe.lua')
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local function press(name,action)
    local field=assert(fields[name])
    machine:logerror('6210_phone_memory_lifecycle: action='..action..'\n')
    field:set_value(1)
    assert(emu.wait(0.15))
    field:set_value(0)
    assert(emu.wait(0.85))
    machine.screens[':screen']:snapshot('6210_phone_memory_lifecycle_'..action..'.png')
end
local input=coroutine.create(function()
    assert(emu.wait(20))
    press('Right Softkey / C','names')
    if _G.npe3_phone_memory_action=='edit' then
        press('Left Softkey / Menu','search')
        press('Left Softkey / Menu','result')
        press('Left Softkey / Menu','contact')
        press('Left Softkey / Menu','options')
        press('Scroll Down','edit_number')
        press('Left Softkey / Menu','number_editor')
        press('Right Softkey / C','number_12')
        press('Keypad 4','number_124')
        press('Left Softkey / Menu','edited')
    else
        assert(_G.npe3_phone_memory_action=='erase' or _G.npe3_phone_memory_action=='cancel')
        for i=1,3 do press('Scroll Down','erase_'..i) end
        press('Left Softkey / Menu','erase_menu')
        press('Left Softkey / Menu','erase_one')
        press('Left Softkey / Menu','erase_confirm')
        press(_G.npe3_phone_memory_action=='cancel' and 'Right Softkey / C' or
              'Left Softkey / Menu',_G.npe3_phone_memory_action)
    end
    machine:logerror('6210_phone_memory_lifecycle: complete=1 firmware_state_forcing=0\n')
end)
_G.npe3_phone_memory_lifecycle=input
assert(coroutine.resume(input))
