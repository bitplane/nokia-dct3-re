-- Physical NPE-3 Menu 4-1 alarm probe; no clock or firmware writes.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_staged_observe.lua')
local machine = manager.machine
local input = coroutine.create(function()
    if not emu.wait(20) then return end
    local function press(column, name, action)
        local key = assert(machine.ioport.ports[':COL.' .. column].fields[name])
        machine:logerror('6210_alarm_probe: action=' .. action .. '\n')
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return false end
        key:set_value(0)
        if not emu.wait(0.85) then return false end
        machine.screens[':screen']:snapshot('6210_alarm_' .. action .. '.png')
        return true
    end
    if not press(1, 'Left Softkey / Menu', 'menu') then return end
    for index = 2, 4 do
        if not press(1, 'Scroll Down', 'menu_' .. index) then return end
    end
    if not press(1, 'Left Softkey / Menu', 'settings') then return end
    if not press(1, 'Left Softkey / Menu', 'alarm') then return end
    for index, step in ipairs({{2, 'Keypad 1'}, {4, 'Keypad 3'},
                              {2, 'Keypad 4'}, {3, 'Keypad 8'}}) do
        if not press(step[1], step[2], 'time_' .. index) then return end
    end
    if not press(1, 'Left Softkey / Menu', 'confirm') then return end
    if not press(0, 'End', 'idle') then return end
    if _G.noki6210_alarm_arm_only then
        if not emu.wait(3) then return end
        machine:logerror('6210_alarm_probe: armed_checkpoint=1\n')
        machine.screens[':screen']:snapshot('6210_alarm_armed.png')
        machine:exit()
        return
    end
    if not emu.wait(45) then return end
    machine.screens[':screen']:snapshot('6210_alarm_elapsed.png')
    if _G.noki6210_alarm_snooze then
        if not press(1, 'Right Softkey / C', 'snooze') then return end
        -- Observe natural recurrence; do not write the clock or assume a delay.
        if not emu.wait(600) then return end
        machine.screens[':screen']:snapshot('6210_alarm_repeated.png')
    end
    if not press(1, 'Left Softkey / Menu', 'stop') then return end
    if not emu.wait(3) then return end
    machine.screens[':screen']:snapshot('6210_alarm_stopped.png')
end)
_G.noki6210_alarm_probe = input
assert(coroutine.resume(input))
