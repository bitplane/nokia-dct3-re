-- Own NSB-1 pending-card checkpoint with physical Navi completion in each branch.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
_G.nsb1_toolkit_observe_only=true
dofile(directory..'nsb1_toolkit_input.lua')
_G.nsb1_toolkit_checkpoint=emu.add_machine_pre_save_notifier(function()
    manager.machine.screens[':screen']:snapshot('nsb1_toolkit_state_prompt.png')
end)
local run=dofile(directory..'native_ui_state_fixture.lua')
run({prefix='nsb1',checkpoint=65,ram_end=0x10fffc,
    contact=function(machine)
        for _,field in pairs(machine.ioport.ports[':COL.1'].fields) do
            if field.mask==4 then return field end
        end
    end})
