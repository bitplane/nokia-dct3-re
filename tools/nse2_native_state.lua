-- Own phone-security startup and single-Navi physical Menu contact.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nse2_security_fixture.lua')
local run = dofile(directory .. 'native_ui_state_fixture.lua')
run({prefix = 'nse2', checkpoint = 18.5, ram_end = 0x10fffc,
    contact = function(machine)
        for _, field in pairs(machine.ioport.ports[':COL.1'].fields) do
            if field.mask == 4 then return field end
        end
    end})
