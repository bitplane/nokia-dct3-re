-- Own phone authentication only; no application key before the idle save.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
_G.nsk1_security_code_only = true
dofile(directory .. 'nsk1_security_fixture.lua')
local run = dofile(directory .. 'native_ui_state_fixture.lua')
run({prefix = 'nsk1', checkpoint = 40, ram_end = 0x10fffc,
    contact = function(machine)
        for _, field in pairs(machine.ioport.ports[':COL.1'].fields) do
            if field.mask == 4 then return field end
        end
    end})
