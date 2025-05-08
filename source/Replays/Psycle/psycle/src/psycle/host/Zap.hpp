#pragma once
namespace psycle { namespace host {
	/// Safer version of delete that clears the pointer automatically. Don't use it for arrays!
	/// \param pointer pointer to single object to be deleted.
	/// \param new_value the new value pointer will be set to. By default it is null.
	template<typename single_object>
	single_object inline * zapObject(single_object *& pointer, single_object * const new_value = 0)
	{
		if(pointer) delete pointer;
		return pointer = new_value;
	}

	/// Safer version of delete[] that clears the pointer automatically. Only use it for arrays!
	/// \param pointer pointer to array to be deleted.
	/// \param new_value the new value pointer will be set to. By default it is null.
	template<typename object_array>
	object_array inline * zapArray(object_array *& pointer, object_array * const new_value = 0)
	{
		if(pointer) delete [] pointer;
		return pointer = new_value;
	}
}}
