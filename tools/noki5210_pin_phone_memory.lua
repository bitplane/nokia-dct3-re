-- Select the handset's own storage, not a SIM-file or firmware-state fixture.
local machine = manager.machine
_G.noki5210_pin_after_accept = function(press, snapshot)
    assert(emu.wait(10))
    snapshot('idle')
    _G.noki5210_pin_duration = 0.18
    _G.noki5210_pin_gap = 0.26
    local mode = _G.nsm5_phone_memory_mode or 'save'
    local read = mode ~= 'save'
    machine:logerror('5210_phone_memory: mode=' .. mode .. ' begin=1\n')
    local function key(name, delay)
        press(name)
        machine:logerror('5210_phone_memory_key: name=' .. name .. ' released=1\n')
        if delay then assert(emu.wait(delay)) end
    end
    key('Right Softkey / C', read and 0.7 or 0.6)
    if mode == 'empty_read' then
        -- Empty SIM and empty Phone look alike; inspect, but do not change, the selector.
        for index = 1, 4 do key('Scroll Down', 0.3) end
        key('Left Softkey / Menu', 0.7)
        snapshot('memory_selected')
        key('End', 1)
        key('Right Softkey / C', 0.7)
    end
    if read then
        key('Left Softkey / Menu', 0.7)
        key('Left Softkey / Menu', 0.7)
        if mode == 'empty_read' then
            snapshot('phone_empty')
        else
            snapshot('phonebook_list')
            key('Left Softkey / Menu', 2.5)
            snapshot('phonebook')
        end
        if mode == 'multi_read' then
            key('End', 1)
            key('Right Softkey / C', 0.7)
            key('Left Softkey / Menu', 0.7)
            key('Left Softkey / Menu', 0.7)
            key('Scroll Down', 0.7)
            snapshot('second_list')
            key('Left Softkey / Menu', 2.5)
            snapshot('second_number')
        elseif mode == 'rename' or mode == 'add' then
            key('Left Softkey / Menu', 0.7)
            key('Left Softkey / Menu', 0.7)
            snapshot('edit_name')
            for index = 1, 3 do key('Right Softkey / C', 0.7) end
            key('Keypad 2', 1.5)
            snapshot('rename_changed')
            key('Left Softkey / Menu', 0.7)
            snapshot('rename_number')
            key('Left Softkey / Menu', 2.5)
            snapshot('rename_replace')
            -- The same editor offers Replace or Add; keep both decisions physical.
            if mode == 'add' then key('Scroll Down', 0.7) end
            key('Left Softkey / Menu', 2.5)
            snapshot('rename_saved')
        elseif mode == 'edit' then
            key('Left Softkey / Menu', 0.7)
            key('Left Softkey / Menu', 0.7)
            snapshot('edit_name')
            -- Edit starts with the name even when entered from number Details.
            key('Left Softkey / Menu', 0.7)
            snapshot('edit_number')
            key('Right Softkey / C', 0.7)
            key('Keypad 4', 0.7)
            snapshot('edit_changed')
            key('Left Softkey / Menu', 2.5)
            snapshot('edit_saved')
        elseif mode == 'erase_cancel' or mode == 'erase' or mode == 'erase_first' then
            key('Left Softkey / Menu', 0.7)
            key('Scroll Down', 0.7)
            key('Scroll Down', 0.7)
            key('Left Softkey / Menu', 0.7)
            snapshot('erase_prompt')
            key(mode == 'erase_cancel' and 'Right Softkey / C' or 'Left Softkey / Menu', 0.7)
            snapshot('erase_result')
        end
    else
        for index = 1, 4 do key('Scroll Down', 0.3) end
        key('Left Softkey / Menu', 0.7)
        key('Left Softkey / Menu', 0.7)
        key('Scroll Down', 0.4)
        key('Left Softkey / Menu', 2)
        snapshot('memory_selected')
        key('End', 1)
        key('Right Softkey / C', 0.7)
        key('Left Softkey / Menu', 0.7)
        key('Left Softkey / Menu', 0.7)
        snapshot('phone_empty')
        key('End', 1)
        key('Right Softkey / C', 0.7)
        key('Scroll Down', 0.4)
        key('Left Softkey / Menu', 0.7)
        for digit in ('232'):gmatch('.') do key('Keypad ' .. digit) end
        assert(emu.wait(1.2))
        snapshot('phone_name')
        key('Left Softkey / Menu', 0.8)
        for digit in ('123'):gmatch('.') do key('Keypad ' .. digit) end
        assert(emu.wait(0.8))
        snapshot('phone_number')
        key('Left Softkey / Menu', 2.5)
        snapshot('phone_saved')
    end
    key('End', 1)
    key('Left Softkey / Menu', 2)
    snapshot('menu')
    key('End', 2)
    snapshot('recovered')
    machine:logerror('5210_phone_memory: mode=' .. mode .. ' complete=1\n')
end
local directory = assert(debug.getinfo(1, 'S').source:match('^@(.*/)'))
dofile(directory .. 'noki5210_pin_late_input.lua')
