-- Keep external-dialog observer lifetime independent of the handset snapshot.
_G.noki8210_sip_restore_observers = {
    emu.add_machine_pre_save_notifier(function()
        manager.machine:logerror('sip_state: saved\n')
    end),
    emu.add_machine_post_load_notifier(function()
        manager.machine:logerror('sip_state: restored\n')
    end)
}
