/**
* @author:  Egorov Sergey <listrid@yandex.ru>
**/
#pragma once
#ifndef _tdFile_WCache_HPP_
#define _tdFile_WCache_HPP_
#include "tdFile.hpp"

class tdFile_WCache
{
public:
    tdFile_WCache(void* buffer, size_t bufferSize)
    {
        m_myByffer   = false;
        m_file       = NULL;
        m_buffer     = (uint8_t*)buffer;
        m_SIZE_CACHE = bufferSize-64;
        m_useBuf = 0;
    }
    tdFile_WCache(size_t cacheSize)
    {
        m_myByffer   = true;
        m_file       = NULL;
        m_buffer     = (uint8_t*)malloc(cacheSize+64);
        m_SIZE_CACHE = cacheSize;
        m_useBuf = 0;
    }
    ~tdFile_WCache()
    {
        if(m_myByffer)
            free(m_buffer);
    }
    inline bool Init(tdFile* file, uint64_t pos, bool multiUse = false)
    {
        m_file   = file;
        m_useBuf = 0;
        m_pos    = pos;
        m_multi  = multiUse;
        return m_file->SetPos(m_pos);
    }
    inline bool Close()
    {
        bool ret = true;
        if(m_file && m_useBuf)
            ret = Flush();
        m_file = NULL;
        return ret;
    }
    size_t Write(const void* data, size_t len)
    {
        if((m_useBuf+len) > m_SIZE_CACHE)
        {
            if(m_multi)
            {
                if(!m_file->SetPos(m_pos))
                    return ~0;
            }
            if(m_file->Write(m_buffer, m_useBuf) != m_useBuf)
                return ~0;
            m_pos   += m_useBuf;
            m_useBuf = 0;
            if(len > m_SIZE_CACHE)
            {
                m_pos += len;
                return m_file->Write((void*)data, len);
            }
        }
        memcpy(m_buffer + m_useBuf, data, len);
        m_useBuf += len;
        if(m_useBuf > m_SIZE_CACHE)
        {
            if(m_multi)
                m_file->SetPos(m_pos);
            m_pos += m_useBuf;
            if(m_file->Write(m_buffer, m_useBuf) != m_useBuf)
                return ~0;
            m_useBuf = 0;
        }
        return len;
    }
    size_t Write(const char* data)
    {
        return Write(data, strlen(data));
    }
    inline uint64_t GetPos()
    {
        return m_pos + m_useBuf;
    }
    inline bool SetPos(uint64_t pos)
    {
        if(!Flush())
            return false;
        m_pos = pos;
        return m_file->SetPos(m_pos);
    }
    inline bool UpdatePos()
    {
        return m_file->SetPos(m_pos);
    }
    bool Flush()
    {
        if(m_multi)
        {
            if(!m_file->SetPos(m_pos))
                return false;
        }
        if(m_useBuf)
        {
            if(m_file->Write(m_buffer, m_useBuf) != m_useBuf)
                return false;
            m_pos += m_useBuf;
            m_useBuf = 0;
            //tdFile_Flush(m_file);
        }
        return true;
    }
    inline bool zWriteU64(uint64_t value)
    {
        while(value > 0x7F)
        {
            m_buffer[m_useBuf++] = (value&0x7F)|0x80;
            value >>= 7;
        }
        m_buffer[m_useBuf++] = (uint8_t)value;
        if(m_useBuf > m_SIZE_CACHE)
        {
            if(m_multi)
            {
                if(!m_file->SetPos(m_pos))
                    return false;
            }
            if(m_file->Write(m_buffer, m_useBuf) != m_useBuf)
                return false;
            m_pos += m_useBuf;
            m_useBuf = 0;
        }
        return true;
    }
    inline bool zWriteI64(int64_t _value)
    {
        if(_value < 0)
        {
            _value = ((~_value)<<1)|1;
        }else{
            _value <<= 1;
        }
        return zWriteU64((uint64_t)_value);
    }
    inline bool zWriteU32(uint32_t value)
    {
        while(value > 0x7F)
        {
            m_buffer[m_useBuf++] = (value&0x7F)|0x80;
            value >>= 7;
        }
        m_buffer[m_useBuf++] = (uint8_t)value;
        if(m_useBuf > m_SIZE_CACHE)
        {
            if(m_multi)
            {
                if(!m_file->SetPos(m_pos))
                    return false;
            }
            if(m_file->Write(m_buffer, m_useBuf) != m_useBuf)
                return false;
            m_pos += m_useBuf;
            m_useBuf = 0;
        }
        return true;
    }
    inline bool zWriteI32(int32_t _value)
    {
        if(_value < 0)
        {
            _value = ((~_value)<<1)|1;
        }else{
            _value <<= 1;
        }
        return zWriteU32((uint32_t)_value);
    }
    bool Copy(tdFile* file, uint64_t pos, uint64_t len)
    {
        if(!Flush())
            return false;
        if(!file->SetPos(pos))
            return false;
        size_t sizeCopy;
        m_pos += len;
        while(len > 0)
        {
            sizeCopy = m_SIZE_CACHE;
            if(len < m_SIZE_CACHE)
                sizeCopy = (size_t)len;
            if(file->Read(m_buffer, sizeCopy) != sizeCopy)
                return false;
            if(m_file->Write(m_buffer, sizeCopy) != sizeCopy)
                return false;
            len -= sizeCopy;
        }
        return true;
    }
private:
    tdFile*  m_file;
    size_t   m_useBuf;
    uint64_t m_pos;
    size_t   m_SIZE_CACHE;
    uint8_t* m_buffer;
    bool     m_myByffer;
    bool     m_multi;
};

#endif //_tdFile_WCache_HPP_
