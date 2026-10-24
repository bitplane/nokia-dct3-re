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
    if read then
        key('Left Softkey / Menu', 0.7)
        key('Left Softkey / Menu', 0.7)
        snapshot('phonebook_list')
        key('Left Softkey / Menu', 2.5)
        snapshot('phonebook')
        if mode == 'edit' then
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
