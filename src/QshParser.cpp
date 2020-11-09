#include "./Common/tdFile_WCache.hpp"
#include "./Common/tdZip.h"
#include "QshParser.h"

#ifdef QSH_GZIP_TEST_CRC32
static uint32_t crc32(const uint8_t *buf, size_t length)
{
    static const uint32_t crc32tab[16] =
    {
        0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
        0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
        0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
        0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
    };
    uint32_t crc = ~0;
    for(size_t i = 0; i < length; ++i)
    {
        crc ^= buf[i];
        crc = crc32tab[crc & 0x0f] ^ (crc >> 4);
        crc = crc32tab[crc & 0x0f] ^ (crc >> 4);
    }
    return crc ^ 0xffffffff;
}
#endif

class QshReader
{
    uint8_t* m_Data;
    size_t   m_DataSize;
    size_t   m_DataPos;
    bool Open(uint8_t* fileData, size_t size);
public:
    QshReader(){ m_Data = NULL; Close(); }
    ~QshReader(){ Close(); }
    void Close()
    {
        m_gzipName[0] = 0;
        if(m_Data)
        {
            free(m_Data);
            m_Data = NULL;
        }
        m_DataPos  = 0;
        m_DataSize = 0;
        m_error    = false;
    }
    bool Open(const char* fileName);
    bool Open(tdZipReader* zip, const char* fileName);

    qsh_str  ReadStr();
    uint8_t  ReadByte();
    uint16_t ReadShort();
    uint32_t ReadU32();
    int64_t  ReadI64();
    int64_t  ReadGrowing(int64_t prev);
    int64_t  ReadDateTime();
    double   ReadDouble();

    char m_gzipName[64];   // имя файла в gzip ( "\0" если нет)
    bool m_error;
};


bool QshReader::Open(uint8_t* fileData, size_t size)
{
    if(fileData[0] == 0x1F && fileData[1] == 0x8B && fileData[2] == 8)//DEFLATE 8
    {//gzip
        uint8_t  flags = fileData[3];
        uint32_t stamp = uint32_t(fileData[4]) |
                         uint32_t(fileData[5] << 8) |
                         uint32_t(fileData[6] << 16) |
                         uint32_t(fileData[7] << 24);
        // header[8]  Ignore extra flags for the moment
        // header[9]  Ignore OS type for the moment
        size_t pos = 10;
        if((flags & 0x08) != 0)//ORIG_NAME    0x08
        {
            size_t i = 0;
            while(i < 64 && fileData[pos])
                m_gzipName[i++] = fileData[pos++];
            if(i == 64)
            {
                free(fileData);
                return false;
            }
            m_gzipName[i] = 0;
            pos++;
        }
        uint32_t* t = (uint32_t*)(fileData + size-8);
        uint32_t realSize = t[1], realSizeData;
        if(realSize > 0x20000000) // 512 Mg 
        {
            free(fileData);
            return false;
        }
        uint8_t* new_fileData = (uint8_t*)malloc(realSize);
        tdInflate(fileData+pos, size - pos, new_fileData, &realSize, &realSizeData);
        if(realSizeData != (size - pos - 8))
        {
            free(fileData);
            free(new_fileData);
            return false;
        }
#ifdef QSH_GZIP_TEST_CRC32
        if(t[0] != crc32(new_fileData, realSize))
        {
            free(fileData);
            free(new_fileData);
            return false;
        }
#endif
        free(fileData);
        fileData = new_fileData;
        size = realSize;
    }
    m_Data = fileData;
    m_DataSize = size;
    if(memcmp("QScalp History Data", m_Data, 19) != 0 || m_Data[19] != 4)
    {
        Close();
        return false;
    }
    m_DataPos = 20;
    return true;
}


bool QshReader::Open(const char* fileName)
{
    Close();
    tdFile file;
    if(!file.Open(fileName, tdFile::r))
        return false;
    size_t size = (size_t)file.Size();
    if(size < 64)
        return false;
    uint8_t* fileData = (uint8_t*)malloc(size);
    if(file.Read(fileData, size) != size)
    {
        free(fileData);
        return false;
    }
    return Open(fileData, size);
}


bool QshReader::Open(tdZipReader* zip, const char* fileName)
{
    Close();
    int fileID = zip->GetFileID(fileName);
    if(fileID < 0)
        return false;
    int size = zip->GetFileSize(fileID);
    if(size < 64)
        return false;
    uint8_t* fileData = (uint8_t*)malloc(size);
    if(!zip->ReadFile(fileID, fileData))
    {
        free(fileData);
        return false;
    }
    return Open(fileData, size);
}


uint8_t QshReader::ReadByte()
{
    if(m_DataPos >= m_DataSize)
    {
        m_error = true;
        return 0;
    }
    return m_Data[m_DataPos++];
}


uint16_t QshReader::ReadShort()
{
    uint16_t ret;
    if(m_DataPos+sizeof(ret) > m_DataSize)
    {
        m_error = true;
        return 0;
    }
    memcpy(&ret, &m_Data[m_DataPos], sizeof(ret));
    m_DataPos += sizeof(ret);
    return ret;
}


uint32_t QshReader::ReadU32()
{
    uint32_t value = 0;
    size_t i = 0;
    if(m_DataPos >= m_DataSize)
    {
        m_error = true;
        return 0;
    }
    while(m_Data[m_DataPos]&0x80)
    {
        value = value|((m_Data[m_DataPos++]&0x7F)<<i);
        if(m_DataPos == m_DataSize)
        {
            m_error = true;
            return 0;
        }
        i +=7;
    }
    return value | (m_Data[m_DataPos++]<<i);
}


int64_t QshReader::ReadI64()
{
    int64_t value = 0;
    size_t shift = 0;
    do
    {
        if(m_DataPos >= m_DataSize)
        {
            m_error = true;
            return 0;
        }
        value |= (int64_t)(m_Data[m_DataPos]&0x7F)<<shift;
        shift +=7;
    } while(m_Data[m_DataPos++]&0x80);
    if(shift < 64 && (m_Data[m_DataPos-1] & 0x40))
        value |= (-1LL) << shift;
    return value;
}


qsh_str QshReader::ReadStr()
{
    qsh_str outStr;
    outStr.len = ReadU32();
    outStr.data = (char*)&m_Data[m_DataPos];
    m_DataPos += outStr.len;
    if(m_DataPos > m_DataSize)
    {
        m_error = true;
        outStr.len = 0;
    }
    return outStr;
}


int64_t QshReader::ReadDateTime()
{
    if(m_DataPos+8 > m_DataSize)
    {
        m_error = true;
        return 0;
    }
    int64_t ret;
    memcpy(&ret, &m_Data[m_DataPos], 8);
    m_DataPos += 8;
    return ret;
}


double QshReader::ReadDouble()
{
    if(m_DataPos+8 > m_DataSize)
    {
        m_error = true;
        return 0;
    }
    double ret;
    memcpy(&ret, &m_Data[m_DataPos], 8);
    m_DataPos += 8;
    return ret;
}


int64_t QshReader::ReadGrowing(int64_t prev)
{
    int64_t ret = this->ReadU32();
    if(ret == 0x0FFFFFFF)//268435455
         return prev + this->ReadI64();
    return prev + ret;
}


//==================================================================================================================================================================
//==================================================================================================================================================================
//==================================================================================================================================================================
//==================================================================================================================================================================


class QshParser_Quotes
{
private:
    int64_t m_QuotesPrevPrice;
    void Del(int64_t price);
    void Add(int64_t price, int64_t volume);
public:
    QshParser_Quotes(qsh_str streameInfo);
    ~QshParser_Quotes();
    struct Quote
    {
        int64_t   m_price;
        int64_t   m_volume;
    };
    struct Quotes
    {
        size_t m_count;     // текущий размер
        size_t m_allocSize; // сколько выделено
        Quote* m_quote;
    };
    bool parse(QshReader* reader);

    qsh_str m_tick;
    double  m_d_tick;
    size_t  m_size; // размер стакана
    Quotes  m_q;
};


class QshParser_Deals
{
    enum DealFlags
    {
        DealFlags_MaskType = 0x03,
        DealFlags_DateTime = 0x04,
        DealFlags_Id       = 0x08,
        DealFlags_OrderId  = 0x10,
        DealFlags_Price    = 0x20,
        DealFlags_Volume   = 0x40,
        DealFlags_OI       = 0x80
    };
public:
    QshParser_Deals(qsh_str streameInfo)
    {
        size_t pos = streameInfo.len;
        if(pos)
        {
            while(pos && streameInfo.data[pos-1] != ':')
                pos--;
            m_tick.data = &streameInfo.data[pos];
            m_tick.len  = streameInfo.len - pos;
            m_d_tick    = atof(m_tick.data);
            if(m_d_tick == 0.)
                m_d_tick = 1.;
        } else{
            m_d_tick = 1.;
            m_tick.Clear();
        }
        m_DateTime = m_Id = m_OI = m_OrderId = m_Price = m_Volume = 0;
    }
    bool parse(QshReader* reader)
    {
        m_flags = (DealFlags)reader->ReadByte();
        if((m_flags & DealFlags::DealFlags_DateTime) != 0)
            m_DateTime = reader->ReadGrowing(m_DateTime);
        if((m_flags & DealFlags::DealFlags_Id) != 0)
            m_Id = reader->ReadGrowing(m_Id);
        if((m_flags & DealFlags::DealFlags_OrderId) != 0)
            m_OrderId += reader->ReadI64();
        if((m_flags & DealFlags::DealFlags_Price) != 0)
            m_Price += reader->ReadI64();
        if((m_flags & DealFlags::DealFlags_Volume) != 0)
            m_Volume = reader->ReadI64();
        if((m_flags & DealFlags::DealFlags_OI) != 0)
            m_OI += reader->ReadI64();
        return !reader->m_error;
    }

    DealFlags m_flags;
    int64_t m_DateTime;
    int64_t m_Id;
    int64_t m_OrderId;
    int64_t m_Price;
    int64_t m_Volume;
    int64_t m_OI;

    qsh_str m_tick;
    double  m_d_tick;
};


class QshParser_OwnOrders
{
    enum OrderFlags
    {
        OrderFlags_DropAll  = 0x01,
        OrderFlags_Active   = 0x02,
        OrderFlags_External = 0x04,
        OrderFlags_Stop     = 0x08,
    };
public:
    QshParser_OwnOrders(qsh_str streameInfo)
    {
        size_t pos = streameInfo.len;
        if(pos)
        {
            while(pos && streameInfo.data[pos-1] != ':')
                pos--;
            m_tick.data = &streameInfo.data[pos];
            m_tick.len  = streameInfo.len - pos;
            m_d_tick    = atof(m_tick.data);
            if(m_d_tick == 0.)
                m_d_tick = 1.;
        }else{
            m_d_tick = 1.;
            m_tick.Clear();
        }
    }
    bool parse(QshReader* reader)
    {
        m_flags = (OrderFlags)reader->ReadByte();
        if((m_flags & OrderFlags::OrderFlags_DropAll) != 0)
        {
            m_Type  = QshParser_OwnOrderType::OwnOrderType_None;
            m_Price = m_Id = m_Quantity = 0;
        } else{
            if((m_flags & OrderFlags::OrderFlags_Active) != 0)
            {
                if((m_flags & OrderFlags::OrderFlags_Stop) != 0)
                    m_Type = QshParser_OwnOrderType::OwnOrderType_Stop;
                else
                    m_Type = QshParser_OwnOrderType::OwnOrderType_Regular;
            }else{
                m_Type = QshParser_OwnOrderType::OwnOrderType_None;
            }
            m_Id       = reader->ReadI64();
            m_Price    = reader->ReadI64();
            m_Quantity = reader->ReadI64();
        }
        return !reader->m_error;
    }
    OrderFlags m_flags;

    QshParser_OwnOrderType m_Type;
    int64_t m_Id;
    int64_t m_Price;
    int64_t m_Quantity;

    qsh_str m_tick;
    double  m_d_tick;
};


class QshParser_OwnTrades
{
public:
    QshParser_OwnTrades(qsh_str streameInfo)
    {
        size_t pos = streameInfo.len;
        if(pos)
        {
            while(pos && streameInfo.data[pos-1] != ':')
                pos--;
            m_tick.data = &streameInfo.data[pos];
            m_tick.len  = streameInfo.len - pos;
            m_d_tick    = atof(m_tick.data);
            if(m_d_tick == 0.)
                m_d_tick = 1.;
        } else{
            m_d_tick = 1.;
            m_tick.Clear();
        }
        m_DateTime = m_TradeId = m_OrderId = m_Price = 0;
    }
    bool parse(QshReader* reader)
    {
        m_DateTime = reader->ReadGrowing(m_DateTime);
        m_TradeId += reader->ReadI64();
        m_OrderId += reader->ReadI64();
        m_Price   += reader->ReadI64();
        m_Quantity = reader->ReadI64();
        return !reader->m_error;
    }
    int64_t m_DateTime;
    int64_t m_TradeId;
    int64_t m_OrderId;
    int64_t m_Price;
    int64_t m_Quantity;

    qsh_str m_tick;
    double  m_d_tick;
};


QshParser_Quotes::QshParser_Quotes(qsh_str streameInfo)
{
    size_t pos = streameInfo.len;
    if(pos)
    {
        while(pos && streameInfo.data[pos-1] != ':')
            pos--;
        m_tick.data = &streameInfo.data[pos];
        m_tick.len  = streameInfo.len - pos;
        m_d_tick    = atof(m_tick.data);
        if(m_d_tick == 0.)
            m_d_tick = 1.;
    }else{
        m_d_tick = 1.;
        m_tick.Clear();
    }
    m_q.m_quote = NULL;
    m_QuotesPrevPrice = 0;
}


QshParser_Quotes::~QshParser_Quotes()
{
    if(m_q.m_quote)
        delete[] m_q.m_quote;
}


void QshParser_Quotes::Del(int64_t price)
{
    size_t index = m_q.m_count;
    for(size_t i = 0; i < m_q.m_count; i++)
    {
        if(m_q.m_quote[i].m_price == price)
        {
            index = i;
            break;
        }
    }
    if(m_q.m_count != index)
    {
        size_t size = (m_q.m_count - index - 1)*sizeof(QshParser_Quotes::Quote);
        if(size)
            memcpy(&m_q.m_quote[index], &m_q.m_quote[index+1], size);
        m_q.m_count--;
    }
}


void QshParser_Quotes::Add(int64_t price, int64_t volume)
{
    size_t index;
    for(index = 0; index < m_q.m_count; index++)
    {
        if(m_q.m_quote[index].m_price <= price)
            break;
    }
    if(m_q.m_count == m_q.m_allocSize)
    {
        m_q.m_allocSize += 10;
        Quote* new_quote = new Quote[m_q.m_allocSize+1];
        memcpy(new_quote, m_q.m_quote, sizeof(Quote)*m_q.m_count);
        delete[] m_q.m_quote;
        m_q.m_quote = new_quote;
    }
    if(index == m_q.m_count)
    {//добавить в конец
        m_q.m_quote[index].m_price  = price;
        m_q.m_quote[index].m_volume = volume;
        m_q.m_count++;
        return;
    }
    if(m_q.m_quote[index].m_price < price)
    {// раздвинуть
        for(size_t i = m_q.m_count; i > index ; i--)
        {
            m_q.m_quote[i].m_price  = m_q.m_quote[i-1].m_price;
            m_q.m_quote[i].m_volume = m_q.m_quote[i-1].m_volume;
        }
        m_q.m_count++;
    }
    // обновить
    m_q.m_quote[index].m_price  = price;
    m_q.m_quote[index].m_volume = volume;
}


bool QshParser_Quotes::parse(QshReader* reader)
{
    static int t = 0;

    size_t count = (size_t)reader->ReadI64();
    if(count > 128)
        return false;
    if(!m_q.m_quote)
    {
        m_q.m_count = 0;
        m_size = count;
        m_q.m_allocSize = count+10;
        m_q.m_quote = new Quote[m_q.m_allocSize+1];
    }
    for(size_t i = 0; i < count; i++)
    {
        m_QuotesPrevPrice += reader->ReadI64();
        int64_t volume = reader->ReadI64();
        if(volume == 0)
        {
            this->Del(m_QuotesPrevPrice);
        }else{
            this->Add(m_QuotesPrevPrice, volume);
        }
    }
    return !reader->m_error;
}


class QshParser_AuxInfo
{
    enum AuxInfoFlags
    {
        AuxInfoFlags_DateTime = 0x01,
        AuxInfoFlags_AskTotal = 0x02,
        AuxInfoFlags_BidTotal = 0x04,
        AuxInfoFlags_OI       = 0x08,
        AuxInfoFlags_Price    = 0x10,
        AuxInfoFlags_SessionInfo = 0x20,
        AuxInfoFlags_Rate      = 0x40,
        AuxInfoFlags_Message   = 0x80,
    };
public:
    QshParser_AuxInfo(qsh_str streameInfo)
    {
        size_t pos = streameInfo.len;
        if(pos)
        {
            while(pos && streameInfo.data[pos-1] != ':')
                pos--;
            m_tick.data = &streameInfo.data[pos];
            m_tick.len  = streameInfo.len - pos;
            m_d_tick    = atof(m_tick.data);
            if(m_d_tick == 0.)
                m_d_tick = 1.;
        }else{
            m_d_tick = 1.;
            m_tick.Clear();
        }
        m_DateTime = m_Price = m_AskTotal = m_BidTotal = m_OI = 0;
        m_Deposit = m_Rate = 0.;
        m_HiLimit = m_LoLimit = 0;
    }
    bool parse(QshReader* reader)
    {
        m_flags = (AuxInfoFlags)reader->ReadByte();
        if((m_flags & AuxInfoFlags::AuxInfoFlags_DateTime) != 0)
            m_DateTime = reader->ReadGrowing(m_DateTime);
        if((m_flags & AuxInfoFlags::AuxInfoFlags_AskTotal) != 0)
            m_AskTotal += reader->ReadI64();
        if((m_flags & AuxInfoFlags::AuxInfoFlags_BidTotal) != 0)
            m_BidTotal += reader->ReadI64();
        if((m_flags & AuxInfoFlags::AuxInfoFlags_OI) != 0)
            m_OI += reader->ReadI64();
        if((m_flags & AuxInfoFlags::AuxInfoFlags_Price) != 0)
            m_Price += reader->ReadI64();
        if((m_flags & AuxInfoFlags::AuxInfoFlags_SessionInfo) != 0)
        {
            m_HiLimit = reader->ReadI64();
            m_LoLimit = reader->ReadI64();
            m_Deposit = reader->ReadDouble();
        }
        if((m_flags & AuxInfoFlags::AuxInfoFlags_Rate) != 0)
            m_Rate = reader->ReadDouble();
        m_Message.Clear();
        if((m_flags & AuxInfoFlags::AuxInfoFlags_Message) != 0)
            m_Message = reader->ReadStr();
        return !reader->m_error;
    }
    AuxInfoFlags m_flags;
    int64_t m_DateTime;
    int64_t m_Price;

    int64_t m_AskTotal;
    int64_t m_BidTotal;
    int64_t m_OI;

    int64_t m_HiLimit;
    int64_t m_LoLimit;
    double m_Deposit;

    double m_Rate;
    qsh_str m_Message;

    qsh_str m_tick;
    double  m_d_tick;
};


class QshParser_OrdLog
{
    enum OrdLogEntryFlags
    {
        OrdLogEntryFlags_DateTime   = 0x01,
        OrdLogEntryFlags_OrderId    = 0x02,
        OrdLogEntryFlags_Price      = 0x04,
        OrdLogEntryFlags_Amount     = 0x08,
        OrdLogEntryFlags_AmountRest = 0x10,
        OrdLogEntryFlags_DealId     = 0x20,
        OrdLogEntryFlags_DealPrice  = 0x40,
        OrdLogEntryFlags_OI         = 0x80
    };
    int64_t m_lastOI;
    int64_t m_lastOrderId, m_lastDealId;
    int64_t m_lastDealPrice;
    int64_t m_lastAmountRest;
public:
    QshParser_OrdLog(qsh_str streameInfo)
    {
        size_t pos = streameInfo.len;
        if(pos)
        {
            while(pos && streameInfo.data[pos-1] != ':')
                pos--;
            m_tick.data = &streameInfo.data[pos];
            m_tick.len  = streameInfo.len - pos;
            m_d_tick    = atof(m_tick.data);
            if(m_d_tick == 0.)
                m_d_tick = 1.;
        }else{
            m_d_tick = 1.;
            m_tick.Clear();
        }
        m_DateTime = m_lastOI = m_lastOrderId = m_Price = m_Amount = 0;
        m_lastAmountRest = m_lastDealId = m_lastDealPrice = 0;
    }
    bool parse(QshReader* reader)
    {
        m_flags = (OrdLogEntryFlags)reader->ReadByte();
        m_OrdLogFlags = (QshParser_OrdLogFlags)reader->ReadShort();

        bool isAdd = (m_OrdLogFlags & QshParser_OrdLogFlags::OrdLogFlags_Add) != 0;
        bool isFill = (m_OrdLogFlags & QshParser_OrdLogFlags::OrdLogFlags_Fill) != 0;

        bool isBuy = (m_OrdLogFlags & QshParser_OrdLogFlags::OrdLogFlags_Buy) != 0;
        bool isSell = (m_OrdLogFlags & QshParser_OrdLogFlags::OrdLogFlags_Sell) != 0;

        if((m_flags & OrdLogEntryFlags::OrdLogEntryFlags_DateTime) != 0)
            m_DateTime = reader->ReadGrowing(m_DateTime);
        if((m_flags & OrdLogEntryFlags::OrdLogEntryFlags_OrderId) == 0)
            m_OrderId = m_lastOrderId;
        else if(isAdd)
            m_OrderId = m_lastOrderId = reader->ReadGrowing(m_lastOrderId);
        else
            m_OrderId = m_lastOrderId + reader->ReadI64();

        if((m_flags & OrdLogEntryFlags::OrdLogEntryFlags_Price) != 0)
            m_Price += reader->ReadI64();
        if((m_flags & OrdLogEntryFlags::OrdLogEntryFlags_Amount) != 0)
            m_Amount = reader->ReadI64();
        if(isFill)
        {
            if((m_flags & OrdLogEntryFlags::OrdLogEntryFlags_AmountRest) != 0)
                m_lastAmountRest = reader->ReadI64();
            if((m_flags & OrdLogEntryFlags::OrdLogEntryFlags_DealId) != 0)
                m_lastDealId = reader->ReadGrowing(m_lastDealId);
            if((m_flags & OrdLogEntryFlags::OrdLogEntryFlags_DealPrice) != 0)
                m_lastDealPrice += reader->ReadI64();
            if((m_flags & OrdLogEntryFlags::OrdLogEntryFlags_OI) != 0)
                m_lastOI += reader->ReadI64();

            m_AmountRest = m_lastAmountRest;
            m_DealId = m_lastDealId;
            m_DealPrice = m_lastDealPrice;
            m_OI = m_lastOI;
        }else{
            m_AmountRest = isAdd ? m_Amount : 0;
            m_DealId = m_DealPrice = m_OI = 0;
        }
        return !reader->m_error;
    }
    OrdLogEntryFlags      m_flags;
    QshParser_OrdLogFlags m_OrdLogFlags;

    int64_t m_DateTime;
    int64_t m_OrderId;
    int64_t m_Price;
    int64_t m_Amount;

    int64_t m_AmountRest;
    int64_t m_DealId;
    int64_t m_DealPrice;
    int64_t m_OI;

    qsh_str m_tick;
    double  m_d_tick;
};


bool QshParser::ReadOrdLog()
{
    QshParser_OrdLog* parser = m_StreamParser[m_activStream].m_OrdLogParser;
    if(!parser->parse(m_reader))
        return false;
    m_OrdLog.m_Flags    = parser->m_OrdLogFlags;
    m_OrdLog.m_tick     = parser->m_d_tick;
    m_OrdLog.m_DateTime = parser->m_DateTime;
    m_OrdLog.m_OrderId  = parser->m_OrderId;
    m_OrdLog.m_Price    = parser->m_Price * parser->m_d_tick;
    m_OrdLog.m_Amount   = parser->m_Amount;
    m_OrdLog.m_AmountRest = parser->m_AmountRest;
    m_OrdLog.m_DealId     = parser->m_DealId;
    m_OrdLog.m_DealPrice  = parser->m_DealPrice * parser->m_d_tick;
    m_OrdLog.m_OI         = parser->m_OI;
    return true;
}


bool QshParser::ReadAuxInfo()
{
    QshParser_AuxInfo* parser = m_StreamParser[m_activStream].m_AuxInfoParser;
    if(!parser->parse(m_reader))
        return false;
    m_AuxInfo.m_tick     = parser->m_d_tick;
    m_AuxInfo.m_DateTime = parser->m_DateTime;
    m_AuxInfo.m_Price    = parser->m_Price * parser->m_d_tick;
    m_AuxInfo.m_Deposit  = parser->m_Deposit;
    m_AuxInfo.m_AskTotal = parser->m_AskTotal;
    m_AuxInfo.m_BidTotal = parser->m_BidTotal;
    m_AuxInfo.m_OI       = parser->m_OI;
    m_AuxInfo.m_Rate     = parser->m_Rate;
    m_AuxInfo.m_HiLimit  = parser->m_HiLimit * parser->m_d_tick;
    m_AuxInfo.m_LoLimit  = parser->m_LoLimit * parser->m_d_tick;
    m_AuxInfo.m_Message  = parser->m_Message;
    return true;
}


bool QshParser::ReadMessages()
{
    m_Message.m_DateTime = m_reader->ReadDateTime()/10000;
    m_Message.m_Type = (QshParser_MessageType)m_reader->ReadByte();
    m_Message.m_Text = m_reader->ReadStr();
    return !m_reader->m_error;
}


bool QshParser::ReadOwnTrades()
{
    QshParser_OwnTrades* parser = m_StreamParser[m_activStream].m_OwnTradesParser;
    if(!parser->parse(m_reader))
        return false;
    m_OwnTrades.m_tick     = parser->m_d_tick;
    m_OwnTrades.m_DateTime = parser->m_DateTime;
    m_OwnTrades.m_TradeId  = parser->m_TradeId;
    m_OwnTrades.m_OrderId  = parser->m_OrderId;
    m_OwnTrades.m_Price    = parser->m_Price*parser->m_d_tick;
    m_OwnTrades.m_Quantity = parser->m_Quantity;
    return true;
}


bool QshParser::ReadOwnOrders()
{
    QshParser_OwnOrders* parser = m_StreamParser[m_activStream].m_OwnOrdersParser;
    if(!parser->parse(m_reader))
        return false;
    m_OwnOrders.m_tick  = parser->m_d_tick;
    m_OwnOrders.m_Type  = parser->m_Type;
    m_OwnOrders.m_Id    = parser->m_Id;
    m_OwnOrders.m_Price = parser->m_Price * parser->m_d_tick;
    m_OwnOrders.m_Quantity = parser->m_Quantity;
    return true;
}


bool QshParser::ReadDeals()
{
    QshParser_Deals* parser = m_StreamParser[m_activStream].m_DealsParser;
    if(!parser->parse(m_reader))
        return false;
    m_Deals.m_tick = parser->m_d_tick;
    m_Deals.m_DateTime = parser->m_DateTime;
    m_Deals.m_Id       = parser->m_Id;
    m_Deals.m_OrderId  = parser->m_OrderId;
    m_Deals.m_Type     = (QshParser_DealType)(parser->m_flags&3);
    m_Deals.m_Price    = parser->m_Price*parser->m_d_tick;
    m_Deals.m_Volume   = parser->m_Volume;
    m_Deals.m_OI       = parser->m_OI;
    return true;
}


bool QshParser::ReadQuotes()
{
    QshParser_Quotes* parser = m_StreamParser[m_activStream].m_QuotesParser;
    if(!parser->parse(m_reader))
        return false;
    size_t count = parser->m_size;
    if(m_Quotes.m_countMax < count)
    {
        if(m_Quotes.m_quote)
            delete[] m_Quotes.m_quote;
        m_Quotes.m_countMax = count;
        m_Quotes.m_quote    = new Quote[m_Quotes.m_countMax];
    }
    if(count > parser->m_q.m_count)
        count = parser->m_q.m_count;

    size_t iAsk, iBid;
    double tick = parser->m_d_tick;
    m_Quotes.m_tick = tick;
    for(iAsk = 0; iAsk < count; iAsk++)
    {
        if(parser->m_q.m_quote[iAsk].m_volume < 0)
            break;
        m_Quotes.m_quote[iAsk].m_volume = parser->m_q.m_quote[iAsk].m_volume;
        m_Quotes.m_quote[iAsk].m_price  = parser->m_q.m_quote[iAsk].m_price*tick;
        m_Quotes.m_quote[iAsk].m_type   = QshParser_QuoteType::QuoteType_Ask;
    }
    if(iAsk)
        m_Quotes.m_quote[iAsk-1].m_type = QshParser_QuoteType::QuoteType_BestAsk;
    iBid = iAsk;
    for(size_t i = iBid; i < count; i++)
    {
        int64_t v = parser->m_q.m_quote[i].m_volume;
        if(v > 0)
            continue;
        m_Quotes.m_quote[iBid].m_volume = -parser->m_q.m_quote[i].m_volume;
        m_Quotes.m_quote[iBid].m_price  = parser->m_q.m_quote[i].m_price*tick;
        m_Quotes.m_quote[iBid].m_type   = QshParser_QuoteType::QuoteType_Bid;
        iBid++;
    }
    if(iAsk < iBid)
        m_Quotes.m_quote[iAsk].m_type = QshParser_QuoteType::QuoteType_BestBid;
    for(size_t i = iBid; i < count; i++)
    {
        m_Quotes.m_quote[i].m_volume = 0;
        m_Quotes.m_quote[i].m_price  = 0;
        m_Quotes.m_quote[i].m_type   = QshParser_QuoteType::QuoteType_Unknown;
    }
    m_Quotes.m_count = count;
    return true;
}


QshParser::QshParser()
{
    m_reader = new QshReader();
    m_StreamCount = 0;
    memset(&m_Quotes, 0, sizeof(m_Quotes));
    Close();
}


QshParser::~QshParser()
{
    delete m_reader;
    if(m_Quotes.m_quote)
        delete[] m_Quotes.m_quote;
}


bool QshParser::OpenEx()
{
    m_AppName = m_reader->ReadStr();
    m_Comment = m_reader->ReadStr();
    if(m_reader->m_error)
    {
        Close();
        return false;
    }
    m_RecDateTime = m_reader->ReadDateTime()/10000;
    m_AppName.data[m_AppName.len] = 0;
    m_Comment.data[m_Comment.len] = 0;

    if(m_reader->m_error)
    {
        Close();
        return false;
    }
    m_StreamCount = m_reader->ReadByte();
    if(m_StreamCount > 8 || !m_StreamCount)
    {
        Close();
        return false;
    }
    for(size_t i = 0; i < m_StreamCount; i++)
    {
        if(m_reader->m_error)
            break;
        m_StreamType[i] = (QshParser_StreamType)m_reader->ReadByte();
        m_StreamEntry[i].Clear();
        if(m_StreamType[i] != StreamType_Messages)
            m_StreamEntry[i] = m_reader->ReadStr();
    }
    m_activDateTime   = m_RecDateTime;
    m_activStreamType = m_StreamType[0];
    ReadNextRecordHeader();
    if(m_reader->m_error)
    {
        Close();
        return false;
    }
    for(size_t i = 0; i < m_StreamCount; i++)
    {
        m_StreamEntry[i].data[m_StreamEntry[i].len] = 0;
        switch(m_StreamType[i])
        {
            case QshParser_StreamType::StreamType_Quotes:    m_StreamParser[i].m_QuotesParser    = new QshParser_Quotes(m_StreamEntry[i]); break;
            case QshParser_StreamType::StreamType_Deals:     m_StreamParser[i].m_DealsParser     = new QshParser_Deals(m_StreamEntry[i]); break;
            case QshParser_StreamType::StreamType_OwnOrders: m_StreamParser[i].m_OwnOrdersParser = new QshParser_OwnOrders(m_StreamEntry[i]); break;
            case QshParser_StreamType::StreamType_OwnTrades: m_StreamParser[i].m_OwnTradesParser = new QshParser_OwnTrades(m_StreamEntry[i]); break;
            case QshParser_StreamType::StreamType_AuxInfo:   m_StreamParser[i].m_AuxInfoParser   = new QshParser_AuxInfo(m_StreamEntry[i]); break;
            case QshParser_StreamType::StreamType_OrdLog:    m_StreamParser[i].m_OrdLogParser    = new QshParser_OrdLog(m_StreamEntry[i]); break;
            default: m_StreamParser[i].m_QuotesParser = NULL;break;
        }
    }
    m_initReadHeader = true;
    m_activStream    = 0;
    return true;
}



bool QshParser::Open(const char* fileName)
{
    if(!m_reader->Open(fileName))
        return false;
    return OpenEx();
}


bool QshParser::Open(tdZipReader* zip, const char* fileName)
{
    if(!m_reader->Open(zip, fileName))
        return false;
    return OpenEx();
}


void QshParser::Close()
{
    for(size_t i = 0; i < m_StreamCount; i++)
    {
        switch(m_StreamType[i])
        {
            case QshParser_StreamType::StreamType_Quotes:    delete m_StreamParser[i].m_QuotesParser; break;
            case QshParser_StreamType::StreamType_Deals:     delete m_StreamParser[i].m_DealsParser; break;
            case QshParser_StreamType::StreamType_OwnOrders: delete m_StreamParser[i].m_OwnOrdersParser; break;
            case QshParser_StreamType::StreamType_OwnTrades: delete m_StreamParser[i].m_OwnTradesParser; break;
            case QshParser_StreamType::StreamType_AuxInfo:   delete m_StreamParser[i].m_AuxInfoParser; break;
            case QshParser_StreamType::StreamType_OrdLog:    delete m_StreamParser[i].m_OrdLogParser; break;
        }
    }
    m_StreamCount = 0;
    m_AppName.Clear();
    m_Comment.Clear();
    memset(m_StreamEntry,  0, sizeof(m_StreamEntry));
    memset(m_StreamType,   0, sizeof(m_StreamType));
    memset(m_StreamParser, 0, sizeof(m_StreamParser));
    memset(m_StreamType,   0, sizeof(m_StreamType));
}


void QshParser::ReadNextRecordHeader()
{
    m_activDateTime = m_reader->ReadGrowing(m_activDateTime);
    if(m_StreamCount > 1)
    {
        m_activStream     = m_reader->ReadByte();
        m_activStreamType = m_StreamType[m_activStream];
    }
}


bool QshParser::Read()
{
    if(m_initReadHeader)
    {
        m_initReadHeader = false;
    }else{
        ReadNextRecordHeader();
    }
    if(m_reader->m_error)
        return false;
    switch(m_activStreamType)
    {
        case QshParser_StreamType::StreamType_Quotes: return ReadQuotes();
        case QshParser_StreamType::StreamType_Deals:  return ReadDeals();
        case QshParser_StreamType::StreamType_OwnOrders: return ReadOwnOrders();
        case QshParser_StreamType::StreamType_OwnTrades: return ReadOwnTrades();
        case QshParser_StreamType::StreamType_Messages:  return ReadMessages();
        case QshParser_StreamType::StreamType_AuxInfo: return ReadAuxInfo();
        case QshParser_StreamType::StreamType_OrdLog:  return ReadOrdLog();
    }
    return false;
}

