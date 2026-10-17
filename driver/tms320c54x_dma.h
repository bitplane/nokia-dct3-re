// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_DMA_H
#define MAME_NOKIA_TMS320C54X_DMA_H

// Enhanced C54x DMA subset corroborated by DA150 loader traffic and SPRU302B.
class tms320c54x_dma_device : public device_t
{
public:
	tms320c54x_dma_device(machine_config const &config, char const *tag, device_t *owner, u32 clock);
	template <typename T> void set_cpu(T &&tag) { m_cpu.set_tag(std::forward<T>(tag)); }
	void set_per_channel_reload(bool value) { m_per_channel_reload = value; }
	// Undivided X2/CLKIN for IDLE3; zero means no external clock is present.
	void set_input_clock(u32 value) { m_input_clock = value; }
	void pll_clock_stop_w(int state);
	auto completion_cb() { return m_completion_cb.bind(); }
	void sync_event(unsigned event);
	void sync_w(unsigned event, int state);
	u16 read(offs_t offset);
	void write(offs_t offset, u16 value);
	// Side-effect-free inspection; does not alter the register index or bus state.
	bool source_matches(unsigned channel, int space, u32 address) const;
	// Unlike source_matches, true only inside this channel's actual source read.
	bool source_read_matches(unsigned channel, int space, u32 address) const;
	// True only inside this channel's actual destination write, not a CPU write.
	bool destination_write_matches(unsigned channel, int space, u32 address) const;
	// Indexed register contents without changing the CPU-facing index latch.
	u16 inspect_register(unsigned index) const;

protected:
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_clock_changed() override;

private:
	TIMER_CALLBACK_MEMBER(transfer);
	void enable(unsigned channel);
	void validate(unsigned channel) const;
	void schedule_transfer(unsigned channel);
	void retime_transfers(u32 old_rate, u32 new_rate);
	u32 transfer_clock() const { return m_pll_stopped ? m_input_clock : clock(); }
	s16 address_step(unsigned index, bool frame_end) const;
	required_device<cpu_device> m_cpu;
	devcb_write8 m_completion_cb;
	u16 m_control = 0, m_index = 0;
	bool m_per_channel_reload = false;
	u32 m_input_clock = 0;
	u32 m_system_clock = 0;
	bool m_pll_stopped = false;
	u64 m_paused_ticks[6] = {};
	bool valid_index(u16 index) const { return index < 0x28 || (m_per_channel_reload && index >= 0x2a && index <= 0x3d); }
	u16 m_regs[0x3e] = {};
	u16 m_frame_elements[6] = {};
	u16 m_sync_levels = 0;
	// Synchronous bus-observer context, never hardware or save-state contents.
	int m_source_read_channel = -1, m_source_read_space = -1;
	u32 m_source_read_address = 0;
	int m_destination_write_channel = -1, m_destination_write_space = -1;
	u32 m_destination_write_address = 0;
	emu_timer *m_timers[6] = {};
};

DECLARE_DEVICE_TYPE(TMS320C54X_DMA, tms320c54x_dma_device)

#endif // MAME_NOKIA_TMS320C54X_DMA_H
