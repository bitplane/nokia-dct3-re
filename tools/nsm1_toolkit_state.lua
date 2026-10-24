-- Save an organically fetched card command; only emulator APIs and keys write.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
_G.nsm1_toolkit_observe_only=true
dofile(directory..'nsm1_toolkit_input.lua')
_G.nsm1_toolkit_checkpoint=emu.add_machine_pre_save_notifier(function()
    manager.machine.screens[':screen']:snapshot('nsm1_toolkit_state_prompt.png')
end)
local run=dofile(directory..'native_ui_state_fixture.lua')
run({prefix='nsm1',checkpoint=65,ram_end=0x11fffc,
    contact=function(machine)
        assert(machine.devices[':maincpu'].spaces.program:read_u8(0x2e0b1e)==0x19,
            'own NSM-1 left-softkey mapping differs')
        for _,field in pairs(machine.ioport.ports[':COL.1'].fields) do
            if field.mask==2 then return field end
        end
    end})
