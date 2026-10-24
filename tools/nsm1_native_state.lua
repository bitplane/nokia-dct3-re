-- Own PIN/phone input only; no application key before native idle save/load.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
_G.nsm1_correct_pin = true
dofile(directory .. 'nsm1_pin_input.lua')
_G.nsm1_phone_code_start = 15
_G.nsm1_followup_contacts = {}
dofile(directory .. 'nsm1_security_input.lua')
local run = dofile(directory .. 'native_ui_state_fixture.lua')
run({prefix = 'nsm1', checkpoint = 40, ram_end = 0x11fffc,
    contact = function(machine)
        assert(machine.devices[':maincpu'].spaces.program:read_u8(0x2e0b1e) == 0x19,
            'own NSM-1 left-softkey mapping differs')
        for _, field in pairs(machine.ioport.ports[':COL.1'].fields) do
            if field.mask == 2 then return field end
        end
    end})
