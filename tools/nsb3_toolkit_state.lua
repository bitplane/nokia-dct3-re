-- Save a fetched DISPLAY TEXT transaction, then replay physical dismissal.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nsb3_pin_correct.lua')
_G.nsb3_toolkit_checkpoint = emu.add_machine_pre_save_notifier(function()
    manager.machine.screens[':screen']:snapshot('nsb3_toolkit_state_prompt.png')
end)
local run = dofile(directory .. 'native_ui_state_fixture.lua')
run({prefix = 'nsb3', checkpoint = 65, ram_end = 0x10fffc,
    contact = function(machine)
        return machine.ioport.ports[':COL.1'].fields['Left Softkey / Menu']
    end})
