-- Own NSM-1 main-menu route to the installed external SIM application.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nsm1_toolkit_interactive.lua')
local machine,fields=manager.machine,{}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local item=_G.nsm1_toolkit_menu_item or 1
assert(item==1 or item==2,'unknown lab menu item')
local until_time=dofile(directory..'fixture_deadline.lua')
local actions=coroutine.create(function()
    local steps={{80,'Left Softkey','main'},{81,'Up','up1'},{84,'Left Softkey','open'}}
    if item==2 then steps[#steps+1]={86,'Down','next'} end
    steps[#steps+1]={item==2 and 88 or 86,'Left Softkey','select'}
    for _,step in ipairs(steps) do
        until_time(step[1])
        local field=assert(fields[step[2]],step[2])
        field:set_value(1)
        machine:logerror('nsm1_toolkit_menu: action='..step[3]..' down=1\n')
        until_time(machine.time:as_double()+.15,.15)
        field:set_value(0)
        machine:logerror('nsm1_toolkit_menu: action='..step[3]..' down=0\n')
        until_time(machine.time:as_double()+.85,.85)
        machine.screens[':screen']:snapshot('nsm1_toolkit_menu_'..step[3]..'.png')
    end
end)
assert(coroutine.resume(actions))
