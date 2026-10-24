-- Own PIN/phone startup and physical Down from Messages to Call log.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nsb3_pin_correct.lua')
local run = dofile(directory .. 'native_ui_state_fixture.lua')
run({prefix = 'nsb3', checkpoint = 40, ram_end = 0x10fffc,
    contact = function(machine)
        for _, port in pairs(machine.ioport.ports) do
            for _, field in pairs(port.fields) do
                if field.name == 'Scroll Down' then return field end
            end
        end
    end})
