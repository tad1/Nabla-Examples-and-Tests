// This is a magical header that provides most HLSL types and intrinsics in C++
#include "nbl/builtin/hlsl/cpp_compat.hlsl"

NBL_CONSTEXPR uint32_t WorkgroupSize = 256;

struct PushConstants
{
	bool withGizmo;
	bool padding[3]; //! size of PushConstants must be multiple of 4
};