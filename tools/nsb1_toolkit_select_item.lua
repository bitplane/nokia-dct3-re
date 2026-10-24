-- Card-issued follow-up using the own single-Navi board's Down and Menu cells.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nsb1_toolkit_menu.lua')
local machine=manager.machine
local function field(column,mask)
    for _,candidate in pairs(machine.ioport.ports[':COL.'..column].fields) do
        if candidate.mask==mask then return candidate end
    end
    error('own NSB-1 contact missing')
end
local function press(when,column,mask,action)
    assert(emu.wait(when-machine.time:as_double()))
    local key=field(column,mask)
    key:set_value(1)
    machine:logerror('nsb1_toolkit_select_item: action='..action..' down=1\n')
    assert(emu.wait(.15))
    key:set_value(0)
    machine:logerror('nsb1_toolkit_select_item: action='..action..' down=0\n')
end
local actions=coroutine.create(function()
    assert(emu.wait(88))
    machine.screens[':screen']:snapshot('nsb1_toolkit_item_prompt.png')
    press(89,0,8,'next')
    assert(emu.wait(.35))
    machine.screens[':screen']:snapshot('nsb1_toolkit_item_selected.png')
    press(90.5,1,4,'confirm')
    assert(emu.wait(.5))
    machine.screens[':screen']:snapshot('nsb1_toolkit_item_result.png')
end)
assert(coroutine.resume(actions))
