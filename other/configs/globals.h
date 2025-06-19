#pragma once

namespace utilities {
	struct globals {
		static int highpass;
		static int opusencode;
		static int opusdecode;
		
		static float gain;

		static bool isOpusHooked;
		static bool isHighpassHooked;

		static bool dbcheck;
	};
}