-- Card-issued follow-up with own NSM-1 physical Down/left-softkey contacts.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nsm1_toolkit_menu.lua')
local machine,fields=manager.machine,{}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local function press(when,name,action)
    assert(emu.wait(when-machine.time:as_double()))
    local field=assert(fields[name],name)
    field:set_value(1)
    machine:logerror('nsm1_toolkit_select_item: action='..action..' down=1\n')
    assert(emu.wait(.15))
    field:set_value(0)
    machine:logerror('nsm1_toolkit_select_item: action='..action..' down=0\n')
end
local actions=coroutine.create(function()
    assert(emu.wait(88))
    machine.screens[':screen']:snapshot('nsm1_toolkit_item_prompt.png')
    press(89,'Down','next')
    assert(emu.wait(.35))
    machine.screens[':screen']:snapshot('nsm1_toolkit_item_selected.png')
    press(90.5,'Left Softkey','confirm')
    assert(emu.wait(.5))
    machine.screens[':screen']:snapshot('nsm1_toolkit_item_result.png')
end)
assert(coroutine.resume(actions))
