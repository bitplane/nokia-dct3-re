-- Own physical authentication; no application key before idle save/load.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nsb1_pin_reload.lua')
local run = dofile(directory .. 'native_ui_state_fixture.lua')
run({prefix = 'nsb1', checkpoint = 40, ram_end = 0x10fffc,
    contact = function(machine)
        for _, field in pairs(machine.ioport.ports[':COL.1'].fields) do
            if field.mask == 4 then return field end
        end
    end})
