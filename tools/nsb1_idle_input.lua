-- Own physical authentication and optional diagnostic-display selection.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nsb1_pin_reload.lua')
local machine = manager.machine
local actions = coroutine.create(function()
    if _G.nsb1_diagnostic_display then
        local definitions = {menu={1,4},up={1,16},zero={3,16},back={1,2}}
        for index, step in ipairs({{28,'menu'},{30,'up'},{32,'menu'},
                                  {35,'zero'},{37,'zero'},{39,'menu'},
                                  {42,'back'},{44,'back'}}) do
            assert(emu.wait(step[1]-machine.time:as_double()))
            local definition, field = definitions[step[2]], nil
            for _, candidate in pairs(machine.ioport.ports[':COL.'..definition[1]].fields) do
                if candidate.mask == definition[2] then field = candidate end
            end
            assert(field, 'own diagnostic contact missing')
            field:set_value(1)
            machine:logerror(string.format('nsb1_diagnostic_key: key=%s down=1 t=%.9f physical_only=1\n',
                step[2],machine.time:as_double()))
            assert(emu.wait(.2))
            field:set_value(0)
            machine:logerror(string.format('nsb1_diagnostic_key: key=%s down=0 t=%.9f physical_only=1\n',
                step[2],machine.time:as_double()))
            assert(emu.wait(1))
            machine.screens[':screen']:snapshot(string.format('nsb1_diagnostic_%02d.png',index))
        end
    else
        for _, time in ipairs({25,36,42}) do
            assert(emu.wait(time-machine.time:as_double()))
            machine.screens[':screen']:snapshot(string.format('nsb1_idle_%02d.png',time))
        end
    end
end)
assert(coroutine.resume(actions))
