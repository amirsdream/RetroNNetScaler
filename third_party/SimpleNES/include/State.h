#pragma once
// Save-state serializer: one state(StateIO&) method per class handles both directions.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

namespace sn
{
class StateIO
{
public:
    // Saving appends to buf; loading reads buf from the start.
    StateIO(std::vector<std::uint8_t>& buf, bool saving)
      : m_buf(buf)
      , m_saving(saving)
    {
    }

    bool saving() const { return m_saving; }
    bool ok() const { return m_ok; }

    template <class T>
    void pod(T& v)
    {
        static_assert(std::is_trivially_copyable<T>::value, "pod() needs a trivially copyable type");
        raw(&v, sizeof(T));
    }

    void bytes(std::vector<std::uint8_t>& v)
    {
        std::uint32_t n = static_cast<std::uint32_t>(v.size());
        pod(n);
        if (!m_saving)
        {
            if (!m_ok || n > m_buf.size() - m_pos)
            {
                m_ok = false;
                return;
            }
            v.resize(n);
        }
        raw(v.data(), n);
    }

    // Pointer into a fixed buffer (e.g. PRG ROM), stored as an offset.
    void ptr(const std::uint8_t*& p, const std::vector<std::uint8_t>& base)
    {
        std::int64_t off = p ? static_cast<std::int64_t>(p - base.data()) : -1;
        pod(off);
        if (!m_saving && m_ok)
            p = off < 0 ? nullptr : base.data() + off;
    }

private:
    void raw(void* data, std::size_t n)
    {
        if (m_saving)
        {
            const auto* b = static_cast<const std::uint8_t*>(data);
            m_buf.insert(m_buf.end(), b, b + n);
        }
        else if (m_ok && n <= m_buf.size() - m_pos)
        {
            std::memcpy(data, m_buf.data() + m_pos, n);
            m_pos += n;
        }
        else
            m_ok = false;
    }

    std::vector<std::uint8_t>& m_buf;
    std::size_t                m_pos = 0;
    bool                       m_saving;
    bool                       m_ok = true;
};
}
