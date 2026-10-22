-- Extend the same physical authentication/DISPLAY TEXT path with card input.
_G.nsb7_toolkit_continue=true
_G.nsb7_toolkit_marker='nsb7_toolkit_interactive'
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_toolkit_input.lua')
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local function press(time,key,action)
    assert(emu.wait(time-machine.time:as_double()))
    machine:logerror('nsb7_toolkit_interactive: action='..action..'\n')
    local field=assert(fields[key])
    field:set_value(1)
    if not emu.wait(0.2) then field:set_value(0); error('input interrupted') end
    field:set_value(0)
end
local input=coroutine.create(function()
    assert(emu.wait(68.5))
    machine.screens[':screen']:snapshot('nsb7_toolkit_inkey.png')
    press(69,'Keypad 5','inkey_5')
    press(70,'Left Softkey / Menu','inkey_confirm')
    assert(emu.wait(71-machine.time:as_double()))
    machine.screens[':screen']:snapshot('nsb7_toolkit_input.png')
    press(72,'Keypad 4','input_4')
    press(73,'Keypad 2','input_2')
    assert(emu.wait(73.5-machine.time:as_double()))
    machine.screens[':screen']:snapshot('nsb7_toolkit_input_filled.png')
    press(74,'Left Softkey / Menu','confirm')
    assert(emu.wait(76-machine.time:as_double()))
    machine.screens[':screen']:snapshot('nsb7_toolkit_interactive_final.png')
    if _G.nsb7_toolkit_menu then
        local steps={{78,'Left Softkey / Menu','menu'},
                {80,'Scroll Up','menu_skip_netmonitor'},{82,'Scroll Up','menu_last'},
                {84,'Left Softkey / Menu','menu_open'},
                {86,'Left Softkey / Menu','menu_select'},
                {88+(_G.nsb7_toolkit_network_wait or 0),'End','menu_exit'}}
        if _G.nsb7_toolkit_call then
            table.insert(steps,6,{90,
                _G.nsb7_toolkit_decline and 'Right Softkey / C' or 'Left Softkey / Menu',
                _G.nsb7_toolkit_decline and 'call_decline' or 'call_accept'})
        end
        if _G.nsb7_toolkit_select_item then
            table.insert(steps,6,{90,'Scroll Down','item_next'})
            table.insert(steps,7,{92,'Left Softkey / Menu','item_accept'})
        end
        if _G.nsb7_toolkit_refresh then
            for index,step in ipairs({{130,'Left Softkey / Menu','refresh_dismiss'},
                    {134,'Keypad 5','refresh_inkey5'},
                    {136,'Left Softkey / Menu','refresh_inkey_confirm'},
                    {140,'Keypad 4','refresh_input4'},{142,'Keypad 2','refresh_input2'},
                    {144,'Left Softkey / Menu','refresh_confirm'}}) do
                table.insert(steps,5+index,step)
            end
        end
        for _,step in ipairs(steps) do
            if _G.nsb7_toolkit_menu_hold and step[3]=='menu_select' then return end
            press(step[1],step[2],step[3])
            assert(emu.wait(step[1]+1-machine.time:as_double()))
            machine.screens[':screen']:snapshot('nsb7_toolkit_'..step[3]..'.png')
        end
    end
    machine:exit()
end)
_G.nsb7_toolkit_interactive_input=input
assert(coroutine.resume(input))
