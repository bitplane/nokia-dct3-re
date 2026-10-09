-- Own NHM-3 Clock 10-2 alarm route; no RTC or firmware writes.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6250_runtime_observe.lua')
local machine = manager.machine
local powered_off = os.getenv('NOKIA_DCT3_6250_ALARM_POWER_OFF') == '1'
local function cell(column, row)
    for _, field in pairs(machine.ioport.ports[':COL.' .. column].fields) do
        if field.mask == (1 << row) then return field end
    end
    error('missing NHM-3 physical cell')
end
local function named(name)
    for _, port in pairs(machine.ioport.ports) do
        if port.fields[name] then return port.fields[name] end
    end
    error('missing NHM-3 key ' .. name)
end
local input = coroutine.create(function()
    if not emu.wait(16) then return end
    local function press(key, action)
        machine:logerror('6250_alarm_physical: action=' .. action .. '\n')
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return false end
        key:set_value(0)
        return emu.wait(0.85)
    end
    if not press(cell(1, 1), 'menu') then return end
    for index = 1, 9 do
        if not press(cell(1, 3), 'down_' .. index) then return end
    end
    if not press(cell(1, 1), 'clock') then return end
    if not press(cell(1, 3), 'clock_down_1') then return end
    if not press(cell(1, 1), 'alarm') then return end
    for index, value in ipairs({'1', '3', '4', '8'}) do
        if not press(named('Keypad ' .. value), 'time_' .. index) then return end
    end
    if not press(cell(1, 1), 'confirm') then return end
    machine.screens[':screen']:snapshot('6250_alarm_confirm.png')
    if not press(named('End'), 'idle') then return end
    machine.screens[':screen']:snapshot('6250_alarm_idle.png')
    if powered_off then
        local power = assert(machine.ioport.ports[':PWR'].fields['Power'])
        machine:logerror('6250_alarm_physical: action=power_off\n')
        power:set_value(1)
        if not emu.wait(4) then power:set_value(0); return end
        power:set_value(0)
        machine:logerror('6250_alarm_physical: action=power_release\n')
        if not emu.wait(10) then return end
        machine.screens[':screen']:snapshot('6250_alarm_off.png')
        local replayed = os.getenv('NOKIA_DCT3_6250_ALARM_RESTORE_SNOOZE') ~= '1' and
                         _G.noki6250_alarm_restore_checkpoint and
                         _G.noki6250_alarm_restore_checkpoint() or 0
        if not emu.wait(31 - replayed) then return end
    elseif not emu.wait(30) then
        return
    end
    -- This phase shows the firmware's Stop/Snooze controls during title blink.
    machine.screens[':screen']:snapshot('6250_alarm_elapsed.png')
    if _G.noki6250_alarm_awake_checkpoint then
        assert(not powered_off, 'awake checkpoint requires powered handset')
        assert(_G.noki6250_alarm_awake_checkpoint())
    end
    if os.getenv('NOKIA_DCT3_6250_ALARM_SNOOZE') == '1' then
        if not press(named('Right Softkey / C'), 'snooze') then return end
        if not emu.wait(3) then return end
        machine.screens[':screen']:snapshot('6250_alarm_snoozed.png')
        machine:logerror('6250_alarm_physical: event=snoozed_presented\n')
        local recurrence_observation = machine.time:as_double() + 310
        if os.getenv('NOKIA_DCT3_6250_ALARM_RESTORE_SNOOZE') == '1' then
            assert(powered_off, 'Snooze checkpoint requires powered-off lifecycle')
            assert(emu.wait(100 - machine.time:as_double()))
            assert(_G.noki6250_alarm_restore_checkpoint())
        end
        if not emu.wait(recurrence_observation - machine.time:as_double()) then return end
        machine.screens[':screen']:snapshot('6250_alarm_recurred.png')
        machine:logerror('6250_alarm_physical: event=recurrence_observed\n')
    end
    if not press(cell(1, 1), 'stop') then return end
    if not emu.wait(3) then return end
    machine.screens[':screen']:snapshot('6250_alarm_stopped.png')
    machine:logerror('6250_alarm_physical: event=stopped_presented\n')
    if powered_off then
        local choice = os.getenv('NOKIA_DCT3_6250_ALARM_POWER_CHOICE')
        if choice then
            assert(choice == 'yes' or choice == 'no', 'invalid activation choice')
            if not press(choice == 'yes' and cell(1, 1) or named('Right Softkey / C'),
                         'activate_' .. choice) then return end
            if not emu.wait(20) then return end
            machine.screens[':screen']:snapshot('6250_alarm_choice.png')
            machine:logerror('6250_alarm_physical: event=choice_presented\n')
        end
    end
end)
_G.noki6250_alarm_input = input
assert(coroutine.resume(input))
