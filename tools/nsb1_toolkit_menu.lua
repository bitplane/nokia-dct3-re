-- Own NSB-1 single-Navi route, retaining its Field test menu position.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nsb1_toolkit_interactive.lua')
local machine=manager.machine
local function field(mask,column)
    for _,candidate in pairs(machine.ioport.ports[':COL.'..column].fields) do
        if candidate.mask==mask then return candidate end
    end
    error('own NSB-1 navigation contact missing')
end
local item=_G.nsb1_toolkit_menu_item or 1
assert(item==1 or item==2,'unknown lab menu item')
local actions=coroutine.create(function()
    local steps={{80,4,'main'},{81,16,'fieldtest'},{82,16,'last'},{84,4,'open'}}
    -- Down occupies column zero; column one's bit8 is unused on this board.
    if item==2 then steps[#steps+1]={86,8,'next',0} end
    steps[#steps+1]={item==2 and 88 or 86,4,'select'}
    for _,step in ipairs(steps) do
        assert(emu.wait(step[1]-machine.time:as_double()))
        local key=field(step[2],step[4] or 1)
        key:set_value(1)
        machine:logerror('nsb1_toolkit_menu: action='..step[3]..' down=1\n')
        assert(emu.wait(.15))
        key:set_value(0)
        machine:logerror('nsb1_toolkit_menu: action='..step[3]..' down=0\n')
        assert(emu.wait(.85))
        machine.screens[':screen']:snapshot('nsb1_toolkit_menu_'..step[3]..'.png')
    end
end)
assert(coroutine.resume(actions))
