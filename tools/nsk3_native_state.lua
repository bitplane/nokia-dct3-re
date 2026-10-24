-- Own cold authentication followed by bounded native idle restoration.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
_G.nsk3_sim_tx_limit = 4096
dofile(directory .. 'nsk3_pin_reload.lua')
_G.nsk3_phone_code_start = 15
dofile(directory .. 'nsk3_security_input.lua')
local run = dofile(directory .. 'native_ui_state_fixture.lua')
run({prefix = 'nsk3', checkpoint = 40, ram_end = 0x10fffc,
    contact = function(machine)
        -- Own table2c7a30: column1/mask2 is logical Menu19.
        assert(machine.devices[':maincpu'].spaces.program:read_u8(0x2c7a36) == 0x19,
            'own NSK-3 Menu mapping differs')
        for _, field in pairs(machine.ioport.ports[':COL.1'].fields) do
            if field.mask == 2 then return field end
        end
    end})
