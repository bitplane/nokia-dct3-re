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
    if _G.noki6210_alarm_power_off then
        local power = assert(machine.ioport.ports[':PWR'].fields['Power'])
        machine:logerror('6210_alarm_probe: action=power_off\n')
        power:set_value(1)
        if not emu.wait(4) then power:set_value(0); return end
        power:set_value(0)
        machine:logerror('6210_alarm_probe: action=power_release\n')
        if not emu.wait(9) then return end
        machine.screens[':screen']:snapshot('6210_alarm_powered_off.png')
        local replayed = not _G.noki6210_alarm_restore_snooze and _G.noki6210_alarm_restore_checkpoint and
            _G.noki6210_alarm_restore_checkpoint() or 0
        if not emu.wait(19 - replayed) then return end
        machine.screens[':screen']:snapshot('6210_alarm_woke.png')
        if not emu.wait(13) then return end
    else
        if not emu.wait(45) then return end
    end
    machine.screens[':screen']:snapshot('6210_alarm_elapsed.png')
    if _G.noki6210_alarm_snooze then
        if not press(1, 'Right Softkey / C', 'snooze') then return end
        -- The alarm title is phase-dependent. One second after the natural
        -- 13:53 recurrence is independently observed to show its visible phase.
        -- Alarm-only rail wake takes several seconds to boot and consume RTC.
        local recurrence = machine.time:as_double() + (_G.noki6210_alarm_power_off and 287 or 283)
        if _G.noki6210_alarm_restore_snooze then
            if not emu.wait(100 - machine.time:as_double()) then return end
            _G.noki6210_alarm_restore_checkpoint()
        end
        if not emu.wait(recurrence - machine.time:as_double()) then return end
        machine.screens[':screen']:snapshot('6210_alarm_repeated.png')
    end
    if not press(1, 'Left Softkey / Menu', 'stop') then return end
    if not emu.wait(3) then return end
    machine.screens[':screen']:snapshot('6210_alarm_stopped.png')
    if _G.noki6210_alarm_power_choice then
        local yes = _G.noki6210_alarm_power_choice == 'yes'
        -- Re-arm debugger-only log caps for the separate activation boot.
        -- These temporaries do not alter firmware, MMIO or device state.
        if yes then machine.debugger:command('do temp5=0;do temp6=0;do temp9=0') end
        if not press(1, yes and 'Left Softkey / Menu' or 'Right Softkey / C',
                     yes and 'activate_yes' or 'activate_no') then return end
        if not emu.wait(20) then return end
        machine.screens[':screen']:snapshot('6210_alarm_power_choice.png')
    end
end)
_G.noki6210_alarm_probe = input
assert(coroutine.resume(input))
