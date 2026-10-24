-- Own NSK-3 authentication and physical diagnostic-display selection only.
local directory = debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
_G.nsk3_sim_tx_limit = 4096
dofile(directory .. 'nsk3_pin_reload.lua')
_G.nsk3_phone_code_start = 15
dofile(directory .. 'nsk3_security_input.lua')
local machine = manager.machine
local memory = machine.devices[':maincpu'].spaces.program
local dsp = assert(machine.devices[':dsp_c54x:cpu'])
local actions = coroutine.create(function()
    local definitions = {menu={1,2,0x19},up={1,4,0x17},zero={3,16,0x0a},back={1,16,0x1a}}
    -- The observed battery notice consumes the first Select; acknowledge it
    -- physically before entering the retained Net monitor menu item.
    for index, step in ipairs({{28,'menu'},{30,'up'},{32,'menu'},{34,'menu'},
                              {37,'zero'},{39,'zero'},{41,'menu'},
                              {44,'back'},{46,'back'}}) do
        assert(emu.wait(step[1]-machine.time:as_double()))
        local definition, field = definitions[step[2]], nil
        local row = 0
        while (1<<row) ~= definition[2] do row = row+1 end
        assert(memory:read_u8(0x2c7a30+row*5+definition[1]) == definition[3], 'own key contract differs')
        for _, candidate in pairs(machine.ioport.ports[':COL.'..definition[1]].fields) do
            if candidate.mask == definition[2] then field = candidate end
        end
        assert(field, 'own diagnostic contact missing')
        field:set_value(1)
        machine:logerror(string.format('nsk3_diagnostic_key: key=%s down=1 t=%.9f physical_only=1\n',
            step[2],machine.time:as_double()))
        assert(emu.wait(.2))
        field:set_value(0)
        machine:logerror(string.format('nsk3_diagnostic_key: key=%s down=0 t=%.9f physical_only=1\n',
            step[2],machine.time:as_double()))
        assert(emu.wait(1))
        machine.screens[':screen']:snapshot(string.format('nsk3_diagnostic_%02d.png',index))
        machine:logerror(string.format('nsk3_diagnostic_native: phase=physical sample=%d pc=%06x stopped=%d idle=%d observer_writes=0\n',
            index,dsp.state.PC.value,dsp.state.ILLEGAL.value,dsp.state.IDLE.value))
    end
end)
assert(coroutine.resume(actions))
