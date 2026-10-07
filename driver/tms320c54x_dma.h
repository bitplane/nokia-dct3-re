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
	u16 read(offs_t offset);
	void write(offs_t offset, u16 value);

protected:
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	TIMER_CALLBACK_MEMBER(transfer);
	void enable(unsigned channel);
	required_device<cpu_device> m_cpu;
	u16 m_control = 0, m_index = 0;
	u16 m_regs[0x28] = {};
	emu_timer *m_timers[6] = {};
};

DECLARE_DEVICE_TYPE(TMS320C54X_DMA, tms320c54x_dma_device)

#endif
