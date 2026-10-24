-- Own phone authentication and physical Field test selection; no guest writes.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
_G.nsk1_security_code_only = true
dofile(directory .. 'nsk1_security_fixture.lua')
local machine = manager.machine
local actions = coroutine.create(function()
    local definitions = {menu={1,4},up={1,16},zero={3,16},back={1,2}}
    -- Up exposes the battery warning; the first Menu acknowledges it.
    for index, step in ipairs({{28,'menu'},{30,'up'},{32,'menu'},{34,'menu'},
                              {37,'zero'},{39,'zero'},{41,'menu'},
                              {44,'back'},{46,'back'}}) do
        assert(emu.wait(step[1]-machine.time:as_double()))
        local definition, field = definitions[step[2]], nil
        for _, candidate in pairs(machine.ioport.ports[':COL.'..definition[1]].fields) do
            if candidate.mask == definition[2] then field = candidate end
        end
        assert(field, 'own diagnostic-display contact unavailable')
        field:set_value(1)
        machine:logerror(string.format('nsk1_diagnostic_key: key=%s down=1 t=%.9f physical_only=1\n',
            step[2], machine.time:as_double()))
        assert(emu.wait(.2))
        field:set_value(0)
        machine:logerror(string.format('nsk1_diagnostic_key: key=%s down=0 t=%.9f physical_only=1\n',
            step[2], machine.time:as_double()))
        assert(emu.wait(1))
        machine.screens[':screen']:snapshot(string.format('nsk1_diagnostic_%02d.png',index))
    end
end)
assert(coroutine.resume(actions))
