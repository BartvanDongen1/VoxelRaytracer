/// @ref core
/// @file glm/ext/vector_float2.hpp

#pragma once
#include "../detail/type_vec2.hpp"

namespace glm
{
	/// @addtogroup core_vector
	/// @{

	/// 2 components vector of single-precision floating-point numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	typedef vec<2, float, defaultp>	vec2;

	/// <summary>
	/// Saves the glm::vec2 class
	/// </summary>
	/// <typeparam name="Archive">Cereal data type</typeparam>
	/// <param name="a_Archive">Cereal data type</param>
	/// <param name="a_Vec2">glm::vec2 data type</param>
	template<class Archive>
	void serialize(Archive& a_Archive, vec2& a_Vec2)
	{
		a_Archive(a_Vec2.x, a_Vec2.y);
	}

	/// @}
}//namespace glm
