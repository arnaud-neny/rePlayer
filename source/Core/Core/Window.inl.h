#pragma once

#include "Window.h"

namespace core
{
    template <typename T>
    inline Window::Serialized<T>::Serialized(const char* name, Window* window)
    {
        window->RegisterSerializedData(m_data, name);
    }

    template <typename T>
    inline Window::Serialized<T>::Serialized(const char* name, T&& defaultValue, class Window* window)
        : m_data(std::forward<T>(defaultValue))
    {
        window->RegisterSerializedData(m_data, name);
    }

    template <typename T>
    inline void Window::RegisterSerializedData(T& data, const char* name)
    {
        if constexpr (std::is_same<T, float>::value)
            m_serializedData.Add({ name, &data, 4, SerializedData::kFloat });
        else if constexpr (std::is_same<T, int8_t>::value)
            m_serializedData.Add({ name, &data, 1, SerializedData::kInt8_t });
        else if constexpr (std::is_same<T, uint8_t>::value)
            m_serializedData.Add({ name, &data, 1, SerializedData::kUint8_t });
        else if constexpr (std::is_same<T, int16_t>::value)
            m_serializedData.Add({ name, &data, 2, SerializedData::kInt16_t });
        else if constexpr (std::is_same<T, uint16_t>::value)
            m_serializedData.Add({ name, &data, 2, SerializedData::kUint16_t });
        else if constexpr (std::is_same<T, int32_t>::value)
            m_serializedData.Add({ name, &data, 4, SerializedData::kInt32_t });
        else if constexpr (std::is_same<T, uint32_t>::value)
            m_serializedData.Add({ name, &data, 4, SerializedData::kUint32_t });
        else if constexpr (std::is_same<T, bool>::value)
            m_serializedData.Add({ name, &data, 1, SerializedData::kBool });
        else if constexpr (std::is_same<T, std::string>::value)
            m_serializedData.Add({ name, &data, sizeof(std::string), SerializedData::kString });
        else if constexpr (std::is_array<T>::value)
        {
            typedef typename std::remove_all_extents_t<T> TT;
            if constexpr (std::is_same<TT, char>::value)
                m_serializedData.Add({ name, data, sizeof(data), SerializedData::kChars, sizeof(data) });
            else
                static_assert(std::is_empty<T>::value && "Array not implemented");
        }
        else if constexpr (std::is_enum<T>::value)
        {
            typedef typename std::underlying_type_t<T> TT;
            if constexpr (std::is_same<TT, int8_t>::value)
                m_serializedData.Add({ name, &data, 1, SerializedData::kInt8_t });
            else if constexpr (std::is_same<TT, uint8_t>::value)
                m_serializedData.Add({ name, &data, 1, SerializedData::kUint8_t });
            else if constexpr (std::is_same<TT, int16_t>::value)
                m_serializedData.Add({ name, &data, 2, SerializedData::kInt16_t });
            else if constexpr (std::is_same<TT, uint16_t>::value)
                m_serializedData.Add({ name, &data, 2, SerializedData::kUint16_t });
            else if constexpr (std::is_same<TT, int32_t>::value)
                m_serializedData.Add({ name, &data, 4, SerializedData::kInt32_t });
            else if constexpr (std::is_same<TT, uint32_t>::value)
                m_serializedData.Add({ name, &data, 4, SerializedData::kUint32_t });
            else
                static_assert(std::is_empty<T>::value && "Enum not implemented");
        }
        else // I can't do a static_assert(0), so I trick it with is_empty, as we can't serialize this kind of data anyway
            static_assert(std::is_empty<T>::value && "Type not implemented");
    }

    template <typename T, typename F>
    inline void Window::RegisterSerializedData(T& data, const char* name, OnLoadedCallback<F> callback)
    {
        static_assert(std::is_base_of<Window, F>::value);
        RegisterSerializedData(data, name);
        m_serializedData.Last().onLoadedCallback = static_cast<OnLoadedCallback<Window>>(callback);
    }

    template <typename T>
    inline void Window::RegisterSerializedData(T& data, const char* name, OnLoadedCallbackStatic callback, uintptr_t userData)
    {
        RegisterSerializedData(data, name);
        m_serializedData.Last().isStaticCallback = 1;
        m_serializedData.Last().userData = userData;
        m_serializedData.Last().onLoadedStaticCallback = callback;
    }
}
//namespace core