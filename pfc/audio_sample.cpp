#include "pfc-lite.h"
#include "audio_sample.h"
#include "primitives.h"
#include "byte_order.h"

namespace pfc {
	unsigned audio_math::bitrate_kbps(uint64_t fileSize, double duration) {
		if (fileSize > 0 && duration > 0) return (unsigned)floor((double)fileSize * 8 / (duration * 1000) + 0.5);
		return 0;
	}
}
