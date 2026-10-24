-- Physical matrix input only; assumes no phone-code comparison result.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_sim_observe.lua')
local machine = manager.machine
local lcd_count = 0
_G.nsb3_phonecode_lcd = machine.devices[':maincpu'].spaces.program:install_write_tap(
    0x20028, 0x2002f, 'nsb3_phonecode_lcd', function(address, data, mask)
        if machine.time:as_double() < 15.9 then return end
        for lane = 0, 3 do
            local offset, shift = address + lane, (3 - lane) * 8
            if (offset == 0x2002b or offset == 0x2002c) and mask & (255 << shift) ~= 0 then
                lcd_count = lcd_count + 1
                if lcd_count <= 32768 then
                    machine:logerror(string.format('nsb3_phonecode_lcd: kind=%s byte=%02x pc=%08x t=%.9f observer_writes=0\n',
                        offset == 0x2002c and 'command' or 'data', (data >> shift) & 255,
                        machine.devices[':maincpu'].state['PC'].value, machine.time:as_double()))
                end
            end
        end
    end)
machine.devices[':maincpu'].debug:bpset(0x2af450, '1',
    'logerror "nsb3_phonecode_gate: match=%08x retry=%02x lr=%08x observer_writes=0\\n",r0,b@10c444,lr;g')
machine.devices[':maincpu'].debug:bpset(0x2af48c, '1',
    'logerror "nsb3_phonecode_matched_branch: retry=%02x observer_writes=0\\n",r1;g')
local actions = coroutine.create(function()
    assert(emu.wait(_G.nsb3_phonecode_start or 14))
    for _, item in ipairs({{2, 'Keypad 1'}, {3, 'Keypad 2'}, {4, 'Keypad 3'},
                           {2, 'Keypad 4'}, {3, 'Keypad 5'}, {1, 'Left Softkey / Menu'}}) do
        local field = assert(machine.ioport.ports[':COL.' .. item[1]].fields[item[2]])
        field:set_value(1)
        machine:logerror(string.format('nsb3_phonecode_key: column=%d name=%s down=1 t=%.9f physical_only=1\n', item[1], item[2], machine.time:as_double()))
        assert(emu.wait(0.15))
        field:set_value(0)
        machine:logerror(string.format('nsb3_phonecode_key: column=%d name=%s down=0 t=%.9f physical_only=1\n', item[1], item[2], machine.time:as_double()))
        assert(emu.wait(0.25))
    end
    assert(emu.wait(2))
    machine.screens[':screen']:snapshot('nsb3_phonecode_result.png')
    for _, delay in ipairs({4, 8}) do
        assert(emu.wait(delay))
        machine:logerror(string.format('nsb3_phonecode_later: pc=%08x retry=%02x t=%.9f observer_writes=0\n',
            machine.devices[':maincpu'].state['PC'].value,
            machine.devices[':maincpu'].spaces.program:read_u8(0x10c444), machine.time:as_double()))
        machine.screens[':screen']:snapshot(string.format('nsb3_phonecode_later_%d.png', delay))
    end
    local menu = assert(machine.ioport.ports[':COL.1'].fields['Left Softkey / Menu'])
    machine:logerror('nsb3_phonecode_menu: down=1 physical_only=1\n')
    menu:set_value(1)
    assert(emu.wait(0.15))
    menu:set_value(0)
    machine:logerror('nsb3_phonecode_menu: down=0 physical_only=1\n')
    assert(emu.wait(2))
    machine.screens[':screen']:snapshot('nsb3_phonecode_menu.png')
end)
assert(coroutine.resume(actions))
