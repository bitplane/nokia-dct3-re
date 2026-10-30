// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

bool tms320c54x_test_state::check_atomic_program_results()
{
	static_assert(native_store_phase == 1000000);
	switch (atomic_fixture_for(m_phase))
	{
	case atomic_fixture::corpus: return check_atomic_corpus_results();
	case atomic_fixture::none: return false;
	}
	return false;
}

} // namespace c54x_test
