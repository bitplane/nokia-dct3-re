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
	auto completion_cb() { return m_completion_cb.bind(); }
	void sync_event(unsigned event);
	u16 read(offs_t offset);
	void write(offs_t offset, u16 value);

protected:
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	TIMER_CALLBACK_MEMBER(transfer);
	void enable(unsigned channel);
	void validate(unsigned channel) const;
	s16 address_step(unsigned index, bool frame_end) const;
	required_device<cpu_device> m_cpu;
	devcb_write8 m_completion_cb;
	u16 m_control = 0, m_index = 0;
	bool m_per_channel_reload = false;
	bool valid_index(u16 index) const { return index < 0x28 || (m_per_channel_reload && index >= 0x2a && index <= 0x3d); }
	u16 m_regs[0x3e] = {};
	u16 m_frame_elements[6] = {};
	emu_timer *m_timers[6] = {};
};

DECLARE_DEVICE_TYPE(TMS320C54X_DMA, tms320c54x_dma_device)

#endif
